#include "marshal.h"

typedef std::pair<std::string, v8::Local<v8::Value> > StringPair;
static bool sortByFirst(const StringPair &a, const StringPair &b) {
  return a.first < b.first;
}

void Marshal::marshalValue(v8::Local<v8::Value> val) {
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
