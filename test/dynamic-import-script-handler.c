#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_module_t *synthetic;

static int env_calls = 0;
static int script_calls = 0;

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
namespace_of(js_env_t *env) {
  int e;

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

static js_value_t *
on_env_import(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, js_value_t *id, void *data) {
  env_calls++;

  assert(data == (void *) 1);

  return namespace_of(env);
}

static js_value_t *
on_script_import(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, js_value_t *id, void *data) {
  script_calls++;

  assert(data == (void *) 2);

  return namespace_of(env);
}

static js_script_t *
prepare(js_env_t *env, const char *file) {
  int e;

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "import('foo.js')", -1, &source);
  assert(e == 0);

  js_script_t *script;
  e = js_prepare_script(env, file, -1, 0, source, &script);
  assert(e == 0);

  return script;
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

  e = js_on_dynamic_import(env, on_env_import, (void *) 1);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  // A script with a handler of its own must reach that handler, and not the one
  // added for the environment.

  js_script_t *scoped = prepare(env, "scoped.js");

  e = js_on_script_dynamic_import(env, scoped, on_script_import, (void *) 2);
  assert(e == 0);

  js_value_t *result;
  e = js_run_prepared_script(env, scoped, &result);
  assert(e == 0);

  assert(script_calls == 1);
  assert(env_calls == 0);

  // A script with no handler of its own must fall back to the environment.

  js_script_t *plain = prepare(env, "plain.js");

  e = js_run_prepared_script(env, plain, &result);
  assert(e == 0);

  assert(script_calls == 1);
  assert(env_calls == 1);

  // Code with no unit of its own must fall back to the environment too.

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "import('foo.js')", -1, &source);
  assert(e == 0);

  e = js_run_script(env, "eval.js", -1, 0, source, &result);
  assert(e == 0);

  assert(script_calls == 1);
  assert(env_calls == 2);

  e = js_delete_script(env, scoped);
  assert(e == 0);

  e = js_delete_script(env, plain);
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
