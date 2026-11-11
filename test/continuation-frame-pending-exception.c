#include <assert.h>
#include <string.h>
#include <uv.h>

#include "../include/js.h"

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

  // Leave an exception pending, as if a callback called from an I/O handler
  // had thrown before the handler restored its frame.
  e = js_throw_error(env, NULL, "boom");
  assert(e == 0);

  js_value_t *previous;
  e = js_set_continuation_frame(env, captured, &previous);
  assert(e == 0);

  // The original exception is still the pending one.
  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  js_value_t *message;
  e = js_get_named_property(env, error, "message", &message);
  assert(e == 0);

  utf8_t text[16];
  e = js_get_value_string_utf8(env, message, text, sizeof(text), NULL);
  assert(e == 0);
  assert(strcmp((char *) text, "boom") == 0);

  // And the restore took effect.
  js_value_t *result;
  e = js_get_continuation_data(env, key, &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);
  assert(value == 1);

  // The frame replaced while the exception was pending can be restored too.
  e = js_set_continuation_frame(env, previous, NULL);
  assert(e == 0);

  e = js_get_continuation_data(env, key, &result);
  assert(e == 0);

  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);
  assert(value == 2);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
