#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

int uncaught_called = 0;

static void
on_uncaught_exception (js_env_t *env, js_value_t *error, void *data) {
  uncaught_called++;
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
  e = js_create_string_utf8(env, (utf8_t *) "err", -1, &error);
  assert(e == 0);

  e = js_fatal_exception(env, error);
  assert(e == 0);

  assert(uncaught_called == 1);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
