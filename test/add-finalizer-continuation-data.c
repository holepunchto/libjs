#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <uv.h>

#include "../include/js.h"

static js_value_t *key;
static bool finalize_called = false;

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  int e;

  finalize_called = true;

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  // Finalizers run in the empty frame, whatever frame is current when the
  // object is collected.
  js_value_t *result;
  e = js_get_continuation_data(env, key, &result);
  assert(e == 0);

  bool is_undefined;
  e = js_is_undefined(env, result, &is_undefined);
  assert(e == 0);
  assert(is_undefined);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_options_t options = {
    .expose_garbage_collection = true,
  };

  js_platform_t *platform;
  e = js_create_platform(loop, &options, &platform);
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

  js_continuation_scope_t *outer;
  e = js_open_continuation_data_scope(env, key, one, &outer);
  assert(e == 0);

  js_handle_scope_t *inner;
  e = js_open_handle_scope(env, &inner);
  assert(e == 0);

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  e = js_add_finalizer(env, object, NULL, on_finalize, NULL, NULL);
  assert(e == 0);

  e = js_close_handle_scope(env, inner);
  assert(e == 0);

  e = js_request_garbage_collection(env);
  assert(e == 0);

  assert(finalize_called);

  // The frame from before the finalizer ran is restored afterwards.
  js_value_t *result;
  e = js_get_continuation_data(env, key, &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);
  assert(value == 1);

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
