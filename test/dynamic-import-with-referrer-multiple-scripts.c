#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_module_t *synthetic;

static js_value_t *expected_id;

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
on_import(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, js_value_t *id, void *data) {
  int e;

  // Each import is attributed to the identifier of the specific script that
  // initiated it, not a shared one.

  bool equals;
  e = js_strict_equals(env, id, expected_id, &equals);
  assert(e == 0);

  assert(equals);

  if (synthetic == NULL) {
    js_value_t *export_names[1];
    e = js_create_string_utf8(env, (utf8_t *) "foo", -1, &export_names[0]);
    assert(e == 0);

    e = js_create_synthetic_module(env, "synthetic", -1, export_names, 1, on_module_evaluate, NULL, &synthetic);
    assert(e == 0);

    e = js_instantiate_module(env, synthetic, NULL, NULL);
    assert(e == 0);
  }

  js_value_t *namespace;
  e = js_get_module_namespace(env, synthetic, &namespace);
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

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "import('foo.js')", -1, &source);
  assert(e == 0);

  js_script_t *first;
  e = js_prepare_script(env, "first.js", -1, 0, source, &first);
  assert(e == 0);

  js_script_t *second;
  e = js_prepare_script(env, "second.js", -1, 0, source, &second);
  assert(e == 0);

  // Running the first script attributes its import to the first script's
  // identifier.

  e = js_get_script_id(env, first, &expected_id);
  assert(e == 0);

  js_value_t *result;
  e = js_run_prepared_script(env, first, &result);
  assert(e == 0);

  // Running the second script attributes its import to the second script's
  // identifier instead.

  e = js_get_script_id(env, second, &expected_id);
  assert(e == 0);

  e = js_run_prepared_script(env, second, &result);
  assert(e == 0);

  e = js_delete_script(env, first);
  assert(e == 0);

  e = js_delete_script(env, second);
  assert(e == 0);

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
