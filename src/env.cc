#include <v8.h>

#include "../include/js.h"
#include "platform.hh"
#include "types.hh"

using v8::ArrayBuffer;
using v8::HandleScope;
using v8::Isolate;

extern "C" int
js_env_init (js_env_t **result) {
  auto allocator = ArrayBuffer::Allocator::NewDefaultAllocator();

  Isolate::CreateParams params;
  params.array_buffer_allocator = allocator;

  auto isolate = Isolate::New(params);

  HandleScope scope(isolate);

  *result = new js_env_s(js::platform, isolate, allocator);

  return 0;
}

extern "C" int
js_env_destroy (js_env_t *env) {
  delete env->allocator;

  env->isolate->Dispose();

  delete env;

  return 0;
}
