#include <assert.h>
#include <uv.h>

#include "../include/js.h"

#include "fixtures/wasm-async.js.h"

static js_env_t *env;
static js_ref_t *ref;

static void
on_timer (uv_timer_t *handle) {
  int e;

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, wasm_async_js, wasm_async_js_len, &script);
  assert(e == 0);

  js_value_t *promise;
  e = js_run_script(env, NULL, 0, 0, script, &promise);
  assert(e == 0);

  e = js_create_reference(env, promise, 1, &ref);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);
}

int
main () {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  uv_timer_t timer;
  e = uv_timer_init(loop, &timer);
  assert(e == 0);

  e = uv_timer_start(&timer, on_timer, 100, 0);
  assert(e == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  js_value_t *promise;
  e = js_get_reference_value(env, ref, &promise);
  assert(e == 0);

  js_value_t *result;
  e = js_get_promise_result(env, promise, &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  assert(value == 42);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
