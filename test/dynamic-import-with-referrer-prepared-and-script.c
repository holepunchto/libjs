#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_module_t *synthetic;

static js_value_t *expected_id;

static js_value_t *forbidden_id;

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

  // The import is attributed to the expected identifier and, crucially, not to
  // the other referrer's identifier: a prepared script's identifier is distinct
  // from the default identifier used by plain scripts.

  bool equals;
  e = js_strict_equals(env, id, expected_id, &equals);
  assert(e == 0);

  assert(equals);

  e = js_strict_equals(env, id, forbidden_id, &equals);
  assert(e == 0);

  assert(equals == false);

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

  js_value_t *default_id;
  e = js_get_default_module_id(env, &default_id);
  assert(e == 0);

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "import('foo.js')", -1, &source);
  assert(e == 0);

  js_script_t *script;
  e = js_prepare_script(env, "test.js", -1, 0, source, &script);
  assert(e == 0);

  js_value_t *prepared_id;
  e = js_get_script_id(env, script, &prepared_id);
  assert(e == 0);

  // A dynamic import() from a prepared script is attributed to the script's own
  // identifier, not the default identifier.

  expected_id = prepared_id;
  forbidden_id = default_id;

  js_value_t *result;
  e = js_run_prepared_script(env, script, &result);
  assert(e == 0);

  // A dynamic import() from a plain script falls back to the default identifier,
  // which is distinct from the prepared script's identifier.

  expected_id = default_id;
  forbidden_id = prepared_id;

  e = js_run_script(env, "test.js", -1, 0, source, &result);
  assert(e == 0);

  e = js_delete_script(env, script);
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
