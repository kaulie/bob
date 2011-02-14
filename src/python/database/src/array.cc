/**
 * @author <a href="mailto:andre.anjos@idiap.ch">Andre Anjos</a> 
 *
 * @brief Python bindings for torch::database::Array 
 */

#include <boost/python.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/preprocessor/stringize.hpp>

#include "core/Dataset2.h"

using namespace boost::python;
namespace db = Torch::core;

static const char* ARRAY_COPY_DOC = "Adapts the size of each dimension of the passed blitz array to the ones of the underlying array and copies the data in it.";
#define ARRAY_COPY_DEF(T,D) .def("bzcopy", &db::Array::copy<T,D>, (arg("self"), arg("array")), ARRAY_COPY_DOC)

static const char* ARRAY_REFER_DOC = "Adapts the size of each dimension of the passed blitz array to the ones of the underlying array and refers to the data in it. WARNING: Updating the content of the blitz array will update the content of the corresponding array in the dataset. Use this method with care!";
#define ARRAY_REFER_DEF(T,N,D) .def(BOOST_PP_STRINGIZE(refer_ ## N ## _ ## D), (blitz::Array<T,D> (db::Array::*)(void))&db::Array::refer<T,D>, (arg("self")), ARRAY_REFER_DOC)

static const char* get_filename(db::Array& a) {
  return a.getFilename().c_str();
}

static void set_filename(db::Array& a, const char* filename) {
  std::string f(filename);
  a.setFilename(f);
}

void bind_database_array() {
  class_<db::Array, boost::shared_ptr<db::Array> >("Array", "Dataset Arrays represent pointers to concrete data serialized on a database. You can load or refer to real blitz::Arrays using this type.", init<const db::Arrayset&>((arg("parent")), "Initializes a new array given its parent"))
    .add_property("id", &db::Array::getId, &db::Array::setId)
    .add_property("loaded", &db::Array::getIsLoaded, &db::Array::setIsLoaded)
    .add_property("filename", &get_filename, &set_filename, "Accesses the filename containing the data for this array, if it was stored on a separate file. This string is empty otherwise.")
    .def("getParentArrayset", &db::Array::getParentArrayset, "Access the parent Arrayset for this array. The Arrayset contains global properties you may be interested at such as array types and dimensions.", return_internal_reference<>())
    ARRAY_COPY_DEF(bool, 1)
    ARRAY_COPY_DEF(int8_t, 1)
    ARRAY_COPY_DEF(int16_t, 1)
    ARRAY_COPY_DEF(int32_t, 1)
    ARRAY_COPY_DEF(int64_t, 1)
    ARRAY_COPY_DEF(uint8_t, 1)
    ARRAY_COPY_DEF(uint16_t, 1)
    ARRAY_COPY_DEF(uint32_t, 1)
    ARRAY_COPY_DEF(uint64_t, 1)
    ARRAY_COPY_DEF(float, 1)
    ARRAY_COPY_DEF(double, 1)
    ARRAY_COPY_DEF(std::complex<float>, 1)
    ARRAY_COPY_DEF(std::complex<double>, 1)
    ARRAY_COPY_DEF(bool, 2)
    ARRAY_COPY_DEF(int8_t, 2)
    ARRAY_COPY_DEF(int16_t, 2)
    ARRAY_COPY_DEF(int32_t, 2)
    ARRAY_COPY_DEF(int64_t, 2)
    ARRAY_COPY_DEF(uint8_t, 2)
    ARRAY_COPY_DEF(uint16_t, 2)
    ARRAY_COPY_DEF(uint32_t, 2)
    ARRAY_COPY_DEF(uint64_t, 2)
    ARRAY_COPY_DEF(float, 2)
    ARRAY_COPY_DEF(double, 2)
    ARRAY_COPY_DEF(std::complex<float>, 2)
    ARRAY_COPY_DEF(std::complex<double>, 2)
    ARRAY_COPY_DEF(bool, 3)
    ARRAY_COPY_DEF(int8_t, 3)
    ARRAY_COPY_DEF(int16_t, 3)
    ARRAY_COPY_DEF(int32_t, 3)
    ARRAY_COPY_DEF(int64_t, 3)
    ARRAY_COPY_DEF(uint8_t, 3)
    ARRAY_COPY_DEF(uint16_t, 3)
    ARRAY_COPY_DEF(uint32_t, 3)
    ARRAY_COPY_DEF(uint64_t, 3)
    ARRAY_COPY_DEF(float, 3)
    ARRAY_COPY_DEF(double, 3)
    ARRAY_COPY_DEF(std::complex<float>, 3)
    ARRAY_COPY_DEF(std::complex<double>, 3)
    ARRAY_COPY_DEF(bool, 4)
    ARRAY_COPY_DEF(int8_t, 4)
    ARRAY_COPY_DEF(int16_t, 4)
    ARRAY_COPY_DEF(int32_t, 4)
    ARRAY_COPY_DEF(int64_t, 4)
    ARRAY_COPY_DEF(uint8_t, 4)
    ARRAY_COPY_DEF(uint16_t, 4)
    ARRAY_COPY_DEF(uint32_t, 4)
    ARRAY_COPY_DEF(uint64_t, 4)
    ARRAY_COPY_DEF(float, 4)
    ARRAY_COPY_DEF(double, 4)
    ARRAY_COPY_DEF(std::complex<float>, 4)
    ARRAY_COPY_DEF(std::complex<double>, 4)
    ARRAY_REFER_DEF(bool, bool, 1)
    ARRAY_REFER_DEF(int8_t, int8, 1)
    ARRAY_REFER_DEF(int16_t, int16, 1)
    ARRAY_REFER_DEF(int32_t, int32, 1)
    ARRAY_REFER_DEF(int64_t, int64, 1)
    ARRAY_REFER_DEF(uint8_t, uint8, 1)
    ARRAY_REFER_DEF(uint16_t, uint16, 1)
    ARRAY_REFER_DEF(uint32_t, uint32, 1)
    ARRAY_REFER_DEF(uint64_t, uint64, 1)
    ARRAY_REFER_DEF(float, float32, 1)
    ARRAY_REFER_DEF(double, float64, 1)
    ARRAY_REFER_DEF(std::complex<float>, complex64, 1)
    ARRAY_REFER_DEF(std::complex<double>, complex128, 1)
    ARRAY_REFER_DEF(bool, bool, 2)
    ARRAY_REFER_DEF(int8_t, int8, 2)
    ARRAY_REFER_DEF(int16_t, int16, 2)
    ARRAY_REFER_DEF(int32_t, int32, 2)
    ARRAY_REFER_DEF(int64_t, int64, 2)
    ARRAY_REFER_DEF(uint8_t, uint8, 2)
    ARRAY_REFER_DEF(uint16_t, uint16, 2)
    ARRAY_REFER_DEF(uint32_t, uint32, 2)
    ARRAY_REFER_DEF(uint64_t, uint64, 2)
    ARRAY_REFER_DEF(float, float32, 2)
    ARRAY_REFER_DEF(double, float64, 2)
    ARRAY_REFER_DEF(std::complex<float>, complex64, 2)
    ARRAY_REFER_DEF(std::complex<double>, complex128, 2)
    ARRAY_REFER_DEF(bool, bool, 3)
    ARRAY_REFER_DEF(int8_t, int8, 3)
    ARRAY_REFER_DEF(int16_t, int16, 3)
    ARRAY_REFER_DEF(int32_t, int32, 3)
    ARRAY_REFER_DEF(int64_t, int64, 3)
    ARRAY_REFER_DEF(uint8_t, uint8, 3)
    ARRAY_REFER_DEF(uint16_t, uint16, 3)
    ARRAY_REFER_DEF(uint32_t, uint32, 3)
    ARRAY_REFER_DEF(uint64_t, uint64, 3)
    ARRAY_REFER_DEF(float, float32, 3)
    ARRAY_REFER_DEF(double, float64, 3)
    ARRAY_REFER_DEF(std::complex<float>, complex64, 3)
    ARRAY_REFER_DEF(std::complex<double>, complex128, 3)
    ARRAY_REFER_DEF(bool, bool, 4)
    ARRAY_REFER_DEF(int8_t, int8, 4)
    ARRAY_REFER_DEF(int16_t, int16, 4)
    ARRAY_REFER_DEF(int32_t, int32, 4)
    ARRAY_REFER_DEF(int64_t, int64, 4)
    ARRAY_REFER_DEF(uint8_t, uint8, 4)
    ARRAY_REFER_DEF(uint16_t, uint16, 4)
    ARRAY_REFER_DEF(uint32_t, uint32, 4)
    ARRAY_REFER_DEF(uint64_t, uint64, 4)
    ARRAY_REFER_DEF(float, float32, 4)
    ARRAY_REFER_DEF(double, float64, 4)
    ARRAY_REFER_DEF(std::complex<float>, complex64, 4)
    ARRAY_REFER_DEF(std::complex<double>, complex128, 4)
    ;
}