#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_module_t *synthetic;

static int first_calls = 0;
static int second_calls = 0;

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
on_first_import(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, js_value_t *id, void *data) {
  first_calls++;

  return namespace_of(env);
}

static js_value_t *
on_second_import(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, js_value_t *id, void *data) {
  second_calls++;

  return namespace_of(env);
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
  e = js_create_string_utf8(env, (utf8_t *) "import('foo.js')", -1, &source);
  assert(e == 0);

  // Prepare a script, give it a handler, and produce a code cache from it.

  js_script_t *script;
  e = js_prepare_script(env, "test.js", -1, 0, source, &script);
  assert(e == 0);

  e = js_on_script_dynamic_import(env, script, on_first_import, NULL);
  assert(e == 0);

  void *data;
  size_t len;
  e = js_create_script_code_cache(env, script, &data, &len);
  assert(e == 0);

  js_value_t *result;
  e = js_run_prepared_script(env, script, &result);
  assert(e == 0);

  assert(first_calls == 1);
  assert(second_calls == 0);

  e = js_delete_script(env, script);
  assert(e == 0);

  // A handler is embedder state rather than compiled state, so a script served
  // from the cache starts without one and takes whichever is added next.

  bool cache_rejected;

  js_script_t *cached;
  e = js_prepare_script_with_code_cache(env, "test.js", -1, 0, source, data, len, &cache_rejected, &cached);
  assert(e == 0);

  assert(cache_rejected == false);

  e = js_on_script_dynamic_import(env, cached, on_second_import, NULL);
  assert(e == 0);

  e = js_run_prepared_script(env, cached, &result);
  assert(e == 0);

  assert(first_calls == 1);
  assert(second_calls == 1);

  e = js_delete_script(env, cached);
  assert(e == 0);

  free(data);

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
