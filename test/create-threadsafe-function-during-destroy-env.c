#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include <js.h>

static bool finalize_called = false;

static void
on_call(js_env_t *env, js_value_t *function, void *context, void *data) {}

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  int e;

  finalize_called = true;

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_threadsafe_function_t *function;
  e = js_create_threadsafe_function(env, NULL, 0, 1, NULL, NULL, NULL, on_call, &function);
  assert(e != 0);

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
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

  js_threadsafe_function_t *function;
  e = js_create_threadsafe_function(env, NULL, 0, 1, on_finalize, NULL, NULL, on_call, &function);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  assert(finalize_called);
}
