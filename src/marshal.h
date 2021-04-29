#include <stdlib.h>
#include <vector>
#include <string>
#include <napi.h>

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

class Marshaller {
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

    template<typename T>
    void _writeEndian(T value, bool wantLittleEndian = true);

  public:
    Marshaller() {
      buffer.reserve(64);
    }

    const std::vector<char> &getBuffer() const {
      return buffer;
    }

    void append(const Marshaller &marshaller) {
      const std::vector<char> &buf = marshaller.getBuffer();
      _writeBytes(&buf[0], buf.size());
    }

    // Marshal the given value depending on its type.
    void marshalValue(Napi::Value val);

    void marshalNone() {
      _writeCode(MARSHAL_NONE);
    }

    void marshalString(const std::string &value) {
      marshalString(&value[0], value.size());
    }

    void marshalString(const char *value, int32_t size) {
      _writeCode(MARSHAL_STRING);
      _writeEndian<int32_t>(size);
      _writeBytes(value, size);
    }

    void marshalUnicode(const char *value, int32_t size) {
      _writeCode(MARSHAL_UNICODE);
      _writeEndian<int32_t>(size);
      _writeBytes(value, size);
    }

    void marshalInt(int32_t value) {
      _writeCode(MARSHAL_INT);
      _writeEndian<int32_t>(value);
    }

    void marshalDouble(double value) {
      _writeCode(MARSHAL_BFLOAT);
      _writeEndian<double>(value);
    }

    void marshalBool(bool value) {
      _writeCode(value ? MARSHAL_TRUE : MARSHAL_FALSE);
    }

    // To marshal a list, call marshalList with a size, followed by size more calls to marshal*.
    void marshalList(int32_t size) {
      _writeCode(MARSHAL_LIST);
      _writeEndian<int32_t>(size);
    }

    // To marshal a tuple, call marshalTuple with a size, followed by size more calls to marshal*.
    void marshalTuple(int32_t size) {
      _writeCode(MARSHAL_TUPLE);
      _writeEndian<int32_t>(size);
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


class Unmarshaller {
  public:
    static Napi::Value parse(const Napi::CallbackInfo& info, const char *data, size_t len) {
      Unmarshaller u(info, data, len);
      return u._parse();
    }

  private:
    std::vector<std::string> stringTable;    // List of interned strings.

    // Data is a reference to the data passed to the constructor. The reason it's safe to avoid a
    // copy is because we'll only use this object from within parse().
    const char *data;
    size_t len;
    uint8_t _lastCode;
    const Napi::CallbackInfo& info;

    Unmarshaller(const Napi::CallbackInfo& _info, const char *_data, size_t _len) : data(_data), len(_len), _lastCode(0), info(_info) {}
    const char *consumeBytes(size_t numBytes);
    bool readUint8(uint8_t *result);
    bool readInt32(int32_t *result);
    bool readFloat64(double *result);
    bool readBytes(size_t len, const char **result);

    Napi::Value fail(const char *msg = NULL) {
      Napi::Env env = info.Env();
      Napi::Error::New(env, msg ? msg : "invalid or truncated marshalled data").ThrowAsJavaScriptException();

      return Napi::Value();
    }


    Napi::Value _parse();
    Napi::Value _parseInt32();
    Napi::Value _parseInt64();
    Napi::Value _parseStringFloat();
    Napi::Value _parseBinaryFloat();
    Napi::Value _parseByteString();
    Napi::Value _parseInterned();
    Napi::Value _parseStringRef();
    Napi::Value _parseList();
    Napi::Value _parseDict();
    Napi::Value _parseUnicode();
};

// Since we have our own endianness code, it's nice to be able to test it. This
// call switches our notion of the host endianness resulting in all incorrect
// marshalling. Obviously, this is only for testing, and is not exposed to JS.
void marshalTestOppositeEndianness(bool useOpposite);
