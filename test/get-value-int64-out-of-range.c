#include <assert.h>
#include <stdint.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static void
check(js_env_t *env, double value, int64_t expected) {
  int e;

  js_value_t *number;
  e = js_create_double(env, value, &number);
  assert(e == 0);

  int64_t result;
  e = js_get_value_int64(env, number, &result);
  assert(e == 0);

  assert(result == expected);
}

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

  // Anything that fits converts as it always did, truncating towards zero.
  check(env, 0, 0);
  check(env, -0.0, 0);
  check(env, 7.9, 7);
  check(env, -7.9, -7);
  check(env, 9007199254740991.0, INT64_C(9007199254740991));
  check(env, -9007199254740991.0, INT64_C(-9007199254740991));

  // The ends of the range are representable exactly as doubles either side, so
  // the boundary itself has to land on the boundary.
  check(env, -9223372036854775808.0, INT64_MIN);

  // Everything below is what a cast leaves undefined.
  check(env, 0.0 / 0.0, 0);
  check(env, 1.0 / 0.0, 0);
  check(env, -1.0 / 0.0, 0);

  check(env, 9223372036854775808.0, INT64_MAX);
  check(env, -9223372036854775809.0, INT64_MIN);
  check(env, 1e300, INT64_MAX);
  check(env, -1e300, INT64_MIN);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
