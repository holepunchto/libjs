#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

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

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "new Set()", -1, &script);
  assert(e == 0);

  js_value_t *value;
  e = js_run_script(env, NULL, 0, 0, script, &value);
  assert(e == 0);

  bool result;
  e = js_is_set(env, value, &result);
  assert(e == 0);
  assert(result);

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  e = js_is_set(env, object, &result);
  assert(e == 0);
  assert(!result);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
