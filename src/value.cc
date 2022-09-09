#include <stddef.h>
#include <v8.h>

#include "../include/js.h"
#include "types.hh"

using v8::Context;
using v8::EscapableHandleScope;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Int32;
using v8::Integer;
using v8::Local;
using v8::Object;
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

static void
on_function_call (const FunctionCallbackInfo<Value> &info) {
  auto wrapper = reinterpret_cast<js_callback_data_t *>(info.Data().As<v8::External>()->Value());

  auto result = wrapper->cb(wrapper->env, reinterpret_cast<const js_callback_info_t *>(&info));

  auto local = *reinterpret_cast<Local<Value> *>(&result);

  info.GetReturnValue().Set(local);
}

extern "C" int
js_create_function (js_env_t *env, const char *name, size_t len, js_callback_t cb, void *data, js_value_t **result) {
  EscapableHandleScope scope(env->isolate);

  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  auto wrapper = new js_callback_data_t(env, cb, data);

  auto external = External::New(env->isolate, wrapper);

  auto fn = Function::New(context, on_function_call, external).ToLocalChecked();

  if (name != nullptr) {
    auto string = String::NewFromUtf8(env->isolate, name, v8::NewStringType::kNormal, len);

    fn->SetName(string.ToLocalChecked());
  }

  *result = reinterpret_cast<js_value_t *>(*scope.Escape(fn));

  return 0;
}

extern "C" int
js_get_global (js_env_t *env, js_value_t **result) {
  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  *result = reinterpret_cast<js_value_t *>(*context->Global());

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

extern "C" int
js_get_named_property (js_env_t *env, js_value_t *object, const char *name, js_value_t **result) {
  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  auto key = String::NewFromUtf8(env->isolate, name, v8::NewStringType::kNormal, -1);

  auto target = *reinterpret_cast<Local<Object> *>(&object);

  auto local = target->Get(context, key.ToLocalChecked());

  *result = reinterpret_cast<js_value_t *>(*local.ToLocalChecked());

  return 0;
}

extern "C" int
js_set_named_property (js_env_t *env, js_value_t *object, const char *name, js_value_t *value) {
  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  auto key = String::NewFromUtf8(env->isolate, name, v8::NewStringType::kNormal, -1);

  auto local = *reinterpret_cast<Local<Value> *>(&value);

  auto target = *reinterpret_cast<Local<Object> *>(&object);

  target->Set(context, key.ToLocalChecked(), local).ToChecked();

  return 0;
}
