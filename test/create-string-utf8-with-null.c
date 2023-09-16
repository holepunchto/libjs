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

  js_value_t *string;
  e = js_create_string_utf8(env, (utf8_t *) "hello\0world", 11, &string);
  assert(e == 0);

  size_t len;
  e = js_get_value_string_utf8(env, string, NULL, 0, &len);
  assert(e == 0);

  assert(len >= 11);

  utf8_t value[11];
  e = js_get_value_string_utf8(env, string, value, len, &len);
  assert(e == 0);

  assert(len == 11);

  assert(memcmp(value, "hello\0world", len) == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
