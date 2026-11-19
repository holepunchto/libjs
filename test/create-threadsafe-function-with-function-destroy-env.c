#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include <js.h>

static bool function_called = false;
static int finalize_calls = 0;

static js_value_t *
on_function(js_env_t *env, js_callback_info_t *info) {
  function_called = true;

  return NULL;
}

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  finalize_calls++;
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

  js_value_t *fn;
  e = js_create_function(env, "fn", -1, on_function, NULL, &fn);
  assert(e == 0);

  js_threadsafe_function_t *function;
  e = js_create_threadsafe_function(env, fn, 0, 1, on_finalize, NULL, NULL, NULL, &function);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_call_threadsafe_function(function, NULL, js_threadsafe_function_nonblocking);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  assert(!function_called);
  assert(finalize_calls == 1);
}
