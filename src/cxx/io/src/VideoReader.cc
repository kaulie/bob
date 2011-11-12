/**
 * @author Andre Anjos <andre.anjos@idiap.ch>
 *
 * Implements a class to read and write Video files and convert the frames into
 * something that torch can understand. This implementation is heavily based on
 * the excellent tutorial here: http://dranger.com/ffmpeg/, with some personal
 * modifications.
 *
 * FFMpeg versions for your reference
 * ffmpeg | avformat | avcodec  | avutil  | swscale | old style | swscale GPL?
 * =======+==========+==========+=========+=========+===========+==============
 * 0.5    | 52.31.0  | 52.20.0  | 49.15.0 | 0.7.1   | yes       | yes
 * 0.5.1  | 52.31.0  | 52.20.1  | 49.15.0 | 0.7.1   | yes       | yes
 * 0.5.2  | 52.31.0  | 52.20.1  | 49.15.0 | 0.7.1   | yes       | yes
 * 0.5.3  | 52.31.0  | 52.20.1  | 49.15.0 | 0.7.1   | yes       | yes
 * 0.6    | 52.64.2  | 52.72.2  | 50.15.1 | 0.11.0  | no        | no
 * 0.6.1  | 52.64.2  | 52.72.2  | 50.15.1 | 0.11.0  | no        | no
 * 0.7    | 52.110.0 | 52.122.0 | 50.43.0 | 0.14.1  | no        | no
 * 0.7.1  | 52.110.0 | 52.122.0 | 50.43.0 | 0.14.1  | no        | no
 * 0.8    | 53.4.0   | 53.7.0   | 51.9.1  | 2.0.0   | no        | no
 */

#include <stdexcept>
#include <boost/format.hpp>
#include <boost/preprocessor.hpp>
#include <limits>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

#include "core/array_check.h"
#include "io/VideoReader.h"
#include "io/Exception.h"
#include "io/VideoException.h"
#include "core/blitz_array.h"

namespace io = Torch::io;
namespace ca = Torch::core::array;

/**
 * When called, initializes the ffmpeg library
 */
static bool initialize_ffmpeg() {
  av_register_all();
  av_log_set_level(-1);
  return true;
}

bool io::VideoReader::s_ffmpeg_initialized = initialize_ffmpeg();

io::VideoReader::VideoReader(const std::string& filename):
  m_filepath(filename),
  m_height(0),
  m_width(0),
  m_nframes(0),
  m_framerate(0),
  m_duration(0),
  m_codecname(""),
  m_codecname_long(""),
  m_formatted_info("")
{
  open();
}

io::VideoReader::VideoReader(const io::VideoReader& other):
  m_filepath(other.m_filepath),
  m_height(0),
  m_width(0),
  m_nframes(0),
  m_framerate(0),
  m_duration(0),
  m_codecname(""),
  m_codecname_long(""),
  m_formatted_info("")
{
  open();
}

io::VideoReader& io::VideoReader::operator= (const io::VideoReader& other) {
  m_filepath = other.m_filepath;
  m_height = 0;
  m_width = 0;
  m_nframes = 0;
  m_framerate = 0.0;
  m_duration = 0;
  m_codecname = "";
  m_codecname_long = "";
  m_formatted_info = "";
  open();
  return *this;
}

void io::VideoReader::open() {
  AVFormatContext* format_ctxt = 0;

  // Opens a video file
  // ffmpeg 0.7 and above [libavformat 52.122.0 = 0x347a00]
# if LIBAVCODEC_VERSION_INT >= 0x347a00
  if (avformat_open_input(&format_ctxt, m_filepath.c_str(), NULL, NULL) != 0) 
# else
  if (av_open_input_file(&format_ctxt, m_filepath.c_str(), NULL, 0, NULL) != 0) 
# endif
  {
    throw io::FileNotReadable(m_filepath);
  }

  // Retrieve stream information
  if (av_find_stream_info(format_ctxt)<0) {
    av_close_input_file(format_ctxt);
    throw io::FFmpegException(m_filepath.c_str(), "cannot find stream info");
  }

  // Look for the first video stream in the file
  int stream_index = -1;
  for (size_t i=0; i<format_ctxt->nb_streams; ++i) {
#   if LIBAVUTIL_VERSION_INT >= 0x330000
    if (format_ctxt->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) 
#   else
    if (format_ctxt->streams[i]->codec->codec_type==CODEC_TYPE_VIDEO) 
#   endif
    {
      stream_index = i;
      break;
    }
  }
  if(stream_index == -1) {
    av_close_input_file(format_ctxt);
    throw io::FFmpegException(m_filepath.c_str(), "cannot find any video stream");
  }

  // Get a pointer to the codec context for the video stream
  AVCodecContext* codec_ctxt = format_ctxt->streams[stream_index]->codec;

  // Hack to correct frame rates that seem to be generated by some codecs 
  if(codec_ctxt->time_base.num > 1000 && codec_ctxt->time_base.den == 1) {
    codec_ctxt->time_base.den = 1000;
  }

  // Find the decoder for the video stream
  AVCodec* codec = avcodec_find_decoder(codec_ctxt->codec_id);

  if (!codec) {
    av_close_input_file(format_ctxt);
    throw io::FFmpegException(m_filepath.c_str(), "unsupported codec required");
  }

  // Open codec
  if (avcodec_open(codec_ctxt, codec) < 0) {
    av_close_input_file(format_ctxt);
    throw io::FFmpegException(m_filepath.c_str(), "cannot open supported codec");
  }

  /**
   * Copies some information from the contexts opened
   */
  m_width = codec_ctxt->width;
  m_height = codec_ctxt->height;
  m_duration = format_ctxt->duration;
  m_nframes = format_ctxt->streams[stream_index]->nb_frames;
  if (m_nframes > 0) {
    //number of frames is known
    m_framerate = m_nframes * AV_TIME_BASE / m_duration;
  }
  else {
    //number of frames is not known
    m_framerate = av_q2d(format_ctxt->streams[stream_index]->r_frame_rate);
    m_nframes = (int)(m_framerate * m_duration / AV_TIME_BASE);
  }
  m_codecname = codec->name;
  m_codecname_long = codec->long_name;

  /**
   * This will create a local description of the contents of the stream, in
   * printable format.
   */
  boost::format fmt("Video file: %s; FFmpeg: avformat-%s; avcodec-%s; avutil-%s; swscale-%d; Codec: %s (%s); Time: %.2f s (%d @ %2.fHz); Size (w x h): %d x %d pixels");
  fmt % m_filepath;
  fmt % BOOST_PP_STRINGIZE(LIBAVFORMAT_VERSION);
  fmt % BOOST_PP_STRINGIZE(LIBAVCODEC_VERSION);
  fmt % BOOST_PP_STRINGIZE(LIBAVUTIL_VERSION);
  fmt % BOOST_PP_STRINGIZE(LIBSWSCALE_VERSION);
  fmt % m_codecname_long;
  fmt % m_codecname;
  fmt % (m_duration / 1e6);
  fmt % m_nframes;
  fmt % m_framerate;
  fmt % m_width;
  fmt % m_height;
  m_formatted_info = fmt.str();

  /**
   * This will make sure we can interface with the io subsystem
   */
  m_typeinfo_video.dtype = m_typeinfo_frame.dtype = core::array::t_uint8;
  m_typeinfo_video.nd = 4;
  m_typeinfo_frame.nd = 3;
  m_typeinfo_video.shape[0] = m_nframes;
  m_typeinfo_video.shape[1] = m_typeinfo_frame.shape[0] = 3;
  m_typeinfo_video.shape[2] = m_typeinfo_frame.shape[1] = m_height;
  m_typeinfo_video.shape[3] = m_typeinfo_frame.shape[2] = m_width;
  m_typeinfo_frame.update_strides();
  m_typeinfo_video.update_strides();

  //closes the codec we used
  avcodec_close(codec_ctxt);

  //and we close the input file
  av_close_input_file(format_ctxt);
}

io::VideoReader::~VideoReader() {
}

void io::VideoReader::load(blitz::Array<uint8_t,4>& data) const {
  ca::blitz_array tmp(data);
  load(tmp);
}

void io::VideoReader::load(ca::interface& b) const {

  //checks if the output array shape conforms to the video specifications,
  //otherwise, throw.
  if (!m_typeinfo_video.is_compatible(b.type())) {
    boost::format s("input buffer (%s) does not conform to the video size specifications (%s)");
    s % b.type().str() % m_typeinfo_video.str();
    throw std::invalid_argument(s.str().c_str());
  }

  unsigned long int frame_size = m_typeinfo_frame.buffer_size();
  uint8_t* ptr = static_cast<uint8_t*>(b.ptr());

  for (const_iterator it=begin(); it!=end();) {
    ca::blitz_array ref(static_cast<void*>(ptr), m_typeinfo_frame);
    it.read(ref);
    ptr += frame_size;
  }

}

io::VideoReader::const_iterator io::VideoReader::begin() const {
  return io::VideoReader::const_iterator(this);
}

io::VideoReader::const_iterator io::VideoReader::end() const {
  return io::VideoReader::const_iterator();
}

/**
 * iterator implementation
 */

io::VideoReader::const_iterator::const_iterator(const io::VideoReader* parent) :
  m_parent(parent),
  m_format_ctxt(0),
  m_stream_index(-1),
  m_codec_ctxt(0),
  m_codec(0),
  m_frame_buffer(0),
  m_rgb_frame_buffer(0),
  m_raw_buffer(0),
  m_current_frame(std::numeric_limits<size_t>::max()),
  m_sws_context(0)
{
  init();
}

io::VideoReader::const_iterator::const_iterator():
  m_parent(0),
  m_format_ctxt(0),
  m_stream_index(-1),
  m_codec_ctxt(0),
  m_codec(0),
  m_frame_buffer(0),
  m_rgb_frame_buffer(0),
  m_raw_buffer(0),
  m_current_frame(std::numeric_limits<size_t>::max()),
  m_sws_context(0)
{
}

io::VideoReader::const_iterator::const_iterator
(const io::VideoReader::const_iterator& other) :
  m_parent(other.m_parent),
  m_format_ctxt(0),
  m_stream_index(-1),
  m_codec_ctxt(0),
  m_codec(0),
  m_frame_buffer(0),
  m_rgb_frame_buffer(0),
  m_raw_buffer(0),
  m_current_frame(std::numeric_limits<size_t>::max()),
  m_sws_context(0)
{
  init();
  (*this) += other.m_current_frame;
}

io::VideoReader::const_iterator::~const_iterator() {
  reset();
}

io::VideoReader::const_iterator& io::VideoReader::const_iterator::operator= (const io::VideoReader::const_iterator& other) {
  reset();
  m_parent = other.m_parent;
  init();
  (*this) += other.m_current_frame;
  return *this;
}

void io::VideoReader::const_iterator::init() {
  const char* filename = m_parent->filename().c_str();

  //basic constructor, prepare readout
  // Opens a video file
  // ffmpeg 0.7 and above [libavformat 53.0.0 = 0x350000]
# if LIBAVCODEC_VERSION_INT >= 0x347a00
  if (avformat_open_input(&m_format_ctxt, filename, NULL, NULL) != 0) 
# else
  if (av_open_input_file(&m_format_ctxt, filename, NULL, 0, NULL) != 0) 
# endif
  {
    throw io::FileNotReadable(filename);
  }

  // Retrieve stream information
  if (av_find_stream_info(m_format_ctxt)<0) {
    throw io::FFmpegException(filename, "cannot find stream info");
  }

  // Look for the first video stream in the file
  for (size_t i=0; i<m_format_ctxt->nb_streams; ++i) {
#   if LIBAVUTIL_VERSION_INT >= 0x330000
    if (m_format_ctxt->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) 
#   else
    if (m_format_ctxt->streams[i]->codec->codec_type==CODEC_TYPE_VIDEO) 
#   endif
    {
      m_stream_index = i;
      break;
    }
  }
  if(m_stream_index == -1) {
    throw io::FFmpegException(filename, "cannot find any video stream");
  }

  // Get a pointer to the codec context for the video stream
  m_codec_ctxt = m_format_ctxt->streams[m_stream_index]->codec;

  // Find the decoder for the video stream
  m_codec = avcodec_find_decoder(m_codec_ctxt->codec_id);

  if (!m_codec) {
    throw io::FFmpegException(filename, "unsupported codec required");
  }

  // Open codec
  if (avcodec_open(m_codec_ctxt, m_codec) < 0) {
    throw io::FFmpegException(filename, "cannot open supported codec");
  }

  // Hack to correct frame rates that seem to be generated by some codecs 
  if(m_codec_ctxt->time_base.num > 1000 && m_codec_ctxt->time_base.den == 1)
    m_codec_ctxt->time_base.den = 1000;

  // Allocate memory for a buffer to read frames
  m_frame_buffer = avcodec_alloc_frame();
  if (!m_frame_buffer) {
    throw io::FFmpegException(filename, "cannot allocate frame buffer");
  }

  // Allocate memory for a second buffer that contains RGB converted data.
  m_rgb_frame_buffer = avcodec_alloc_frame();
  if (!m_rgb_frame_buffer) {
    throw io::FFmpegException(filename, "cannot allocate RGB frame buffer");
  }

  // Allocate memory for the raw data buffer
  int nbytes = avpicture_get_size(PIX_FMT_RGB24, m_codec_ctxt->width,
      m_codec_ctxt->height);
  m_raw_buffer = (uint8_t*)av_malloc(nbytes*sizeof(uint8_t));
  if (!m_raw_buffer) {
    throw io::FFmpegException(filename, "cannot allocate raw frame buffer");
  }
  
  // Assign appropriate parts of buffer to image planes in m_rgb_frame_buffer
  avpicture_fill((AVPicture *)m_rgb_frame_buffer, m_raw_buffer, PIX_FMT_RGB24,
      m_parent->width(), m_parent->height());

  /**
   * Initializes the software scaler (SWScale) so we can convert images from
   * the movie native format into RGB. You can define which kind of
   * interpolation to perform. Some options from libswscale are:
   * SWS_FAST_BILINEAR, SWS_BILINEAR, SWS_BICUBIC, SWS_X, SWS_POINT, SWS_AREA
   * SWS_BICUBLIN, SWS_GAUSS, SWS_SINC, SWS_LANCZOS, SWS_SPLINE
   */
  m_sws_context = sws_getContext(m_parent->width(), m_parent->height(),
      m_codec_ctxt->pix_fmt, m_parent->width(), m_parent->height(),
      PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
  if (!m_sws_context) {
    throw io::FFmpegException(filename, "cannot initialize software scaler");
  }

  //At this point we are ready to start reading out frames.
  m_current_frame = 0;
  
  //The file maybe valid, but contain zero frames... We check for this here:
  if (m_current_frame >= m_parent->numberOfFrames()) {
    //transform the current iterator in "end"
    reset();
  }
}

void io::VideoReader::const_iterator::reset() {
  //free-up memory
  if (m_frame_buffer) { 
    av_free(m_frame_buffer); 
    m_frame_buffer = 0;
  }
  
  if (m_rgb_frame_buffer) { 
    av_free(m_rgb_frame_buffer); 
    m_rgb_frame_buffer = 0;
  }

  if (m_raw_buffer) { 
    av_free(m_raw_buffer); 
    m_raw_buffer=0; 
  }
  
  if (m_sws_context) { 
    sws_freeContext(m_sws_context);
    m_sws_context=0; 
  }
  
  //closes the codec we used
  if (m_codec_ctxt) {
    avcodec_close(m_codec_ctxt);
    m_codec = 0;
  }
  
  //closes the video file we opened
  if (m_format_ctxt) {
    av_close_input_file(m_format_ctxt);
    m_codec_ctxt = 0;
    m_format_ctxt = 0;
  }

  m_current_frame = std::numeric_limits<size_t>::max(); //that means "end" 

  m_parent = 0;
}

void io::VideoReader::const_iterator::read(blitz::Array<uint8_t,3>& data) {
  ca::blitz_array tmp(data);
  read(tmp);
}

void io::VideoReader::const_iterator::read(ca::interface& data) {

  //checks if we have not passed the end of the video sequence already
  if(m_current_frame > m_parent->numberOfFrames()) {
    throw io::IndexError(m_current_frame);
  }

  const ca::typeinfo& info = data.type();

  //checks if the output array shape conforms to the video specifications,
  //otherwise, throw
  if (!info.is_compatible(m_parent->m_typeinfo_frame)) {
    boost::format s("input buffer (%s) does not conform to the video frame size specifications (%s)");
    s % info.str() % m_parent->m_typeinfo_frame.str();
    throw std::invalid_argument(s.str().c_str());
  }

  int gotPicture = 0;
  AVPacket packet;
  av_init_packet(&packet);

  while (av_read_frame(m_format_ctxt, &packet) >= 0) {
    // Is this a packet from the video stream?
    if (packet.stream_index == m_stream_index) {
  
      // Decodes video frame, store it on my buffer
      // ffmpeg 0.6 and above [libavcodec 52.72.2 = 0x344802]
#if LIBAVCODEC_VERSION_INT >= 0x344802
      avcodec_decode_video2(m_codec_ctxt, m_frame_buffer, &gotPicture, &packet);
#else
      avcodec_decode_video(m_codec_ctxt, m_frame_buffer, &gotPicture, packet.data, packet.size);
#endif

      // Did we get a video frame?
      if (gotPicture) {
        sws_scale(m_sws_context, m_frame_buffer->data, m_frame_buffer->linesize,
            0, m_parent->height(), m_rgb_frame_buffer->data, 
            m_rgb_frame_buffer->linesize);

        // Got the image - exit
        ++m_current_frame;

        // Frees the packet that was allocated by av_read_frame
        av_free_packet(&packet);
        break;
      }
    }

    // Frees the packet that was allocated by av_read_frame
    av_free_packet(&packet);
  }

  // Copies the data into the destination array. Here is some background: Torch
  // arranges the data for a colored image like: (color-bands, height, width).
  // That makes it easy to extract a given band from the image as its memory is
  // contiguous. FFmpeg prefers the following encoding (height, width,
  // color-bands).
  //
  // Note: The FFmpeg way to read and write image data is hard-coded and
  // impossible to circumvent by passing a different stride setup.

  // transpose the data.
  blitz::TinyVector<int,3> shape;
  blitz::TinyVector<int,3> stride;
  
  shape = info.shape[0], info.shape[1], info.shape[2];
  stride = info.stride[0], info.stride[1], info.stride[2];
  blitz::Array<uint8_t,3> dst(static_cast<uint8_t*>(data.ptr()), shape, stride,
      blitz::neverDeleteData);
  
  shape = info.shape[1], info.shape[2], info.shape[0];
  blitz::Array<uint8_t,3> src(m_rgb_frame_buffer->data[0], shape, 
      blitz::neverDeleteData);
  dst = src.transpose(2,0,1);

  if (m_current_frame >= m_parent->numberOfFrames()) {
    //transform the current iterator in "end"
    reset();
  }
}

/**
 * frame seek functions extracted from:
 * http://stackoverflow.com/questions/5261658/how-to-seek-in-ffmpeg-c-c
 * TODO: Based on the methods bellow, implement better frame seeking.
 */

/**
bool seekMs(int tsms)
{
   //printf("**** SEEK TO ms %d. LLT: %d. LT: %d. LLF: %d. LF: %d. LastFrameOk: %d\n",tsms,LastLastFrameTime,LastFrameTime,LastLastFrameNumber,LastFrameNumber,(int)LastFrameOk);

   // Convert time into frame number
   DesiredFrameNumber = ffmpeg::av_rescale(tsms,pFormatCtx->streams[videoStream]->time_base.den,pFormatCtx->streams[videoStream]->time_base.num);
   DesiredFrameNumber/=1000;

   return seekFrame(DesiredFrameNumber);
}

bool seekFrame(ffmpeg::int64_t frame)
{

   //printf("**** seekFrame to %d. LLT: %d. LT: %d. LLF: %d. LF: %d. LastFrameOk: %d\n",(int)frame,LastLastFrameTime,LastFrameTime,LastLastFrameNumber,LastFrameNumber,(int)LastFrameOk);

   // Seek if:
   // - we don't know where we are (Ok=false)
   // - we know where we are but:
   //    - the desired frame is after the last decoded frame (this could be optimized: if the distance is small, calling decodeSeekFrame may be faster than seeking from the last key frame)
   //    - the desired frame is smaller or equal than the previous to the last decoded frame. Equal because if frame==LastLastFrameNumber we don't want the LastFrame, but the one before->we need to seek there
   if( (LastFrameOk==false) || ((LastFrameOk==true) && (frame<=LastLastFrameNumber || frame>LastFrameNumber) ) )
   {
      //printf("\t avformat_seek_file\n");
      if(ffmpeg::avformat_seek_file(pFormatCtx,videoStream,0,frame,frame,AVSEEK_FLAG_FRAME)<0)
         return false;

      avcodec_flush_buffers(pCodecCtx);

      DesiredFrameNumber = frame;
      LastFrameOk=false;
   }
   //printf("\t decodeSeekFrame\n");

   return decodeSeekFrame(frame);

   return true;
}

**/

/**
 * This method does essentially the same as read(), except it skips a few
 * operations to get a better performance.
 */
io::VideoReader::const_iterator& io::VideoReader::const_iterator::operator++ () {
  //checks if we have not passed the end of the video sequence already
  if(m_current_frame > m_parent->numberOfFrames()) {
    throw io::IndexError(m_current_frame);
  }

  int gotPicture = 0;
  AVPacket packet;
  av_init_packet(&packet);

  while (av_read_frame(m_format_ctxt, &packet) >= 0) {
    // Is this a packet from the video stream?
    if (packet.stream_index == m_stream_index) {
      // Decodes video frame, store it on my buffer
#if LIBAVCODEC_VERSION_INT >= 0x344802
      avcodec_decode_video2(m_codec_ctxt, m_frame_buffer, &gotPicture, &packet);
#else
      avcodec_decode_video(m_codec_ctxt, m_frame_buffer, &gotPicture, packet.data, packet.size);
#endif

      // Did we get a video frame?
      if (gotPicture) {
        // Got the image - exit
        ++m_current_frame;

        // Frees the packet that was allocated by av_read_frame
        av_free_packet(&packet);
        break;
      }
    }

    // Frees the packet that was allocated by av_read_frame
    av_free_packet(&packet);
  }

  if (m_current_frame >= m_parent->numberOfFrames()) {
    //transform the current iterator in "end"
    reset();
  }
  return *this;
}

io::VideoReader::const_iterator& io::VideoReader::const_iterator::operator+= (size_t frames) {
  for (size_t i=0; i<frames; ++i) ++(*this);
  return *this;
}

bool io::VideoReader::const_iterator::operator== (const const_iterator& other) {
  return (this->m_parent == other.m_parent) && (this->m_current_frame == other.m_current_frame);
}

bool io::VideoReader::const_iterator::operator!= (const const_iterator& other) {
  return !(*this == other);
}
