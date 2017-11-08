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

typedef std::pair<std::string, v8::Local<v8::Value> > StringPair;
static bool sortByFirst(const StringPair &a, const StringPair &b) {
  return a.first < b.first;
}

void Marshaller::marshalValue(v8::Local<v8::Value> val) {
  if (val->IsBoolean()) {
    marshalBool(val->BooleanValue());
  } else if (val->IsInt32()) {
    marshalInt(val->Int32Value());
  } else if (val->IsNumber()) {
    marshalDouble(val->NumberValue());
  } else if (val->IsString()) {
    Nan::Utf8String strVal(val);
    marshalUnicode(*strVal, strVal.length());
  } else if (val->IsArray()) {
    v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(val);
    int length = array->Length();
    marshalList(length);
    for (int i = 0; i < length; i++) {
      marshalValue(Nan::Get(array, i).ToLocalChecked());
    }
  } else if (node::Buffer::HasInstance(val)) {
    v8::Local<v8::Object> buffer = Nan::To<v8::Object>(val).ToLocalChecked();
    marshalString(node::Buffer::Data(buffer), node::Buffer::Length(buffer));
  } else if (val->IsObject()) {
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(val);
    v8::Local<v8::Array> array = Nan::GetPropertyNames(object).ToLocalChecked();
    // Keys need to be serialized in sorted order.
    int length = array->Length();
    std::vector<StringPair> keys;
    keys.reserve(length);
    for (int i = 0; i < length; i++) {
      v8::Local<v8::Value> name = Nan::Get(array, i).ToLocalChecked();
      Nan::Utf8String strVal(name);
      if (*strVal) {
        keys.push_back(std::make_pair(std::string(*strVal, strVal.length()), name));
      }
    }
    std::sort(keys.begin(), keys.end(), sortByFirst);
    marshalDictBegin();
    for (size_t i = 0; i < keys.size(); i++) {
      v8::Local<v8::Value> name = keys[i].second;
      marshalValue(name);
      marshalValue(Nan::Get(object, name).ToLocalChecked());
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

Nan::MaybeLocal<v8::Value> Unmarshaller::_parse() {
  uint8_t code = 0;
  if (!readUint8(&code)) { return fail(); }
  _lastCode = code;
  switch (code) {
    case MARSHAL_NULL:       return Nan::Null();
    case MARSHAL_NONE:       return Nan::Null();
    case MARSHAL_FALSE:      return Nan::False();
    case MARSHAL_TRUE:       return Nan::True();
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
    case MARSHAL_FLOAT:     return Nan::Null();
    // None of the following are supported.
    case MARSHAL_STOPITER:
    case MARSHAL_ELLIPSIS:
    case MARSHAL_COMPLEX:
    case MARSHAL_LONG:
    case MARSHAL_CODE:
    case MARSHAL_UNKNOWN:
    case MARSHAL_SET:
    case MARSHAL_FROZENSET:  return Nan::Null();
    default:                 return Nan::Null();
  }
}


// A shorthand usd internally below.
static inline Nan::MaybeLocal<v8::Value> emptyValue() {
  return Nan::MaybeLocal<v8::Value>();
}

// Helper used internally below.
template<typename T>
static Nan::MaybeLocal<v8::Value> toMaybeLocalValue(Nan::MaybeLocal<T> value) {
  return value.IsEmpty() ? emptyValue() : Nan::MaybeLocal<v8::Value>(value.ToLocalChecked());
}


Nan::MaybeLocal<v8::Value> Unmarshaller::_parseInt32() {
  int32_t value = 0;
  if (!readInt32(&value)) { return fail(); }
  return Nan::New<v8::Number>(value);
}

Nan::MaybeLocal<v8::Value> Unmarshaller::_parseInt64() {
  int32_t low = 0, hi = 0;
  if (!readInt32(&low) || !readInt32(&hi)) { return fail(); }
  if ((hi == 0 && low >= 0) || (hi == -1 && low < 0)) {
    return Nan::New<v8::Number>(low);
  }
  // TODO We could actually support 53 bits or so, and offer imprecise doubles for larger ones.
  // Or pass along a raw representation, such as https://github.com/broofa/node-int64.
  return fail("int64 only supports 32-bit values for now");
}

Nan::MaybeLocal<v8::Value> Unmarshaller::_parseBinaryFloat() {
  double value = 0;
  if (!readFloat64(&value)) { return fail(); }
  return Nan::New<v8::Number>(value);
}


Nan::MaybeLocal<v8::Value> Unmarshaller::_parseByteString() {
  int32_t len = 0;
  const char *buf = NULL;
  if (!readInt32(&len) || !readBytes(len, &buf)) { return fail(); }
  return toMaybeLocalValue(Nan::CopyBuffer(buf, len));
}

Nan::MaybeLocal<v8::Value> Unmarshaller::_parseUnicode() {
  int32_t len = 0;
  const char *buf = NULL;
  if (!readInt32(&len) || !readBytes(len, &buf)) { return fail(); }
  return toMaybeLocalValue(Nan::New<v8::String>(buf, len));
}

Nan::MaybeLocal<v8::Value> Unmarshaller::_parseInterned() {
  int32_t len = 0;
  const char *buf = NULL;
  if (!readInt32(&len) || !readBytes(len, &buf)) { return fail(); }
  stringTable.push_back(std::string(buf, len));
  return toMaybeLocalValue(Nan::CopyBuffer(buf, len));
}

Nan::MaybeLocal<v8::Value> Unmarshaller::_parseStringRef() {
  int32_t index = 0;
  if (!readInt32(&index)) { return fail(); }
  if (index >= 0 && size_t(index) < stringTable.size()) {
    const std::string &result = stringTable[index];
    return toMaybeLocalValue(Nan::CopyBuffer(&result[0], result.size()));
  } else {
    return fail("Invalid interned string reference");
  }
}

Nan::MaybeLocal<v8::Value> Unmarshaller::_parseList() {
  int32_t len = 0;
  if (!readInt32(&len)) { return fail(); }

  Nan::EscapableHandleScope scope;
  v8::Local<v8::Array> result = Nan::New<v8::Array>(len);
  for (int i = 0; i < len; i++) {
    Nan::MaybeLocal<v8::Value> item = _parse();
    if (item.IsEmpty()) { return emptyValue(); }
    Nan::Set(result, i, item.ToLocalChecked());
  }
  return scope.Escape(result);
}

Nan::MaybeLocal<v8::Value> Unmarshaller::_parseDict() {
  Nan::EscapableHandleScope scope;
  v8::Local<v8::Object> result = Nan::New<v8::Object>();
  while (true) {
    Nan::MaybeLocal<v8::Value> key = _parse();
    if (key.IsEmpty()) { return emptyValue(); }

    if (_lastCode == MARSHAL_NULL) { break; }

    Nan::MaybeLocal<v8::Value> value = Unmarshaller::_parse();
    if (value.IsEmpty()) { return emptyValue(); }

    Nan::Set(result, key.ToLocalChecked(), value.ToLocalChecked());
  }
  return scope.Escape(result);
}
