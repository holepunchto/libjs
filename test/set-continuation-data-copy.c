#include <assert.h>
#include <stdbool.h>
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

  js_value_t *a;
  e = js_create_symbol(env, NULL, &a);
  assert(e == 0);

  js_value_t *b;
  e = js_create_symbol(env, NULL, &b);
  assert(e == 0);

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  js_value_t *two;
  e = js_create_uint32(env, 2, &two);
  assert(e == 0);

  // set object in symbol slot
  e = js_set_continuation_data(env, a, object);
  assert(e == 0);

  // force rebuild of the entire stored map
  e = js_set_continuation_data(env, b, two);
  assert(e == 0);

  js_value_t *newObject;
  e = js_get_continuation_data(env, a, &newObject);

  bool isEqual;
  e = js_strict_equals(env, object, newObject, &isEqual);
  assert(e == 0);
  assert(isEqual);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
