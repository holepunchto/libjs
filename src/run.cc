#include <v8.h>

#include "../include/js.h"
#include "types.h"

using v8::Context;
using v8::Local;
using v8::Script;
using v8::String;
using v8::Value;

extern "C" int
js_run_script (js_env_t *env, js_value_t *script, js_value_t **result) {
  auto local = *reinterpret_cast<Local<Value> *>(&script);

  auto context = *reinterpret_cast<Local<Context> *>(&env->context);

  auto compiled = Script::Compile(context, local.As<String>());

  *result = reinterpret_cast<js_value_t *>(*compiled.ToLocalChecked()->Run(context).ToLocalChecked());

  return 0;
}
