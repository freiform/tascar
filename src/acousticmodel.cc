#include "acousticmodel.h"
#include "errorhandling.h"

using namespace TASCAR;
using namespace TASCAR::Acousticmodel;

mask_t::mask_t()
  : inv_falloff(1.0),mask_inner(false),active(true)
{
}

double mask_t::gain(const pos_t& p)
{
  double d(nextpoint(p).norm());
  d = 0.5+0.5*cos(M_PI*std::min(1.0,d*inv_falloff));
  if( mask_inner )
    return 1.0-d;
  return d;
}

diffuse_source_t::diffuse_source_t(uint32_t chunksize,TASCAR::levelmeter_t& rmslevel_)
  : audio(chunksize),
    falloff(1.0),
    active(true),
    rmslevel(rmslevel_)
{
}

void diffuse_source_t::preprocess()
{
  rmslevel.update(audio.w());
}

acoustic_model_t::acoustic_model_t(double c,double fs,uint32_t chunksize,
                                   source_t* src, receiver_t* receiver,
                                   const std::vector<obstacle_t*>& obstacles,
                                   const acoustic_model_t* parent, 
                                   const reflector_t* reflector)
  : soundpath_t( src, parent, reflector ),
    c_(c),
    fs_(fs),
    src_(src),
    receiver_(receiver),
    receiver_data(receiver_->create_data(fs,chunksize)),
    source_data(src->create_data(fs,chunksize)),
    obstacles_(obstacles),
    audio(chunksize),
    chunksize(audio.size()),
    dt(1.0/std::max(1.0f,(float)chunksize)),
    distance(1.0),
    gain(1.0),
    dscale(fs/(c_*7782.0)),
    air_absorption(0.5),
    delayline((src->maxdist/c_)*fs,fs,c_,src->sincorder,64),
  airabsorption_state(0.0),
  layergain(0.0),
  dlayergain(1.0/(receiver->layerfadelen*fs)),
  ismorder(getorder())
{
  pos_t prel;
  receiver_->update_refpoint(src_->position,src_->position,prel,distance,gain,false,src_->gainmodel);
  gain = 1.0;
  vstate.resize(obstacles_.size());
  if(receiver_->layers & src_->layers)
    layergain = 1.0;
}

acoustic_model_t::~acoustic_model_t()
{
  if( receiver_data )
    delete receiver_data;
  if( source_data )
    delete source_data;
}
 
/**
   \ingroup callgraph
*/
uint32_t acoustic_model_t::process()
{
  if( src_->active )
    update_position();
  if( receiver_->render_point && 
      receiver_->active && 
      src_->active && 
      (!receiver_->gain_zero) && 
      (src_->ismmin <= ismorder) &&
      (ismorder <= src_->ismmax) &&
      (receiver_->ismmin <= ismorder) &&
      (ismorder <= receiver_->ismmax) &&
      ((!reflector) || reflector->active) &&
      ((receiver_->layers & src_->layers) || (layergain > 0))
      ){
    bool layeractive(receiver_->layers & src_->layers);
    if( visible ){
      pos_t prel;
      double nextdistance(0.0);
      double nextgain(1.0);
      // calculate relative geometry between source and receiver:
      double srcgainmod(1.0);
      // update effective position/calculate ISM geometry:
      position = get_effective_position( receiver_->position, srcgainmod );
      // read audio from source, update radation position:
      pos_t prelsrc(receiver_->position);
      prelsrc -= src_->position;
      prelsrc /= src_->orientation;
      if( src_->read_source( prelsrc, src_->inchannels, audio, source_data ) ){
        prelsrc *= src_->orientation;
        prelsrc -= receiver_->position;
        prelsrc *= -1.0;
        position = prelsrc;
      }
      receiver_->update_refpoint( primary->position, position, prel, nextdistance, nextgain, ismorder>0, src_->gainmodel );
      if( nextdistance > src_->maxdist )
        return 0;
      nextgain *= srcgainmod;
      double next_air_absorption(exp(-nextdistance*dscale));
      double ddistance((std::max(0.0,nextdistance-c_*receiver_->delaycomp)-distance)*dt);
      double dgain((nextgain-gain)*dt);
      double dairabsorption((next_air_absorption-air_absorption)*dt);
      apply_reflectionfilter( audio );
      for(uint32_t k=0;k<chunksize;++k){
        distance+=ddistance;
        gain+=dgain;
        float c1(air_absorption+=dairabsorption);
        float c2(1.0f-c1);
        // apply air absorption:
        if(layeractive){
          if(layergain < 1.0)
            layergain += dlayergain;
        }else{
          if(layergain > 0.0)
            layergain -= dlayergain;
        }
        c1 *= layergain*gain*delayline.get_dist_push(distance,audio[k]);
        airabsorption_state = c2*airabsorption_state+c1;
        make_friendly_number(airabsorption_state);
        audio[k] = airabsorption_state;
      }
      if( ((gain!=0)||(dgain!=0)) ){
        // calculate obstacles:
        for(uint32_t kobj=0;kobj!=obstacles_.size();++kobj){
          obstacle_t* p_obj(obstacles_[kobj]);
          if( p_obj->active ){
            // apply diffraction model:
            p_obj->process(position,receiver_->position,audio,c_,fs_,vstate[kobj],p_obj->transmission);
          }
        }
        // end obstacles
        if( src_->minlevel > 0 ){
          if( audio.rms() <= src_->minlevel )
            return 0;
        }
        receiver_->add_pointsource(prel,std::min(0.5*M_PI,0.25*M_PI*src_->size/std::max(0.01,nextdistance)),audio,receiver_data);
        return 1;
      }
    } // of visible
  }else{
    delayline.add_chunk(audio);
  }
  return 0;
}

obstacle_t::obstacle_t()
  : active(true)
{
}

reflector_t::reflector_t()
  : active(true),
    reflectivity(1.0),
    damping(0.0),
    edgereflection(true)
{
}

void reflector_t::apply_reflectionfilter( TASCAR::wave_t& audio, double& lpstate ) const
{
  double c1(reflectivity * (1.0-damping));
  float* p_begin(audio.d);
  float* p_end(p_begin+audio.n);
  for(float* pf=p_begin;pf!=p_end;++pf)
    *pf = (lpstate = lpstate*damping + *pf * c1);
}

receiver_graph_t::receiver_graph_t( double c, double fs, uint32_t chunksize, 
                                    const std::vector<source_t*>& sources,
                                    const std::vector<diffuse_source_t*>& diffusesources,
                                    const std::vector<reflector_t*>& reflectors,
                                    const std::vector<obstacle_t*>& obstacles,
                                    receiver_t* receiver,
                                    uint32_t ism_order )
  : active_pointsource(0),
    active_diffusesource(0)
{
  // diffuse models:
  if( receiver->render_diffuse )
    for(uint32_t kSrc=0;kSrc<diffusesources.size();++kSrc)
      diffuse_acoustic_model.push_back(new diffuse_acoustic_model_t(fs,chunksize,diffusesources[kSrc],receiver));
  // all primary and image sources:
  if( receiver->render_point ){
    // primary sources:
    for(uint32_t kSrc=0;kSrc<sources.size();++kSrc)
        acoustic_model.push_back(new acoustic_model_t(c,fs,chunksize,sources[kSrc],receiver,obstacles));
    if( receiver->render_image && (ism_order > 0) ){
      uint32_t num_mirrors_start(acoustic_model.size());
      // first order image sources:
      for(uint32_t ksrc=0;ksrc<sources.size();++ksrc)
        for(uint32_t kreflector=0;kreflector<reflectors.size();++kreflector)
          acoustic_model.push_back(new acoustic_model_t(c,fs,chunksize,sources[ksrc],receiver,obstacles,acoustic_model[ksrc],reflectors[kreflector]));
      // now higher order image sources:
      uint32_t num_mirrors_end(acoustic_model.size());
      for(uint32_t korder=1;korder<ism_order;++korder){
        for(uint32_t ksrc=num_mirrors_start;ksrc<num_mirrors_end;++ksrc)
          for(uint32_t kreflector=0;kreflector<reflectors.size();++kreflector)
            if( acoustic_model[ksrc]->reflector != reflectors[kreflector] )
              acoustic_model.push_back(new acoustic_model_t(c,fs,chunksize,acoustic_model[ksrc]->src_,receiver,obstacles,acoustic_model[ksrc],reflectors[kreflector]));
        num_mirrors_start = num_mirrors_end;
        num_mirrors_end = acoustic_model.size();
      }
    }
  }
}

world_t::world_t( double c, double fs, uint32_t chunksize, const std::vector<source_t*>& sources,const std::vector<diffuse_source_t*>& diffusesources,const std::vector<reflector_t*>& reflectors,const std::vector<obstacle_t*>& obstacles,const std::vector<receiver_t*>& receivers,const std::vector<mask_t*>& masks,uint32_t ism_order)
  : receivers_(receivers),
    masks_(masks),
    active_pointsource(0),
    active_diffusesource(0),
    total_pointsource(0),
    total_diffusesource(0)
{
  for( uint32_t krec=0;krec<receivers.size();++krec){
    receivergraphs.push_back(new receiver_graph_t( c, fs, chunksize, 
                                                   sources, diffusesources,
                                                   reflectors,
                                                   obstacles,
                                                   receivers[krec],
                                                   ism_order));
    total_pointsource += receivergraphs.back()->get_total_pointsource();
    total_diffusesource += receivergraphs.back()->get_total_diffusesource();
  }
}

world_t::~world_t()
{
  for( std::vector<receiver_graph_t*>::reverse_iterator it=receivergraphs.rbegin();it!=receivergraphs.rend();++it)
    delete (*it);
}

/**
   \ingroup callgraph
 */
void world_t::process()
{
  uint32_t local_active_point(0);
  uint32_t local_active_diffuse(0);
  //mirrormodel.process();
  // calculate mask gains:
  for(uint32_t k=0;k<receivers_.size();++k){
    double gain_inner(1.0);
    if( receivers_[k]->use_global_mask || receivers_[k]->boundingbox.active ){
      // first calculate attentuation based on bounding box:
      if( receivers_[k]->boundingbox.active ){
        shoebox_t maskbox;
        maskbox.size = receivers_[k]->boundingbox.size;
        receivers_[k]->boundingbox.get_6dof(maskbox.center, maskbox.orientation);
        double d(maskbox.nextpoint(receivers_[k]->position).norm());
        gain_inner *= 0.5+0.5*cos(M_PI*std::min(1.0,d/std::max(receivers_[k]->boundingbox.falloff,1e-10)));
      }
      // then calculate attenuation based on global masks:
      if( receivers_[k]->use_global_mask ){
        uint32_t c_outer(0);
        double gain_outer(0.0);
        for(uint32_t km=0;km<masks_.size();++km){
          if( masks_[km]->active ){
            pos_t p(receivers_[k]->position);
            if( masks_[km]->mask_inner ){
              gain_inner = std::min(gain_inner,masks_[km]->gain(p));
            }else{
              c_outer++;
              gain_outer = std::max(gain_outer,masks_[km]->gain(p));
            }
          }
        }
        if( c_outer > 0 )
          gain_inner *= gain_outer;
      }
    }
    receivers_[k]->set_next_gain(gain_inner);
  }
  // calculate acoustic models:
  for( std::vector<receiver_graph_t*>::iterator ig=receivergraphs.begin();ig!=receivergraphs.end();++ig){
    (*ig)->process();
    local_active_point += (*ig)->get_active_pointsource();
    local_active_diffuse += (*ig)->get_active_diffusesource();
  }
  // apply receiver gain:
  for(uint32_t k=0;k<receivers_.size();k++){
    receivers_[k]->post_proc();
    receivers_[k]->apply_gain();
  }
  active_pointsource = local_active_point;
  active_diffusesource = local_active_diffuse;
}

void receiver_graph_t::process()
{
  uint32_t local_active_point(0);
  uint32_t local_active_diffuse(0);
  // calculate acoustic model:
  for(unsigned int k=0;k<acoustic_model.size();k++)
    local_active_point += acoustic_model[k]->process();
  for(unsigned int k=0;k<diffuse_acoustic_model.size();k++)
    local_active_diffuse += diffuse_acoustic_model[k]->process();
  active_pointsource = local_active_point;
  active_diffusesource = local_active_diffuse;
}

receiver_graph_t::~receiver_graph_t()
{
  for(std::vector<acoustic_model_t*>::reverse_iterator it=acoustic_model.rbegin();it!=acoustic_model.rend();++it)
    delete (*it);
  for(std::vector<diffuse_acoustic_model_t*>::reverse_iterator it=diffuse_acoustic_model.rbegin();it!=diffuse_acoustic_model.rend();++it)
    delete (*it);
}

diffuse_acoustic_model_t::diffuse_acoustic_model_t(double fs,uint32_t chunksize,diffuse_source_t* src,receiver_t* receiver)
  : src_(src),
    receiver_(receiver),
    receiver_data(receiver_->create_data(fs,chunksize)),
    audio(src->audio.size()),
    chunksize(audio.size()),
    dt(1.0/std::max(1u,chunksize)),
    gain(1.0)
{
  pos_t prel;
  double d(1.0);
  receiver_->update_refpoint(src_->center,src_->center,prel,d,gain,false,GAIN_INVR);
  gain = 0;
}

diffuse_acoustic_model_t::~diffuse_acoustic_model_t()
{
  if( receiver_data )
    delete receiver_data;
}
 
/**
   \ingroup callgraph
 */
uint32_t diffuse_acoustic_model_t::process()
{
  pos_t prel;
  double d(0.0);
  double nextgain(1.0);
  // calculate relative geometry between source and receiver:
  receiver_->update_refpoint(src_->center,src_->center,prel,d,nextgain,false,GAIN_INVR);
  shoebox_t box(*src_);
  //box.size = src_->size;
  box.center = pos_t();
  pos_t prel_nonrot(prel);
  prel_nonrot *= receiver_->orientation;
  d = box.nextpoint(prel_nonrot).norm();
  nextgain = 0.5+0.5*cos(M_PI*std::min(1.0,d*src_->falloff));
  if( !((gain==0) && (nextgain==0))){
    audio.rotate(src_->audio,receiver_->orientation);
    double dgain((nextgain-gain)*dt);
    for(uint32_t k=0;k<chunksize;k++){
      gain+=dgain;
      if( receiver_->active && src_->active ){
        audio.w()[k] *= gain;
        audio.x()[k] *= gain;
        audio.y()[k] *= gain;
        audio.z()[k] *= gain;
      }
    }
    if( receiver_->render_diffuse && receiver_->active && src_->active && (!receiver_->gain_zero) ){
      audio *= receiver_->diffusegain;
      receiver_->add_diffusesource(audio,receiver_data);
      return 1;
    }
  }
  return 0;
}

receiver_t::receiver_t(xmlpp::Element* xmlsrc)
  : receivermod_t(xmlsrc),
    render_point(true),
    render_diffuse(true),
    render_image(true),
    ismmin(0),
    ismmax(2147483647),
    layers(0xffffffff),
    use_global_mask(true),
    diffusegain(1.0),
    falloff(-1.0),
    delaycomp(0.0),
    layerfadelen(1.0),
    active(true),
    boundingbox(find_or_add_child("boundingbox")),
    gain_zero(false),
    x_gain(1.0),
    dx_gain(0),
    next_gain(1.0),
    fade_timer(0),
    fade_rate(1),
    next_fade_gain(1),
  previous_fade_gain(1),
  prelim_next_fade_gain(1),
  prelim_previous_fade_gain(1),
  fade_gain(1),
  is_prepared(false)
{
  GET_ATTRIBUTE(size);
  get_attribute_bool("point",render_point);
  get_attribute_bool("diffuse",render_diffuse);
  get_attribute_bool("image",render_image);
  get_attribute_bool("globalmask",use_global_mask);
  get_attribute_db("diffusegain",diffusegain);
  GET_ATTRIBUTE(ismmin);
  GET_ATTRIBUTE(ismmax);
  GET_ATTRIBUTE_BITS(layers);
  GET_ATTRIBUTE(falloff);
  GET_ATTRIBUTE(delaycomp);
  GET_ATTRIBUTE(layerfadelen);
}

void receiver_t::write_xml()
{
  receivermod_t::write_xml();
  SET_ATTRIBUTE(size);
  set_attribute_bool("point",render_point);
  set_attribute_bool("diffuse",render_diffuse);
  set_attribute_bool("image",render_image);
  set_attribute_bool("globalmask",use_global_mask);
  set_attribute_db("diffusegain",diffusegain);
  SET_ATTRIBUTE(ismmin);
  SET_ATTRIBUTE(ismmax);
  SET_ATTRIBUTE(falloff);
  SET_ATTRIBUTE(delaycomp);
  boundingbox.write_xml();
}

void receiver_t::prepare( chunk_cfg_t& cf_ )
{
  receivermod_t::prepare( cf_ );
  n_channels = get_num_channels();
  update();
  cf_ = *(chunk_cfg_t*)this;
  for(uint32_t k=0;k<n_channels;k++){
    outchannelsp.push_back(new wave_t(n_fragment));
    outchannels.push_back(wave_t(*(outchannelsp.back())));
  }
  is_prepared = true;
}

void receiver_t::release()
{
  receivermod_t::release();
  outchannels.clear();
  for( uint32_t k=0;k<outchannelsp.size();++k)
    delete outchannelsp[k];
  outchannelsp.clear();
  is_prepared = false;
}

receiver_t::~receiver_t()
{
}

void receiver_t::clear_output()
{
  for(uint32_t ch=0;ch<outchannels.size();ch++)
    outchannels[ch].clear();
}

/**
   \ingroup callgraph
 */
void receiver_t::add_pointsource(const pos_t& prel, double width, const wave_t& chunk, receivermod_base_t::data_t* data)
{
  receivermod_t::add_pointsource(prel,width,chunk,outchannels,data);
}

/**
   \ingroup callgraph
 */
void receiver_t::post_proc()
{
  postproc(outchannels);
}

/**
   \ingroup callgraph
 */
void receiver_t::add_diffusesource(const amb1wave_t& chunk, receivermod_base_t::data_t* data)
{
  receivermod_t::add_diffusesource(chunk,outchannels,data);
}

void receiver_t::update_refpoint(const pos_t& psrc_physical, const pos_t& psrc_virtual, pos_t& prel, double& distance, double& gain, bool b_img, gainmodel_t gainmodel )
{
  
  if( (size.x!=0)&&(size.y!=0)&&(size.z!=0) ){
    prel = psrc_physical;
    prel -= position;
    prel /= orientation;
    distance = prel.norm();
    shoebox_t box;
    box.size = size;
    double sizedist = pow(size.x*size.y*size.z,0.33333);
    double d(box.nextpoint(prel).norm());
    if( falloff > 0 )
      gain = (0.5+0.5*cos(M_PI*std::min(1.0,d/falloff)))/std::max(0.1,sizedist);
    else{
      switch( gainmodel ){
      case GAIN_INVR :
        gain = 1.0/std::max(1.0,d+sizedist);
        break;
      case GAIN_UNITY :
        gain = 1.0/std::max(1.0,sizedist);
        break;
      }
    }
  }else{
    prel = psrc_virtual;
    prel -= position;
    prel /= orientation;
    distance = prel.norm();
    switch( gainmodel ){
    case GAIN_INVR :
      gain = 1.0/std::max(0.1,distance);
      break;
    case GAIN_UNITY :
      gain = 1.0;
      break;
    }
    double physical_dist(TASCAR::distance(psrc_physical,position));
    if( b_img && (physical_dist > distance) ){
      gain = 0.0;
    }
  }
  make_friendly_number(gain);
}

void receiver_t::set_next_gain(double g)
{
  next_gain = g;
  gain_zero = (next_gain==0) && (x_gain==0);
}

void receiver_t::apply_gain()
{
  dx_gain = (next_gain-x_gain)*t_inc;
  uint32_t ch(get_num_channels());
  if( ch > 0 ){
    uint32_t psize(outchannels[0].size());
    for(uint32_t k=0;k<psize;k++){
      double g(x_gain+=dx_gain);
      if( fade_timer > 0 ){
        --fade_timer;
        previous_fade_gain = prelim_previous_fade_gain;
        next_fade_gain = prelim_next_fade_gain;
        fade_gain = previous_fade_gain + (next_fade_gain - previous_fade_gain)*(0.5+0.5*cos(fade_timer*fade_rate));
      }
      g *= fade_gain;
      for(uint32_t c=0;c<ch;c++){
        outchannels[c][k] *= g;
      }
    }
  }
}

void receiver_t::set_fade( double targetgain, double duration )
{
  fade_timer = 0;
  prelim_previous_fade_gain = fade_gain;
  prelim_next_fade_gain = targetgain;
  fade_rate = M_PI*t_sample/duration;
  fade_timer = std::max(1u,(uint32_t)(f_sample*duration));
}

TASCAR::Acousticmodel::boundingbox_t::boundingbox_t(xmlpp::Element* xmlsrc)
  : dynobject_t(xmlsrc),falloff(1.0),active(false)
{
  dynobject_t::get_attribute("size",size);
  dynobject_t::get_attribute("falloff",falloff);
  dynobject_t::get_attribute_bool("active",active);
}

void TASCAR::Acousticmodel::boundingbox_t::write_xml()
{
  dynobject_t::write_xml();
  dynobject_t::set_attribute("size",size);
  dynobject_t::set_attribute("falloff",falloff);
  dynobject_t::set_attribute_bool("active",active);
}

/**
   \brief Apply diffraction model 

   \param p_src Source position
   \param p_rec Receiver position
   \param audio Audio chunk
   \param c Speed of sound
   \param fs Sampling rate
   \param state Diffraction filter states
   \param drywet Direct-to-diffracted ratio

   \return Effective source position

   \ingroup callgraph
*/
pos_t diffractor_t::process(pos_t p_src, const pos_t& p_rec, wave_t& audio, double c, double fs, state_t& state,float drywet)
{
  // calculate intersection:
  pos_t p_is;
  double w(0);
  bool is_intersect(intersection(p_src,p_rec,p_is,&w));
  if( (w <= 0) || (w >= 1) )
    is_intersect = false;
  if( is_intersect ){
    bool is_outside(false);
    pos_t pne;
    nearest(p_is,&is_outside,&pne);
    p_is = pne;
    if( is_outside )
      is_intersect = false;
  }
  // calculate filter:
  double dt(1.0/audio.n);
  double dA1(-state.A1*dt);
  if( is_intersect ){
    // calculate geometry:
    pos_t p_is_src(p_src-p_is);
    pos_t p_rec_is(p_is-p_rec);
    p_rec_is.normalize();
    double d_is_src(p_is_src.norm());
    if( d_is_src > 0 )
      p_is_src *= 1.0/d_is_src;
    // calculate first zero crossing frequency:
    double cos_theta(std::max(0.0,dot_prod(p_is_src,p_rec_is)));
    double sin_theta(std::max(EPS,sqrt(1.0-cos_theta*cos_theta)));
    double f0(3.8317*c/(PI2*aperture*sin_theta));
    // calculate filter coefficient increment:
    dA1 = (exp(-M_PI*f0/fs)-state.A1)*dt;
    // return effective source position:
    p_rec_is *= d_is_src;
    p_src = p_is+p_rec_is;
  }
  // apply low pass filter to audio chunk:
  for(uint32_t k=0;k<audio.n;k++){
    state.A1 += dA1;
    double B0(1.0-state.A1);
    state.s1 = state.s1*state.A1+audio[k]*B0;
    audio[k] = drywet*audio[k] + (1.0f-drywet)*(state.s2 = state.s2*state.A1+state.s1*B0);
  }
  return p_src;
}


source_t::source_t(xmlpp::Element* xmlsrc)
  : sourcemod_t(xmlsrc),
    ismmin(0),
    ismmax(2147483647),
    layers(0xffffffff),
    maxdist(3700),
    minlevel(0),
    sincorder(0),
    gainmodel(GAIN_INVR),
    size(0),
    active(true),
    is_prepared(false)
{
  GET_ATTRIBUTE(size);
  GET_ATTRIBUTE(maxdist);
  GET_ATTRIBUTE_DBSPL(minlevel);
  std::string gr;
  get_attribute("gainmodel",gr);
  if( gr.empty() )
    gr = "1/r";
  if( gr == "1/r" )
    gainmodel = GAIN_INVR;
  else if( gr == "1" )
    gainmodel = GAIN_UNITY;
  else
    throw TASCAR::ErrMsg("Invalid gain model "+gr+"(valid gain models: \"1/r\", \"1\").");
  GET_ATTRIBUTE(sincorder);
  GET_ATTRIBUTE(ismmin);
  GET_ATTRIBUTE(ismmax);
  GET_ATTRIBUTE_BITS(layers);
}

source_t::~source_t()
{
}

void source_t::write_xml()
{
  sourcemod_t::write_xml();
  SET_ATTRIBUTE(sincorder);
  SET_ATTRIBUTE(ismmin);
  SET_ATTRIBUTE(ismmax);
  SET_ATTRIBUTE(size);
  SET_ATTRIBUTE(maxdist);
  SET_ATTRIBUTE_DBSPL(minlevel);
}

void source_t::prepare( chunk_cfg_t& cf_ )
{
  sourcemod_t::prepare( cf_ );
  n_channels = get_num_channels();
  update();
  cf_ = *(chunk_cfg_t*)this;
  for(uint32_t k=0;k<n_channels;k++){
    inchannelsp.push_back(new wave_t(n_fragment));
    inchannels.push_back(wave_t(*(inchannelsp.back())));
  }
  is_prepared = true;
}

void source_t::release()
{
  sourcemod_t::release();
  inchannels.clear();
  for( uint32_t k=0;k<inchannelsp.size();++k)
    delete inchannelsp[k];
  inchannelsp.clear();
  is_prepared = false;
}

void source_t::process_plugins( const TASCAR::transport_t& tp )
{
  for( std::vector<TASCAR::audioplugin_t*>::iterator p=plugins.begin();
       p!= plugins.end();
       ++p)
    (*p)->ap_process( inchannels, position, tp );
}

soundpath_t::soundpath_t(const source_t* src, const soundpath_t* parent_, const reflector_t* generator_)
  : parent((parent_?parent_:this)),
    primary((parent_?(parent_->primary):src)),
    reflector(generator_),
    visible(true)
{
  reflectionfilterstates.resize(getorder());
  for(uint32_t k=0;k<reflectionfilterstates.size();++k)
    reflectionfilterstates[k] = 0;
}

void soundpath_t::update_position()
{
  visible = true;
  if( reflector ){
    // calculate image position and orientation:
    p_cut = reflector->nearest_on_plane(parent->position);
    // calculate nominal image source position:
    pos_t p_img(p_cut);
    p_img *= 2.0;
    p_img -= parent->position;
    // if image source is in front of reflector then return:
    if( dot_prod( p_img-p_cut, reflector->get_normal() ) > 0 )
      visible = false;
    position = p_img;
    orientation = parent->orientation;
  }else{
    position = primary->position;
    orientation = primary->orientation;
  }
}

uint32_t soundpath_t::getorder() const
{
  if( parent != this )
    return parent->getorder()+1;
  else
    return 0;
}

void soundpath_t::apply_reflectionfilter( TASCAR::wave_t& audio )
{
  uint32_t k(0);
  const reflector_t* pr(reflector);
  const soundpath_t* ps(this);
  while( pr ){
    pr->apply_reflectionfilter( audio, reflectionfilterstates[k] );
    ++k;
    ps = ps->parent;
    pr = ps->reflector;
  }
}

pos_t soundpath_t::get_effective_position( const pos_t& p_rec, double& gain )
{
  if( !reflector )
    return position;
  // calculate orthogonal point on plane:
  pos_t pcut_rec(reflector->nearest_on_plane(p_rec));
  // if receiver is behind reflector then return zero:
  if( dot_prod( p_rec-pcut_rec, reflector->get_normal() ) < 0 ){
    gain = 0;
    return position;
  }
  double len_receiver(distance(pcut_rec,p_rec));
  double len_src(distance( p_cut, position ));
  // calculate intersection:
  double ratio(len_receiver/std::max(1e-6,(len_receiver+len_src)));
  pos_t p_is(p_cut-pcut_rec);
  p_is *= ratio;
  p_is += pcut_rec;
  p_is = reflector->nearest(p_is);
  gain = pow(std::max(0.0,dot_prod((p_rec-p_is).normal(),(p_is-position).normal())),2.7);
  make_friendly_number(gain);
  if( reflector->edgereflection ){
    double len_img(distance(p_is,position));
    pos_t p_eff((p_is-p_rec).normal());
    p_eff *= len_img;
    p_eff += p_is;
    return p_eff;
  }
  return position;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
