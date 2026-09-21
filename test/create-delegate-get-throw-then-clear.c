#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static int get_called = 0;

static js_value_t *
get(js_env_t *env, js_value_t *property, void *data) {
  int e;

  get_called++;

  e = js_throw_error(env, NULL, "nope");
  assert(e == 0);

  // Clearing the error is meant to leave the interceptor able to carry on and
  // answer normally, so nothing may survive it.

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  bool has_exception;
  e = js_is_exception_pending(env, &has_exception);
  assert(e == 0);

  assert(!has_exception);

  js_value_t *result;
  e = js_create_uint32(env, 42, &result);
  assert(e == 0);

  return result;
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

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_delegate_callbacks_t callbacks = {
    .get = get,
  };

  js_value_t *delegate;
  e = js_create_delegate(env, &callbacks, NULL, NULL, NULL, &delegate);
  assert(e == 0);

  js_value_t *value;
  e = js_get_named_property(env, delegate, "foo", &value);
  assert(e == 0);

  assert(get_called);

  uint32_t number;
  e = js_get_value_uint32(env, value, &number);
  assert(e == 0);

  assert(number == 42);

  bool has_exception;
  e = js_is_exception_pending(env, &has_exception);
  assert(e == 0);

  assert(!has_exception);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
