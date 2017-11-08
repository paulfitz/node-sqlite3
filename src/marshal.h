#include <stdlib.h>
#include <vector>
#include <string>
#include <nan.h>

enum MarshalCode {
  MARSHAL_NULL     = '0',
  MARSHAL_NONE      = 'N',
  MARSHAL_FALSE     = 'F',
  MARSHAL_TRUE      = 'T',
  MARSHAL_STOPITER  = 'S',
  MARSHAL_ELLIPSIS  = '.',
  MARSHAL_INT       = 'i',
  MARSHAL_INT64     = 'I',
  MARSHAL_FLOAT     = 'f',
  MARSHAL_BFLOAT    = 'g',
  MARSHAL_COMPLEX   = 'x',
  MARSHAL_LONG      = 'l',
  MARSHAL_STRING    = 's',
  MARSHAL_INTERNED  = 't',
  MARSHAL_STRINGREF = 'R',
  MARSHAL_TUPLE     = '(',
  MARSHAL_LIST      = '[',
  MARSHAL_DICT      = '{',
  MARSHAL_CODE      = 'c',
  MARSHAL_UNICODE   = 'u',
  MARSHAL_UNKNOWN   = '?',
  MARSHAL_SET       = '<',
  MARSHAL_FROZENSET = '>',
};

class Marshal {
  private:
    std::vector<char> buffer;

    void _writeCode(MarshalCode code) {
      buffer.push_back(static_cast<char>(code));
    }

    void _writeBytes(const void *bytes, size_t nbytes) {
      size_t offset = buffer.size();
      buffer.resize(buffer.size() + nbytes);
      memcpy(&buffer[offset], bytes, nbytes);
    }

  public:
    Marshal() {
      buffer.reserve(64);
    }

    const std::vector<char> &getBuffer() const {
      return buffer;
    }

    void append(const Marshal &marshal) {
      const std::vector<char> &buf = marshal.getBuffer();
      _writeBytes(&buf[0], buf.size());
    }

    // Marshal the given value depending on its type.
    void marshalValue(v8::Local<v8::Value> val);

    void marshalNone() {
      _writeCode(MARSHAL_NONE);
    }

    void marshalString(const std::string &value) {
      marshalString(&value[0], value.size());
    }

    void marshalString(const char *value, int32_t size) {
      _writeCode(MARSHAL_STRING);
      _writeBytes(&size, sizeof(int32_t));
      _writeBytes(value, size);
    }

    void marshalUnicode(const char *value, int32_t size) {
      _writeCode(MARSHAL_UNICODE);
      _writeBytes(&size, sizeof(int32_t));
      _writeBytes(value, size);
    }

    void marshalInt(int32_t value) {
      _writeCode(MARSHAL_INT);
      _writeBytes(&value, sizeof(int32_t));
    }

    void marshalDouble(double value) {
      _writeCode(MARSHAL_BFLOAT);
      _writeBytes(&value, sizeof(double));
    }

    void marshalBool(bool value) {
      _writeCode(value ? MARSHAL_TRUE : MARSHAL_FALSE);
    }

    // To marshal a list, call marshalList with a size, followed by size more calls to marshal*.
    void marshalList(int32_t size) {
      _writeCode(MARSHAL_LIST);
      _writeBytes(&size, sizeof(int32_t));
    }

    // To marshal a tuple, call marshalTuple with a size, followed by size more calls to marshal*.
    void marshalTuple(int32_t size) {
      _writeCode(MARSHAL_TUPLE);
      _writeBytes(&size, sizeof(int32_t));
    }

    // To marshal a dictionary, call marshalDictBegin(), followed by an even number of calls to
    // marshal* (for alternating keys and values), followed by marshalDictEnd().
    void marshalDictBegin() {
      _writeCode(MARSHAL_DICT);
    }

    void marshalDictEnd() {
      _writeCode(MARSHAL_NULL);
    }
};
