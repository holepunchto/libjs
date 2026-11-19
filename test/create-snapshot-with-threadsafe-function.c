#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <uv.h>

#include "../include/js.h"

static bool finalize_called = false;

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  finalize_called = true;
}

static void
on_call(js_env_t *env, js_value_t *function, void *context, void *data) {}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_snapshot(loop, platform, NULL, &env);
  assert(e == 0);

  js_threadsafe_function_t *threadsafe_function;
  e = js_create_threadsafe_function(env, NULL, 0, 1, on_finalize, NULL, NULL, on_call, &threadsafe_function);
  assert(e == 0);

  void *data;
  size_t len;
  e = js_take_snapshot(env, &data, &len);
  assert(e == 0);

  free(data);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  assert(finalize_called);
}
