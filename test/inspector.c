#include <assert.h>
#include <stdlib.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static int message_called = 0;

static void
on_response(js_env_t *env, js_inspector_t *inspector, js_value_t *message, void *data) {
  int e;

  message_called++;

  size_t len;
  e = js_get_value_string_utf8(env, message, NULL, 0, &len);
  assert(e == 0);

  len += 1 /* NULL */;

  utf8_t *response = malloc(len);
  e = js_get_value_string_utf8(env, message, response, len, NULL);
  assert(e == 0);

  printf("response=%s\n", response);

  free(response);
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

  js_value_t *message;
  e = js_create_string_utf8(env, (utf8_t *) "{ \"id\": 1, \"method\": \"Runtime.evaluate\", \"params\": { \"expression\": \"1 + 2\" } }", -1, &message);
  assert(e == 0);

  e = js_send_inspector_request(env, inspector, message);
  assert(e == 0);

  assert(message_called);

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
