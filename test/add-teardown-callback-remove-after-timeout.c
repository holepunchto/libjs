#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static uv_timer_t timer;

static bool teardown_called = false;

static void
on_teardown (js_env_t *env, void *data);

static void
on_timer (uv_timer_t *timer) {
  int e;

  js_env_t *env = (js_env_t *) timer->data;

  e = js_remove_teardown_callback(env, on_teardown, NULL);
  assert(e == 0);

  e = js_unref_env(env);
  assert(e == 0);
}

static void
on_teardown (js_env_t *env, void *data) {
  int e;

  teardown_called = true;

  uv_loop_t *loop;
  e = js_get_env_loop(env, &loop);
  assert(e == 0);

  e = uv_timer_init(loop, &timer);
  assert(e == 0);

  timer.data = (void *) env;

  e = uv_timer_start(&timer, on_timer, 1000, 0);
  assert(e == 0);

  e = js_ref_env(env);
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
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  e = js_add_teardown_callback(env, on_teardown, NULL);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  assert(teardown_called);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
