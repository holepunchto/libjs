#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_module_t *synthetic;

static int env_calls = 0;
static int function_calls = 0;

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

  return namespace_of(env);
}

static js_value_t *
on_function_import(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, js_value_t *id, void *data) {
  function_calls++;

  assert(data == (void *) 2);

  return namespace_of(env);
}

static js_value_t *
compile(js_env_t *env, const char *file) {
  int e;

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "return import('foo.js')", -1, &source);
  assert(e == 0);

  js_value_t *fn;
  e = js_compile_function(env, "fn", -1, file, -1, NULL, 0, 0, source, &fn);
  assert(e == 0);

  return fn;
}

static void
call(js_env_t *env, js_value_t *fn) {
  int e;

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *promise;
  e = js_call_function(env, global, fn, 0, NULL, &promise);
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

  e = js_on_dynamic_import(env, on_env_import, NULL);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  // A function with a handler of its own must reach that handler, and not the
  // one added for the environment.

  js_value_t *scoped = compile(env, "scoped.js");

  e = js_on_function_dynamic_import(env, scoped, on_function_import, (void *) 2);
  assert(e == 0);

  call(env, scoped);

  assert(function_calls == 1);
  assert(env_calls == 0);

  // A function with no handler of its own must fall back to the environment.

  js_value_t *plain = compile(env, "plain.js");

  call(env, plain);

  assert(function_calls == 1);
  assert(env_calls == 1);

  // A handler is write-once, so a second one must be refused.

  e = js_on_function_dynamic_import(env, scoped, on_env_import, NULL);
  assert(e != 0);

  js_value_t *exception;
  e = js_get_and_clear_last_exception(env, &exception);
  assert(e == 0);

  // Code run with `js_run_script()` is attributed to the shared default
  // identifier, so a function defined there has no unit of its own to register
  // against.

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "(function () { return import('foo.js') })", -1, &source);
  assert(e == 0);

  js_value_t *shared;
  e = js_run_script(env, "shared.js", -1, 0, source, &shared);
  assert(e == 0);

  e = js_on_function_dynamic_import(env, shared, on_function_import, (void *) 2);
  assert(e != 0);

  e = js_get_and_clear_last_exception(env, &exception);
  assert(e == 0);

  // It still imports, through the handler added for the environment.

  call(env, shared);

  assert(function_calls == 1);
  assert(env_calls == 2);

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
