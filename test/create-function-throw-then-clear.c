#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static bool fn_called = false;

static js_value_t *
on_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  fn_called = true;

  e = js_throw_error(env, NULL, "err");
  assert(e == 0);

  // Clearing the error is meant to leave the callback able to carry on and
  // return normally, so nothing may survive it.

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

  js_value_t *fn;
  e = js_create_function(env, "hello", -1, on_call, NULL, &fn);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  e = js_set_named_property(env, global, "hello", fn);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "hello()", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  assert(fn_called);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  assert(value == 42);

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
