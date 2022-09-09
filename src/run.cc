#include <v8.h>

#include "../include/js.h"
#include "types.hh"

using v8::Context;
using v8::Local;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::Value;

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
