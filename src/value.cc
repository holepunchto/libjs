#include <stddef.h>
#include <v8.h>

#include "../include/js.h"
#include "types.h"

using v8::Int32;
using v8::Integer;
using v8::Local;
using v8::String;
using v8::Uint32;
using v8::Value;

extern "C" int
js_create_int32 (js_env_t *env, int32_t value, js_value_t **result) {
  auto uint = Integer::New(env->isolate, value);

  *result = reinterpret_cast<js_value_t *>(*uint);

  return 0;
}

extern "C" int
js_create_uint32 (js_env_t *env, uint32_t value, js_value_t **result) {
  auto uint = Integer::NewFromUnsigned(env->isolate, value);

  *result = reinterpret_cast<js_value_t *>(*uint);

  return 0;
}

extern "C" int
js_create_string_utf8 (js_env_t *env, const char *value, ssize_t len, js_value_t **result) {
  auto string = String::NewFromUtf8(env->isolate, value, v8::NewStringType::kNormal, len);

  *result = reinterpret_cast<js_value_t *>(*string.ToLocalChecked());

  return 0;
}

extern "C" int
js_get_value_int32 (js_env_t *env, js_value_t *value, int32_t *result) {
  auto local = *reinterpret_cast<Local<Value> *>(&value);

  *result = local.As<Int32>()->Value();

  return 0;
}

extern "C" int
js_get_value_uint32 (js_env_t *env, js_value_t *value, uint32_t *result) {
  auto local = *reinterpret_cast<Local<Value> *>(&value);

  *result = local.As<Uint32>()->Value();

  return 0;
}
