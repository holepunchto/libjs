#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_module_t *
on_resolve(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data) {
  return NULL;
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
  e = js_create_string_utf8(env, (utf8_t *) "export const value = 42", -1, &source);
  assert(e == 0);

  js_module_t *module;
  e = js_create_module(env, "test.js", -1, 0, source, NULL, NULL, &module);
  assert(e == 0);

  e = js_instantiate_module(env, module, on_resolve, NULL);
  assert(e == 0);

  js_value_t *result;
  e = js_run_module(env, module, &result);
  assert(e == 0);

  js_value_t *namespace;
  e = js_get_module_namespace(env, module, &namespace);
  assert(e == 0);

  js_object_type_t type;
  e = js_get_object_type(env, namespace, &type);
  assert(e == 0);

  assert(type == js_module_namespace);

  bool is_module_namespace;
  e = js_is_module_namespace(env, namespace, &is_module_namespace);
  assert(e == 0);

  assert(is_module_namespace);

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
