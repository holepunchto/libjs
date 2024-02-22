#include <assert.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"
#include "helpers.h"

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

  js_value_t *description;
  e = js_create_string_utf8(env, (utf8_t *) "symbol", -1, &description);
  assert(e == 0);

  js_value_t *symbol;
  e = js_create_symbol(env, description, &symbol);
  assert(e == 0);

  js_value_t *string;
  e = js_coerce_to_string(env, symbol, &string);
  assert(e != 0);

  js_print_pending_exception(env);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
