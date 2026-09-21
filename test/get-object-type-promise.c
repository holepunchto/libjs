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

  js_deferred_t *deferred;
  js_value_t *promise;
  e = js_create_promise(env, &deferred, &promise);
  assert(e == 0);

  js_object_type_t type;
  e = js_get_object_type(env, promise, &type);
  assert(e == 0);

  assert(type == js_promise);

  bool is_promise;
  e = js_is_promise(env, promise, &is_promise);
  assert(e == 0);

  assert(is_promise);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
