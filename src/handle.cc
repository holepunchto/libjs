#include <v8.h>

#include "../include/js.h"
#include "types.h"

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
