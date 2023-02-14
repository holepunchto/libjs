#include <assert.h>
#include <uv.h>

#include "../include/js.h"

js_deferred_t *deferred;

static void
on_module_evaluate (js_env_t *env, js_module_t *module, void *data) {
  int e;

  js_value_t *name;
  e = js_create_string_utf8(env, "foo", -1, &name);
  assert(e == 0);

  js_value_t *value;
  e = js_create_promise(env, &deferred, &value);
  assert(e == 0);

  e = js_set_module_export(env, module, name, value);
  assert(e == 0);
}

static js_module_t *
on_module_resolve (js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data) {
  int e;

  js_value_t *export_names[1];
  e = js_create_string_utf8(env, "foo", -1, &export_names[0]);
  assert(e == 0);

  js_module_t *module;
  e = js_create_synthetic_module(env, "synthetic", -1, export_names, 1, on_module_evaluate, NULL, &module);
  assert(e == 0);

  return module;
}

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

  js_value_t *source;
  e = js_create_string_utf8(env, "import { foo } from 'foo.js'; await foo", -1, &source);
  assert(e == 0);

  js_module_t *module;
  e = js_create_module(env, "test.js", -1, 0, source, &module);
  assert(e == 0);

  e = js_instantiate_module(env, module, on_module_resolve, NULL);
  assert(e == 0);

  js_value_t *promise;
  e = js_run_module(env, module, &promise);
  assert(e == 0);

  js_promise_state_t state;
  e = js_get_promise_state(env, promise, &state);
  assert(e == 0);

  assert(state == js_promise_pending);

  e = js_resolve_deferred(env, deferred, source);
  assert(e == 0);

  e = js_get_promise_state(env, promise, &state);
  assert(e == 0);

  assert(state == js_promise_fulfilled);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
