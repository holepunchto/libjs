#include <assert.h>
#include <string.h>
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

  utf16_t input[] = {'h', 'e', 'l', 'l', 'o'};

  js_value_t *string;
  e = js_create_string_utf16le(env, input, 5, &string);
  assert(e == 0);

  size_t len;
  e = js_get_value_string_utf16le(env, string, NULL, 0, &len);
  assert(e == 0);

  assert(len == 5);

  utf16_t output[5];
  e = js_get_value_string_utf16le(env, string, output, 5, &len);
  assert(e == 0);

  assert(len == 5);
  assert(memcmp(output, input, sizeof(input)) == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
