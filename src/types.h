#pragma once

#include <v8.h>

struct js_env_s {
  v8::Platform *platform;
  v8::Isolate *isolate;
  v8::ArrayBuffer::Allocator *allocator;
  v8::Persistent<v8::Context> context;

  js_env_s(v8::Platform *platform, v8::Isolate *isolate, v8::ArrayBuffer::Allocator *allocator)
      : platform(platform),
        isolate(isolate),
        allocator(allocator),
        context(isolate, v8::Context::New(isolate)) {}
};

struct js_handle_scope_s {
  v8::HandleScope scope;

  js_handle_scope_s(v8::Isolate *isolate)
      : scope(isolate) {}
};
