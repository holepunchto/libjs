#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

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
  e = js_create_string_utf8(env, (utf8_t *) "export const foo = 42", -1, &source);
  assert(e == 0);

  js_module_t *module;
  e = js_create_module(env, "test.js", -1, 0, source, NULL, NULL, &module);
  assert(e == 0);

  void *data;
  size_t len;
  e = js_create_module_code_cache(env, module, &data, &len);
  assert(e == 0);

  e = js_delete_module(env, module);
  assert(e == 0);

  // Consume the cache against a different source. The stale cache must be
  // rejected and the module recompiled from the new source.
  //
  // The engine keys the cache on the source length (not its full text), so the
  // replacement must differ in length to be reliably rejected.

  js_value_t *other;
  e = js_create_string_utf8(env, (utf8_t *) "export const foo = 4321", -1, &other);
  assert(e == 0);

  bool cache_rejected;

  js_module_t *cached;
  e = js_create_module_with_code_cache(env, "test.js", -1, 0, other, data, len, &cache_rejected, NULL, NULL, &cached);
  assert(e == 0);

  assert(cache_rejected == true);

  e = js_instantiate_module(env, cached, NULL, NULL);
  assert(e == 0);

  js_value_t *result;
  e = js_run_module(env, cached, &result);
  assert(e == 0);

  js_value_t *namespace;
  e = js_get_module_namespace(env, cached, &namespace);
  assert(e == 0);

  js_value_t *name;
  e = js_create_string_utf8(env, (utf8_t *) "foo", -1, &name);
  assert(e == 0);

  js_value_t *foo;
  e = js_get_property(env, namespace, name, &foo);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, foo, &value);
  assert(e == 0);

  assert(value == 4321);

  e = js_delete_module(env, cached);
  assert(e == 0);

  free(data);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
