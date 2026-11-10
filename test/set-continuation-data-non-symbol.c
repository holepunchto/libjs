#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include "../include/js.h"

static void
expect_failure(js_env_t *env, js_value_t *key, const char *expected) {
  int e;

  js_value_t *one;
  e = js_create_uint32(env, 1, &one);
  assert(e == 0);

  e = js_set_continuation_data(env, key, one);
  assert(e != 0);

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  js_value_t *message;
  e = js_get_named_property(env, error, "message", &message);
  assert(e == 0);

  utf8_t text[256];
  e = js_get_value_string_utf8(env, message, text, sizeof(text), NULL);
  assert(e == 0);

  assert(strstr((char *) text, expected) != NULL);
}

static void
expect_success(js_env_t *env, js_value_t *symbol) {
  int e;

  js_value_t *one;
  e = js_create_uint32(env, 1, &one);
  assert(e == 0);

  e = js_set_continuation_data(env, symbol, one);
  assert(e == 0);

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);
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

  js_value_t *failingKey;
  e = js_create_string_utf8(env, (utf8_t *) "not a symbol", -1, &failingKey);

  js_value_t *
    undefined;
  e = js_get_undefined(env, &undefined);
  assert(e == 0);

  // Attempting to set without a symbol fails
  expect_failure(env, failingKey, "keyed by a symbol");

  // Attempting to set with a symbol succeeds
  expect_success(env, a);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
