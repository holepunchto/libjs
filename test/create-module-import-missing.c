#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

js_module_t *synthetic_module;

int uncaught_called = 0;

static void
on_uncaught_exception(js_env_t *env, js_value_t *error, void *data) {
  uncaught_called++;
}

static js_module_t *
on_module_resolve(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data) {
  int e;

  js_value_t *export_names[1];
  e = js_create_string_utf8(env, (utf8_t *) "bar", -1, &export_names[0]);
  assert(e == 0);

  e = js_create_synthetic_module(env, "synthetic", -1, export_names, 1, NULL, NULL, &synthetic_module);
  assert(e == 0);

  return synthetic_module;
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

  e = js_on_uncaught_exception(env, on_uncaught_exception, NULL);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "import { foo } from 'foo.js'", -1, &source);
  assert(e == 0);

  js_module_t *module;
  e = js_create_module(env, "test.js", -1, 0, source, NULL, NULL, &module);
  assert(e == 0);

  e = js_instantiate_module(env, module, on_module_resolve, NULL);

  // The engine may fail early during module instantiation.
  if (e != 0) assert(uncaught_called == 1);

  // Otherwise, it must fail when evaluating the module
  else {
    js_value_t *result;
    e = js_run_module(env, module, &result);
    assert(e == 0);

    js_promise_state_t state;
    e = js_get_promise_state(env, result, &state);
    assert(e == 0);

    assert(state == js_promise_rejected);
  }

  e = js_delete_module(env, module);
  assert(e == 0);

  e = js_delete_module(env, synthetic_module);
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
