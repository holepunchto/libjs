#include <assert.h>
#include <string.h>
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

  js_value_t *number;
  e = js_create_uint32(env, 42, &number);
  assert(e == 0);

  js_value_t *string;
  e = js_coerce_to_string(env, number, &string);
  assert(e == 0);

  uint8_t value[3];
  e = js_get_value_string_utf8(env, string, value, 3, NULL);
  assert(e == 0);

  assert(strcmp((char *) value, "42") == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
