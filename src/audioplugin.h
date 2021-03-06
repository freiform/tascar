#ifndef AUDIOPLUGIN_H
#define AUDIOPLUGIN_H

#include "xmlconfig.h"
#include "audiochunks.h"
#include "tascarplugin.h"

namespace TASCAR {

  /**
     \brief Transport state and time information

     Typically the session time, corresponding to the first audio
     sample in a chunk.
   */
  class transport_t {
  public:
    transport_t();
    uint64_t session_time_samples;//!< Session time in samples
    double session_time_seconds;//!< Session time in seconds
    uint64_t object_time_samples;//!< Object time in samples
    double object_time_seconds;//!< Object time in seconds
    bool rolling;//!< Transport state
  };

  class audioplugin_cfg_t {
  public:
    audioplugin_cfg_t(xmlpp::Element* xmlsrc, const std::string& name, const std::string& parentname):xmlsrc(xmlsrc),name(name),parentname(parentname){};
    xmlpp::Element* xmlsrc;
    const std::string& name;
    const std::string& parentname;
    std::string modname;
  };

  /**
     \brief Base class of audio processing plugins
  */
  class audioplugin_base_t : public xml_element_t, public audiostates_t {
  public:
    audioplugin_base_t( const audioplugin_cfg_t& cfg );
    virtual void write_xml() {};
    virtual ~audioplugin_base_t();
    virtual void ap_process(std::vector<wave_t>& chunk, const TASCAR::pos_t& pos, const TASCAR::transport_t& tp) = 0;
    virtual void add_variables( TASCAR::osc_server_t* srv ) {};
    const std::string& get_name() const { return name; };
    const std::string& get_modname() const { return modname; };
  protected:
    std::string name;
    std::string modname;
  private:
    bool prepared;
  };

  class audioplugin_t : public audioplugin_base_t {
  public:
    audioplugin_t( const audioplugin_cfg_t& cfg );
    void write_xml();
    virtual ~audioplugin_t();
    virtual void ap_process(std::vector<wave_t>& chunk, const TASCAR::pos_t& pos, const TASCAR::transport_t& tp);
    virtual void prepare( chunk_cfg_t& );
    virtual void add_variables( TASCAR::osc_server_t* srv );
    virtual void release();
  private:
    audioplugin_t(const audioplugin_t&);
    std::string plugintype;
    void* lib;
    TASCAR::audioplugin_base_t* libdata;
  };

}

#define REGISTER_AUDIOPLUGIN(x) TASCAR_PLUGIN( audioplugin_base_t, const audioplugin_cfg_t&, x )

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
