#include <assert.h>
#include <uv.h>

#include "../include/js.h"

int
main (int argc, char *argv[]) {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_set_flags_from_string("--expose-gc", -1);

  js_platform_t *platform;
  e = js_create_platform(loop, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *value;
  e = js_create_object(env, &value);
  assert(e == 0);

  js_ref_t *ref;
  e = js_create_reference(env, value, 0, &ref);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  js_request_garbage_collection(env);

  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *result;
  e = js_get_reference_value(env, ref, &result);
  assert(e == 0);

  assert(result == NULL);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
