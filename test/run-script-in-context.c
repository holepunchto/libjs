#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

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

  js_context_t *context;
  e = js_create_context(env, &context);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "globalThis", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script_in_context(env, context, NULL, 0, 0, script, &result);
  assert(e == 0);

  {
    js_value_t *global;
    e = js_get_global(env, &global);
    assert(e == 0);

    bool equals;
    e = js_strict_equals(env, result, global, &equals);
    assert(e == 0);

    assert(equals == false);
  }

  {
    js_value_t *global;
    e = js_get_global_in_context(env, context, &global);
    assert(e == 0);

    bool equals;
    e = js_strict_equals(env, result, global, &equals);
    assert(e == 0);

    assert(equals);
  }

  e = js_destroy_context(env, context);
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
