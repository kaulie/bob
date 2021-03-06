/**
 * @file core/python/version.cc
 * @date Wed Apr 27 23:16:03 2011 +0200
 * @author Andre Anjos <andre.anjos@idiap.ch>
 *
 * @brief Describes ways to retrieve version information about all dependent
 * packages.
 *
 * Copyright (C) 2011-2013 Idiap Research Institute, Martigny, Switzerland
 */

#include <bob/config.h>

#include <boost/python.hpp>
#include <string>
#include <blitz/blitz.h>
#include <boost/version.hpp>
#include <boost/format.hpp>
#include <cstring>
#if WITH_PERFTOOLS
#include <google/tcmalloc.h>
#endif

extern "C" {
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
}

using namespace boost::python;

/**
 * Describes the blitz version and information
 */
static str blitz_version() {
  std::string retval(BZ_VERSION);
  return str(retval);
}

/**
 * Describes the version of Boost libraries installed
 */
static str boost_version() {
  boost::format f("%d.%d.%d");
  f % (BOOST_VERSION / 100000);
  f % (BOOST_VERSION / 100 % 1000);
  f % (BOOST_VERSION % 100);
  return str(f.str());
}

/**
 * Describes the compiler version
 */
static tuple compiler_version() {
# if defined(__GNUC__) && !defined(__llvm__)
  boost::format f("%s.%s.%s");
  f % BOOST_PP_STRINGIZE(__GNUC__);
  f % BOOST_PP_STRINGIZE(__GNUC_MINOR__);
  f % BOOST_PP_STRINGIZE(__GNUC_PATCHLEVEL__);
  return make_tuple(str("gcc"), str(f.str()));
# elif defined(__llvm__) && !defined(__clang__)
  return make_tuple(str("llvm-gcc"), str(__VERSION__));
# elif defined(__clang__)
  return make_tuple(str("clang"), str(__clang_version__));
# else
  return str("unsupported");
# endif
}

/**
 * Python version with which we compiled the extensions
 */
static str python_version() {
  boost::format f("%s.%s.%s");
  f % BOOST_PP_STRINGIZE(PY_MAJOR_VERSION);
  f % BOOST_PP_STRINGIZE(PY_MINOR_VERSION);
  f % BOOST_PP_STRINGIZE(PY_MICRO_VERSION);
  return str(f.str());
}

/**
 * Numpy version
 */
static str numpy_version() {
  return str(BOOST_PP_STRINGIZE(NPY_VERSION));
}

/**
 * Google profiler version, if available
 */
static str perftools_version() {
#if WITH_PERFTOOLS
  boost::format f("%s.%s.%s");
  f % BOOST_PP_STRINGIZE(TC_VERSION_MAJOR);
  f % BOOST_PP_STRINGIZE(TC_VERSION_MINOR);
  if (std::strlen(TC_VERSION_PATCH) == 0) f % "0";
  else f % BOOST_PP_STRINGIZE(TC_VERSION_PATCH);
  return str(f.str());
#else
  return str("unavailable");
#endif
}

/**
 * Bind, whether we have compiled Bob in DEBUG or in RELEASE mode
 */

static bool is_debug(){
#ifndef NDEBUG
  return true;
#else
  return false;
#endif
}

void bind_core_version() {
  dict vdict;
  vdict["Blitz++"] = blitz_version();
  vdict["Boost"] = boost_version();
  vdict["Compiler"] = compiler_version();
  vdict["Python"] = python_version();
  vdict["NumPy"] = numpy_version();
  vdict["Google Perftools"] = perftools_version();
  scope().attr("version") = vdict;

  def("is_debug", &is_debug, "Returns True if Bob was compiled in DEBUG mode, or False otherwise");
}
