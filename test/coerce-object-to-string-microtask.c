#include <assert.h>
#include <stdbool.h>
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
  e = js_create_string_utf8(env, (utf8_t *) "globalThis.ran = false; ({ toString() { Promise.resolve().then(() => { globalThis.ran = true }); return 'hello' } })", -1, &script);
  assert(e == 0);

  js_value_t *object;
  e = js_run_script(env, NULL, 0, 0, script, &object);
  assert(e == 0);

  js_value_t *string;
  e = js_coerce_to_string(env, object, &string);
  assert(e == 0);

  // The microtask queued by `toString()` must run once no JavaScript is left
  // on the stack.

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *ran;
  e = js_get_named_property(env, global, "ran", &ran);
  assert(e == 0);

  bool value;
  e = js_get_value_bool(env, ran, &value);
  assert(e == 0);

  assert(value);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
