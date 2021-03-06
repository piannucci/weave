/********************************************
  copyright 1999 McMillan Enterprises, Inc.
  www.mcmillan-inc.com

  modified for weave by eric jones.
*********************************************/

#if !defined(OBJECT_H_INCLUDED_)
#define OBJECT_H_INCLUDED_

#include <Python.h>
#include <limits.h>
#include <string>
#include <complex>

// for debugging
#include <iostream>

namespace py {

void fail(PyObject*, const char* msg);

//---------------------------------------------------------------------------
// py::object -- A simple C++ interface to Python objects.
//
// This is the basic type from which all others are derived from.  It is
// also quite useful on its own.  The class is very light weight as far as
// data contents, carrying around only two python pointers.
//---------------------------------------------------------------------------

class object
{
protected:

  //-------------------------------------------------------------------------
  // _obj is the underlying pointer to the real python object.
  //-------------------------------------------------------------------------
  PyObject* _obj;

  //-------------------------------------------------------------------------
  // grab_ref (rename to grab_ref)
  //
  // incref new owner, decref old owner, and adjust to new owner
  //-------------------------------------------------------------------------
  void grab_ref(PyObject* newObj) {
      // be careful to incref before decref if old is same as new
      Py_XINCREF(newObj);
      Py_XDECREF(_own);
      _own = _obj = newObj;
  };

  //-------------------------------------------------------------------------
  // lose_ref (rename to lose_ref)
  //
  // decrease reference count without destroying the object.
  //-------------------------------------------------------------------------
  static PyObject* lose_ref(PyObject* o)
    { if (o != 0) --(o->ob_refcnt); return o; }

private:
  //-------------------------------------------------------------------------
  // _own is set to _obj if we "own" a reference to _obj, else zero
  //-------------------------------------------------------------------------
  PyObject* _own;

public:
  //-------------------------------------------------------------------------
  // forward declaration of reference obj returned when [] used as an lvalue.
  //-------------------------------------------------------------------------
  class keyed_ref;

  object(const std::nullptr_t &) : _obj (0), _own (0) { };
  object(const object& other)
    : _obj (0), _own (0) { grab_ref(other); };
  object(PyObject* obj)
    : _obj (0), _own (0) { grab_ref(obj);   };

  //-------------------------------------------------------------------------
  //  Numeric constructors
  //-------------------------------------------------------------------------
  object(bool val) {
    _obj = _own = PyLong_FromLong((int)val);
  };
  object(int val) {
    _obj = _own = PyLong_FromLong((int)val);
  };
  object(unsigned int val) {
    _obj = _own = PyLong_FromUnsignedLong(val);
  };
  object(long val) {
    _obj = _own = PyLong_FromLong(val);
  };
  object(unsigned long val) {
    _obj = _own = PyLong_FromUnsignedLong(val);
  };
  object(double val) {
    _obj = _own = PyFloat_FromDouble(val);
  };
  object(const std::complex<double>& val) {
    _obj = _own = PyComplex_FromDoubles(val.real(),val.imag());
  };

  //-------------------------------------------------------------------------
  // string constructors
  //-------------------------------------------------------------------------
  object(const char* val) {
    _obj = _own = PyUnicode_FromString(val);
  };
  object(const std::string& val) : _obj (0), _own (0) {
    _obj = _own = PyUnicode_FromString(val.c_str());
  };

  //-------------------------------------------------------------------------
  // destructor
  //-------------------------------------------------------------------------
  virtual ~object() {
    Py_XDECREF(_own);
  };

  //-------------------------------------------------------------------------
  // casting operators
  //-------------------------------------------------------------------------
  operator PyObject* () const {
    return _obj;
  };

  operator int () const {
    int _val;
    _val = PyLong_AsLong(_obj);
    if (PyErr_Occurred()) {
        PyErr_Clear();
        fail(PyExc_TypeError, "cannot convert value to integer");
    }
    return _val;
  };
  operator float () const {
    float _val;
    _val = (float) PyFloat_AsDouble(_obj);
    if (PyErr_Occurred()) {
        PyErr_Clear();
        fail(PyExc_TypeError, "cannot convert value to float");
    }
    return _val;
  };
  operator double () const {
    double _val;
    _val = PyFloat_AsDouble(_obj);
    if (PyErr_Occurred()) {
        PyErr_Clear();
        fail(PyExc_TypeError, "cannot convert value to double");
    }
    return _val;
  };
  operator std::complex<double> () const {
    double real, imag;
    real = PyComplex_RealAsDouble(_obj);
    imag = PyComplex_ImagAsDouble(_obj);
    if (PyErr_Occurred()) {
        PyErr_Clear();
        fail(PyExc_TypeError, "cannot convert value to complex");
    }
    return std::complex<double>(real, imag);
  };
  operator std::string () const {
    if (!PyUnicode_Check(_obj))
        fail(PyExc_TypeError, "cannot convert value to std::string");
    return std::string(PyUnicode_AsUTF8(_obj));
  };
  operator const char* () const {
    if (!PyUnicode_Check(_obj))
        fail(PyExc_TypeError, "cannot convert value to const char*");
    return PyUnicode_AsUTF8(_obj);
  };

  //-------------------------------------------------------------------------
  // equal operator
  //-------------------------------------------------------------------------
  object& operator=(const object& other) {
    grab_ref(other);
    return *this;
  };

  //-------------------------------------------------------------------------
  // printing
  //
  // This needs to become more sophisticated and handle objects that
  // implement the file protocol.
  //-------------------------------------------------------------------------
  void print(FILE* f, int flags=0) const {
    int res = PyObject_Print(_obj, f, flags);
    if (res == -1)
        throw 1;
  };

  void print(object f, int flags=0) const {
    int res = PyFile_WriteObject(_obj, f, flags);
    if (res == -1)
        throw 1;
  };

  //-------------------------------------------------------------------------
  // hasattr -- test if object has specified attribute
  //-------------------------------------------------------------------------
  int hasattr(const char* nm) const {
    return PyObject_HasAttrString(_obj, nm) == 1;
  };
  int hasattr(const std::string& nm) const {
    return PyObject_HasAttrString(_obj, nm.c_str()) == 1;
  };
  int hasattr(object& nm) const {
    return PyObject_HasAttr(_obj, nm) == 1;
  };


  //-------------------------------------------------------------------------
  // attr -- retreive attribute/method from object
  //-------------------------------------------------------------------------
  object attr(const char* nm) const {
    PyObject* val = PyObject_GetAttrString(_obj, nm);
    if (!val)
        throw 1;
    return object(lose_ref(val));
  };

  object attr(const std::string& nm) const {
    return attr(nm.c_str());
  };

  object attr(const object& nm) const {
    PyObject* val = PyObject_GetAttr(_obj, nm);
    if (!val)
        throw 1;
    return object(lose_ref(val));
  };

  //-------------------------------------------------------------------------
  // setting attributes
  //
  // There is a combinatorial explosion here of function combinations.
  // perhaps there is a casting fix someone can suggest.
  //-------------------------------------------------------------------------
  template<class T>
  void set_attr(const char* nm, T val) {
    int res = PyObject_SetAttrString(_obj, nm, object(val));
    if (res == -1)
        throw 1;
  };

  template<class T>
  void set_attr(const std::string& nm, T val) {
    int res = PyObject_SetAttrString(_obj, nm.c_str(), object(val));
    if (res == -1)
        throw 1;
  };

  template<class T>
  void set_attr(const object& nm, T val) {
    int res = PyObject_SetAttr(_obj, nm, object(val));
    if (res == -1)
        throw 1;
  };

  //-------------------------------------------------------------------------
  // del attributes/methods from object
  //-------------------------------------------------------------------------
  void del(const char* nm) {
    int result = PyObject_DelAttrString(_obj, nm);
    if (result == -1)
        throw 1;
  };
  void del(const std::string& nm) {
    int result = PyObject_DelAttrString(_obj, nm.c_str());
    if (result == -1)
        throw 1;
  };
  void del(const object& nm) {
    int result = PyObject_DelAttr(_obj, nm);
    if (result ==-1)
        throw 1;
  };

  //-------------------------------------------------------------------------
  // comparison
  // !! NOT TESTED
  //-------------------------------------------------------------------------
  template<class T>
  int cmp(T other) const {
    object o(other);
    return PyObject_RichCompareBool(_obj, o, Py_GT) - PyObject_RichCompareBool(_obj, o, Py_LT);
  };

  template<class T> bool operator == (const T& other) const { return PyObject_RichCompareBool(_obj, other, Py_EQ); };
  template<class T> bool operator != (const T& other) const { return PyObject_RichCompareBool(_obj, other, Py_NE); };
  template<class T> bool operator < (const T& other) const { return PyObject_RichCompareBool(_obj, other, Py_LT); };
  template<class T> bool operator > (const T& other) const { return PyObject_RichCompareBool(_obj, other, Py_GT); };
  template<class T> bool operator <= (const T& other) const { return PyObject_RichCompareBool(_obj, other, Py_LE); };
  template<class T> bool operator >= (const T& other) const { return PyObject_RichCompareBool(_obj, other, Py_GE); };

  //-------------------------------------------------------------------------
  // string representations
  //
  //-------------------------------------------------------------------------
  std::string repr() const {
    object result = PyObject_Repr(_obj);
    if (!(PyObject*)result)
        throw 1;
    return std::string(PyUnicode_AsUTF8(result));
  };

  std::string str() const {
    object result = PyObject_Str(_obj);
    if (!(PyObject*)result)
        throw 1;
    return std::string(PyUnicode_AsUTF8(result));
  };

  //-------------------------------------------------------------------------
  // calling methods on object
  //
  // Note: I changed args_tup from a tuple& to a object& so that it could
  //       be inlined instead of implemented i weave_imp.cpp.  This
  //       provides less automatic type checking, but is potentially faster.
  //-------------------------------------------------------------------------
  object mcall(const char* nm) {
    object method = attr(nm);
    PyObject* result = PyEval_CallObjectWithKeywords(method,NULL,NULL);
    if (!result)
      throw 1; // signal exception has occurred.
    return object(lose_ref(result));
  }

  object mcall(const char* nm, object& args_tup) {
    object method = attr(nm);
    PyObject* result = PyEval_CallObjectWithKeywords(method,args_tup,NULL);
    if (!result)
      throw 1; // signal exception has occurred.
    return object(lose_ref(result));
  }

  object mcall(const char* nm, object& args_tup, object& kw_dict) {
    object method = attr(nm);
    PyObject* result = PyEval_CallObjectWithKeywords(method,args_tup,kw_dict);
    if (!result)
      throw 1; // signal exception has occurred.
    return object(lose_ref(result));
  }

  object mcall(const std::string& nm) {
    return mcall(nm.c_str());
  }
  object mcall(const std::string& nm, object& args_tup) {
    return mcall(nm.c_str(),args_tup);
  }
  object mcall(const std::string& nm, object& args_tup, object& kw_dict) {
    return mcall(nm.c_str(),args_tup,kw_dict);
  }

  //-------------------------------------------------------------------------
  // calling callable objects
  //
  // Note: see not on mcall()
  //-------------------------------------------------------------------------
  object call() const {
    PyObject *rslt = PyEval_CallObjectWithKeywords(*this, NULL, NULL);
    if (rslt == 0)
      throw 1;
    return object(lose_ref(rslt));
  }
  object call(object& args_tup) const {
    PyObject *rslt = PyEval_CallObjectWithKeywords(*this, args_tup, NULL);
    if (rslt == 0)
      throw 1;
    return object(lose_ref(rslt));
  }
  object call(object& args_tup, object& kw_dict) const {
    PyObject *rslt = PyEval_CallObjectWithKeywords(*this, args_tup, kw_dict);
    if (rslt == 0)
      throw 1;
    return object(lose_ref(rslt));
  }

  //-------------------------------------------------------------------------
  // check if object is callable
  //-------------------------------------------------------------------------
  bool is_callable() const {
    return PyCallable_Check(_obj) == 1;
  };

  //-------------------------------------------------------------------------
  // retreive the objects hash value
  //-------------------------------------------------------------------------
  int hash() const {
    int result = PyObject_Hash(_obj);
    if (result == -1 && PyErr_Occurred())
        throw 1;
    return result;
  };

  //-------------------------------------------------------------------------
  // test whether object is true
  //-------------------------------------------------------------------------
  bool is_true() const {
    return PyObject_IsTrue(_obj) == 1;
  };

  //-------------------------------------------------------------------------
  // test for null
  //-------------------------------------------------------------------------
  bool is_null() {
    return (_obj == NULL);
  }

  /*
   * //-------------------------------------------------------------------------
  // test whether object is not true
  //-------------------------------------------------------------------------
#if defined(__GNUC__) && __GNUC__ < 3
  bool not() const {
#else
  bool operator not() const {
#endif
    return PyObject_Not(_obj) == 1;
  };
  */

  //-------------------------------------------------------------------------
  // return the variable type for the object
  //-------------------------------------------------------------------------
  object type() const {
    PyObject* result = PyObject_Type(_obj);
    if (!result)
        throw 1;
    return lose_ref(result);
  };

  object is_int() const {
    return PyLong_Check(_obj) == 1;
  };

  object is_float() const {
    return PyFloat_Check(_obj) == 1;
  };

  object is_complex() const {
    return PyComplex_Check(_obj) == 1;
  };

  object is_list() const {
    return PyList_Check(_obj) == 1;
  };

  object is_tuple() const {
    return PyTuple_Check(_obj) == 1;
  };

  object is_dict() const {
    return PyDict_Check(_obj) == 1;
  };

  object is_string() const {
    return PyUnicode_Check(_obj) == 1;
  };

  //-------------------------------------------------------------------------
  // size, len, and length are all synonyms.
  //
  // length() is useful because it allows the same code to work with STL
  // strings as works with py::objects.
  //-------------------------------------------------------------------------
  int size() const {
    int result = PyObject_Size(_obj);
    if (result == -1)
        throw 1;
    return result;
  };
  int len() const {
    return size();
  };
  int length() const {
    return size();
  };

  //-------------------------------------------------------------------------
  // set_item
  //
  // To prevent a combonatorial explosion, only objects are allowed for keys.
  // Users are encouraged to use the [] interface for setting values.
  //-------------------------------------------------------------------------
  virtual void set_item(const object& key, const object& val) {
    int rslt = PyObject_SetItem(_obj, key, val);
    if (rslt==-1)
      throw 1;
  };

  //-------------------------------------------------------------------------
  // operator[]
  //-------------------------------------------------------------------------
  template<class T>
  keyed_ref operator [] (T key);

  //-------------------------------------------------------------------------
  // iter methods
  // !! NOT TESTED
  //-------------------------------------------------------------------------

  //-------------------------------------------------------------------------
  //  iostream operators
  //-------------------------------------------------------------------------
  friend std::ostream& operator <<(std::ostream& os, py::object& obj);
  //-------------------------------------------------------------------------
  //  refcount utilities
  //-------------------------------------------------------------------------

  PyObject* disown() {
    _own = 0;
    return _obj;
  };

  int refcount() {
    return _obj->ob_refcnt;
  }
};


//---------------------------------------------------------------------------
// keyed_ref
//
// Provides a reference value when operator[] returns an lvalue.  The
// reference has to keep track of its parent object and its key in the parent
// object so that it can insert a new value into the parent at the
// appropriate place when a new value is assigned to the keyed_ref object.
//
// The keyed_ref class is also used by the py::dict class derived from
// py::object
// !! Note: Need to check ref counting on key and parent here.
//---------------------------------------------------------------------------
class object::keyed_ref : public object
{
  object& _parent;
  object _key;
public:
  keyed_ref(object obj, object& parent, const object& key)
      : object(obj), _parent(parent), _key(key) {};
  virtual ~keyed_ref() {};

  keyed_ref& operator=(const object& other) {
    grab_ref(other);
    _parent.set_item(_key, other);
    return *this;
  }

  keyed_ref& operator=(const keyed_ref& other) {
    object oth = other;
    return operator=( oth );
  }
};

template<class T>
object::keyed_ref object::operator [] (T key) {
  object k(key);
  object rslt = PyObject_GetItem(_obj, k);
  lose_ref(rslt);
  if (!(PyObject*)rslt)
  {
    // don't throw error for when [] fails because it might be on left hand
    // side (a[0] = 1).  If the obj was just created, it will be filled
    // with NULL values, and setting the values should be ok.  However, we
    // do want to catch index errors that might occur on the right hand side
    // (obj = a[4] when a has len==3).
      if (PyErr_ExceptionMatches(PyExc_KeyError))
           PyErr_Clear(); // Ignore key errors
      else if (PyErr_ExceptionMatches(PyExc_IndexError))
        throw 1;
  }
  return object::keyed_ref(rslt, *this, k);
}

} // namespace


#endif // !defined(OBJECT_H_INCLUDED_)
