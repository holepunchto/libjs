#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"
#include "helpers.h"

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_platform_limits_t limits = {.version = 0};
  e = js_get_platform_limits(platform, &limits);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  if (limits.arraybuffer_length < SIZE_MAX) {
    js_value_t *arraybuffer;
    e = js_create_arraybuffer(env, limits.arraybuffer_length + 1, NULL, &arraybuffer);
    assert(e != 0);

    js_print_pending_exception(env);
  }

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
