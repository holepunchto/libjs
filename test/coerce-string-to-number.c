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

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *string;
  e = js_create_string_utf8(env, (utf8_t *) "42", -1, &string);
  assert(e == 0);

  js_value_t *number;
  e = js_coerce_to_number(env, string, &number);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, number, &value);
  assert(e == 0);

  assert(value == 42);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
