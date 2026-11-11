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
  e = js_create_string_utf8(env, (utf8_t *) "not a symbol", -1, &key);
  assert(e == 0);

  js_value_t *one;
  e = js_create_uint32(env, 1, &one);
  assert(e == 0);

  e = js_set_continuation_data(env, key, one);
  assert(e == js_pending_exception);

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  js_value_t *message;
  e = js_get_named_property(env, error, "message", &message);
  assert(e == 0);

  utf8_t text[256];
  e = js_get_value_string_utf8(env, message, text, sizeof(text), NULL);
  assert(e == 0);
  assert(strstr((char *) text, "keyed by a symbol") != NULL);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
