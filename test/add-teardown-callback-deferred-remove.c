#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static uv_timer_t timer;

static bool teardown_called = false;

static void
on_teardown(js_deferred_teardown_t *handle, void *data) {
  int e;

  teardown_called = true;
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

  js_deferred_teardown_t *handle;
  e = js_add_deferred_teardown_callback(env, on_teardown, (void *) env, &handle);
  assert(e == 0);

  e = js_finish_deferred_teardown_callback(handle);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  assert(teardown_called == false);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
