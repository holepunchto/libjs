#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <libplatform/libplatform.h>
#include <v8.h>

#include "../include/js.h"

using namespace v8;

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

static v8::Platform *js_platform = nullptr;

extern "C" int
js_platform_init (const char *path) {
  assert(js_platform == nullptr);

  V8::InitializeICUDefaultLocation(path);
  V8::InitializeExternalStartupData(path);

  js_platform = v8::platform::NewDefaultPlatform().release();

  V8::InitializePlatform(js_platform);
  V8::Initialize();

  return 0;
}

extern "C" int
js_platform_destroy () {
  assert(js_platform != nullptr);

  V8::Dispose();
  V8::DisposePlatform();

  delete js_platform;

  return 0;
}

extern "C" int
js_env_init (js_env_t **result) {
  auto allocator = ArrayBuffer::Allocator::NewDefaultAllocator();

  Isolate::CreateParams params;
  params.array_buffer_allocator = allocator;

  auto isolate = Isolate::New(params);

  HandleScope scope(isolate);

  *result = new js_env_s(js_platform, isolate, allocator);

  return 0;
}

extern "C" int
js_env_destroy (js_env_t *env) {
  delete env->allocator;

  env->isolate->Dispose();

  delete env;

  return 0;
}

extern "C" int
js_open_handle_scope (js_env_t *env, js_handle_scope_t **result) {
  *result = new js_handle_scope_s(env->isolate);

  return 0;
}

extern "C" int
js_close_handle_scope (js_env_t *env, js_handle_scope_t *scope) {
  delete scope;

  return 0;
}

extern "C" int
js_run_script (js_env_t *env, js_value_t *script, js_value_t **result) {
  auto local = *reinterpret_cast<Local<Value> *>(&script);

  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  ScriptCompiler::Source source(local.As<String>());

  env->isolate->Enter();
  context->Enter();

  auto compiled = ScriptCompiler::Compile(context, &source);

  *result = reinterpret_cast<js_value_t *>(*compiled.ToLocalChecked()->Run(context).ToLocalChecked());

  context->Exit();
  env->isolate->Exit();

  return 0;
}

extern "C" int
js_run_module (js_env_t *env, js_value_t *module, const char *name, js_value_t **result) {
  auto local = *reinterpret_cast<Local<Value> *>(&module);

  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  ScriptOrigin origin(env->isolate, String::NewFromUtf8(env->isolate, name).ToLocalChecked());

  ScriptCompiler::Source source(local.As<String>());

  env->isolate->Enter();
  context->Enter();

  auto compiled = ScriptCompiler::CompileModule(env->isolate, &source).ToLocalChecked();

  *result = reinterpret_cast<js_value_t *>(*compiled->Evaluate(context).ToLocalChecked());

  context->Exit();
  env->isolate->Exit();

  return 0;
}

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
js_create_string_utf8 (js_env_t *env, const char *value, size_t len, js_value_t **result) {
  auto string = String::NewFromUtf8(env->isolate, value, NewStringType::kNormal, len);

  *result = reinterpret_cast<js_value_t *>(*string.ToLocalChecked());

  return 0;
}

static void
on_function_call (const FunctionCallbackInfo<Value> &info) {
  auto wrapper = reinterpret_cast<js_callback_data_t *>(info.Data().As<External>()->Value());

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
    auto string = String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, len);

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

int
js_get_null (js_env_t *env, js_value_t **result) {
  *result = reinterpret_cast<js_value_t *>(*Null(env->isolate));

  return 0;
}

int
js_get_undefined (js_env_t *env, js_value_t **result) {
  *result = reinterpret_cast<js_value_t *>(*Undefined(env->isolate));

  return 0;
}

int
js_get_boolean (js_env_t *env, bool value, js_value_t **result) {
  if (value) {
    *result = reinterpret_cast<js_value_t *>(*True(env->isolate));
  } else {
    *result = reinterpret_cast<js_value_t *>(*False(env->isolate));
  }

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

  auto key = String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, -1);

  auto target = *reinterpret_cast<Local<Object> *>(&object);

  auto local = target->Get(context, key.ToLocalChecked());

  *result = reinterpret_cast<js_value_t *>(*local.ToLocalChecked());

  return 0;
}

extern "C" int
js_set_named_property (js_env_t *env, js_value_t *object, const char *name, js_value_t *value) {
  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  auto key = String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, -1);

  auto local = *reinterpret_cast<Local<Value> *>(&value);

  auto target = *reinterpret_cast<Local<Object> *>(&object);

  target->Set(context, key.ToLocalChecked(), local).ToChecked();

  return 0;
}

int
js_call_function (js_env_t *env, js_value_t *recv, js_value_t *fn, size_t argc, const js_value_t *argv[], js_value_t **result) {
  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  auto local_recv = *reinterpret_cast<Local<Value> *>(&recv);

  auto local_fn = *reinterpret_cast<Local<Function> *>(&fn);

  TryCatch try_catch(env->isolate);

  auto local = local_fn->Call(
    context,
    local_recv,
    argc,
    reinterpret_cast<Local<Value> *>(const_cast<js_value_t **>(argv))
  );

  if (try_catch.HasCaught()) {
    env->exception.Reset(env->isolate, try_catch.Exception());

    return -1;
  } else {
    *result = reinterpret_cast<js_value_t *>(*local.ToLocalChecked());

    return 0;
  }
}

int
js_get_callback_info (js_env_t *env, const js_callback_info_t *info, size_t *argc, js_value_t *argv[], js_value_t *self, void **data) {
  auto v8_info = reinterpret_cast<const FunctionCallbackInfo<Value> &>(*info);

  if (argv != nullptr) {
    for (size_t i = 0, n = *argc; i < n; i++) {
      argv[i] = reinterpret_cast<js_value_t *>(*v8_info[i]);
    }
  }

  if (argc != nullptr) {
    *argc = v8_info.Length();
  }

  if (self != nullptr) {
    self = reinterpret_cast<js_value_t *>(*v8_info.This());
  }

  if (data != nullptr) {
    *data = reinterpret_cast<js_callback_data_t *>(v8_info.Data().As<External>()->Value())->data;
  }

  return 0;
}
