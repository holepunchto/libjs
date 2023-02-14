#include <assert.h>
#include <uv.h>

#include "../include/js.h"

bool uncaught_called = false;

static void
on_uncaught_exception (js_env_t *env, js_value_t *error, void *data) {
  uncaught_called = true;
}

static js_module_t *
on_module_resolve (js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data) {
  int e;

  js_value_t *export_names[1];
  e = js_create_string_utf8(env, "bar", -1, &export_names[0]);
  assert(e == 0);

  js_module_t *module;
  e = js_create_synthetic_module(env, "synthetic", -1, export_names, 1, NULL, NULL, &module);
  assert(e == 0);

  return module;
}

int
main () {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, &env);
  assert(e == 0);

  e = js_on_uncaught_exception(env, on_uncaught_exception, NULL);
  assert(e == 0);

  js_value_t *source;
  e = js_create_string_utf8(env, "import { foo } from 'foo.js'", -1, &source);
  assert(e == 0);

  js_module_t *module;
  e = js_create_module(env, "test.js", -1, 0, source, &module);
  assert(e == 0);

  e = js_instantiate_module(env, module, on_module_resolve, NULL);
  assert(e != 0);

  assert(uncaught_called);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}