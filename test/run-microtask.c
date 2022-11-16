#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include "../include/js.h"

bool fn_called = false;

static void
on_call (js_env_t *env, void *data) {
  fn_called = true;
}

int
main (int argc, char *argv[]) {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_platform_init(loop, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_env_init(loop, platform, &env);
  assert(e == 0);

  e = js_queue_microtask(env, on_call, NULL);
  assert(e == 0);

  assert(!fn_called);

  uv_run(loop, UV_RUN_DEFAULT);

  assert(fn_called);

  e = js_env_destroy(env);
  assert(e == 0);

  e = js_platform_destroy(platform);
  assert(e == 0);
}
