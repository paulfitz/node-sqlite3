#include "marshal.h"

// ======================================================================
// Endianness
// ======================================================================
//
// Node provides similar functions for Buffer and for DataView, but actually
// using those is hard and annoying. Reimplementing turns out to be easier.

static bool _isHostLittleEndian() {
  union { uint8_t u8[2]; uint16_t u16; } endianness = { { 1, 0 } };
  return endianness.u16 == 1;
}
static bool isHostLittleEndian = _isHostLittleEndian();

// For testing only; when called with true, (un)marshalling will be all wrong.
void marshalTestOppositeEndianness(bool useOpposite) {
  bool real = _isHostLittleEndian();
  isHostLittleEndian = useOpposite ? !real : real;
}

// Copy sizeof(T) bytes of value to dest, with requested endianness.
template<typename T>
void writeEndian(char *dest, T value, bool wantLittleEndian = true) {
  union Convert {
    T val;
    char bytes[sizeof(T)];
  };
  union Convert convert = { value };
  if (wantLittleEndian == isHostLittleEndian) {
    std::copy(&convert.bytes[0], &convert.bytes[0] + sizeof(T), dest);
  } else {
    std::reverse_copy(&convert.bytes[0], &convert.bytes[0] + sizeof(T), dest);
  }
}

// Return value constructed with the first sizeof(T) bytes of data, with requested endianness.
template<typename T>
T readEndian(const char *data, bool wantLittleEndian = true) {
  union Convert {
    T val;
    char bytes[sizeof(T)];
  };
  union Convert convert = { 0 };

  if (wantLittleEndian == isHostLittleEndian) {
    std::copy(data, data + sizeof(T), convert.bytes);
  } else {
    std::reverse_copy(data, data + sizeof(T), convert.bytes);
  }
  return convert.val;
}


// ======================================================================
// Marshaller
// ======================================================================

template<typename T>
void Marshaller::_writeEndian(T value, bool wantLittleEndian) {
  size_t offset = buffer.size();
  buffer.resize(buffer.size() + sizeof(T));
  writeEndian<T>(&buffer[offset], value, wantLittleEndian);
}

// Instantiate the types we use explicitly.
template void Marshaller::_writeEndian<int32_t>(int32_t value, bool wantLittleEndian);
template void Marshaller::_writeEndian<double>(double value, bool wantLittleEndian);

typedef std::pair<std::string, Napi::Value > StringPair;
static bool sortByFirst(const StringPair &a, const StringPair &b) {
  return a.first < b.first;
}

// A Napi substitute IsInt32()
inline bool OtherIsInt(Napi::Number source) {
    double orig_val = source.DoubleValue();
    double int_val = (double)source.Int32Value();
    if (orig_val == int_val) {
        return true;
    } else {
        return false;
    }
}

void Marshaller::marshalValue(Napi::Value val) {
  if (val.IsBoolean()) {
    marshalBool(val.As<Napi::Boolean>().Value());
  } else if (val.IsNumber()) {
    Napi::Number num = val.As<Napi::Number>();
    if (OtherIsInt(num)) {
      marshalInt(num.Int32Value());
    } else {
      marshalDouble(num.DoubleValue());
    }
  } else if (val.IsString()) {
    std::string strVal = val.As<Napi::String>();
    marshalUnicode(strVal.c_str(), strVal.length());
  } else if (val.IsArray()) {
    Napi::Array array = val.As<Napi::Array>();
    int length = array.Length();
    marshalList(length);
    for (int i = 0; i < length; i++) {
      marshalValue((array).Get(i));
    }
  } else if (val.IsBuffer()) {
    Napi::Object buffer = val.As<Napi::Object>();
    marshalString(buffer.As<Napi::Buffer<char>>().Data(), buffer.As<Napi::Buffer<char>>().Length());
  } else if (val.IsObject()) {
    Napi::Object object = val.As<Napi::Object>();
    Napi::Array array = object.GetPropertyNames();
    // Keys need to be serialized in sorted order.
    int length = array.Length();
    std::vector<StringPair> keys;
    keys.reserve(length);
    for (int i = 0; i < length; i++) {
      Napi::Value name = (array).Get(i);
      std::string strVal = name.As<Napi::String>();
      keys.push_back(std::make_pair(strVal, name));
    }
    std::sort(keys.begin(), keys.end(), sortByFirst);
    marshalDictBegin();
    for (size_t i = 0; i < keys.size(); i++) {
      Napi::Value name = keys[i].second;
      marshalValue(name);
      marshalValue((object).Get(name));
    }
    marshalDictEnd();
  } else {
    marshalNone();
  }
}

// ======================================================================
// Unmarshaller
// ======================================================================

const char *Unmarshaller::consumeBytes(size_t numBytes) {
  if (len < numBytes) { return NULL; }
  const char *ret = data;
  data += numBytes;
  len -= numBytes;
  return ret;
}

bool Unmarshaller::readUint8(uint8_t *result) {
  const char *bytes = consumeBytes(1);
  if (!bytes) { return false; }
  *result = bytes[0];
  return true;
}


bool Unmarshaller::readInt32(int32_t *result) {
  const char *bytes = consumeBytes(4);
  if (!bytes) { return false; }
  *result = readEndian<int32_t>(bytes);
  return true;
}

bool Unmarshaller::readFloat64(double *result) {
  const char *bytes = consumeBytes(8);
  if (!bytes) { return false; }
  *result = readEndian<double>(bytes);
  return true;
}

bool Unmarshaller::readBytes(size_t len, const char **result) {
  const char *bytes = consumeBytes(len);
  if (!bytes) { return false; }
  *result = bytes;
  return true;
}

Napi::Value Unmarshaller::_parse() {
  Napi::Env env = info.Env();
  uint8_t code = 0;
  if (!readUint8(&code)) { return fail(); }
  _lastCode = code;
  switch (code) {
    case MARSHAL_NULL:       return env.Null();
    case MARSHAL_NONE:       return env.Null();
    case MARSHAL_FALSE:      return Napi::Boolean::New(env, false);
    case MARSHAL_TRUE:       return Napi::Boolean::New(env, true);
    case MARSHAL_INT:        return _parseInt32();
    case MARSHAL_INT64:      return _parseInt64();
    case MARSHAL_BFLOAT:     return _parseBinaryFloat();
    case MARSHAL_STRING:     return _parseByteString();
    case MARSHAL_TUPLE:      return _parseList();
    case MARSHAL_LIST:       return _parseList();
    case MARSHAL_DICT:       return _parseDict();
    case MARSHAL_UNICODE:    return _parseUnicode();
    case MARSHAL_INTERNED:   return _parseInterned();
    case MARSHAL_STRINGREF:  return _parseStringRef();

    // We could support it, but it's unclear if we can parse consistently with
    // Python, and it's a deprecated way to serialize floats anyway.
    case MARSHAL_FLOAT:     return env.Null();
    // None of the following are supported.
    case MARSHAL_STOPITER:
    case MARSHAL_ELLIPSIS:
    case MARSHAL_COMPLEX:
    case MARSHAL_LONG:
    case MARSHAL_CODE:
    case MARSHAL_UNKNOWN:
    case MARSHAL_SET:
    case MARSHAL_FROZENSET:  return env.Null();
    default:                 return env.Null();
  }
}


// A shorthand usd internally below.
static inline Napi::Value emptyValue() {
  return Napi::Value();
}

// Helper used internally below.
template<typename T>
static Napi::Value toMaybeLocalValue(const Napi::Value& value) {
  return value; // value.IsEmpty() ? emptyValue() : Napi::Value(value);
}


Napi::Value Unmarshaller::_parseInt32() {
  int32_t value = 0;
  if (!readInt32(&value)) { return fail(); }
  Napi::Env env = info.Env();
  return Napi::Number::New(env, value);
}

Napi::Value Unmarshaller::_parseInt64() {
  int32_t low = 0, hi = 0;
  if (!readInt32(&low) || !readInt32(&hi)) { return fail(); }
  if ((hi == 0 && low >= 0) || (hi == -1 && low < 0)) {
    Napi::Env env = info.Env();
    return Napi::Number::New(env, low);
  }
  // TODO We could actually support 53 bits or so, and offer imprecise doubles for larger ones.
  // Or pass along a raw representation, such as https://github.com/broofa/node-int64.
  return fail("int64 only supports 32-bit values for now");
}

Napi::Value Unmarshaller::_parseBinaryFloat() {
  double value = 0;
  if (!readFloat64(&value)) { return fail(); }
  Napi::Env env = info.Env();
  return Napi::Number::New(env, value);
}


Napi::Value Unmarshaller::_parseByteString() {
  int32_t len = 0;
  const char *buf = NULL;
  if (!readInt32(&len) || !readBytes(len, &buf)) { return fail(); }
  Napi::Env env = info.Env();
  return Napi::Buffer<char>::Copy(env, buf, len);
}

Napi::Value Unmarshaller::_parseUnicode() {
  int32_t len = 0;
  const char *buf = NULL;
  if (!readInt32(&len) || !readBytes(len, &buf)) { return fail(); }
  Napi::Env env = info.Env();
  return Napi::String::New(env, buf, len);
}

Napi::Value Unmarshaller::_parseInterned() {
  int32_t len = 0;
  const char *buf = NULL;
  if (!readInt32(&len) || !readBytes(len, &buf)) { return fail(); }
  stringTable.push_back(std::string(buf, len));
  Napi::Env env = info.Env();
  return Napi::Buffer<char>::Copy(env, buf, len);
}

Napi::Value Unmarshaller::_parseStringRef() {
  int32_t index = 0;
  if (!readInt32(&index)) { return fail(); }
  if (index >= 0 && size_t(index) < stringTable.size()) {
    const std::string &result = stringTable[index];
    Napi::Env env = info.Env();
    return Napi::Buffer<char>::Copy(env, &result[0], result.size());
  } else {
    return fail("Invalid interned string reference");
  }
}

Napi::Value Unmarshaller::_parseList() {
  int32_t len = 0;
  if (!readInt32(&len)) { return fail(); }

  Napi::Env env = info.Env();
  Napi::EscapableHandleScope scope(env);
  Napi::Array result = Napi::Array::New(env, len);
  for (int i = 0; i < len; i++) {
    Napi::Value item = _parse();
    if (item.IsEmpty()) { return emptyValue(); }
    (result).Set(i, item);
  }
  return scope.Escape(result);
}

Napi::Value Unmarshaller::_parseDict() {
  Napi::Env env = info.Env();
  Napi::EscapableHandleScope scope(env);
  Napi::Object result = Napi::Object::New(env);
  while (true) {
    Napi::Value key = _parse();
    if (key.IsEmpty()) { return emptyValue(); }

    if (_lastCode == MARSHAL_NULL) { break; }

    Napi::Value value = Unmarshaller::_parse();
    if (value.IsEmpty()) { return emptyValue(); }

    (result).Set(key, value);
  }
  return scope.Escape(result);
}
