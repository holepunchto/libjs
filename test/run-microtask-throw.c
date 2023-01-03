#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include "../include/js.h"

bool uncaught_called = false;
bool fn_called = false;

static void
on_uncaught_exception (js_env_t *env, js_value_t *error, void *data) {
  uncaught_called = true;
}

static void
on_call (js_env_t *env, void *data) {
  int e;

  fn_called = true;

  js_value_t *err;
  e = js_create_string_utf8(env, "error", -1, &err);
  assert(e == 0);

  e = js_throw(env, err);
  assert(e == 0);
}

int
main () {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, &env);
  assert(e == 0);

  e = js_on_uncaught_exception(env, on_uncaught_exception, NULL);
  assert(e == 0);

  e = js_queue_microtask(env, on_call, NULL);
  assert(e == 0);

  assert(!fn_called);

  uv_run(loop, UV_RUN_DEFAULT);

  assert(fn_called);
  assert(uncaught_called);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
