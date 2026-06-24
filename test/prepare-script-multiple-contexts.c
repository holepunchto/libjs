#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static uint32_t
run(js_env_t *env, js_script_t *script) {
  int e;

  js_value_t *result;
  e = js_run_prepared_script(env, script, &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  return value;
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

  // The same prepared script runs against the global of whichever context is
  // entered, with each context holding its own independent state.

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "globalThis.count = (globalThis.count || 0) + 1", -1, &source);
  assert(e == 0);

  js_script_t *script;
  e = js_prepare_script(env, "test.js", -1, 0, source, &script);
  assert(e == 0);

  js_context_t *first;
  e = js_create_context(env, &first);
  assert(e == 0);

  js_context_t *second;
  e = js_create_context(env, &second);
  assert(e == 0);

  e = js_enter_context(env, first);
  assert(e == 0);

  assert(run(env, script) == 1);
  assert(run(env, script) == 2);

  e = js_exit_context(env, first);
  assert(e == 0);

  e = js_enter_context(env, second);
  assert(e == 0);

  // The second context starts fresh rather than seeing the first context's
  // counter.

  assert(run(env, script) == 1);

  e = js_exit_context(env, second);
  assert(e == 0);

  e = js_destroy_context(env, first);
  assert(e == 0);

  e = js_destroy_context(env, second);
  assert(e == 0);

  e = js_delete_script(env, script);
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
