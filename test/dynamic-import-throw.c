#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

int import_called = 0;

static js_value_t *
on_import(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, js_value_t *id, void *data) {
  int e;

  import_called++;

  e = js_throw_error(env, "ERR_MODULE_NOT_FOUND", "Cannot find module");
  assert(e == 0);

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

  e = js_on_dynamic_import(env, on_import, NULL);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "import('foo.js')", -1, &script);
  assert(e == 0);

  // A dynamic `import()` evaluates to a promise and so must not throw at the
  // call site, no matter how the handler fails.

  js_value_t *result;
  e = js_run_script(env, "test.js", -1, 0, script, &result);
  assert(e == 0);

  assert(import_called == 1);

  bool has_exception;
  e = js_is_exception_pending(env, &has_exception);
  assert(e == 0);

  assert(!has_exception);

  js_promise_state_t state;
  e = js_get_promise_state(env, result, &state);
  assert(e == 0);

  assert(state == js_promise_rejected);

  // The promise must be rejected with what the handler made pending.

  js_value_t *reason;
  e = js_get_promise_result(env, result, &reason);
  assert(e == 0);

  js_value_t *message;
  e = js_get_named_property(env, reason, "message", &message);
  assert(e == 0);

  utf8_t value[19];
  e = js_get_value_string_utf8(env, message, value, 19, NULL);
  assert(e == 0);

  assert(strcmp((char *) value, "Cannot find module") == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
