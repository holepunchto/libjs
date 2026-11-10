#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

// Profiles an environment while its loop waits on a timer, with a second
// environment on the same loop. The wait must show up as idle time. Sampling
// lags under load, so only a quarter of the wait must be sampled.

static js_env_t *a;
static js_inspector_t *inspector;

static char *profile = NULL;

static void
on_response(js_env_t *env, js_inspector_t *inspector, const char *message, size_t len, void *data) {
  if (strstr(message, "\"id\":3") == NULL) return;

  profile = strndup(message, len);
}

static void
send_request(const char *message) {
  int e;

  e = js_send_inspector_request(a, inspector, message, -1);
  assert(e == 0);
}

static void
on_timer(uv_timer_t *handle) {
  send_request("{ \"id\": 3, \"method\": \"Profiler.stop\" }");

  uv_close((uv_handle_t *) handle, NULL);
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  e = js_create_env(loop, platform, NULL, &a);
  assert(e == 0);

  js_env_t *b;
  e = js_create_env(loop, platform, NULL, &b);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(a, &scope);
  assert(e == 0);

  e = js_create_inspector(a, &inspector);
  assert(e == 0);

  e = js_on_inspector_response(a, inspector, on_response, NULL);
  assert(e == 0);

  e = js_connect_inspector(a, inspector);
  assert(e == 0);

  send_request("{ \"id\": 1, \"method\": \"Profiler.enable\" }");
  send_request("{ \"id\": 4, \"method\": \"Profiler.setSamplingInterval\", \"params\": { \"interval\": 1000 } }");
  send_request("{ \"id\": 2, \"method\": \"Profiler.start\" }");

  uv_timer_t timer;
  e = uv_timer_init(loop, &timer);
  assert(e == 0);

  e = uv_timer_start(&timer, on_timer, 200, 0);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  assert(profile != NULL);

  js_value_t *source;
  e = js_create_string_utf8(a, (utf8_t *) profile, -1, &source);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(a, &global);
  assert(e == 0);

  e = js_set_named_property(a, global, "response", source);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(a, (utf8_t *) "const { nodes, startTime, endTime } = JSON.parse(response).result.profile\n"
                                          "const idle = nodes.find((node) => node.callFrame.functionName === '(idle)')\n"
                                          "idle ? Math.floor(idle.hitCount * 1000 / (endTime - startTime) * 100) : 0",
                            -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(a, NULL, 0, 0, script, &result);
  assert(e == 0);

  uint32_t idle;
  e = js_get_value_uint32(a, result, &idle);
  assert(e == 0);

  assert(idle >= 25);

  free(profile);

  e = js_destroy_inspector(a, inspector);
  assert(e == 0);

  e = js_close_handle_scope(a, scope);
  assert(e == 0);

  e = js_destroy_env(a);
  assert(e == 0);

  e = js_destroy_env(b);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
