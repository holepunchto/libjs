#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_module_t *module;

static bool import_called = false;

static void
assert_attribute(js_env_t *env, js_value_t *attributes, const char *name, const char *expected) {
  int e;

  js_value_t *value;
  e = js_get_named_property(env, attributes, name, &value);
  assert(e == 0);

  utf8_t actual[16];
  e = js_get_value_string_utf8(env, value, actual, sizeof(actual), NULL);
  assert(e == 0);

  assert(strcmp((char *) actual, expected) == 0);
}

static void
on_module_evaluate(js_env_t *env, js_module_t *module, void *data) {}

static js_value_t *
on_import(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, js_value_t *id, void *data) {
  int e;

  import_called = true;

  js_value_t *names;
  e = js_get_property_names(env, assertions, &names);
  assert(e == 0);

  uint32_t len;
  e = js_get_array_length(env, names, &len);
  assert(e == 0);

  assert(len == 3);

  assert_attribute(env, assertions, "a", "1");
  assert_attribute(env, assertions, "b", "2");
  assert_attribute(env, assertions, "c", "3");

  e = js_create_synthetic_module(env, "synthetic", -1, NULL, 0, on_module_evaluate, NULL, &module);
  assert(e == 0);

  e = js_instantiate_module(env, module, NULL, NULL);
  assert(e == 0);

  js_value_t *namespace;
  e = js_get_module_namespace(env, module, &namespace);
  assert(e == 0);

  return namespace;
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
  e = js_create_string_utf8(env, (utf8_t *) "import('foo.js', { with: { a: '1', b: '2', c: '3' } })", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, "test.js", -1, 0, script, &result);
  assert(e == 0);

  assert(import_called);

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
