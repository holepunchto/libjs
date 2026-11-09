#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

// A native function of one environment calls into a second environment on the
// same thread and then keeps working in the first. Entering the second
// environment's isolate must leave the first one entered again afterwards.

static js_env_t *b;

static uint32_t
eval_uint32(js_env_t *env, const char *source) {
  int e;

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) source, -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  return value;
}

static js_value_t *
on_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  js_handle_scope_t *scope;
  e = js_open_handle_scope(b, &scope);
  assert(e == 0);

  uint32_t value = eval_uint32(b, "21 * 2");

  e = js_close_handle_scope(b, scope);
  assert(e == 0);

  js_value_t *result;
  e = js_create_uint32(env, value, &result);
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

  js_env_t *a;
  e = js_create_env(loop, platform, NULL, &a);
  assert(e == 0);

  e = js_create_env(loop, platform, NULL, &b);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(a, &scope);
  assert(e == 0);

  js_value_t *fn;
  e = js_create_function(a, "fromB", -1, on_call, NULL, &fn);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(a, &global);
  assert(e == 0);

  e = js_set_named_property(a, global, "fromB", fn);
  assert(e == 0);

  // Each call enters the second environment and returns to the first, which
  // goes on creating values and running JavaScript.
  assert(eval_uint32(a, "let sum = 0; for (let i = 0; i < 1000; i++) sum += fromB(); sum") == 42000);

  e = js_close_handle_scope(a, scope);
  assert(e == 0);

  e = js_destroy_env(b);
  assert(e == 0);

  e = js_destroy_env(a);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
