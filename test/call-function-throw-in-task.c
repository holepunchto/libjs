#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static bool fn_called = false;
static bool uncaught_called = false;

static void
on_uncaught_exception(js_env_t *env, js_value_t *error, void *data) {
  uncaught_called = true;
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

  e = js_call_function(env, global, argv[0], 0, NULL, NULL);
  assert(e == js_pending_exception);

  return NULL;
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_options_t options = {
    .expose_garbage_collection = true,
  };

  js_platform_t *platform;
  e = js_create_platform(loop, &options, &platform);
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

  // The cleanup callback of a finalization registry runs in a task, outside
  // any call made with libjs.

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "globalThis.caught = false; globalThis.registry = new FinalizationRegistry(() => { try { hello(() => { throw new Error('err') }) } catch (err) { globalThis.caught = err.message === 'err' } }); (() => registry.register({}, 0))()", -1, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, NULL);
  assert(e == 0);

  e = js_request_garbage_collection(env);
  assert(e == 0);

  uv_run(loop, UV_RUN_NOWAIT);

  assert(fn_called);

  // The error thrown into the cleanup callback must reach its catch block
  // rather than be reported as uncaught.

  assert(!uncaught_called);

  js_value_t *caught;
  e = js_get_named_property(env, global, "caught", &caught);
  assert(e == 0);

  bool value;
  e = js_get_value_bool(env, caught, &value);
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
