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

  js_value_t *bindings;
  e = js_get_bindings(env, &bindings);
  assert(e == 0);

  bool is_object;
  e = js_is_object(env, bindings, &is_object);
  assert(e == 0);

  assert(is_object);

  js_value_t *value;
  e = js_create_uint32(env, 7, &value);
  assert(e == 0);

  e = js_set_named_property(env, bindings, "marker", value);
  assert(e == 0);

  js_value_t *bindings_again;
  e = js_get_bindings(env, &bindings_again);
  assert(e == 0);

  js_value_t *marker;
  e = js_get_named_property(env, bindings_again, "marker", &marker);
  assert(e == 0);

  uint32_t n;
  e = js_get_value_uint32(env, marker, &n);
  assert(e == 0);

  assert(n == 7);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
