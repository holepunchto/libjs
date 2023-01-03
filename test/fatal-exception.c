#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include "../include/js.h"

bool uncaught_called = false;

static void
on_uncaught_exception (js_env_t *env, js_value_t *error, void *data) {
  uncaught_called = true;
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

  js_value_t *error;
  e = js_create_string_utf8(env, "err", -1, &error);
  assert(e == 0);

  e = js_fatal_exception(env, error);
  assert(e == 0);

  assert(uncaught_called);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
