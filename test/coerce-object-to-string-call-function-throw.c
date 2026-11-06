#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static bool fn_called = false;

static void
on_uncaught_exception(js_env_t *env, js_value_t *error, void *data) {
  assert(false);
}

static js_value_t *
on_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  fn_called = true;

  size_t argc = 1;
  js_value_t *argv[1];
  e = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  // The error must be rethrown into the `toString()` method being called
  // rather than be reported as uncaught.

  e = js_call_function(env, global, argv[0], 0, NULL, NULL);
  assert(e == js_pending_exception);

  return NULL;
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

  e = js_on_uncaught_exception(env, on_uncaught_exception, NULL);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
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
  e = js_create_string_utf8(env, (utf8_t *) "({ toString() { try { hello(() => { throw new Error('err') }) } catch (err) { return err.message } } })", -1, &script);
  assert(e == 0);

  js_value_t *object;
  e = js_run_script(env, NULL, 0, 0, script, &object);
  assert(e == 0);

  js_value_t *result;
  e = js_coerce_to_string(env, object, &result);
  assert(e == 0);

  assert(fn_called);

  utf8_t value[4];
  e = js_get_value_string_utf8(env, result, value, 4, NULL);
  assert(e == 0);

  assert(strcmp((char *) value, "err") == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
