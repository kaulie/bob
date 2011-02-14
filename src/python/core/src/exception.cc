/**
 * @file src/python/core/src/exception.cc
 * @author <a href="mailto:andre.anjos@idiap.ch">Andre Anjos</a> 
 *
 * @brief Binds the core extension into Python. Please note that, for each
 * exception type you only need to declare once the translator. All other
 * modules will benefit from it automatically.
 */

#include <boost/python.hpp>

#include "core/Exception.h"

using namespace boost::python;

/**
 * The following lines of code implement exception translation from C++ into
 * python using Boost.Python and the instructions found on this webpage:
 *
 * http://stackoverflow.com/questions/2261858/boostpython-export-custom-exception
 * They were just slightly modified to make it easier to apply the code for
 * different situations.
 */

template <typename T> struct CxxToPythonTranslator {
  /**
   * This static class variable will hold a pointer to the exception type as
   * defined by the boost::python
   */
  static PyObject* pyExceptionType;

  /**
   * Do the exception translation for the specific exception we are trying to
   * tackle.
   */
  static void translateException(const T& ex) {
    assert(pyExceptionType != NULL);
    boost::python::object pythonExceptionInstance(ex);
    PyErr_SetObject(pyExceptionType, pythonExceptionInstance.ptr());
  }

  /**
   * Constructor will instantiate all required parameters for this standard
   * exception handler and create the pythonic bindings in one method call
   */
  CxxToPythonTranslator(const char* python_name, const char* python_doc) {
    class_<T> pythonEquivalentException(python_name, python_doc, init<>("Creates a new exception of this type"));
    pythonEquivalentException.def("__str__", &T::what);
    pyExceptionType = pythonEquivalentException.ptr();
    register_exception_translator<T>(&translateException);
  }

};

template <typename T> PyObject* CxxToPythonTranslator<T>::pyExceptionType = 0;

#define BIND_EXCEPTION(TYPE,NAME,DOC) CxxToPythonTranslator<TYPE>(NAME, DOC)

/**
 * This method is only useful to test exception throwing in Python code.
 */
static void throw_exception(void) {
  throw Torch::core::Exception();
}

void bind_core_exception() {
  BIND_EXCEPTION(Torch::core::Exception, "Exception", "The core Exception class should be used as a basis for all Torch-Python exceptions.");
  def("throw_exception", &throw_exception);
}