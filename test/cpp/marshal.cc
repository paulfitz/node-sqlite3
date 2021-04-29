#include <napi.h>
#include <uv.h>
#include <string>
#include <vector>
#include "../../src/marshal.h"


Napi::Value Serialize(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() > 0) {
    Marshaller m;
    m.marshalValue(info[0]);
    const std::vector<char> &buffer = m.getBuffer();
    Napi::Env env = info.Env();
    return Napi::Buffer<char>::Copy(env, &buffer[0], buffer.size());
  }
  return env.Null();
}

Napi::Value Parse(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() > 0) {
    if (!info[0].IsBuffer()) {
      Napi::Error::New(env, "Argument must be a buffer").ThrowAsJavaScriptException();
      return env.Null();
    } else {
      Napi::Object buffer = info[0].As<Napi::Object>();
      Napi::Value result = Unmarshaller::parse(info,
          buffer.As<Napi::Buffer<char>>().Data(), buffer.As<Napi::Buffer<char>>().Length());
      if (!result.IsEmpty()) {
        return result;
      }
    }
  }
  return env.Null();
}

Napi::Value TestOppositeEndianness(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() > 0) {
    marshalTestOppositeEndianness(info[0].As<Napi::Boolean>().Value());
  }
  return env.Null();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "serialize"),
              Napi::Function::New(env, Serialize));
  exports.Set(Napi::String::New(env, "parse"),
              Napi::Function::New(env, Parse));
  exports.Set(Napi::String::New(env, "testOppositeEndianness"),
              Napi::Function::New(env, TestOppositeEndianness));
  return exports;
  /*
  (target
    ).Set(Napi::String::New(env, "serialize")
    , Napi::Function::New(env, Serialize)
  );
  (target
    ).Set(Napi::String::New(env, "parse")
    , Napi::Function::New(env, Parse)
  );
  (target
    ).Set(Napi::String::New(env, "testOppositeEndianness")
    , Napi::Function::New(env, TestOppositeEndianness)
  );
  */
}

NODE_API_MODULE(marshal, Init)
