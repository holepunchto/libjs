#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include "../include/js.h"

bool fn_called = false;

js_value_t *
on_call (js_env_t *env, js_callback_info_t *info) {
  int e;

  fn_called = true;

  return NULL;
}

int
main () {
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

  js_value_t *fn;
  e = js_create_function(env, "hello", -1, on_call, NULL, &fn);
  assert(e == 0);

  js_threadsafe_function_t *tsfn;
  e = js_create_threadsafe_function(env, fn, 0, 1, NULL, NULL, NULL, NULL, &tsfn);
  assert(e == 0);

  e = js_call_threadsafe_function(tsfn, NULL, js_threadsafe_function_nonblocking);
  assert(e == 0);

  e = js_release_threadsafe_function(tsfn, js_threadsafe_function_release);
  assert(e == 0);

  e = js_call_threadsafe_function(tsfn, NULL, js_threadsafe_function_nonblocking);
  assert(e != 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  assert(fn_called);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
