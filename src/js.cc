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
  Platform *platform;
  Isolate *isolate;
  ArrayBuffer::Allocator *allocator;
  Persistent<Context> context;
  Persistent<Value> exception;

  js_env_s(Platform *platform, Isolate *isolate, ArrayBuffer::Allocator *allocator)
      : platform(platform),
        isolate(isolate),
        allocator(allocator),
        context(isolate, Context::New(isolate)),
        exception() {}
};

struct js_handle_scope_s {
  HandleScope scope;

  js_handle_scope_s(Isolate *isolate)
      : scope(isolate) {}
};

struct js_escapable_handle_scope_s {
  EscapableHandleScope scope;
  bool escaped;

  js_escapable_handle_scope_s(Isolate *isolate)
      : scope(isolate),
        escaped(false) {}
};

struct js_ref_s {
  Persistent<Value> value;
  uint32_t count;

  js_ref_s(Isolate *isolate, Local<Value> value, uint32_t count)
      : value(isolate, value),
        count(count) {}
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

static Platform *js_platform = nullptr;

extern "C" int
js_platform_init (const char *path) {
  assert(js_platform == nullptr);

  V8::InitializeICUDefaultLocation(path);
  V8::InitializeExternalStartupData(path);

  js_platform = platform::NewDefaultPlatform().release();

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
js_set_flags_from_string (const char *string, size_t len) {
  if (len == (size_t) -1) {
    V8::SetFlagsFromString(string);
  } else {
    V8::SetFlagsFromString(string, len);
  }

  return 0;
}

extern "C" int
js_set_flags_from_command_line (int *argc, char **argv, bool remove_flags) {
  V8::SetFlagsFromCommandLine(argc, argv, remove_flags);

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
js_open_escapable_handle_scope (js_env_t *env, js_escapable_handle_scope_t **result) {
  *result = new js_escapable_handle_scope_s(env->isolate);

  return 0;
}

extern "C" int
js_close_escapable_handle_scope (js_env_t *env, js_escapable_handle_scope_t *scope) {
  delete scope;

  return 0;
}

extern "C" int
js_escape_handle (js_env_t *env, js_escapable_handle_scope_t *scope, js_value_t *escapee, js_value_t **result) {
  if (scope->escaped) {
    return -1;
  }

  scope->escaped = true;

  auto local = *reinterpret_cast<Local<Value> *>(&escapee);

  *result = reinterpret_cast<js_value_t *>(*scope->scope.Escape(local));

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

static void
on_reference_finalize (const WeakCallbackInfo<js_ref_t> &info) {
  auto reference = info.GetParameter();

  reference->value.Reset();
}

inline void
js_set_weak_reference (js_env_t *env, js_ref_t *reference) {
  reference->value.SetWeak(reference, on_reference_finalize, WeakCallbackType::kParameter);
}

inline void
js_clear_weak_reference (js_env_t *env, js_ref_t *reference) {
  reference->value.ClearWeak();
}

extern "C" int
js_create_reference (js_env_t *env, js_value_t *value, uint32_t count, js_ref_t **result) {
  auto reference = new js_ref_t(env->isolate, *reinterpret_cast<Local<Value> *>(&value), count);

  if (reference->count == 0) js_set_weak_reference(env, reference);

  *result = reference;

  return 0;
}

extern "C" int
js_delete_reference (js_env_t *env, js_ref_t *reference) {
  delete reference;

  return 0;
}

extern "C" int
js_reference_ref (js_env_t *env, js_ref_t *reference, uint32_t *result) {
  reference->count++;

  if (reference->count == 1) js_clear_weak_reference(env, reference);

  if (result != nullptr) {
    *result = reference->count;
  }

  return 0;
}

extern "C" int
js_reference_unref (js_env_t *env, js_ref_t *reference, uint32_t *result) {
  if (reference->count == 0) return -1;

  reference->count--;

  if (reference->count == 0) js_set_weak_reference(env, reference);

  if (result != nullptr) {
    *result = reference->count;
  }

  return 0;
}

extern "C" int
js_get_reference_value (js_env_t *env, js_ref_t *reference, js_value_t **result) {
  if (reference->value.IsEmpty()) {
    *result = nullptr;
  } else {
    *result = reinterpret_cast<js_value_t *>(*reference->value.Get(env->isolate));
  }

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

extern "C" int
js_create_object (js_env_t *env, js_value_t **result) {
  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  context->Enter();

  auto object = Object::New(env->isolate);

  context->Exit();

  *result = reinterpret_cast<js_value_t *>(*object);

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

extern "C" int
js_get_null (js_env_t *env, js_value_t **result) {
  *result = reinterpret_cast<js_value_t *>(*Null(env->isolate));

  return 0;
}

extern "C" int
js_get_undefined (js_env_t *env, js_value_t **result) {
  *result = reinterpret_cast<js_value_t *>(*Undefined(env->isolate));

  return 0;
}

extern "C" int
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

extern "C" int
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

extern "C" int
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

extern "C" int
js_request_garbage_collection (js_env_t *env) {
  env->isolate->RequestGarbageCollectionForTesting(Isolate::GarbageCollectionType::kFullGarbageCollection);

  return 0;
}
