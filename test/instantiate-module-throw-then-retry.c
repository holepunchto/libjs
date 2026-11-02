#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_module_t *dependency;

static js_module_t *
on_module_resolve_throw(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data) {
  int e;

  e = js_throw_error(env, NULL, "Cannot find module");
  assert(e == 0);

  return NULL;
}

static js_module_t *
on_module_resolve(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data) {
  return dependency;
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

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "import 'dependency.js'", -1, &source);
  assert(e == 0);

  js_module_t *module;
  e = js_create_module(env, "test.js", -1, 0, source, NULL, NULL, &module);
  assert(e == 0);

  e = js_instantiate_module(env, module, on_module_resolve_throw, NULL);
  assert(e == js_pending_exception);

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  // A module whose instantiation failed may be instantiated again, this time
  // with its import resolved.

  js_value_t *dependency_source;
  e = js_create_string_utf8(env, (utf8_t *) "globalThis.evaluated = true", -1, &dependency_source);
  assert(e == 0);

  e = js_create_module(env, "dependency.js", -1, 0, dependency_source, NULL, NULL, &dependency);
  assert(e == 0);

  e = js_instantiate_module(env, module, on_module_resolve, NULL);
  assert(e == 0);

  js_value_t *promise;
  e = js_run_module(env, module, &promise);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *evaluated;
  e = js_get_named_property(env, global, "evaluated", &evaluated);
  assert(e == 0);

  bool result;
  e = js_get_value_bool(env, evaluated, &result);
  assert(e == 0);

  assert(result);

  e = js_delete_module(env, dependency);
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
