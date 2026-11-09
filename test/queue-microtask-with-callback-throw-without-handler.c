#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static void
on_microtask(js_env_t *env, void *data) {
  int e;

  e = js_throw_error(env, NULL, "native");
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

  e = js_queue_microtask_with_callback(env, on_microtask, NULL);
  assert(e == 0);

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  js_value_t *message;
  e = js_get_named_property(env, error, "message", &message);
  assert(e == 0);

  utf8_t value[8];
  e = js_get_value_string_utf8(env, message, value, 8, NULL);
  assert(e == 0);

  assert(strcmp((char *) value, "native") == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
