#include <nan.h>
#include <string>
#include <vector>
#include "../../src/marshal.h"

NAN_METHOD(Serialize) {
  if (info.Length() > 0) {
    Marshal m;
    m.marshalValue(info[0]);
    const std::vector<char> &buffer = m.getBuffer();
    info.GetReturnValue().Set(Nan::CopyBuffer(&buffer[0], buffer.size()).ToLocalChecked());
  }
}

NAN_MODULE_INIT(Init) {
  Nan::Set(target
    , Nan::New<v8::String>("serialize").ToLocalChecked()
    , Nan::New<v8::FunctionTemplate>(Serialize)->GetFunction()
  );
}

NODE_MODULE(marshal, Init)
