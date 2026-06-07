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

  e = js_throw_type_error(env, "code", "type error");
  assert(e == 0);

  bool pending;
  e = js_is_exception_pending(env, &pending);
  assert(e == 0);

  assert(pending);

  js_value_t *exception;
  e = js_get_and_clear_last_exception(env, &exception);
  assert(e == 0);

  bool is_error;
  e = js_is_error(env, exception, &is_error);
  assert(e == 0);

  assert(is_error);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
