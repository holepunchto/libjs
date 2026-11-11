#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <uv.h>

#include "../include/js.h"

static js_value_t *key;
static js_deferred_t *deferred;
static uint32_t seen = 0;

static js_continuation_scope_t *
open_variable_scope(js_env_t *env, uint32_t value) {
  int e;

  js_value_t *local;
  e = js_create_uint32(env, value, &local);
  assert(e == 0);

  js_continuation_scope_t *scope;
  e = js_open_continuation_variable_scope(env, key, local, &scope);
  assert(e == 0);

  return scope;
}

static uint32_t
get_variable(js_env_t *env) {
  int e;

  js_value_t *result;
  e = js_get_continuation_variable(env, key, &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  return value;
}

static void
on_unhandled_rejection(js_env_t *env, js_value_t *reason, js_value_t *promise, void *data) {
  seen = get_variable(env);
}

static js_value_t *
on_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  js_continuation_scope_t *scope = open_variable_scope(env, 1);

  js_value_t *undefined;
  e = js_get_undefined(env, &undefined);
  assert(e == 0);

  e = js_reject_deferred(env, deferred, undefined);
  assert(e == 0);

  // The rejection is reported at the checkpoint after this call returns, by
  // which time the scope has been closed.
  e = js_close_continuation_scope(env, scope);
  assert(e == 0);

  return NULL;
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  e = js_on_unhandled_rejection(env, on_unhandled_rejection, NULL);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  e = js_create_symbol(env, NULL, &key);
  assert(e == 0);

  js_value_t *promise;
  e = js_create_promise(env, &deferred, &promise);
  assert(e == 0);

  js_value_t *fn;
  e = js_create_function(env, "fn", -1, on_call, NULL, &fn);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_continuation_scope_t *outer = open_variable_scope(env, 2);

  e = js_call_function(env, global, fn, 0, NULL, NULL);
  assert(e == 0);

  // The handler runs with the snapshot from when the promise was rejected,
  // and the snapshot from before it ran is restored afterwards.
  assert(seen == 1);
  assert(get_variable(env) == 2);

  e = js_close_continuation_scope(env, outer);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
