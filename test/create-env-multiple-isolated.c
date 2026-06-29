#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

// Verifies that two environments living on the same thread keep entirely
// separate state. A global defined in one environment must not be visible in
// the other.

static void
set_global(js_env_t *env, uint32_t value) {
  int e;

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *property;
  e = js_create_uint32(env, value, &property);
  assert(e == 0);

  e = js_set_named_property(env, global, "value", property);
  assert(e == 0);
}

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

  js_handle_scope_t *scope_a;
  e = js_open_handle_scope(a, &scope_a);
  assert(e == 0);

  js_handle_scope_t *scope_b;
  e = js_open_handle_scope(b, &scope_b);
  assert(e == 0);

  set_global(a, 1);
  set_global(b, 2);

  // Each environment sees only its own global.
  assert(eval_uint32(a, "value") == 1);
  assert(eval_uint32(b, "value") == 2);

  // Mutating one environment's global leaves the other untouched.
  assert(eval_uint32(a, "value = 100") == 100);
  assert(eval_uint32(b, "value") == 2);

  // Defining a fresh global in one environment must not leak into the other.
  assert(eval_uint32(a, "globalThis.only_a = 7, only_a") == 7);
  assert(eval_uint32(b, "typeof only_a === 'undefined' ? 0 : 1") == 0);

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
