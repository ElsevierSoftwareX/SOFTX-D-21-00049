// This file is part of the HörTech Open Master Hearing Aid (openMHA)
// Copyright © 2010 2011 2012 2013 2014 2015 2016 2018 2019 2020 HörTech gGmbH
// Copyright © 2021 HörTech gGmbH
//
// openMHA is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, version 3 of the License.
//
// openMHA is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License, version 3 for more details.
//
// You should have received a copy of the GNU Affero General Public License, 
// version 3 along with openMHA.  If not, see <http://www.gnu.org/licenses/>.

#include <time.h>
#include <pthread.h>
#include "mha_plugin.hh"
#include "mha_fifo.h"
#include <sndfile.h>
#include <sys/time.h>
#include <atomic>

class wavwriter_t {
public:
    wavwriter_t(bool active,const mhaconfig_t& cf,unsigned int fifosize,unsigned int minwrite,
                const std::string& prefix,bool use_date, const std::string& format_name_);
    ~wavwriter_t();
    void process(mha_wave_t*);
    void exit_request();
private:
    static void* write_thread(void* this_){((wavwriter_t*)this_)->write_thread();return NULL;};
    void write_thread();
    void create_soundfile(const std::string& prefix, bool use_date);
    /** Converts the format_name string to the corresponding int according to libsndfile and
     *  writes it into the format field of sf_info
     *  throws if no format of this name is available
     * @param sf_info Destination sf_info struct for the format
     * @throw MHA_Error If no sample format of name format_name is offered by libsndfile
     */
    void set_format(SF_INFO& sf_info);
    std::atomic<bool> close_session;
    bool act_;
    mhaconfig_t cf_;
    SNDFILE* sf;
    mha_fifo_lf_t<mha_real_t> fifo;
    unsigned int minw_;
    pthread_t writethread;
    float* data;
    std::string format_name;
};

class wavrec_t : public MHAPlugin::plugin_t<wavwriter_t> {
public:
    mha_wave_t* process(mha_wave_t*);
    void prepare(mhaconfig_t& cf);
    void release();
    wavrec_t(algo_comm_t iac, const std::string & configured_name);
private:
    void start_new_session();
    MHAParser::bool_t record;
    MHAParser::int_t fifolen;
    MHAParser::int_t minwrite;
    MHAParser::string_t prefix;
    MHAParser::bool_t use_date;
    MHAParser::kw_t output_sample_format;
    MHAEvents::patchbay_t<wavrec_t> patchbay;
};

wavrec_t::wavrec_t(algo_comm_t iac, const std::string & configured_name)
    : MHAPlugin::plugin_t<wavwriter_t>("wav file recorder", iac),
      record("Record session. Each write access with argument \"yes\" will "
             "start a\n"
             "new file with current time and date as a name.",
             "no"),
      fifolen("Length of FIFO in samples", "262144", "[2,]"),
      minwrite("Minimal write length (must be less then fifolen)", "65536",
               "[1,]"),
      prefix("Path (including path delimiter) and file prefix", ""),
      use_date("Use date and time (yes), or only prefix (no)", "yes"),
      output_sample_format("Output sample format", "32_bit_float",
                           "[32_bit_float]")
{
  // make the plug-in findable via "?listid"
  set_node_id(configured_name);

  insert_member(fifolen);
  insert_member(minwrite);
  insert_member(prefix);
  insert_member(use_date);
  insert_member(record);
  insert_member(output_sample_format);
  patchbay.connect(&record.writeaccess, this, &wavrec_t::start_new_session);
  int count(0);
  sf_command(NULL, SFC_GET_FORMAT_SUBTYPE_COUNT, &count, sizeof(int));
  for (int k = 0; k < count; k++) {
    SF_FORMAT_INFO format_info;
    format_info.format = k;
    sf_command(NULL, SFC_GET_FORMAT_SUBTYPE, &format_info, sizeof(format_info));
    std::string fmtname(format_info.name);
    MHAParser::strreplace(fmtname, " ", "_");
    output_sample_format.data.add_entry(fmtname);
  }
}

mha_wave_t* wavrec_t::process(mha_wave_t* s)
{
    poll_config()->process(s);
    return s;
}

void wavrec_t::prepare(mhaconfig_t& )
{
    start_new_session();
}

void wavrec_t::release()
{
    auto latest_cfg=peek_config();
    if(latest_cfg)
        latest_cfg->exit_request();
}

void wavrec_t::start_new_session()
{
    auto latest_cfg=peek_config();
    if(latest_cfg)
        latest_cfg->exit_request();
    push_config(new wavwriter_t(record.data,input_cfg(),fifolen.data,minwrite.data,prefix.data,use_date.data,output_sample_format.data.get_value()));
}

void wavwriter_t::create_soundfile(const std::string& prefix, bool use_date)
{
    std::string fname;
    if( use_date ){
        time_t tm(time(NULL));
#ifdef _WIN32
        fname = ctime(&tm);
#else
        char timestr[28];
        fname = ctime_r(&tm,timestr);
#endif
        fname.erase(fname.size()-1,1);
        MHAParser::strreplace(fname,":","-");
        MHAParser::strreplace(fname,"Mon "," ");
        MHAParser::strreplace(fname,"Tue "," ");
        MHAParser::strreplace(fname,"Wed "," ");
        MHAParser::strreplace(fname,"Thu "," ");
        MHAParser::strreplace(fname,"Fri "," ");
        MHAParser::strreplace(fname,"Sat "," ");
        MHAParser::strreplace(fname,"Sun "," ");
        MHAParser::strreplace(fname," ","_");
    }
    fname = prefix+fname+".wav";
    SF_INFO sfinfo;
    memset(&sfinfo,0,sizeof(sfinfo));
    sfinfo.samplerate = (int)cf_.srate;
    sfinfo.channels = cf_.channels;

    sfinfo.format = SF_FORMAT_WAV;
    set_format(sfinfo);
    sf = sf_open(fname.c_str(),SFM_WRITE,&sfinfo);
    sf_command(sf, SFC_SET_CLIPPING, NULL, SF_TRUE);
    if( !sf )
        throw MHA_Error(__FILE__,__LINE__,"Unable to create sound file %s: %s",fname.c_str(),sf_strerror(sf));
}

wavwriter_t::wavwriter_t(bool active,const mhaconfig_t& cf,unsigned int fifosize,unsigned int minwrite,
                         const std::string& prefix, bool use_date, const std::string& format_name_)
    : close_session(false),
      act_(active),
      cf_(cf),
      sf(NULL),
      fifo(fifosize),
      minw_(minwrite),
      data(new float[fifosize]),
      format_name(format_name_)
{
    if(minw_ >= fifosize )
        throw MHA_Error(__FILE__,__LINE__,"minwrite must be less then fifosize (minwrite: %u, fifosize: %u)",minw_,fifosize);
    if( (cf_.channels == 0) || (cf_.srate==0) )
        act_ = false;
    if( act_ ){
        create_soundfile(prefix, use_date);
        pthread_create(&writethread,NULL,&wavwriter_t::write_thread,this);
    }
}

wavwriter_t::~wavwriter_t()
{
    delete [] data;
}

void wavwriter_t::process(mha_wave_t* s)
{
    if( act_ ){
        auto nSamps=(fifo.get_available_space()/cf_.channels)*cf_.channels; // only write multiples of channels
        fifo.write(s->buf,std::min(nSamps,size(s)));
    }
}

void wavwriter_t::exit_request(){
    if(!close_session.load()){
        close_session.store(true);
        if( act_ ){
            pthread_join(writethread,NULL);
        }
        if( sf ){
            sf_close(sf);
            sf=nullptr;
        }
    }
    return;
}

void wavwriter_t::write_thread()
{
    while(!close_session.load()){
        mha_msleep(1);
        if( sf && (fifo.get_fill_count() > minw_) ){
            unsigned int frames = fifo.get_fill_count()/cf_.channels;
            fifo.read(data,frames*cf_.channels);
            sf_writef_float(sf,data,frames);
        }
    }
    if( sf && fifo.get_fill_count() ){
        unsigned int frames = fifo.get_fill_count()/cf_.channels;
        fifo.read(data,frames*cf_.channels);
        sf_writef_float(sf,data,frames);
    }
}

void wavwriter_t::set_format(SF_INFO& sfinfo) {
  int count(0);
  sf_command(NULL, SFC_GET_FORMAT_SUBTYPE_COUNT, &count, sizeof(int));
  for (int k = 0; k < count; k++) {
    SF_FORMAT_INFO format_info;
    format_info.format = k;
    sf_command(NULL, SFC_GET_FORMAT_SUBTYPE, &format_info, sizeof(format_info));
    std::string tmp(format_info.name);
    MHAParser::strreplace(tmp, " ", "_");
    if (format_name == tmp) {
      sfinfo.format =
          (sfinfo.format & (SF_FORMAT_TYPEMASK | SF_FORMAT_ENDMASK)) |
          format_info.format;
      return ;
    }
  }
  // This must be a bug in the implementation as the range of the cfg variable is restricted to
  // the list of available formats of libsndfile
  throw MHA_Error(__FILE__,__LINE__,"Bug: Could not find format: %s",format_name.c_str());
}

MHAPLUGIN_CALLBACKS(wavrec,wavrec_t,wave,wave)
MHAPLUGIN_DOCUMENTATION\
(wavrec,
 "data-export disk-files",
 "Wave file recorder plugin. This plugin writes the current audio signal to a wave file in a thread-safe manner."
 " A new wave file is opened every time the record variable is set to yes. The file is"
 " closed on any of \"cmd=release\", \"cmd=quit\" or \"record=no\". Note that \"cmd=stop\" does not"
 " close the wave file. After the the close command is given, it can take an unspecified, but usually small amount"
 " amount of time until the file is actually closed and ready for further processing. \n"
 " The name (and path) of the output file is chosen by the prefix configuration variable. By default the current"
 " date and time are appended to the file name, this behaviour can be controlled by the \"use\\_date\" variable.\n"
 "The \"fifolen\" and \"minwrite\" variables control the behaviour of the fifo buffer and should usually remain unchanged.")

/*
 * Local Variables:
 * compile-command: "make"
 * c-basic-offset: 4
 * End:
 */
