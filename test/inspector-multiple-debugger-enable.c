#include <assert.h>
#include <stdlib.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static int message_called = 0;

static void
on_response(js_env_t *env, js_inspector_t *inspector, const char *message, size_t len, void *data) {
  message_called++;

  printf("message=%.*s\n", (int) len, message);
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

  js_inspector_t *inspector1;
  e = js_create_inspector(env, &inspector1);
  assert(e == 0);

  e = js_on_inspector_response_transitional(env, inspector1, on_response, NULL);
  assert(e == 0);

  e = js_connect_inspector(env, inspector1);
  assert(e == 0);

  const char *message = "{ \"id\": 1, \"method\": \"Debugger.enable\", \"params\": {} }";

  e = js_send_inspector_request_transitional(env, inspector1, message, -1);
  assert(e == 0);

  assert(message_called);

  js_inspector_t *inspector2;
  e = js_create_inspector(env, &inspector2);
  assert(e == 0);

  e = js_on_inspector_response_transitional(env, inspector2, on_response, NULL);
  assert(e == 0);

  e = js_connect_inspector(env, inspector2);
  assert(e == 0);

  message_called = 0;

  e = js_send_inspector_request_transitional(env, inspector2, message, -1);
  assert(e == 0);

  assert(message_called);

  e = js_destroy_inspector(env, inspector1);
  assert(e == 0);
  e = js_destroy_inspector(env, inspector2);
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
