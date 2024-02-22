#include <assert.h>
#include <uv.h>

#include "../include/js.h"

#include "fixtures/wasm-async-log.js.h"

static uv_async_t async;

static js_env_t *env;

static bool log_called = false;

static js_value_t *
on_log (js_env_t *env, js_callback_info_t *info) {
  int e;

  log_called = true;

  js_value_t *argv[1];
  size_t argc = 1;

  e = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, argv[0], &value);
  assert(e == 0);

  assert(value == 42);

  uv_close((uv_handle_t *) &async, NULL);

  return NULL;
}

static void
on_timer (uv_timer_t *handle) {
  int e;

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, wasm_async_log_js, wasm_async_log_js_len, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, NULL);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);
}

static void
on_async (uv_async_t *handle) {}

int
main () {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *fn;
  e = js_create_function(env, "log", -1, on_log, NULL, &fn);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  e = js_set_named_property(env, global, "log", fn);
  assert(e == 0);

  uv_timer_t timer;
  e = uv_timer_init(loop, &timer);
  assert(e == 0);

  e = uv_timer_start(&timer, on_timer, 100, 0);
  assert(e == 0);

  e = uv_async_init(loop, &async, on_async);
  assert(e == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
