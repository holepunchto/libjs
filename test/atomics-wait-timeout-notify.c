#include <assert.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

#include "fixtures/atomics-wait-timeout-notify.js.h"

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

  js_value_t *script;
  e = js_create_string_utf8(env, atomics_wait_timeout_notify_js, atomics_wait_timeout_notify_js_len, &script);
  assert(e == 0);

  js_value_t *promise;
  e = js_run_script(env, NULL, 0, 0, script, &promise);
  assert(e == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  js_value_t *result;
  e = js_get_promise_result(env, promise, &result);
  assert(e == 0);

  utf8_t value[10];
  e = js_get_value_string_utf8(env, result, value, 10, NULL);
  assert(e == 0);

  assert(strcmp((char *) value, "ok") == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
