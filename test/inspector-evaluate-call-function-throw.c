#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static bool fn_called = false;
static bool response_called = false;

static void
on_uncaught_exception(js_env_t *env, js_value_t *error, void *data) {
  assert(false);
}

static void
on_response(js_env_t *env, js_inspector_t *inspector, const char *message, size_t len, void *data) {
  response_called = true;
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

  // The error must be rethrown into the JavaScript being evaluated rather than
  // be reported as uncaught.

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

  js_inspector_t *inspector;
  e = js_create_inspector(env, &inspector);
  assert(e == 0);

  e = js_on_inspector_response(env, inspector, on_response, NULL);
  assert(e == 0);

  e = js_connect_inspector(env, inspector);
  assert(e == 0);

  const char *message = "{ \"id\": 1, \"method\": \"Runtime.evaluate\", \"params\": { \"expression\": \"try { hello(() => { throw new Error('err') }) } catch (err) { globalThis.caught = err.message === 'err' }\" } }";

  e = js_send_inspector_request(env, inspector, message, -1);
  assert(e == 0);

  assert(response_called);
  assert(fn_called);

  js_value_t *caught;
  e = js_get_named_property(env, global, "caught", &caught);
  assert(e == 0);

  bool value;
  e = js_get_value_bool(env, caught, &value);
  assert(e == 0);

  assert(value);

  e = js_destroy_inspector(env, inspector);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
