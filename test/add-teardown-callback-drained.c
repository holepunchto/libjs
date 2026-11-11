#include <assert.h>
#include <uv.h>

#include "../include/js.h"

static int teardown_calls = 0;

static void
on_late_teardown(void *data) {
  teardown_calls++;
}

static void
on_late_deferred_teardown(js_deferred_teardown_t *handle, void *data) {
  int e;

  teardown_calls++;

  e = js_finish_deferred_teardown_callback(handle);
  assert(e == 0);
}

static void
on_teardown(void *data) {
  int e;

  js_env_t *env = data;

  teardown_calls++;

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *exception;

  e = js_add_teardown_callback(env, on_late_teardown, NULL);
  assert(e != 0);

  e = js_get_and_clear_last_exception(env, &exception);
  assert(e == 0);

  e = js_add_deferred_teardown_callback(env, on_late_deferred_teardown, NULL, NULL);
  assert(e != 0);

  e = js_get_and_clear_last_exception(env, &exception);
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

  e = js_add_teardown_callback(env, on_teardown, (void *) env);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  assert(teardown_calls == 1);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
