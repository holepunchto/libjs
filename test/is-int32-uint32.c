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

  js_value_t *neg;
  e = js_create_int32(env, -1, &neg);
  assert(e == 0);

  js_value_t *pos;
  e = js_create_uint32(env, 1, &pos);
  assert(e == 0);

  js_value_t *dbl;
  e = js_create_double(env, 1.5, &dbl);
  assert(e == 0);

  bool result;

  e = js_is_int32(env, neg, &result);
  assert(e == 0 && result);

  e = js_is_int32(env, pos, &result);
  assert(e == 0 && result);

  e = js_is_int32(env, dbl, &result);
  assert(e == 0 && !result);

  e = js_is_uint32(env, pos, &result);
  assert(e == 0 && result);

  e = js_is_uint32(env, neg, &result);
  assert(e == 0 && !result);

  e = js_is_uint32(env, dbl, &result);
  assert(e == 0 && !result);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
