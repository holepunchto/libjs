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
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *value;
  e = js_create_object(env, &value);
  assert(e == 0);

  js_ref_t *ref1;
  e = js_create_reference(env, value, 1, &ref1);
  assert(e == 0);

  js_ref_t *ref2;
  e = js_create_reference(env, value, 1, &ref2);
  assert(e == 0);

  e = js_delete_reference(env, ref1);
  assert(e == 0);

  e = js_delete_reference(env, ref2);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
