#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static int unhandled_called = 0;

static void
on_unhandled_rejection(js_env_t *env, js_value_t *reason, js_value_t *promise, void *data) {
  int e;

  unhandled_called++;

  e = js_throw_error(env, NULL, "err");
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

  e = js_on_unhandled_rejection(env, on_unhandled_rejection, NULL);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "Promise.reject(1); Promise.reject(2)", -1, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, NULL);
  assert(e == 0);

  // The error thrown by the handler leaves the second rejection unreported,
  // which must not outlive the environment.

  assert(unhandled_called == 1);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
