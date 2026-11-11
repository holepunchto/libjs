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

  js_value_t *a;
  e = js_create_symbol(env, NULL, &a);
  assert(e == 0);

  js_value_t *b;
  e = js_create_symbol(env, NULL, &b);
  assert(e == 0);

  js_value_t *one;
  e = js_create_uint32(env, 1, &one);
  assert(e == 0);

  js_value_t *two;
  e = js_create_uint32(env, 2, &two);
  assert(e == 0);

  js_value_t *undefined;
  e = js_get_undefined(env, &undefined);
  assert(e == 0);

  // Nothing has been stored yet.
  expect_undefined(env, a);
  expect_undefined(env, b);

  // Storing under one symbol is only visible under that symbol.
  js_continuation_scope_t *first;
  e = js_open_continuation_data_scope(env, a, one, &first);
  assert(e == 0);

  expect_uint32(env, a, 1);
  expect_undefined(env, b);

  // Storing under a second symbol leaves the first one alone.
  js_continuation_scope_t *second;
  e = js_open_continuation_data_scope(env, b, two, &second);
  assert(e == 0);

  expect_uint32(env, a, 1);
  expect_uint32(env, b, 2);

  // Storing undefined removes the entry, again only for that symbol.
  js_continuation_scope_t *third;
  e = js_open_continuation_data_scope(env, a, undefined, &third);
  assert(e == 0);

  expect_undefined(env, a);
  expect_uint32(env, b, 2);

  // Closing each scope restores the data from before it was opened.
  e = js_close_continuation_scope(env, third);
  assert(e == 0);

  expect_uint32(env, a, 1);
  expect_uint32(env, b, 2);

  e = js_close_continuation_scope(env, second);
  assert(e == 0);

  expect_uint32(env, a, 1);
  expect_undefined(env, b);

  e = js_close_continuation_scope(env, first);
  assert(e == 0);

  expect_undefined(env, a);
  expect_undefined(env, b);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
