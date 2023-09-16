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

  js_value_t *value;
  e = js_create_bigint_uint64(env, 42, &value);
  assert(e == 0);

  js_ref_t *ref;
  e = js_create_reference(env, value, 1, &ref);
  assert(e == 0);

  js_value_t *result;
  e = js_get_reference_value(env, ref, &result);
  assert(e == 0);

  assert(result != NULL);

  bool is_bigint;
  e = js_is_bigint(env, result, &is_bigint);
  assert(e == 0);

  assert(is_bigint);

  e = js_delete_reference(env, ref);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
