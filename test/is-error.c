#include <assert.h>
#include <utf.h>
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

  js_value_t *message;
  e = js_create_string_utf8(env, (utf8_t *) "boom", -1, &message);
  assert(e == 0);

  js_value_t *error;
  e = js_create_error(env, NULL, message, &error);
  assert(e == 0);

  bool is_error;
  e = js_is_error(env, error, &is_error);
  assert(e == 0);

  assert(is_error);

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  e = js_is_error(env, object, &is_error);
  assert(e == 0);

  assert(!is_error);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
