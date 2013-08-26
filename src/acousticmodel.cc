#include "acousticmodel.h"
#include "defs.h"
#include <iostream>

using namespace TASCAR;
using namespace TASCAR::Acousticmodel;


void sink_t::clear()
{
  for(uint32_t ch=0;ch<outchannels.size();ch++)
    outchannels[ch].clear();
}

void sink_t::update_refpoint(const pos_t& psrc, pos_t& prel, double& distance, double& gain)
{
  prel = psrc;
  prel -= position;
  prel /= orientation;
  distance = prel.norm();
  gain = 1.0/std::max(0.1,distance);
}

pointsource_t::pointsource_t(uint32_t chunksize)
  : audio(chunksize), active(true)
{
}

pointsource_t::~pointsource_t()
{
}

doorsource_t::doorsource_t(uint32_t chunksize)
  : pointsource_t(chunksize),
    falloff(1.0)
{
}

void doorsource_t::update_effective_position(const pos_t& sinkp,pos_t& srcpos,double& gain)
{
  srcpos = nearest(sinkp);
  pos_t sinkn(srcpos);
  sinkn -= sinkp;
  gain *= std::max(0.0,-dot_prod(sinkn.normal(),normal));
  gain *= 0.5-0.5*cos(M_PI*std::min(1.0,distance(srcpos,sinkp)*falloff));
}

void pointsource_t::update_effective_position(const pos_t& sinkp,pos_t& srcpos,double& gain)
{
}

diffuse_source_t::diffuse_source_t(uint32_t chunksize)
  : audio(chunksize),
    falloff(1.0),
    active(true)
{
}

acoustic_model_t::acoustic_model_t(double fs,pointsource_t* src,sink_t* sink,const std::vector<obstacle_t*>& obstacles)
  : src_(src),sink_(sink),obstacles_(obstacles),
    audio(src->audio.size()),
    chunksize(audio.size()),
    dt(1.0/chunksize),
    distance(1.0),
    gain(1.0),
    dscale(fs/(340.0*7782.0)),
    delayline(480000,fs,340.0),
    airabsorption_state(0.0),
    sink_data(sink_->create_sink_data())
{
  //DEBUG(audio.size());
  //DEBUG(dt);
  pos_t prel;
  sink_->update_refpoint(src_->position,prel,distance,gain);
  //distance = sink_->relative_position(src_->position).norm();
  //DEBUG(distance);
  //DEBUG(gain);
}

acoustic_model_t::~acoustic_model_t()
{
  if( sink_data )
    delete sink_data;
}
 
void acoustic_model_t::process()
{
  if( sink_->active && src_->active ){
    pos_t prel;
    double nextdistance(0.0);
    double nextgain(1.0);
    // calculate relative geometry between source and sink:
    pos_t effective_srcpos(src_->position);
    double srcgainmod(1.0);
    src_->update_effective_position(sink_->position,effective_srcpos,srcgainmod);
    sink_->update_refpoint(effective_srcpos,prel,nextdistance,nextgain);
    nextgain *= srcgainmod;
    double next_air_absorption(exp(-nextdistance*dscale));
    double ddistance((nextdistance-distance)*dt);
    double dgain((nextgain-gain)*dt);
    double dairabsorption((next_air_absorption-air_absorption)*dt);
    for(uint32_t k=0;k<chunksize;k++){
      distance+=ddistance;
      gain+=dgain;
      delayline.push(src_->audio[k]);
      float c1(air_absorption+=dairabsorption);
      float c2(1.0f-c1);
      // apply air absorption:
      airabsorption_state = c2*airabsorption_state+c1*delayline.get_dist(distance)*gain;
      audio[k] = airabsorption_state;
    }
    sink_->add_source(prel,audio,sink_data);
  }
}


mirrorsource_t::mirrorsource_t(pointsource_t* src,reflector_t* reflector)
  : pointsource_t(src->audio.size()),
    src_(src),reflector_(reflector)
{
}

void mirrorsource_t::process()
{
  pos_t nearest_point(reflector_->nearest_on_plane(src_->position));
  //DEBUG(nearest_point.print_cart());
  nearest_point -= src_->position;
  //DEBUG(nearest_point.print_cart());
  //double r(dot_prod(nearest_point.normal(),reflector_->get_normal()));
  position = nearest_point;
  position *= 2.0;
  position += src_->position;
  ////DEBUG(r);
  //DEBUG(position.print_cart());
  // todo: add reflection (no diffraction) processing:
  for(uint32_t k=0;k<audio.size();k++)
    audio[k] = src_->audio[k];
}

mirror_model_t::mirror_model_t(const std::vector<pointsource_t*>& pointsources,
                               const std::vector<reflector_t*>& reflectors)
{
  for(uint32_t ksrc=0;ksrc<pointsources.size();ksrc++)
    for(uint32_t kmir=0;kmir<reflectors.size();kmir++)
      mirrorsource.push_back(mirrorsource_t(pointsources[ksrc],reflectors[kmir]));
}

void mirror_model_t::process()
{
  for(uint32_t k=0;k<mirrorsource.size();k++)
    mirrorsource[k].process();
}

std::vector<mirrorsource_t*> mirror_model_t::get_mirror_sources()
{
  std::vector<mirrorsource_t*> r;
  for(uint32_t k=0;k<mirrorsource.size();k++)
    r.push_back(&(mirrorsource[k]));
  return r;
}

std::vector<pointsource_t*> mirror_model_t::get_sources()
{
  std::vector<pointsource_t*> r;
  for(uint32_t k=0;k<mirrorsource.size();k++)
    r.push_back(&(mirrorsource[k]));
  return r;
}

world_t::world_t(double fs,const std::vector<pointsource_t*>& sources,const std::vector<diffuse_source_t*>& diffusesources,const std::vector<reflector_t*>& reflectors,const std::vector<sink_t*>& sinks)
  : mirrormodel(sources,reflectors)
{
  for(uint32_t kSrc=0;kSrc<diffusesources.size();kSrc++)
    for(uint32_t kSink=0;kSink<sinks.size();kSink++){
      //DEBUG(kSrc);
      //DEBUG(kSink);
      //DEBUG(diffusesources[kSrc]);
      //DEBUG(sinks[kSink]);
      diffuse_acoustic_model.push_back(new diffuse_acoustic_model_t(fs,diffusesources[kSrc],sinks[kSink]));
    }
  for(uint32_t kSrc=0;kSrc<sources.size();kSrc++)
    for(uint32_t kSink=0;kSink<sinks.size();kSink++)
      acoustic_model.push_back(new acoustic_model_t(fs,sources[kSrc],sinks[kSink]));
  std::vector<mirrorsource_t*> msources(mirrormodel.get_mirror_sources());
  for(uint32_t kSrc=0;kSrc<msources.size();kSrc++)
    for(uint32_t kSink=0;kSink<sinks.size();kSink++)
      acoustic_model.push_back(new acoustic_model_t(fs,msources[kSrc],sinks[kSink],std::vector<obstacle_t*>(1,msources[kSrc]->get_reflector())));
}

world_t::~world_t()
{
  for(unsigned int k=0;k<acoustic_model.size();k++)
    delete acoustic_model[k];
  for(unsigned int k=0;k<diffuse_acoustic_model.size();k++)
    delete diffuse_acoustic_model[k];
}


void world_t::process()
{
  mirrormodel.process();
  for(unsigned int k=0;k<acoustic_model.size();k++)
    acoustic_model[k]->process();
  for(unsigned int k=0;k<diffuse_acoustic_model.size();k++)
    diffuse_acoustic_model[k]->process();
}

diffuse_acoustic_model_t::diffuse_acoustic_model_t(double fs,diffuse_source_t* src,sink_t* sink)
  : src_(src),sink_(sink),
    audio(src->audio.size()),
    chunksize(audio.size()),
    dt(1.0/chunksize),
    gain(1.0),
    sink_data(sink_->create_sink_data())
{
  //DEBUG(audio.size());
  //DEBUG(dt);
  pos_t prel;
  double d(1.0);
  sink_->update_refpoint(src_->center,prel,d,gain);
  //distance = sink_->relative_position(src_->position).norm();
  //DEBUG(distance);
  //DEBUG(gain);
}

diffuse_acoustic_model_t::~diffuse_acoustic_model_t()
{
  if( sink_data )
    delete sink_data;
}
 
void diffuse_acoustic_model_t::process()
{
  if( sink_->active && src_->active ){
    pos_t prel;
    double d(0.0);
    double nextgain(1.0);
    // calculate relative geometry between source and sink:
    //DEBUG(src_->size.print_cart());
    sink_->update_refpoint(src_->center,prel,d,nextgain);
    d = src_->nextpoint(prel).norm();
    //DEBUG(d);
    nextgain = 0.5+0.5*cos(M_PI*std::min(1.0,d*src_->falloff));
    double dgain((nextgain-gain)*dt);
    for(uint32_t k=0;k<chunksize;k++){
      gain+=dgain;
      audio.w()[k] = gain*src_->audio.w()[k];
      audio.x()[k] = gain*src_->audio.x()[k];
      audio.y()[k] = gain*src_->audio.y()[k];
      audio.z()[k] = gain*src_->audio.z()[k];
    }
    sink_->add_source(prel,audio,sink_data);
  }
}



//  class mirror_model_t {
//  public:
//    mirror_model_t::mirror_model_t(const std::vector<pointsource_t*>& pointsources,
//                   const std::vector<reflector_t*>& reflectors,
//                   uint32_t order);
//    void mirror_model_t::process();
//    std::vector<pointsource_t*> mirror_model_t::get_mirrors();
//  private:
//    std::vector<mirrorsource_t> mirrorsource;
//  };
//

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */