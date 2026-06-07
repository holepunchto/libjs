#include <assert.h>
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

  js_value_t *first;
  e = js_symbol_for(env, "shared", -1, &first);
  assert(e == 0);

  js_value_t *second;
  e = js_symbol_for(env, "shared", -1, &second);
  assert(e == 0);

  bool equal;
  e = js_strict_equals(env, first, second, &equal);
  assert(e == 0);

  assert(equal);

  bool is_symbol;
  e = js_is_symbol(env, first, &is_symbol);
  assert(e == 0);

  assert(is_symbol);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
