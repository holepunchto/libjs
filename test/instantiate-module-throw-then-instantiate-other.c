#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_module_t *
on_module_resolve(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data) {
  int e;

  e = js_throw_error(env, NULL, "Cannot find module");
  assert(e == 0);

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

  js_value_t *failing_source;
  e = js_create_string_utf8(env, (utf8_t *) "import 'missing.js'", -1, &failing_source);
  assert(e == 0);

  js_module_t *failing;
  e = js_create_module(env, "failing.js", -1, 0, failing_source, NULL, NULL, &failing);
  assert(e == 0);

  js_value_t *other_source;
  e = js_create_string_utf8(env, (utf8_t *) "export default 42", -1, &other_source);
  assert(e == 0);

  js_module_t *other;
  e = js_create_module(env, "other.js", -1, 0, other_source, NULL, NULL, &other);
  assert(e == 0);

  // A failed instantiation must leave every other module that has been
  // created but not yet instantiated intact.

  e = js_instantiate_module(env, failing, on_module_resolve, NULL);
  assert(e == js_pending_exception);

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  e = js_instantiate_module(env, other, NULL, NULL);
  assert(e == 0);

  js_value_t *promise;
  e = js_run_module(env, other, &promise);
  assert(e == 0);

  js_value_t *namespace;
  e = js_get_module_namespace(env, other, &namespace);
  assert(e == 0);

  js_value_t *value;
  e = js_get_named_property(env, namespace, "default", &value);
  assert(e == 0);

  uint32_t result;
  e = js_get_value_uint32(env, value, &result);
  assert(e == 0);

  assert(result == 42);

  e = js_delete_module(env, failing);
  assert(e == 0);

  e = js_delete_module(env, other);
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
