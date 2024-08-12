#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_module_t *
on_module_resolve (js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data) {
  assert(false);

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

  js_context_t *context;
  e = js_create_context(env, &context);
  assert(e == 0);

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "export default globalThis", -1, &source);
  assert(e == 0);

  js_module_t *module;
  e = js_create_module(env, "test.js", -1, 0, source, NULL, NULL, &module);
  assert(e == 0);

  e = js_enter_context(env, context);
  assert(e == 0);

  e = js_instantiate_module(env, module, on_module_resolve, NULL);
  assert(e == 0);

  js_value_t *promise;
  e = js_run_module(env, module, &promise);
  assert(e == 0);

  js_promise_state_t state;
  e = js_get_promise_state(env, promise, &state);
  assert(e == 0);

  assert(state == js_promise_fulfilled);

  js_value_t *namespace;
  e = js_get_module_namespace(env, module, &namespace);
  assert(e == 0);

  js_value_t *result;
  e = js_get_named_property(env, namespace, "default", &result);
  assert(e == 0);

  {
    js_value_t *global;
    e = js_get_global(env, &global);
    assert(e == 0);

    bool equals;
    e = js_strict_equals(env, result, global, &equals);
    assert(e == 0);

    assert(equals);
  }

  e = js_exit_context(env, context);
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

  e = js_delete_module(env, module);
  assert(e == 0);

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
