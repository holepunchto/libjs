#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static bool response_called = false;

static void
on_response(js_env_t *env, js_inspector_t *inspector, const char *message, size_t len, void *data) {
  int e;

  response_called = true;

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

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_inspector_t *inspector;
  e = js_create_inspector(env, &inspector);
  assert(e == 0);

  e = js_on_inspector_response(env, inspector, on_response, NULL);
  assert(e == 0);

  e = js_connect_inspector(env, inspector);
  assert(e == 0);

  const char *message = "{ \"id\": 1, \"method\": \"Runtime.evaluate\", \"params\": { \"expression\": \"1 + 2\" } }";

  e = js_send_inspector_request(env, inspector, message, -1);
  assert(e == 0);

  assert(response_called);

  // Nothing is there to receive the error thrown by the response callback, so
  // it must remain pending.
  bool has_exception;
  e = js_is_exception_pending(env, &has_exception);
  assert(e == 0);

  assert(has_exception);

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  js_value_t *value;
  e = js_get_named_property(env, error, "message", &value);
  assert(e == 0);

  utf8_t message_value[4];
  e = js_get_value_string_utf8(env, value, message_value, 4, NULL);
  assert(e == 0);

  assert(strcmp((char *) message_value, "err") == 0);

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
