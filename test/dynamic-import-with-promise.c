#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_module_t *module;

static void
on_module_evaluate(js_env_t *env, js_module_t *module, void *data) {
  int e;

  js_value_t *name;
  e = js_create_string_utf8(env, (utf8_t *) "foo", -1, &name);
  assert(e == 0);

  js_value_t *value;
  e = js_create_uint32(env, 42, &value);
  assert(e == 0);

  e = js_set_module_export(env, module, name, value);
  assert(e == 0);
}

static js_value_t *
on_import(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, void *data) {
  int e;

  js_value_t *export_names[1];
  e = js_create_string_utf8(env, (utf8_t *) "foo", -1, &export_names[0]);
  assert(e == 0);

  e = js_create_synthetic_module(env, "synthetic", -1, export_names, 1, on_module_evaluate, NULL, &module);
  assert(e == 0);

  e = js_instantiate_module(env, module, NULL, NULL);
  assert(e == 0);

  js_value_t *namespace;
  e = js_get_module_namespace(env, module, &namespace);
  assert(e == 0);

  js_value_t *promise;

  js_deferred_t *deferred;
  e = js_create_promise(env, &deferred, &promise);
  assert(e == 0);

  e = js_resolve_deferred(env, deferred, namespace);
  assert(e == 0);

  return promise;
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

  e = js_on_dynamic_import(env, on_import, NULL);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "const foo = import('foo.js')", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, "test.js", -1, 0, script, &result);
  assert(e == 0);

  e = js_delete_module(env, module);
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
