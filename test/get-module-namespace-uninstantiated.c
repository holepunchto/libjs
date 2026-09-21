#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

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

  js_value_t *export_names[1];
  e = js_create_string_utf8(env, (utf8_t *) "foo", -1, &export_names[0]);
  assert(e == 0);

  js_module_t *synthetic;
  e = js_create_synthetic_module(env, "synthetic", -1, export_names, 1, on_module_evaluate, NULL, &synthetic);
  assert(e == 0);

  // The namespace does not exist until the module has been instantiated, so
  // asking for it now must fail rather than abort.

  js_value_t *namespace = (js_value_t *) 42;
  e = js_get_module_namespace(env, synthetic, &namespace);
  assert(e == js_pending_exception);

  assert(namespace == (js_value_t *) 42);

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  // Once instantiated, the same call succeeds.

  e = js_instantiate_module(env, synthetic, NULL, NULL);
  assert(e == 0);

  e = js_get_module_namespace(env, synthetic, &namespace);
  assert(e == 0);

  bool is_module_namespace;
  e = js_is_module_namespace(env, namespace, &is_module_namespace);
  assert(e == 0);

  assert(is_module_namespace);

  e = js_delete_module(env, synthetic);
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
