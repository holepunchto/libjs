#include <assert.h>
#include <stdint.h>
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

  js_value_t *narrow;
  e = js_create_int32(env, -7, &narrow);
  assert(e == 0);

  int32_t i32;
  e = js_get_value_int32(env, narrow, &i32);
  assert(e == 0);

  assert(i32 == -7);

  int64_t i64;
  e = js_get_value_int64(env, narrow, &i64);
  assert(e == 0);

  assert(i64 == -7);

  js_value_t *big;
  e = js_create_double(env, 1e10, &big);
  assert(e == 0);

  e = js_get_value_int64(env, big, &i64);
  assert(e == 0);

  assert(i64 == (int64_t) 1e10);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
