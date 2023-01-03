#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include "../include/js.h"

bool fn_called = false;

js_value_t *
on_call (js_env_t *env, js_callback_info_t *info) {
  int e;

  fn_called = true;

  js_value_t *script;
  e = js_create_string_utf8(env, "throw 'err'", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, script, &result);
  assert(e == -1);

  return NULL;
}

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

  js_value_t *fn;
  e = js_create_function(env, "hello", -1, on_call, NULL, &fn);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  e = js_set_named_property(env, global, "hello", fn);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, "hello()", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, script, &result);
  assert(e == -1);

  assert(fn_called);

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
