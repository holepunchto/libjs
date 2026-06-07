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

  js_value_t *prototype;
  e = js_get_prototype(env, object, &prototype);
  assert(e == 0);

  bool is_object;
  e = js_is_object(env, prototype, &is_object);
  assert(e == 0);

  assert(is_object);

  js_value_t *to_string;
  e = js_get_named_property(env, prototype, "toString", &to_string);
  assert(e == 0);

  bool is_function;
  e = js_is_function(env, to_string, &is_function);
  assert(e == 0);

  assert(is_function);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
