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

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  js_value_t *own_key;
  e = js_create_string_utf8(env, (utf8_t *) "own", -1, &own_key);
  assert(e == 0);

  js_value_t *one;
  e = js_create_uint32(env, 1, &one);
  assert(e == 0);

  e = js_set_property(env, object, own_key, one);
  assert(e == 0);

  bool has;
  e = js_has_own_property(env, object, own_key, &has);
  assert(e == 0);

  assert(has);

  js_value_t *inherited_key;
  e = js_create_string_utf8(env, (utf8_t *) "toString", -1, &inherited_key);
  assert(e == 0);

  e = js_has_own_property(env, object, inherited_key, &has);
  assert(e == 0);

  assert(!has);

  e = js_has_property(env, object, inherited_key, &has);
  assert(e == 0);

  assert(has);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
