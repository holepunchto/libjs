#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static int teardown_called = 0;

static void
on_teardown (void *data) {
  teardown_called++;

  assert((intptr_t) data == teardown_called);
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

  e = js_add_teardown_callback(env, on_teardown, (void *) 3);
  assert(e == 0);

  e = js_add_teardown_callback(env, on_teardown, (void *) 2);
  assert(e == 0);

  e = js_add_teardown_callback(env, on_teardown, (void *) 1);
  assert(e == 0);

  e = js_remove_teardown_callback(env, on_teardown, (void *) 3);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  assert(teardown_called == 2);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
