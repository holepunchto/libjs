#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static void
assert_code(js_env_t *env, js_value_t *error) {
  int e;

  js_value_t *code;
  e = js_get_named_property(env, error, "code", &code);
  assert(e == 0);

  utf8_t value[4];
  e = js_get_value_string_utf8(env, code, value, 4, NULL);
  assert(e == 0);

  assert(strcmp((char *) value, "ERR") == 0);
}

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

  // The code of an error is its own property, so a setter on the prototype
  // chain must not run when it is added.

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "Object.defineProperty(Error.prototype, 'code', { set () { throw new Error('setter') }, configurable: true })", -1, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, NULL);
  assert(e == 0);

  js_value_t *code;
  e = js_create_string_utf8(env, (utf8_t *) "ERR", -1, &code);
  assert(e == 0);

  js_value_t *message;
  e = js_create_string_utf8(env, (utf8_t *) "err", -1, &message);
  assert(e == 0);

  js_value_t *error;
  e = js_create_error(env, code, message, &error);
  assert(e == 0);

  assert_code(env, error);

  e = js_throw_error(env, "ERR", "err");
  assert(e == 0);

  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  assert_code(env, error);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
