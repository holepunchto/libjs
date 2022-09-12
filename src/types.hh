#pragma once

#include <v8.h>

#include "../include/js.h"

typedef struct js_callback_data_s js_callback_data_t;

struct js_env_s {
  v8::Platform *platform;
  v8::Isolate *isolate;
  v8::ArrayBuffer::Allocator *allocator;
  v8::Persistent<v8::Context> context;
  v8::Persistent<v8::Value> exception;

  js_env_s(v8::Platform *platform, v8::Isolate *isolate, v8::ArrayBuffer::Allocator *allocator)
      : platform(platform),
        isolate(isolate),
        allocator(allocator),
        context(isolate, v8::Context::New(isolate)),
        exception() {}
};

struct js_handle_scope_s {
  v8::HandleScope scope;

  js_handle_scope_s(v8::Isolate *isolate)
      : scope(isolate) {}
};

struct js_callback_data_s {
  js_env_t *env;
  js_callback_t cb;
  void *data;

  js_callback_data_s(js_env_t *env, js_callback_t cb, void *data)
      : env(env),
        cb(cb),
        data(data) {}
};
