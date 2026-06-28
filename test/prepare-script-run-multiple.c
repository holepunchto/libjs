#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

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

  // The script increments a counter on each run, letting us observe that the
  // same prepared script can be run more than once.

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "globalThis.count = (globalThis.count || 0) + 1", -1, &source);
  assert(e == 0);

  js_script_t *script;
  e = js_prepare_script(env, "test.js", -1, 0, source, &script);
  assert(e == 0);

  for (uint32_t i = 1; i <= 3; i++) {
    js_value_t *result;
    e = js_run_prepared_script(env, script, &result);
    assert(e == 0);

    uint32_t value;
    e = js_get_value_uint32(env, result, &value);
    assert(e == 0);

    assert(value == i);
  }

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
