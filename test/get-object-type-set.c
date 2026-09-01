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

  js_value_t *set;
  e = js_run_script(env, NULL, 0, 0, script, &set);
  assert(e == 0);

  js_object_type_t type;
  e = js_get_object_type(env, set, &type);
  assert(e == 0);

  assert(type == js_set);

  bool is_set;
  e = js_is_set(env, set, &is_set);
  assert(e == 0);

  assert(is_set);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
