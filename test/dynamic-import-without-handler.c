#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

int uncaught_called = 0;

static void
on_uncaught_exception(js_env_t *env, js_value_t *error, void *data) {
  uncaught_called++;
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

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "import('foo.js')", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, "test.js", -1, 0, script, &result);

  // The engine may fail early during script evaluation.
  if (e != 0) assert(uncaught_called == 1);

  // Otherwise, it must reject the import promise.
  else {
    js_promise_state_t state;
    e = js_get_promise_state(env, result, &state);
    assert(e == 0);

    assert(state == js_promise_rejected);
  }

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
