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

  js_value_t *script;
  e = js_create_string_utf8(env, "Symbol('foo')", -1, &script);
  assert(e == 0);

  js_value_t *value;
  e = js_run_script(env, script, &value);
  assert(e == 0);

  js_value_type_t type;
  e = js_typeof(env, value, &type);
  assert(e == 0);

  assert(type == js_symbol);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}