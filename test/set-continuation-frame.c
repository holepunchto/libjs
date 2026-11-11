#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include "../include/js.h"

static void
expect_uint32(js_env_t *env, js_value_t *symbol, uint32_t expected) {
  int e;

  js_value_t *result;
  e = js_get_continuation_data(env, symbol, &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  assert(value == expected);
}

static void
expect_undefined(js_env_t *env, js_value_t *symbol) {
  int e;

  js_value_t *result;
  e = js_get_continuation_data(env, symbol, &result);
  assert(e == 0);

  bool is_undefined;
  e = js_is_undefined(env, result, &is_undefined);
  assert(e == 0);

  assert(is_undefined);
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

  js_value_t *key;
  e = js_create_symbol(env, NULL, &key);
  assert(e == 0);

  js_value_t *one;
  e = js_create_uint32(env, 1, &one);
  assert(e == 0);

  js_value_t *two;
  e = js_create_uint32(env, 2, &two);
  assert(e == 0);

  e = js_set_continuation_data(env, key, one);
  assert(e == 0);

  js_value_t *captured;
  e = js_get_continuation_frame(env, &captured);
  assert(e == 0);

  e = js_set_continuation_data(env, key, two);
  assert(e == 0);

  // Restoring the captured frame brings back the data it held.
  js_value_t *previous;
  e = js_set_continuation_frame(env, captured, &previous);
  assert(e == 0);

  expect_uint32(env, key, 1);

  // The replaced frame can be restored in turn.
  e = js_set_continuation_frame(env, previous, NULL);
  assert(e == 0);

  expect_uint32(env, key, 2);

  // The empty frame holds no data.
  e = js_set_continuation_frame(env, NULL, &previous);
  assert(e == 0);

  expect_undefined(env, key);

  // A frame taken while empty can be restored like any other.
  js_value_t *empty;
  e = js_get_continuation_frame(env, &empty);
  assert(e == 0);

  e = js_set_continuation_frame(env, previous, NULL);
  assert(e == 0);

  expect_uint32(env, key, 2);

  e = js_set_continuation_frame(env, empty, NULL);
  assert(e == 0);

  expect_undefined(env, key);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
