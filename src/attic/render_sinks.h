#ifndef RENDER_SINKS_H
#define RENDER_SINKS_H

#include "amb33defs.h"
#include "scene.h"
#include "delayline.h"

/*

  sources {

  point sources: pos
  
  room sources [amb1]: pos+dimension (shoe box)
  
  }[internal/jack/file]


  receiver-sources {

  diffuse-rev

  }

  acoustic-model

  receivers {

  amb11

  amb33

  amb110

  point

  shoe_box

  [wfs/nfc_hoa/...]

  }




 */

namespace TASCAR {

  /*
  class receiver_base_t : public object_t {
  public:
    receiver_base_t();
    virtual uint32_t output_channels() = 0;
    void connect_source(const source_base_t* src);
    virtual void prepare() = 0;
    virtual void process(uint32_t n, std::vector<float*> sOut) = 0;
  protected:
    std::vector<const source_base_t*> sources;
  };
  */

  class trackpan_amb33_t {
  public:
    trackpan_amb33_t(double srate, uint32_t fragsize, double maxdist);
    trackpan_amb33_t(const trackpan_amb33_t&);
    ~trackpan_amb33_t();
  protected:
    TASCAR::varidelay_t delayline;
    double srate_;
    double dt_sample;
    double dt_update;
    uint32_t fragsize_;
    // ambisonic weights:
    float _w[AMB33::idx::channels];
    float w_current[AMB33::idx::channels];
    float dw[AMB33::idx::channels];
    float d_current;
    float dd;
    float clp_current;
    float dclp;
    float y;
    float dscale;
    double maxdist;
  public:
    void process(uint32_t n, float* vIn, const std::vector<float*>& outBuffer, uint32_t tp_frame, double tp_time, bool tp_rolling, TASCAR::Scene::sound_t*);
  protected:
    void updatepar(pos_t);
  };

  class mirror_pan_t : public trackpan_amb33_t {
  public:
    mirror_pan_t(double srate, uint32_t fragsize, double maxdist);
    void process(uint32_t n, float* vIn, const std::vector<float*>& outBuffer, uint32_t tp_frame, double tp_time, bool tp_rolling, TASCAR::Scene::sound_t*,TASCAR::Scene::face_object_t*);
  private:
    float c1_current;
    float dc1;
    float c2_current;
    float dc2;
    float my;
  };

  //class reverb_line_t {
  //public:
  //  reverb_line_t(double srate, uint32_t fragsize, double maxdist);
  //  reverb_line_t(const reverb_line_t&);
  //protected:
  //  varidelay_t delayline;
  //  double srate_;
  //  double dt_sample;
  //  double dt_update;
  //  uint32_t fragsize_;
  //  float d_global_current;
  //  float dd_global;
  //  float d_border_current;
  //  float dd_border;
  //  float clp_current;
  //  float dclp;
  //  float y;
  //  float dscale;
  //  double maxdist;
  //public:
  //  void process(uint32_t n, float* vIn, float* vOut, uint32_t tp_frame, double tp_time, bool tp_rolling, TASCAR::Scene::sound_t*, TASCAR::Scene::diffuse_reverb_t* reverb);
  //};
  
};

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
