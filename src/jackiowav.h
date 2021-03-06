/**
   \file jackiowav.h
   \ingroup apptascar
   \brief simultaneously play a sound file and record from jack
   \author Giso Grimm
   \date 2012,2014,2017

   \section license License (GPL)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

*/
#ifndef JACKIOWAV_H
#define JACKIOWAV_H

#include "tascar.h"
#include <stdlib.h>
#include <string.h>
//#include <getopt.h>
#include <unistd.h>

/**
   \ingroup apptascar
*/
class jackio_t : public jackc_transport_t {
public:
  /**
     \param ifname Input file name
     \param ofname Output file name
     \param ports Output and Input ports (the first N ports are assumed to be output ports, N = number of channels in input file)
     \param freewheel Optionally use freewheeling mode
     \param autoconnect Automatically connect to hardware ports.
     \param verbose Show more infos on console.
  */
  jackio_t(const std::string& ifname,const std::string& ofname,
	   const std::vector<std::string>& ports,const std::string& jackname = "jackio",int freewheel = 0,int autoconnect = 0, bool verbose = false);
  jackio_t(double duration,const std::string& ofname,
	   const std::vector<std::string>& ports,const std::string& jackname = "jackio",int freewheel = 0,int autoconnect = 0, bool verbose = false);
  void set_transport_start(double start, bool wait);
  ~jackio_t();
  /**
     \brief start processing
  */
  void run();
private:
  SNDFILE* sf_in;
  SNDFILE* sf_out;
  SF_INFO sf_inf_in;
  SF_INFO sf_inf_out;
  float* buf_in;
  float* buf_out;
  unsigned int pos;
  bool b_quit;
  bool start;
  bool freewheel_;
  bool use_transport;
  uint32_t startframe;
  uint32_t nframes_total;
  std::vector<std::string> p;
  int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer,uint32_t tp_frame, bool tp_rolling);
  void log(const std::string& msg);
  bool b_cb;
  bool b_verbose;
  bool wait_;
public:
  float cpuload;
  uint32_t xruns;
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
