#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"
#include "helpers.h"

js_value_t *
on_call (js_env_t *env, js_callback_info_t *info) {
  int e;

  e = js_terminate_execution(env);
  assert(e == 0);

  return NULL;
}

int
main () {
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

  js_value_t *fn;
  e = js_create_function(env, "terminate", -1, on_call, NULL, &fn);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  e = js_set_named_property(env, global, "terminate", fn);
  assert(e == 0);

  {
    const char *source =
      "try { terminate() } catch {}"
      "(() => {})();" // Trigger a stack check
      "throw 'err'";

    js_value_t *script;
    e = js_create_string_utf8(env, (utf8_t *) source, -1, &script);
    assert(e == 0);

    js_value_t *result;
    e = js_run_script(env, NULL, 0, 0, script, &result);
    assert(e != 0);

    js_print_pending_exception(env);
  }

  {
    const char *source =
      "(() => {})();" // Trigger a stack check
      "1 + 2";

    js_value_t *script;
    e = js_create_string_utf8(env, (utf8_t *) source, -1, &script);
    assert(e == 0);

    js_value_t *result;
    e = js_run_script(env, NULL, 0, 0, script, &result);
    assert(e == 0);

    uint32_t value;
    e = js_get_value_uint32(env, result, &value);
    assert(e == 0);

    assert(value == 3);
  }

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
