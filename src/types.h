#ifndef JS_TYPES_H
#define JS_TYPES_H

#include <v8.h>

using v8::ArrayBuffer;
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Persistent;
using v8::Platform;

struct js_platform_s {
  Platform *platform;

  js_platform_s(Platform *platform)
      : platform(platform) {}
};

struct js_env_s {
  Isolate *isolate;
  ArrayBuffer::Allocator *allocator;
  Persistent<Context> context;

  js_env_s(Isolate *isolate, ArrayBuffer::Allocator *allocator)
      : isolate(isolate),
        allocator(allocator),
        context(isolate, Context::New(isolate)) {}
};

struct js_handle_scope_s {
  HandleScope scope;

  js_handle_scope_s(Isolate *isolate)
      : scope(isolate) {}
};

#endif // JS_TYPES_H
