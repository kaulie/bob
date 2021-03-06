/**
 * @file io/python/video.cc
 * @date Wed Jun 22 17:50:08 2011 +0200
 * @author Andre Anjos <andre.anjos@idiap.ch>
 *
 * @brief Binds Video constructions to python
 *
 * Copyright (C) 2011-2013 Idiap Research Institute, Martigny, Switzerland
 */

#include <boost/python.hpp>
#include <boost/python/slice.hpp>

#include <bob/io/VideoReader.h>
#include <bob/io/VideoWriter.h>
#include <bob/config.h>

#include <bob/io/VideoUtilities.h>
#include <bob/python/exception.h>
#include <bob/python/ndarray.h>
#include <bob/python/gil.h>

extern "C" {
#if LIBAVUTIL_VERSION_INT >= 0x320f01 //50.15.1 @ ffmpeg-0.6
#  include <libavutil/opt.h>
#  include <libavutil/pixdesc.h>
#endif
}

using namespace boost::python;

#if !HAVE_FFMPEG_AVCODEC_AVCODECID
#define AVCodecID CodecID
#endif

/**
 * Helper method for the the iterator wrapping
 */
static inline object pass_through(object const& o) { return o; }

/**
 * Python wrapper to make the Video::Reader::const_iterator look like a python
 * iterator
 */
struct iterator_wrapper {

  /**
   * Method to return the value pointed by the iterator and advance it
   */
  static object next (bob::io::VideoReader::const_iterator& o) {
    const bob::io::VideoReader* reader = o.parent();

    if (!reader) { //stop iteration immediately
      PYTHON_ERROR(StopIteration, "no more data");
    }

    //load the next frame: if an error is detected internally, throw
    bob::python::py_array retval(reader->frame_type());
    bool ok = o.read(retval); //note that this will advance the iterator
    if (!ok) PYTHON_ERROR(StopIteration, "iteration finished");
    return retval.pyobject();
  }

  /**
   * Method to wrap the iterator for python
   */
  static void wrap () {
    class_<bob::io::VideoReader::const_iterator>("VideoReaderIterator", no_init)
      .def("next", next)
      .def("__next__", next)
      .def("__iter__", pass_through)
      ;
  }

};

/**
 * Python wrapper to read a single frame from a video sequence, allowing the
 * implementation of a __getitem__() functionality on VideoReader objects.
 */
static object videoreader_getitem (bob::io::VideoReader& v, object intobj) {
  Py_ssize_t sframe = extract<Py_ssize_t>(long_(intobj));
  size_t frame = sframe;
  if (sframe < 0) frame = v.numberOfFrames() + sframe;

  if (frame >= v.numberOfFrames()) { //basic check
    PYTHON_ERROR(IndexError, "invalid index (" SIZE_T_FMT ") >= number of frames (" SIZE_T_FMT ")", frame, v.numberOfFrames());
  }

  bob::python::py_array retval(v.frame_type());
  bob::io::VideoReader::const_iterator it = v.begin();
  it += frame;
  it.read(retval); //read and throw if a problem occurs
  return retval.pyobject();
}

/**
 * Python wrapper to read multiple frames from a video sequence, allowing the
 * implementation of a __getitem__() functionality on VideoReader objects.
 */
static object videoreader_getslice (bob::io::VideoReader& v, slice sobj) {
  size_t start = 0;
  PySliceObject* sl = (PySliceObject*)sobj.ptr();
  if (sl->start != Py_None) {
#if PY_VERSION_HEX >= 0x03000000
    Py_ssize_t sstart = PyLong_AsLong(sl->start);
#else
    Py_ssize_t sstart = PyInt_AsLong(sl->start);
#endif
    start = sstart;
    if (sstart < 0) start = v.numberOfFrames() + sstart;
  }

  if (start >= v.numberOfFrames()) { //basic check
    PYTHON_ERROR(IndexError, "invalid start (" SIZE_T_FMT ") >= number of frames (" SIZE_T_FMT ")", start, v.numberOfFrames());
  }

  //the stop value may be None
  size_t stop = v.numberOfFrames();
  if (sl->stop != Py_None) {
#if PY_VERSION_HEX >= 0x03000000
    Py_ssize_t sstop = PyLong_AsLong(sl->stop);
#else
    Py_ssize_t sstop = PyInt_AsLong(sl->stop);
#endif
    stop = sstop;
    if (sstop < 0) stop = v.numberOfFrames() + sstop;
  }
  if (stop >= v.numberOfFrames()) stop = v.numberOfFrames()+1;

  //the step value may be None
  int64_t step = 1;
  if (sl->step != Py_None) {
#if PY_VERSION_HEX >= 0x03000000
    step = PyLong_AsLong(sl->step);
#else
    step = PyInt_AsLong(sl->step);
#endif
  }

  //length of the sequence
  int length = (stop-start)/step;
  if (length == 0) length = 1; //a single return

  list retval;
  bob::io::VideoReader::const_iterator it = v.begin();
  it += start;
  for (size_t i=start; it.parent() && i<stop; i+=step, it+=(step-1)) {
    bob::python::check_signals(); //catches keyboard interruption
    bob::python::py_array tmp(v.frame_type());
    it.read(tmp); //throw if a problem occurs while reading the video
    retval.append(tmp.pyobject());
  }

  bob::python::py_array py_retval(retval, str("uint8"));
  return py_retval.pyobject();
}

static object videoreader_load(bob::io::VideoReader& reader,
  bool raise_on_error=false) {
  bob::python::py_array tmp(reader.video_type());
  size_t frames_read = 0;
  frames_read = reader.load(tmp, raise_on_error, bob::python::check_signals);
  return make_tuple(frames_read, tmp.pyobject());
}

BOOST_PYTHON_FUNCTION_OVERLOADS(videoreader_load_overloads, videoreader_load, 1, 2)

static void videowriter_append(bob::io::VideoWriter& writer, object a) {
  bob::python::convert_t result = bob::python::convertible_to(a, writer.frame_type(),
      false, true);
  if (result != bob::python::IMPOSSIBLE) {
    bob::python::dtype dtype(writer.frame_type().dtype);
    bob::python::py_array tmp(a, dtype.self());
    writer.append(tmp);
  }
  else {
    bob::python::dtype dtype(writer.video_type().dtype);
    bob::python::py_array tmp(a, dtype.self());
    writer.append(tmp);
  }
}

/**
 * Describes a given codec or returns an empty dictionary, in case the codec
 * cannot be accessed
 */
static object describe_codec(const AVCodec* codec) {

  dict retval;

  retval["name"] = codec->name;
  retval["long_name"] = codec->long_name;
  retval["id"] = (unsigned)codec->id;

  // get pix formats
  if (codec->pix_fmts) {
    list pixfmt;
    unsigned int i=0;
    while(codec->pix_fmts[i] != -1) {
#if LIBAVUTIL_VERSION_INT >= 0x320f01 //50.15.1 @ ffmpeg-0.6
      pixfmt.append(av_get_pix_fmt_name(codec->pix_fmts[i++]));
#else
      pixfmt.append(avcodec_get_pix_fmt_name(codec->pix_fmts[i++]));
#endif
    }
    retval["pixfmts"] = tuple(pixfmt);
  }
  else retval["pixfmts"] = object();

  // get specific framerates for the codec, if any:
  const AVRational* rate = codec->supported_framerates;
  list rates;
  while (rate && rate->num && rate->den) {
    rates.append( ((double)rate->num)/((double)rate->den) );
    ++rate;
  }
  retval["specific_framerates_hz"] = tuple(rates);

  // get codec capabilities
# ifdef CODEC_CAP_LOSSLESS
  retval["lossless"] = (bool)(codec->capabilities & CODEC_CAP_LOSSLESS);
# endif
# ifdef CODEC_CAP_EXPERIMENTAL
  retval["experimental"] = (bool)(codec->capabilities & CODEC_CAP_EXPERIMENTAL);
# endif
# ifdef CODEC_CAP_DELAY
  retval["delay"] = (bool)(codec->capabilities & CODEC_CAP_DELAY);
# endif
# ifdef CODEC_CAP_HWACCEL
  retval["hardware_accelerated"] = (bool)(codec->capabilities & CODEC_CAP_HWACCEL);
# endif
  retval["encode"] = (bool)(avcodec_find_encoder(codec->id));
  retval["decode"] = (bool)(avcodec_find_decoder(codec->id));

  return retval;
}

/**
 * Describes a given codec or raises, in case the codec cannot be accessed
 */
static object describe_encoder_by_name(const char* name) {
  AVCodec* codec = avcodec_find_encoder_by_name(name);
  if (!codec) PYTHON_ERROR(RuntimeError, "ffmpeg::avcodec_find_encoder_by_name(`%s') did not return a valid codec", name);
  return describe_codec(codec);
}

/**
 * Describes a given codec or raises, in case the codec cannot be accessed
 */
static object describe_encoder_by_id(int id) {
  AVCodec* codec = avcodec_find_encoder((AVCodecID)id);
  if (!codec) PYTHON_ERROR(RuntimeError, "ffmpeg::avcodec_find_encoder(%d == 0x%x) did not return a valid codec", id, id);
  return describe_codec(codec);
}

/**
 * Describes a given codec or raises, in case the codec cannot be accessed
 */
static object describe_decoder_by_name(const char* name) {
  AVCodec* codec = avcodec_find_decoder_by_name(name);
  if (!codec) PYTHON_ERROR(RuntimeError, "ffmpeg::avcodec_find_decoder_by_name(`%s') did not return a valid codec", name);
  return describe_codec(codec);
}

/**
 * Describes a given codec or raises, in case the codec cannot be accessed
 */
static object describe_decoder_by_id(int id) {
  AVCodec* codec = avcodec_find_decoder((AVCodecID)id);
  if (!codec) PYTHON_ERROR(RuntimeError, "ffmpeg::avcodec_find_encoder(%d == 0x%x) did not return a valid codec", id, id);
  return describe_codec(codec);
}

/**
 * Returns all input formats supported, related codecs and extensions
 */
static dict supported_iformat_dictionary() {
  std::map<std::string, AVInputFormat*> m;
  bob::io::detail::ffmpeg::iformats_supported(m);
  dict retval;

  for (auto k=m.begin(); k!=m.end(); ++k) {
    dict property;
    property["name"] = k->second->name;
    property["long_name"] = k->second->long_name;

    // get extensions
    std::vector<std::string> exts;
    bob::io::detail::ffmpeg::tokenize_csv(k->second->extensions, exts);
    list ext_list;
    for (auto ext=exts.begin(); ext!=exts.end(); ++ext) ext_list.append(*ext);
    property["extensions"] = tuple(ext_list);

    retval[k->first] = property;
  }

  return retval;
}

static dict available_iformat_dictionary() {
  std::map<std::string, AVInputFormat*> m;
  bob::io::detail::ffmpeg::iformats_installed(m);
  dict retval;

  for (auto k=m.begin(); k!=m.end(); ++k) {
    dict property;
    property["name"] = k->second->name;
    property["long_name"] = k->second->long_name;

    // get extensions
    std::vector<std::string> exts;
    bob::io::detail::ffmpeg::tokenize_csv(k->second->extensions, exts);
    list ext_list;
    for (auto ext=exts.begin(); ext!=exts.end(); ++ext) ext_list.append(*ext);
    property["extensions"] = tuple(ext_list);

    retval[k->first] = property;
  }

  return retval;
}

/**
 * Returns all output formats supported, related codecs and extensions
 */
static dict supported_oformat_dictionary() {
  std::map<std::string, AVOutputFormat*> m;
  bob::io::detail::ffmpeg::oformats_supported(m);
  dict retval;

  for (auto k=m.begin(); k!=m.end(); ++k) {
    dict property;
    property["name"] = k->second->name;
    property["long_name"] = k->second->long_name;
    property["mime_type"] = k->second->mime_type;

    // get extensions
    std::vector<std::string> exts;
    bob::io::detail::ffmpeg::tokenize_csv(k->second->extensions, exts);
    list ext_list;
    for (auto ext=exts.begin(); ext!=exts.end(); ++ext) ext_list.append(*ext);
    property["extensions"] = tuple(ext_list);

    // get recommended codec
    if (!k->second->video_codec) {
      property["default_codec"] = object();
    }
    else {
      AVCodec* codec = avcodec_find_encoder(k->second->video_codec);
      if (!codec) property["default_codec"] = object();
      else property["default_codec"] = describe_codec(codec);
    }

    // supported codec list
    std::vector<const AVCodec*> codecs;
    bob::io::detail::ffmpeg::oformat_supported_codecs(k->second->name, codecs);
    dict supported_codecs;
    for (auto c=codecs.begin(); c!=codecs.end(); ++c) {
      supported_codecs[(*c)->name] = describe_codec(*c);
    }
    property["supported_codecs"] = supported_codecs;

    retval[k->first] = property;
  }

  return retval;
}

static dict available_oformat_dictionary() {
  std::map<std::string, AVOutputFormat*> m;
  bob::io::detail::ffmpeg::oformats_installed(m);
  dict retval;

  for (auto k=m.begin(); k!=m.end(); ++k) {
    dict property;
    property["name"] = k->second->name;
    property["long_name"] = k->second->long_name;
    property["mime_type"] = k->second->mime_type;

    // get extensions
    std::vector<std::string> exts;
    bob::io::detail::ffmpeg::tokenize_csv(k->second->extensions, exts);
    list ext_list;
    for (auto ext=exts.begin(); ext!=exts.end(); ++ext) ext_list.append(*ext);
    property["extensions"] = tuple(ext_list);

    // get recommended codec
    if (!k->second->video_codec) {
      property["default_codec"] = object();
    }
    else {
      AVCodec* codec = avcodec_find_encoder(k->second->video_codec);
      if (!codec) property["default_codec"] = object();
      else property["default_codec"] = describe_codec(codec);
    }

    retval[k->first] = property;
  }

  return retval;
}

/**
 * Returns a dictionary of available codecs
 */
static object supported_codec_dictionary() {
  std::map<std::string, const AVCodec*> m;
  bob::io::detail::ffmpeg::codecs_supported(m);
  dict retval;
  for (auto k=m.begin(); k!=m.end(); ++k) {
    retval[k->first] = describe_codec(k->second);
  }
  return retval;
}

static object available_codec_dictionary() {
  std::map<std::string, const AVCodec*> m;
  bob::io::detail::ffmpeg::codecs_installed(m);
  dict retval;
  for (auto k=m.begin(); k!=m.end(); ++k) {
    retval[k->first] = describe_codec(k->second);
  }
  return retval;
}

void bind_io_video() {
  iterator_wrapper().wrap(); //wraps bob::io::VideoReader::const_iterator

  class_<bob::io::VideoReader, boost::shared_ptr<bob::io::VideoReader> >("VideoReader",
      "VideoReader objects can read data from video files. The current implementation uses `FFmpeg <http://ffmpeg.org>`_ (or `libav <http://libav.org>`_ if FFmpeg is not available) which is a stable freely available video encoding and decoding library, designed specifically for these tasks. You can read an entire video in memory by using the 'load()' method or use video iterators to read it frame by frame and avoid overloading your machine's memory. The maximum precision data `FFmpeg` will yield is a 24-bit (8-bit per band) representation of each pixel (32-bit depths are also supported by `FFmpeg`, but not by Bob presently). So, the input of data using this class uses ``uint8`` as base element type. Output will be colored using the RGB standard, with each band varying between 0 and 255, with zero meaning pure black and 255, pure white (color).", init<const std::string&, optional<bool> >((arg("self"), arg("filename"), arg("check")=true), "Initializes a new VideoReader object by giving the input file path to read. Format and codec will be extracted from the video metadata, automatically, by ``FFmpeg``. By default, if the format and/or the codec are not supported by this version of Bob, an exception will be raised. You can (at your own risk) set the ``check`` to ``False`` to avoid this check."))
    .add_property("filename", make_function(&bob::io::VideoReader::filename, return_value_policy<copy_const_reference>()), "The full path to the file that will be decoded by this object")
    .add_property("height", &bob::io::VideoReader::height, "The height of each frame in the video (a multiple of 2)")
    .add_property("width", &bob::io::VideoReader::width, "The width of each frame in the video (a multiple of 2)")
    .add_property("number_of_frames", &bob::io::VideoReader::numberOfFrames, "The number of frames in this video file")
    .def("__len__", &bob::io::VideoReader::numberOfFrames)
    .add_property("duration", &bob::io::VideoReader::duration, "Total duration of this video file in microseconds (long)")
    .add_property("format_name", make_function(&bob::io::VideoReader::formatName, return_value_policy<copy_const_reference>()), "Short name of the format in which this video file was recorded in")
    .add_property("format_long_name", make_function(&bob::io::VideoReader::formatLongName, return_value_policy<copy_const_reference>()), "Verbose name of the format in which this video file was recorded in")
    .add_property("codec_name", make_function(&bob::io::VideoReader::codecName, return_value_policy<copy_const_reference>()), "Short name of the codec that will be used to decode this video file")
    .add_property("codec_long_name", make_function(&bob::io::VideoReader::codecLongName, return_value_policy<copy_const_reference>()), "Verbose name of the codec that will be used to decode this video file")
    .add_property("frame_rate", &bob::io::VideoReader::frameRate, "Video's announced frame rate (note there are video formats with variable frame rates)")
    .add_property("info", make_function(&bob::io::VideoReader::info, return_value_policy<copy_const_reference>()), "Informative string containing many details of this video and available ffmpeg bindings that will read it")
    .add_property("video_type", make_function(&bob::io::VideoReader::video_type, return_value_policy<copy_const_reference>()), "Typing information to load all of the file at once")
    .add_property("frame_type", make_function(&bob::io::VideoReader::frame_type, return_value_policy<copy_const_reference>()), "Typing information to load the file frame by frame.")
    .def("__load__", &videoreader_load, videoreader_load_overloads((arg("self"), arg("raise_on_error")=false), "Loads all of the video stream in a numpy ndarray organized in this way: (frames, color-bands, height, width). I'll dynamically allocate the output array and return it to you. The flag ``raise_on_error``, which is set to ``False`` by default influences the error reporting in case problems are found with the video file. If you set it to ``True``, we will report problems raising exceptions. If you either don't set it or set it to ``False``, we will truncate the file at the frame with problems and will not report anything. It is your task to verify if the number of frames returned matches the expected number of frames as reported by the property ``number_of_frames`` in this object."))
    .def("__iter__", &bob::io::VideoReader::begin, with_custodian_and_ward_postcall<0,1>())
    .def("__getitem__", &videoreader_getitem)
    .def("__getitem__", &videoreader_getslice)
    ;

  class_<bob::io::VideoWriter, boost::shared_ptr<bob::io::VideoWriter>, boost::noncopyable>("VideoWriter",
     "Use objects of this class to create and write video files using `FFmpeg <http://ffmpeg.org>`_ (or `libav <http://libav.org>`_ if FFmpeg is not available).",
     init<const std::string&, size_t, size_t, optional<float, float, size_t, const std::string&, const std::string&, bool> >((arg("self"), arg("filename"), arg("height"), arg("width"), arg("framerate")=25., arg("bitrate")=1500000., arg("gop")=12, arg("codec")="", arg("format")="", arg("check")=true), "Creates a new output file given the input parameters. The format and codec to be used will be derived from the filename extension unless you define them explicetly (you can set both or just one of these two optional parameters)")
     )
    .add_property("filename", make_function(&bob::io::VideoReader::filename, return_value_policy<copy_const_reference>()), "The full path to the file that will be encoded by this object")
    .add_property("height", &bob::io::VideoWriter::height, "The height of the output video file (must be a multiple of 2)")
    .add_property("width", &bob::io::VideoWriter::width, "The width of the output video file (must be a multiple of 2)")
    .add_property("number_of_frames", &bob::io::VideoWriter::numberOfFrames, "The current number of frames pushed into this video file by the user")
    .def("__len__", &bob::io::VideoWriter::numberOfFrames)
    .add_property("duration", &bob::io::VideoWriter::duration, "The duration of this video file in microseconds (long)")
    .add_property("format_name", &bob::io::VideoWriter::formatName, "Short name of the format in which this video file will be recorded in")
    .add_property("format_long_name", &bob::io::VideoWriter::formatLongName, "Verbose name of the format in which this video file will be recorded in")
    .add_property("codec_name", &bob::io::VideoWriter::codecName, "Short name of the `FFmpeg` that will be used to encode the video stream in this file")
    .add_property("codec_long_name", &bob::io::VideoWriter::codecLongName, "Verbose name of the `FFmpeg` that will be used to encode the video stream in this file")
    .add_property("frame_rate", &bob::io::VideoWriter::frameRate, "The indicative frame rate of this video file")
    .add_property("bit_rate", &bob::io::VideoWriter::bitRate, "The indicative bit rate for this video file, given as a hint to `FFmpeg` (compression levels are subject to the picture textures)")
    .add_property("gop", &bob::io::VideoWriter::gop, "Group of pictures setting (see the `Wikipedia entry <http://en.wikipedia.org/wiki/Group_of_pictures>`_ for details on this setting)")
    .add_property("info", &bob::io::VideoWriter::info, "Informative string containing many details of this video and available ffmpeg bindings that will read it")
    .add_property("is_opened", &bob::io::VideoWriter::is_opened, "A boolean flag, indicating if the video is still opened for writing (or has already been closed by the user using ``close()``)")
    .def("close", &bob::io::VideoWriter::close, (arg("self")), "Closes the current video stream and forces writing the trailer. After this point the video is finalized and cannot be written to anymore.")
    .add_property("video_type", make_function(&bob::io::VideoWriter::video_type, return_value_policy<copy_const_reference>()), "Typing information to load all of the file at once")
    .add_property("frame_type", make_function(&bob::io::VideoWriter::frame_type, return_value_policy<copy_const_reference>()), "Typing information to load the file frame by frame.")
    .def("append", &videowriter_append, (arg("self"), arg("frame")), "Writes a new frame or set of frames to the file. The frame should be setup as a array with 3 dimensions organized in this way (RGB color-bands, height, width). Sets of frames should be setup as a 4D array in this way: (frame-number, RGB color-bands, height, width).\n\n.. note::\n\n  At present time we only support arrays that have C-style storages (if you pass reversed arrays or arrays with Fortran-style storage, the result is undefined).")
    ;

  def("available_video_codecs", &available_codec_dictionary, "Returns a dictionary containing a detailed description of the built-in codecs for videos that are available but **not necessarily supported**");
  def("supported_video_codecs", &supported_codec_dictionary, "Returns a dictionary containing a detailed description of the built-in codecs for videos that are fully supported");
  def("describe_video_encoder", &describe_encoder_by_name, (arg("name")), "Describes a given video encoder (codec) starting with a name");
  def("describe_video_encoder", &describe_encoder_by_id, (arg("id")), "Describes a given video encoder (codec) starting with an integer identifier");
  def("describe_video_decoder", &describe_decoder_by_name, (arg("name")), "Describes a given video decoder (codec) starting with a name");
  def("describe_video_decoder", &describe_decoder_by_id, (arg("id")), "Describes a given video decoder (codec) starting with an integer identifier");
  def("available_videoreader_formats", &available_iformat_dictionary, "Returns a dictionary containing a detailed description of the built-in input formats that are available, but **not necessarily supported**");
  def("supported_videoreader_formats", &supported_iformat_dictionary, "Returns a dictionary containing a detailed description of the built-in input formats that are fully supported");
  def("available_videowriter_formats", &available_oformat_dictionary, "Returns a dictionary containing a detailed description of the built-in output formats and default encoders for videos that are available, but **not necessarily supported**");
  def("supported_videowriter_formats", &supported_oformat_dictionary, "Returns a dictionary containing a detailed description of the built-in output formats and default encoders for videos that are fully supported");
}
