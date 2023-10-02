#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static void
on_module_meta (js_env_t *env, js_module_t *module, js_value_t *meta, void *data) {
  int e;

  js_value_t *value;
  e = js_create_uint32(env, 42, &value);
  assert(e == 0);

  e = js_set_named_property(env, meta, "foo", value);
  assert(e == 0);
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

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "export const foo = import.meta.foo", -1, &source);
  assert(e == 0);

  js_module_t *module;
  e = js_create_module(env, "test.js", -1, 0, source, on_module_meta, NULL, &module);
  assert(e == 0);

  e = js_instantiate_module(env, module, NULL, NULL);
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

  js_value_t *property;
  e = js_get_named_property(env, namespace, "foo", &property);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, property, &value);
  assert(e == 0);

  assert(value == 42);

  e = js_delete_module(env, module);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
