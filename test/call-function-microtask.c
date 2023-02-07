#include <assert.h>
#include <uv.h>

#include "../include/js.h"

int
main () {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, &env);
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

  js_value_t *script;
  e = js_create_string_utf8(env, "() => { Promise.resolve().then(() => value *= 2); return value }", -1, &script);
  assert(e == 0);

  js_value_t *fn;
  e = js_run_script(env, NULL, 0, script, &fn);
  assert(e == 0);

  js_value_t *result;
  e = js_call_function(env, global, fn, 0, NULL, &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  assert(value == 42);

  // The call to `js_call_function()` happened without JavaScript already
  // executing on the stack and so a microtask checkpoint was performed before
  // returning.

  {
    js_value_t *property;
    e = js_get_named_property(env, global, "value", &property);
    assert(e == 0);

    uint32_t value;
    e = js_get_value_uint32(env, property, &value);
    assert(e == 0);

    assert(value == 84);
  }

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
