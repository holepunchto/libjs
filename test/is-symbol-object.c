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

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "Object(Symbol('abc'))", -1, &script);
  assert(e == 0);

  js_value_t *boxed;
  e = js_run_script(env, NULL, 0, 0, script, &boxed);
  assert(e == 0);

  bool is_symbol_object;
  e = js_is_symbol_object(env, boxed, &is_symbol_object);
  assert(e == 0);

  assert(is_symbol_object);

  js_value_t *primitive;
  e = js_create_symbol(env, NULL, &primitive);
  assert(e == 0);

  e = js_is_symbol_object(env, primitive, &is_symbol_object);
  assert(e == 0);

  assert(!is_symbol_object);

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  e = js_is_symbol_object(env, object, &is_symbol_object);
  assert(e == 0);

  assert(!is_symbol_object);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
