#include <assert.h>
#include <uv.h>

#include "../include/js.h"

#include "fixtures/wasm-async.js.h"

int
main () {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, &env);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (char *) wasm_async_js, wasm_async_js_len, &script);
  assert(e == 0);

  js_value_t *promise;
  e = js_run_script(env, script, &promise);
  assert(e == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  js_value_t *result;
  e = js_get_promise_result(env, promise, &result);
  assert(e == 0);

  uint32_t value;
  js_get_value_uint32(env, result, &value);
  assert(e == 0);

  assert(value == 42);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
