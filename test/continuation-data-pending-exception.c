#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <uv.h>

#include "../include/js.h"

static js_value_t *key;
static js_value_t *one;
static js_value_t *two;

static void
expect_uint32(js_env_t *env, uint32_t expected) {
  int e;

  js_value_t *result;
  e = js_get_continuation_data(env, key, &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  assert(value == expected);
}

static void
expect_pending(js_env_t *env, const char *expected) {
  int e;

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  js_value_t *message;
  e = js_get_named_property(env, error, "message", &message);
  assert(e == 0);

  utf8_t text[16];
  e = js_get_value_string_utf8(env, message, text, sizeof(text), NULL);
  assert(e == 0);
  assert(strcmp((char *) text, expected) == 0);
}

static void
set_and_get(js_env_t *env) {
  int e;

  e = js_set_continuation_data(env, key, one);
  assert(e == 0);

  expect_uint32(env, 1);

  e = js_set_continuation_data(env, key, two);
  assert(e == 0);

  expect_uint32(env, 2);
}

static js_value_t *
on_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  // Within a call from JavaScript, the engine itself holds the exception.
  e = js_throw_error(env, NULL, "nested");
  assert(e == 0);

  set_and_get(env);

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

  e = js_create_uint32(env, 1, &one);
  assert(e == 0);

  e = js_create_uint32(env, 2, &two);
  assert(e == 0);

  e = js_throw_error(env, NULL, "boom");
  assert(e == 0);

  set_and_get(env);

  expect_pending(env, "boom");

  js_value_t *fn;
  e = js_create_function(env, "fn", -1, on_call, NULL, &fn);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  e = js_call_function(env, global, fn, 0, NULL, NULL);
  assert(e == js_pending_exception);

  expect_pending(env, "nested");

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
