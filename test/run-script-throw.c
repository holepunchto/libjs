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
  e = js_create_env(loop, platform, &env);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, "throw 'err'", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, script, &result);
  assert(e == -1);

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  char value[4];
  e = js_get_value_string_utf8(env, error, value, 4, NULL);
  assert(e == 0);

  assert(strcmp(value, "err") == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
