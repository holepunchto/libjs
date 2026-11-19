#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include <js.h>

static bool finalize_called = false;

static void
on_call(js_env_t *env, js_value_t *function, void *context, void *data) {}

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  finalize_called = true;
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

  // The thread holding the function may still call it, which lets go of it.
  e = js_call_threadsafe_function(function, NULL, js_threadsafe_function_nonblocking);
  assert(e == js_closing);
}
