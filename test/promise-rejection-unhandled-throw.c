#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static bool unhandled_called = false;

static void
on_unhandled_rejection(js_env_t *env, js_value_t *reason, js_value_t *promise, void *data) {
  int e;

  unhandled_called = true;

  e = js_throw_error(env, NULL, "err");
  assert(e == 0);
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

  e = js_on_unhandled_rejection(env, on_unhandled_rejection, NULL);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_deferred_t *rejected;
  js_value_t *promise;
  e = js_create_promise(env, &rejected, &promise);
  assert(e == 0);

  js_deferred_t *resolved;
  e = js_create_promise(env, &resolved, &promise);
  assert(e == 0);

  js_value_t *undefined;
  e = js_get_undefined(env, &undefined);
  assert(e == 0);

  // With no JavaScript on the stack, the rejection is reported right away and
  // the error thrown by the handler has nothing to receive it.

  e = js_reject_deferred(env, rejected, undefined);
  assert(e == 0);

  assert(unhandled_called);

  // Resolving a deferred enters the engine, which may discard its own record
  // of the error. The error must remain pending regardless.

  e = js_resolve_deferred(env, resolved, undefined);
  assert(e == 0);

  bool has_exception;
  e = js_is_exception_pending(env, &has_exception);
  assert(e == 0);

  assert(has_exception);

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  js_value_t *message;
  e = js_get_named_property(env, error, "message", &message);
  assert(e == 0);

  utf8_t value[4];
  e = js_get_value_string_utf8(env, message, value, 4, NULL);
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
