#include <unordered_map>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <libplatform/libplatform.h>
#include <v8.h>

#include "../include/js.h"

using namespace v8;

typedef struct js_callback_data_s js_callback_data_t;

typedef enum {
  js_context_environment = 1,
} js_context_index_t;

struct js_env_s {
  Platform *platform;
  Isolate *isolate;
  ArrayBuffer::Allocator *allocator;
  Persistent<Context> context;
  Persistent<Value> exception;
  std::unordered_multimap<int, js_module_s *> modules;

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

struct js_module_s {
  Local<Module> module;
  js_module_resolve_cb resolve;
  js_synethic_module_cb evaluate;
  void *data;

  js_module_s(Local<Module> module, void *data)
      : module(module),
        resolve(nullptr),
        evaluate(nullptr),
        data(data) {}
};

struct js_ref_s {
  Persistent<Value> value;
  uint32_t count;

  js_ref_s(Isolate *isolate, Local<Value> value, uint32_t count)
      : value(isolate, value),
        count(count) {}
};

struct js_deferred_s {
  Persistent<Promise::Resolver> resolver;

  js_deferred_s(Isolate *isolate, Local<Promise::Resolver> resolver)
      : resolver(isolate, resolver) {}
};

struct js_callback_data_s {
  js_env_t *env;
  js_function_cb cb;
  void *data;

  js_callback_data_s(js_env_t *env, js_function_cb cb, void *data)
      : env(env),
        cb(cb),
        data(data) {}
};

struct js_task_s : Task {
  js_env_t *env;
  js_task_cb cb;
  void *data;

  js_task_s(js_env_t *env, js_task_cb cb, void *data)
      : env(env),
        cb(cb),
        data(data) {}

  void
  Run () override {
    cb(env, data);
  }
};

static inline js_env_t *
get_env (Local<Context> context) {
  return reinterpret_cast<js_env_t *>(context->GetAlignedPointerFromEmbedderData(js_context_environment));
}

static inline js_module_t *
get_module (Local<Context> context, Local<Module> referrer) {
  auto env = get_env(context);

  auto range = env->modules.equal_range(referrer->GetIdentityHash());

  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->module == referrer) {
      return it->second;
    }
  }

  return nullptr;
}

template <typename T>
static inline Local<T>
to_local (Persistent<T> &persistent) {
  return *reinterpret_cast<Local<T> *>(&persistent);
}

template <typename T = Value>
static inline Local<T>
to_local (js_value_t *value) {
  return *reinterpret_cast<Local<T> *>(&value);
}

template <typename T>
static inline js_value_t *
from_local (Local<T> local) {
  return reinterpret_cast<js_value_t *>(*local);
}

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

  isolate->SetMicrotasksPolicy(MicrotasksPolicy::kExplicit);

  HandleScope scope(isolate);

  auto env = new js_env_s(js_platform, isolate, allocator);

  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  context->SetAlignedPointerInEmbedderData(js_context_environment, env);

  *result = env;

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
  if (scope->escaped) return -1;

  scope->escaped = true;

  auto local = *reinterpret_cast<Local<Value> *>(&escapee);

  *result = reinterpret_cast<js_value_t *>(*scope->scope.Escape(local));

  return 0;
}

extern "C" int
js_run_script (js_env_t *env, js_value_t *source, js_value_t **result) {
  auto local = *reinterpret_cast<Local<Value> *>(&source);

  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  ScriptCompiler::Source v8_source(local.As<String>());

  env->isolate->Enter();
  context->Enter();

  auto compiled = ScriptCompiler::Compile(context, &v8_source).ToLocalChecked();

  *result = reinterpret_cast<js_value_t *>(*compiled->Run(context).ToLocalChecked());

  context->Exit();
  env->isolate->Exit();

  return 0;
}

extern "C" int
js_create_module (js_env_t *env, const char *name, size_t len, js_value_t *source, void *data, js_module_t **result) {
  auto local = to_local(source);

  auto context = to_local(env->context);

  ScriptOrigin origin(
    env->isolate,
    String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, len).ToLocalChecked(),
    0,
    0,
    false,
    -1,
    Local<Value>(),
    false,
    false,
    true
  );

  ScriptCompiler::Source v8_source(local.As<String>(), origin);

  env->isolate->Enter();
  context->Enter();

  auto compiled = ScriptCompiler::CompileModule(env->isolate, &v8_source).ToLocalChecked();

  context->Exit();
  env->isolate->Exit();

  auto module = new js_module_t(compiled, data);

  env->modules.emplace(compiled->GetIdentityHash(), module);

  *result = module;

  return 0;
}

static MaybeLocal<Value>
on_evaluate_synethic_module (Local<Context> context, Local<Module> referrer) {
  auto env = get_env(context);

  auto module = get_module(context, referrer);

  auto result = module->evaluate(env, module, module->data);

  if (result == nullptr) return Undefined(env->isolate);

  return to_local(result);
}

extern "C" int
js_create_synthetic_module (js_env_t *env, const char *name, size_t len, const js_value_t *export_names[], size_t names_len, js_synethic_module_cb cb, void *data, js_module_t **result) {
  auto context = to_local(env->context);

  auto local = reinterpret_cast<Local<String> *>(const_cast<js_value_t **>(export_names));

  std::vector<Local<String>> names(local, local + names_len);

  env->isolate->Enter();
  context->Enter();

  auto compiled = Module::CreateSyntheticModule(
    env->isolate,
    String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, len).ToLocalChecked(),
    names,
    on_evaluate_synethic_module
  );

  context->Exit();
  env->isolate->Exit();

  auto module = new js_module_t(compiled, data);

  module->evaluate = cb;

  env->modules.emplace(compiled->GetIdentityHash(), module);

  *result = module;

  return 0;
}

extern "C" int
js_delete_module (js_env_t *env, js_module_t *module) {
  delete module;

  return 0;
}

extern "C" int
js_set_module_export (js_env_t *env, js_module_t *module, js_value_t *name, js_value_t *value) {
  auto local = module->module;

  auto success = local->SetSyntheticModuleExport(env->isolate, to_local<String>(name), to_local(value));

  return success.FromMaybe(false) ? 0 : -1;
}

static MaybeLocal<Module>
on_resolve_module (Local<Context> context, Local<String> specifier, Local<FixedArray> assertions, Local<Module> referrer) {
  auto env = get_env(context);

  auto module = get_module(context, referrer);

  auto result = module->resolve(
    env,
    from_local(specifier),
    from_local(assertions),
    module,
    module->data
  );

  if (result == nullptr) return MaybeLocal<Module>();

  return result->module;
}

extern "C" int
js_instantiate_module (js_env_t *env, js_module_t *module, js_module_resolve_cb cb) {
  auto context = to_local(env->context);

  module->resolve = cb;

  auto local = module->module;

  auto success = local->InstantiateModule(context, on_resolve_module);

  return success.FromMaybe(false) ? 0 : -1;
}

extern "C" int
js_run_module (js_env_t *env, js_module_t *module, js_value_t **result) {
  auto context = to_local(env->context);

  auto local = module->module;

  env->isolate->Enter();
  context->Enter();

  *result = from_local(local->Evaluate(context).ToLocalChecked());

  context->Exit();
  env->isolate->Exit();

  return 0;
}

static void
on_reference_finalize (const WeakCallbackInfo<js_ref_t> &info) {
  auto reference = info.GetParameter();

  reference->value.Reset();
}

static inline void
js_set_weak_reference (js_env_t *env, js_ref_t *reference) {
  reference->value.SetWeak(reference, on_reference_finalize, WeakCallbackType::kParameter);
}

static inline void
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
js_create_function (js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data, js_value_t **result) {
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
js_create_promise (js_env_t *env, js_deferred_t **deferred, js_value_t **promise) {
  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  auto resolver = Promise::Resolver::New(context).ToLocalChecked();

  *deferred = new js_deferred_t(env->isolate, resolver);

  *promise = reinterpret_cast<js_value_t *>(*resolver->GetPromise());

  return 0;
}

static inline int
on_conclude_deferred (js_env_t *env, js_deferred_t *deferred, js_value_t *resolution, bool resolved) {
  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  auto resolver = Local<Promise::Resolver>::New(env->isolate, deferred->resolver);

  auto local = *reinterpret_cast<Local<Value> *>(&resolution);

  auto status = resolved
                  ? resolver->Resolve(context, local)
                  : resolver->Reject(context, local);

  delete deferred;

  return status.FromMaybe(false) ? 0 : -1;
}

extern "C" int
js_resolve_deferred (js_env_t *env, js_deferred_t *deferred, js_value_t *resolution) {
  return on_conclude_deferred(env, deferred, resolution, true);
}

extern "C" int
js_reject_deferred (js_env_t *env, js_deferred_t *deferred, js_value_t *resolution) {
  return on_conclude_deferred(env, deferred, resolution, false);
}

extern "C" int
js_typeof (js_env_t *env, js_value_t *value, js_value_type_t *result) {
  auto local = to_local(value);

  if (local->IsNumber()) {
    *result = js_number;
  } else if (local->IsBigInt()) {
    *result = js_bigint;
  } else if (local->IsString()) {
    *result = js_string;
  } else if (local->IsFunction()) {
    *result = js_function;
  } else if (local->IsExternal()) {
    *result = js_external;
  } else if (local->IsObject()) {
    *result = js_object;
  } else if (local->IsBoolean()) {
    *result = js_boolean;
  } else if (local->IsUndefined()) {
    *result = js_undefined;
  } else if (local->IsSymbol()) {
    *result = js_symbol;
  } else if (local->IsNull()) {
    *result = js_null;
  }

  return 0;
}

extern "C" int
js_is_array (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsArray();

  return 0;
}

extern "C" int
js_is_arraybuffer (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsArrayBuffer();

  return 0;
}

extern "C" int
js_is_number (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsNumber();

  return 0;
}

extern "C" int
js_is_bigint (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsBigInt();

  return 0;
}

extern "C" int
js_is_null (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsNull();

  return 0;
}

extern "C" int
js_is_undefined (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsUndefined();

  return 0;
}

extern "C" int
js_is_symbol (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsSymbol();

  return 0;
}

extern "C" int
js_is_boolean (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsBoolean();

  return 0;
}

extern "C" int
js_is_external (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsExternal();

  return 0;
}

extern "C" int
js_is_string (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsString();

  return 0;
}

extern "C" int
js_is_function (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsFunction();

  return 0;
}

extern "C" int
js_is_object (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsObject();

  return 0;
}

extern "C" int
js_is_date (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsDate();

  return 0;
}

extern "C" int
js_is_error (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsNativeError();

  return 0;
}

extern "C" int
js_is_typedarray (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsTypedArray();

  return 0;
}

extern "C" int
js_is_dataview (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsDataView();

  return 0;
}

extern "C" int
js_is_promise (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsPromise();

  return 0;
}

extern "C" int
js_strict_equals (js_env_t *env, js_value_t *a, js_value_t *b, bool *result) {
  *result = to_local(a)->StrictEquals(to_local(b));

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
js_get_arraybuffer_info (js_env_t *env, js_value_t *arraybuffer, void **data, size_t *len) {
  auto local = to_local(arraybuffer).As<ArrayBuffer>();

  if (data != nullptr) {
    *data = local->Data();
  }

  if (len != nullptr) {
    *len = local->ByteLength();
  }

  return 0;
}

extern "C" int
js_get_typedarray_info (js_env_t env, js_value_t *typedarray, js_typedarray_type_t *type, size_t *len, void **data, js_value_t **arraybuffer, size_t *offset) {
  auto local = to_local(typedarray).As<TypedArray>();

  if (type != nullptr) {
    if (local->IsInt8Array()) {
      *type = js_int8_array;
    } else if (local->IsUint8Array()) {
      *type = js_uint8_array;
    } else if (local->IsUint8ClampedArray()) {
      *type = js_uint8_clamped_array;
    } else if (local->IsInt16Array()) {
      *type = js_int16_array;
    } else if (local->IsUint16Array()) {
      *type = js_uint16_array;
    } else if (local->IsInt32Array()) {
      *type = js_int32_array;
    } else if (local->IsUint32Array()) {
      *type = js_uint32_array;
    } else if (local->IsFloat32Array()) {
      *type = js_float32_array;
    } else if (local->IsFloat64Array()) {
      *type = js_float64_array;
    } else if (local->IsBigInt64Array()) {
      *type = js_bigint64_array;
    } else if (local->IsBigUint64Array()) {
      *type = js_biguint64_array;
    }
  }

  if (len != nullptr) {
    *len = local->Length();
  }

  Local<ArrayBuffer> buffer;

  if (data != nullptr || arraybuffer != nullptr) {
    buffer = local->Buffer();
  }

  if (data != nullptr) {
    *data = static_cast<uint8_t *>(buffer->Data()) + local->ByteOffset();
  }

  if (arraybuffer != nullptr) {
    *arraybuffer = from_local(buffer);
  }

  if (offset != nullptr) {
    *offset = local->ByteOffset();
  }

  return 0;
}

extern "C" int
js_get_dataview_info (js_env_t *env, js_value_t *dataview, size_t *len, void **data, js_value_t **arraybuffer, size_t *offset) {
  auto local = to_local(dataview).As<DataView>();

  if (len != nullptr) {
    *len = local->ByteLength();
  }

  Local<ArrayBuffer> buffer;

  if (data != nullptr || arraybuffer != nullptr) {
    buffer = local->Buffer();
  }

  if (data != nullptr) {
    *data = static_cast<uint8_t *>(buffer->Data()) + local->ByteOffset();
  }

  if (arraybuffer != nullptr) {
    *arraybuffer = from_local(buffer);
  }

  if (offset != nullptr) {
    *offset = local->ByteOffset();
  }

  return 0;
}

extern "C" int
js_throw (js_env_t *env, js_value_t *error) {
  env->isolate->ThrowException(to_local(error));

  return 0;
}

static void
on_microtask (void *data) {
  auto task = reinterpret_cast<js_task_t *>(data);

  task->cb(task->env, task->data);

  delete task;
}

extern "C" int
js_queue_microtask (js_env_t *env, js_task_cb cb, void *data) {
  auto context = to_local(env->context);

  auto task = new js_task_t(env, cb, data);

  context->Enter();

  env->isolate->EnqueueMicrotask(on_microtask, task);

  context->Exit();

  return 0;
}

extern "C" int
js_run_microtasks (js_env_t *env) {
  auto context = to_local(env->context);

  context->Enter();

  env->isolate->PerformMicrotaskCheckpoint();

  context->Exit();

  return 0;
}

extern "C" int
js_queue_macrotask (js_env_t *env, js_task_cb cb, void *data) {
  auto tasks = js_platform->GetForegroundTaskRunner(env->isolate);

  tasks->PostTask(std::make_unique<js_task_t>(env, cb, data));

  return 0;
}

extern "C" int
js_run_macrotasks (js_env_t *env) {
  while (
    platform::PumpMessageLoop(env->platform, env->isolate, platform::MessageLoopBehavior::kDoNotWait)
  ) {
  }

  return 0;
}

extern "C" int
js_request_garbage_collection (js_env_t *env) {
  env->isolate->RequestGarbageCollectionForTesting(Isolate::GarbageCollectionType::kFullGarbageCollection);

  return 0;
}
