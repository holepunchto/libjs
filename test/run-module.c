#include <assert.h>

#include "../include/js.h"

int
main (int argc, char *argv[]) {
  int e;

  e = js_platform_init(argv[0]);
  assert(e == 0);

  js_env_t *env;
  e = js_env_init(&env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, "12345", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_module(env, script, "test.js", &result);
  assert(e == 0);

  uint32_t value;
  js_get_value_uint32(env, result, &value);
  assert(e == 0);

  assert(value == 12345);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_env_destroy(env);
  assert(e == 0);

  e = js_platform_destroy();
  assert(e == 0);
}
