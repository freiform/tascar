#include "receivers.h"
#include "defs.h"
#include <iostream>
#include <stdio.h>
#include "errorhandling.h"

using namespace TASCAR;
using namespace TASCAR::Acousticmodel;

receiver_omni_t::receiver_omni_t(uint32_t chunksize, pos_t size, double falloff, bool b_point, bool b_diffuse,
                         pos_t mask_size,
                         double mask_falloff,
                         bool mask_use,bool global_mask_use)
  : receiver_t(chunksize, size, falloff, b_point, b_diffuse,mask_size,mask_falloff,mask_use,global_mask_use)
{
  outchannels = std::vector<wave_t>(1,wave_t(chunksize));
}

//void receiver_omni_t::update_refpoint(const pos_t& psrc, pos_t& prel, double& distance, double& gain)
//{
//  prel = psrc;
//  prel -= position;
//  distance = prel.norm();
//  gain = 1.0/std::max(0.1,distance);
//}

void receiver_omni_t::add_source(const pos_t& prel, const wave_t& chunk, receiver_data_t*)
{
  outchannels[0] += chunk;
}

void receiver_omni_t::add_source(const pos_t& prel, const amb1wave_t& chunk, receiver_data_t*)
{
  outchannels[0] += chunk.w();
}


receiver_cardioid_t::receiver_cardioid_t(uint32_t chunksize, pos_t size, double falloff, bool b_point, bool b_diffuse,
                         pos_t mask_size,
                         double mask_falloff,
                         bool mask_use,bool global_mask_use)
  : receiver_t(chunksize, size, falloff, b_point, b_diffuse,mask_size,mask_falloff,mask_use,global_mask_use)
{
  outchannels = std::vector<wave_t>(1,wave_t(chunksize));
}


void receiver_cardioid_t::add_source(const pos_t& prel, const wave_t& chunk, receiver_data_t* sd)
{
  data_t* d((data_t*)sd);
  float dazgain((0.5*cos(prel.azim())+0.5 - d->azgain)*dt);
  for(uint32_t k=0;k<chunk.size();k++)
    outchannels[0][k] += chunk[k]*(d->azgain+=dazgain);
}

void receiver_cardioid_t::add_source(const pos_t& prel, const amb1wave_t& chunk, receiver_data_t* sd)
{
  data_t* d((data_t*)sd);
  float dazgain((0.5*cos(prel.azim())+0.5 - d->azgain)*dt);
  for(uint32_t k=0;k<chunk.w().size();k++)
    outchannels[0][k] += chunk.w()[k]*(d->azgain+=dazgain);
}


receiver_amb3h3v_t::data_t::data_t()
{
  for(uint32_t k=0;k<AMB33::idx::channels;k++)
    _w[k] = w_current[k] = dw[k] = 0;
  rotz_current[0] = 0;
  rotz_current[1] = 0;
}
 
receiver_amb3h3v_t::receiver_amb3h3v_t(uint32_t chunksize, pos_t size, double falloff, bool b_point, bool b_diffuse,
                         pos_t mask_size,
                         double mask_falloff,
                         bool mask_use,bool global_mask_use)
  : receiver_t(chunksize, size, falloff, b_point, b_diffuse,mask_size,mask_falloff,mask_use,global_mask_use)
{
  outchannels = std::vector<wave_t>(AMB33::idx::channels,wave_t(chunksize));
}

void receiver_amb3h3v_t::add_source(const pos_t& prel, const wave_t& chunk, receiver_data_t* sd)
{
  data_t* d((data_t*)sd);
  float az = prel.azim();
  float el = prel.elev();
  float t, x2, y2, z2;
  // this is taken from AMB plugins by Fons and Joern:
  d->_w[AMB33::idx::w] = MIN3DB;
  t = cosf (el);
  d->_w[AMB33::idx::x] = t * cosf (az);
  d->_w[AMB33::idx::y] = t * sinf (az);
  d->_w[AMB33::idx::z] = sinf (el);
  x2 = d->_w[AMB33::idx::x] * d->_w[AMB33::idx::x];
  y2 = d->_w[AMB33::idx::y] * d->_w[AMB33::idx::y];
  z2 = d->_w[AMB33::idx::z] * d->_w[AMB33::idx::z];
  d->_w[AMB33::idx::u] = x2 - y2;
  d->_w[AMB33::idx::v] = 2 * d->_w[AMB33::idx::x] * d->_w[AMB33::idx::y];
  d->_w[AMB33::idx::s] = 2 * d->_w[AMB33::idx::z] * d->_w[AMB33::idx::x];
  d->_w[AMB33::idx::t] = 2 * d->_w[AMB33::idx::z] * d->_w[AMB33::idx::y];
  d->_w[AMB33::idx::r] = (3 * z2 - 1) / 2;
  d->_w[AMB33::idx::p] = (x2 - 3 * y2) * d->_w[AMB33::idx::x];
  d->_w[AMB33::idx::q] = (3 * x2 - y2) * d->_w[AMB33::idx::y];
  t = 2.598076f * d->_w[AMB33::idx::z]; 
  d->_w[AMB33::idx::n] = t * d->_w[AMB33::idx::u];
  d->_w[AMB33::idx::o] = t * d->_w[AMB33::idx::v];
  t = 0.726184f * (5 * z2 - 1);
  d->_w[AMB33::idx::l] = t * d->_w[AMB33::idx::x];
  d->_w[AMB33::idx::m] = t * d->_w[AMB33::idx::y];
  d->_w[AMB33::idx::k] = d->_w[AMB33::idx::z] * (5 * z2 - 3) / 2;
  for(unsigned int k=0;k<AMB33::idx::channels;k++)
    d->dw[k] = (d->_w[k] - d->w_current[k])*dt;
  for( unsigned int i=0;i<chunk.size();i++){
    for( unsigned int k=0;k<AMB33::idx::channels;k++){
      outchannels[k][i] += (d->w_current[k] += d->dw[k]) * chunk[i];
    }
  }
}

void receiver_amb3h3v_t::add_source(const pos_t& prel, const amb1wave_t& chunk, receiver_data_t* sd)
{
  data_t* d((data_t*)sd);
  float az = prel.azim();
  d->drotz[0] = (cos(az) - d->rotz_current[0])*dt;;
  d->drotz[1] = (sin(az) - d->rotz_current[1])*dt;
  for( unsigned int i=0;i<chunk.size();i++){
    d->rotz_current[0] += d->drotz[0];
    d->rotz_current[1] += d->drotz[1];
    float x(d->rotz_current[0]*chunk.x()[i] + d->rotz_current[1]*chunk.y()[i]);
    float y(-d->rotz_current[1]*chunk.x()[i] + d->rotz_current[0]*chunk.y()[i]);
    float z(chunk.z()[i]);
    outchannels[AMB33::idx::w][i] += chunk.w()[i];
    outchannels[AMB33::idx::x][i] += x;
    outchannels[AMB33::idx::y][i] += y;
    outchannels[AMB33::idx::z][i] += z;
    //outchannels[AMB30::idx::w][i] += chunk.w()[i];
    //outchannels[AMB30::idx::x][i] += chunk.x()[i];
    //outchannels[AMB30::idx::y][i] += chunk.y()[i];
  }
}

std::string receiver_amb3h3v_t::get_channel_postfix(uint32_t channel) const
{
  char ctmp[32];
  sprintf(ctmp,".%g%c",floor(sqrt((double)channel)),AMB33::channelorder[channel]);
  return ctmp;
}



receiver_amb3h0v_t::data_t::data_t()
{
  for(uint32_t k=0;k<AMB30::idx::channels;k++)
    _w[k] = w_current[k] = dw[k] = 0;
  rotz_current[0] = 0;
  rotz_current[1] = 0;
}
 
receiver_amb3h0v_t::receiver_amb3h0v_t(uint32_t chunksize, pos_t size, double falloff, bool b_point, bool b_diffuse,
                         pos_t mask_size,
                         double mask_falloff,
                         bool mask_use,bool global_mask_use)
  : receiver_t(chunksize, size, falloff, b_point, b_diffuse,mask_size,mask_falloff,mask_use,global_mask_use)
{
  outchannels = std::vector<wave_t>(AMB30::idx::channels,wave_t(chunksize));
}

void receiver_amb3h0v_t::add_source(const pos_t& prel, const wave_t& chunk, receiver_data_t* sd)
{
  data_t* d((data_t*)sd);
  float az = prel.azim();
  float x2, y2;
  // this is more or less taken from AMB plugins by Fons and Joern:
  d->_w[AMB30::idx::w] = MIN3DB;
  d->_w[AMB30::idx::x] = cosf (az);
  d->_w[AMB30::idx::y] = sinf (az);
  x2 = d->_w[AMB30::idx::x] * d->_w[AMB30::idx::x];
  y2 = d->_w[AMB30::idx::y] * d->_w[AMB30::idx::y];
  d->_w[AMB30::idx::u] = x2 - y2;
  d->_w[AMB30::idx::v] = 2.0f * d->_w[AMB30::idx::x] * d->_w[AMB30::idx::y];
  d->_w[AMB30::idx::p] = (x2 - 3.0f * y2) * d->_w[AMB30::idx::x];
  d->_w[AMB30::idx::q] = (3.0f * x2 - y2) * d->_w[AMB30::idx::y];
  for(unsigned int k=0;k<AMB30::idx::channels;k++)
    d->dw[k] = (d->_w[k] - d->w_current[k])*dt;
  for( unsigned int i=0;i<chunk.size();i++){
    for( unsigned int k=0;k<AMB30::idx::channels;k++){
      outchannels[k][i] += (d->w_current[k] += d->dw[k]) * chunk[i];
    }
  }
}

void receiver_amb3h0v_t::add_source(const pos_t& prel, const amb1wave_t& chunk, receiver_data_t* sd)
{
  data_t* d((data_t*)sd);
  float az = prel.azim();
  d->drotz[0] = (cos(az) - d->rotz_current[0])*dt;;
  d->drotz[1] = (sin(az) - d->rotz_current[1])*dt;
  for( unsigned int i=0;i<chunk.size();i++){
    d->rotz_current[0] += d->drotz[0];
    d->rotz_current[1] += d->drotz[1];
    float x(d->rotz_current[0]*chunk.x()[i] + d->rotz_current[1]*chunk.y()[i]);
    float y(-d->rotz_current[1]*chunk.x()[i] + d->rotz_current[0]*chunk.y()[i]);
    outchannels[AMB30::idx::w][i] += chunk.w()[i];
    outchannels[AMB30::idx::x][i] += x;
    outchannels[AMB30::idx::y][i] += y;
    //outchannels[AMB30::idx::w][i] += chunk.w()[i];
    //outchannels[AMB30::idx::x][i] += chunk.x()[i];
    //outchannels[AMB30::idx::y][i] += chunk.y()[i];
  }
}

std::string receiver_amb3h0v_t::get_channel_postfix(uint32_t channel) const
{
  char ctmp[32];
  sprintf(ctmp,".%g%c",floor((double)(channel+1)*0.5),AMB30::channelorder[channel]);
  return ctmp;
}

receiver_nsp_t::data_t::data_t()
{
  for(uint32_t k=0;k<MAX_VBAP_CHANNELS;k++)
    w[k] = dw[k] = x[k] = y[k] = z[k] = dx[k] = dy[k] = dz[k] = 0;
}
 
receiver_nsp_t::receiver_nsp_t(uint32_t chunksize, pos_t size, double falloff, bool b_point, bool b_diffuse,
                         pos_t mask_size,
                         double mask_falloff,
                         bool mask_use,bool global_mask_use, const std::vector<pos_t>& spkpos_)
  : receiver_t(chunksize, size, falloff, b_point, b_diffuse,mask_size,mask_falloff,mask_use,global_mask_use)
{
  if( spkpos_.size() > MAX_VBAP_CHANNELS )
    throw TASCAR::ErrMsg("number of VBAP channels is to large.");
  if( !spkpos_.size() )
    throw TASCAR::ErrMsg("at least one speaker required in nsp panning.");
  outchannels = std::vector<wave_t>(spkpos_.size(),wave_t(chunksize));
  for(uint32_t k=0;k<spkpos_.size();k++)
    spkpos.push_back(spkpos_[k].normal());
}

void receiver_nsp_t::add_source(const pos_t& prel, const wave_t& chunk, receiver_data_t* sd)
{
  data_t* d((data_t*)sd);
  pos_t psrc(prel.normal());
  uint32_t kmin(0);
  double dmin(distance(psrc,spkpos[kmin]));
  double dist(0);
  for(unsigned int k=1;k<outchannels.size();k++)
    if( (dist = distance(psrc,spkpos[k]))<dmin ){
      kmin = k;
      dmin = dist;
    }
  for(unsigned int k=0;k<outchannels.size();k++)
    d->dw[k] = ((k==kmin) - d->w[k])*dt;
  for( unsigned int i=0;i<chunk.size();i++){
    for( unsigned int k=0;k<outchannels.size();k++){
      outchannels[k][i] += (d->w[k] += d->dw[k]) * chunk[i];
    }
  }
}

void receiver_nsp_t::add_source(const pos_t& prel, const amb1wave_t& chunk, receiver_data_t* sd)
{
  data_t* d((data_t*)sd);
  pos_t psrc(prel.normal());
  uint32_t kmin(0);
  double dmin(distance(psrc,spkpos[kmin]));
  double dist(0);
  for(unsigned int k=1;k<outchannels.size();k++)
    if( (dist = distance(psrc,spkpos[k]))<dmin ){
      kmin = k;
      dmin = dist;
    }
  pos_t px(1,0,0);
  pos_t py(0,1,0);
  pos_t pz(0,0,1);
  px *= orientation;
  py *= orientation;
  pz *= orientation;
  for(unsigned int k=0;k<outchannels.size();k++)
    d->dw[k] = (0.701 - d->w[k])*dt;
  for(unsigned int k=0;k<outchannels.size();k++)
    d->dx[k] = (dot_prod(px,spkpos[k]) - d->x[k])*dt;
  for(unsigned int k=0;k<outchannels.size();k++)
    d->dy[k] = (dot_prod(py,spkpos[k]) - d->y[k])*dt;
  for(unsigned int k=0;k<outchannels.size();k++)
    d->dz[k] = (dot_prod(pz,spkpos[k]) - d->z[k])*dt;
  for( unsigned int i=0;i<chunk.size();i++){
    for( unsigned int k=0;k<outchannels.size();k++){
      outchannels[k][i] += (d->w[k] += d->dw[k]) * chunk.w()[i];
      outchannels[k][i] += (d->x[k] += d->dx[k]) * chunk.x()[i];
      outchannels[k][i] += (d->y[k] += d->dy[k]) * chunk.y()[i];
      outchannels[k][i] += (d->z[k] += d->dz[k]) * chunk.z()[i];
    }
  }
}

std::string receiver_nsp_t::get_channel_postfix(uint32_t channel) const
{
  char ctmp[32];
  sprintf(ctmp,".%d",channel);
  return ctmp;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
