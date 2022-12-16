#include <assert.h>
#include <uv.h>

#include "../include/js.h"

int
main () {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  {
    js_value_t *global;
    e = js_get_global(env, &global);
    assert(e == 0);

    js_value_t *value;
    e = js_create_uint32(env, 42, &value);
    assert(e == 0);

    e = js_set_named_property(env, global, "life", value);
    assert(e == 0);
  }

  js_value_t *script;
  e = js_create_string_utf8(env, "life", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, script, &result);
  assert(e == 0);

  {
    uint32_t value;
    js_get_value_uint32(env, result, &value);
    assert(e == 0);

    assert(value == 42);
  }

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
