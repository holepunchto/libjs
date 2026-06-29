#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

// Exercises microtask draining for two environments living on the same thread.
// Each environment schedules a microtask that mutates its own global; the
// checkpoint performed when returning from `js_call_function()` must run
// against the correct environment.

static js_handle_scope_t *
setup(js_env_t *env, js_value_t **fn) {
  int e;

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *property;
  e = js_create_uint32(env, 42, &property);
  assert(e == 0);

  e = js_set_named_property(env, global, "value", property);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "function f() { Promise.resolve().then(function () { return value *= 2 }); return value }; f", -1, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, fn);
  assert(e == 0);

  return scope;
}

static uint32_t
call_and_read(js_env_t *env, js_value_t *fn) {
  int e;

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *result;
  e = js_call_function(env, global, fn, 0, NULL, &result);
  assert(e == 0);

  // The synchronous return value is read before the microtask runs.
  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  return value;
}

static uint32_t
read_global(js_env_t *env) {
  int e;

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *property;
  e = js_get_named_property(env, global, "value", &property);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, property, &value);
  assert(e == 0);

  return value;
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *a;
  e = js_create_env(loop, platform, NULL, &a);
  assert(e == 0);

  js_env_t *b;
  e = js_create_env(loop, platform, NULL, &b);
  assert(e == 0);

  js_value_t *fn_a;
  js_handle_scope_t *scope_a = setup(a, &fn_a);

  js_value_t *fn_b;
  js_handle_scope_t *scope_b = setup(b, &fn_b);

  // Interleave the two environments. Each returns the pre-microtask value and
  // then doubles its own global via a microtask checkpoint.
  assert(call_and_read(a, fn_a) == 42);
  assert(call_and_read(b, fn_b) == 42);

  assert(read_global(a) == 84);
  assert(read_global(b) == 84);

  e = js_close_handle_scope(a, scope_a);
  assert(e == 0);

  e = js_close_handle_scope(b, scope_b);
  assert(e == 0);

  e = js_destroy_env(a);
  assert(e == 0);

  e = js_destroy_env(b);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
