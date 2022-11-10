#include <assert.h>

#include "../include/js.h"

int
main (int argc, char *argv[]) {
  int e;

  js_platform_t *platform;
  e = js_platform_init(&platform);
  assert(e == 0);

  js_env_t *env;
  e = js_env_init(platform, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, "() => { throw 'err' }", -1, &script);
  assert(e == 0);

  js_value_t *fn;
  e = js_run_script(env, script, &fn);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *result;
  e = js_call_function(env, global, fn, 0, NULL, &result);
  assert(e == -1);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_env_destroy(env);
  assert(e == 0);

  e = js_platform_destroy(platform);
  assert(e == 0);
}
