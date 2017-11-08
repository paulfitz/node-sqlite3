#include <nan.h>
#include <string>
#include <vector>
#include "../../src/marshal.h"


NAN_METHOD(Serialize) {
  if (info.Length() > 0) {
    Marshaller m;
    m.marshalValue(info[0]);
    const std::vector<char> &buffer = m.getBuffer();
    info.GetReturnValue().Set(Nan::CopyBuffer(&buffer[0], buffer.size()).ToLocalChecked());
  }
}

NAN_METHOD(Parse) {
  if (info.Length() > 0) {
    if (!node::Buffer::HasInstance(info[0])) {
      Nan::ThrowError("Argument must be a buffer");
    } else {
      v8::Local<v8::Object> buffer = Nan::To<v8::Object>(info[0]).ToLocalChecked();
      Nan::MaybeLocal<v8::Value> result = Unmarshaller::parse(
          node::Buffer::Data(buffer), node::Buffer::Length(buffer));
      if (!result.IsEmpty()) {
        info.GetReturnValue().Set(result.ToLocalChecked());
      }
    }
  }
}

NAN_METHOD(TestOppositeEndianness) {
  if (info.Length() > 0) {
    marshalTestOppositeEndianness(Nan::To<bool>(info[0]).FromJust());
  }
}

NAN_MODULE_INIT(Init) {
  Nan::Set(target
    , Nan::New<v8::String>("serialize").ToLocalChecked()
    , Nan::New<v8::FunctionTemplate>(Serialize)->GetFunction()
  );
  Nan::Set(target
    , Nan::New<v8::String>("parse").ToLocalChecked()
    , Nan::New<v8::FunctionTemplate>(Parse)->GetFunction()
  );
  Nan::Set(target
    , Nan::New<v8::String>("testOppositeEndianness").ToLocalChecked()
    , Nan::New<v8::FunctionTemplate>(TestOppositeEndianness)->GetFunction()
  );
}

NODE_MODULE(marshal, Init)
