#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include "../include/js.h"

static js_value_t *key;
static uint32_t seen = 0;

static js_value_t *
on_fulfilled(js_env_t *env, js_callback_info_t *info) {
  int e;

  js_value_t *result;
  e = js_get_continuation_data(env, key, &result);
  assert(e == 0);

  e = js_get_value_uint32(env, result, &seen);
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

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  e = js_create_symbol(env, NULL, &key);
  assert(e == 0);

  js_value_t *one;
  e = js_create_uint32(env, 1, &one);
  assert(e == 0);

  js_value_t *two;
  e = js_create_uint32(env, 2, &two);
  assert(e == 0);

  e = js_set_continuation_data(env, key, two);
  assert(e == 0);

  // create a promise and deferred callback
  js_deferred_t *deferred;
  js_value_t *promise;
  e = js_create_promise(env, &deferred, &promise);
  assert(e == 0);

  js_value_t *callback;
  e = js_create_function(env, "onFulfilled", -1, on_fulfilled, NULL, &callback);
  assert(e == 0);

  js_value_t *then;
  e = js_get_named_property(env, promise, "then", &then);
  assert(e == 0);

  // callback is registered on promise resolution
  e = js_call_function(env, promise, then, 1, &callback, NULL);
  assert(e == 0);

  // alter the slot data
  e = js_set_continuation_data(env, key, one);
  assert(e == 0);

  js_value_t *undefined;
  e = js_get_undefined(env, &undefined);
  assert(e == 0);

  // resolving the promise runs the callback with the old slot data
  e = js_resolve_deferred(env, deferred, undefined);
  assert(e == 0);

  assert(seen == 2);

  // after the callback runs the old slot data is restored
  js_value_t *curr;
  e = js_get_continuation_data(env, key, &curr);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, curr, &value);
  assert(e == 0);
  assert(value == 1);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
