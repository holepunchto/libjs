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

  js_value_t *string;
  e = js_create_string_utf8(env, (utf8_t *) "hello", -1, &string);
  assert(e == 0);

  utf8_t value[6];
  size_t written;
  e = js_get_value_string_utf8(env, string, value, 6, &written);
  assert(e == 0);

  assert(strcmp((char *) value, "hello") == 0);
  assert(value[5] == '\0');
  assert(written == 5);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
