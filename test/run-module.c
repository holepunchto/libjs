#include <assert.h>
#include <uv.h>

#include "../include/js.h"

static js_value_t *
on_module_evaluate (js_env_t *env, js_module_t *module, void *data) {
  int e;

  js_value_t *name;
  e = js_create_string_utf8(env, "foo", -1, &name);
  assert(e == 0);

  js_value_t *value;
  e = js_create_uint32(env, 42, &value);
  assert(e == 0);

  e = js_set_module_export(env, module, name, value);
  assert(e == 0);

  return NULL;
}

static js_module_t *
on_module_resolve (js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data) {
  int e;

  js_value_t *export_names[1];
  e = js_create_string_utf8(env, "foo", -1, &export_names[0]);
  assert(e == 0);

  js_module_t *module;
  e = js_create_synthetic_module(env, "synthetic", -1, (const js_value_t **) export_names, 1, on_module_evaluate, NULL, &module);
  assert(e == 0);

  e = js_instantiate_module(env, module, on_module_resolve);
  assert(e == 0);

  return module;
}

int
main (int argc, char *argv[]) {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_platform_init(loop, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_env_init(loop, platform, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *source;
  e = js_create_string_utf8(env, "import { foo } from 'foo.js'", -1, &source);
  assert(e == 0);

  js_module_t *module;
  e = js_create_module(env, "test.js", -1, source, NULL, &module);
  assert(e == 0);

  e = js_instantiate_module(env, module, on_module_resolve);
  assert(e == 0);

  js_value_t *result;
  e = js_run_module(env, module, &result);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_env_destroy(env);
  assert(e == 0);

  e = js_platform_destroy(platform);
  assert(e == 0);
}
