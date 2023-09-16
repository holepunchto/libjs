#include <assert.h>
#include <utf.h>
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

  js_value_t *description;
  e = js_create_string_utf8(env, (utf8_t *) "hello world", -1, &description);
  assert(e == 0);

  js_value_t *value;
  e = js_create_symbol(env, description, &value);
  assert(e == 0);

  js_ref_t *ref;
  e = js_create_reference(env, value, 1, &ref);
  assert(e == 0);

  js_value_t *result;
  e = js_get_reference_value(env, ref, &result);
  assert(e == 0);

  assert(result != NULL);

  bool is_symbol;
  e = js_is_symbol(env, result, &is_symbol);
  assert(e == 0);

  assert(is_symbol);

  e = js_delete_reference(env, ref);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
