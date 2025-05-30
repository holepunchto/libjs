#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

js_value_t *
on_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "() => { Promise.resolve().then(() => value *= 2); return value }", -1, &script);
  assert(e == 0);

  js_value_t *fn;
  e = js_run_script(env, NULL, 0, 0, script, &fn);
  assert(e == 0);

  js_value_t *result;
  e = js_call_function(env, global, fn, 0, NULL, &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  assert(value == 42);

  // The call to `js_call_function()` happened with JavaScript already executing
  // on the stack and so no microtask checkpoint must be performed.

  {
    js_value_t *property;
    e = js_get_named_property(env, global, "value", &property);
    assert(e == 0);

    uint32_t value;
    e = js_get_value_uint32(env, property, &value);
    assert(e == 0);

    assert(value == 42);
  }

  return NULL;
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

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  {
    js_value_t *property;
    e = js_create_uint32(env, 42, &property);
    assert(e == 0);

    e = js_set_named_property(env, global, "value", property);
    assert(e == 0);
  }

  js_value_t *fn;
  e = js_create_function(env, "fn", -1, on_call, NULL, &fn);
  assert(e == 0);

  e = js_set_named_property(env, global, "fn", fn);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "fn()", -1, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, NULL);
  assert(e == 0);

  // The call to `js_run_script()` happened without JavaScript already executing
  // on the stack and so a microtask checkpoint was performed before returning.

  {
    js_value_t *property;
    e = js_get_named_property(env, global, "value", &property);
    assert(e == 0);

    uint32_t value;
    e = js_get_value_uint32(env, property, &value);
    assert(e == 0);

    assert(value == 84);
  }

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
