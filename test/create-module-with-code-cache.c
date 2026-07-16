#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_module_t *bar;

static js_value_t *expected_id;

static js_module_t *
on_module_resolve(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data) {
  int e;

  utf8_t file[1024];
  e = js_get_value_string_utf8(env, specifier, file, 1024, NULL);
  assert(e == 0);

  assert(strcmp((char *) file, "bar.js") == 0);

  // The import must be attributed to the freshly minted identifier of the
  // cache-backed referrer module, proving the identifier mechanism survives a
  // code cache round-trip even though the cache itself carries no identifier.

  js_value_t *id;
  e = js_get_module_id(env, referrer, &id);
  assert(e == 0);

  bool equals;
  e = js_strict_equals(env, id, expected_id, &equals);
  assert(e == 0);

  assert(equals);

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "export default 42", -1, &source);
  assert(e == 0);

  e = js_create_module(env, "bar.js", -1, 0, source, NULL, NULL, &bar);
  assert(e == 0);

  return bar;
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
  e = js_create_string_utf8(env, (utf8_t *) "import bar from 'bar.js'", -1, &source);
  assert(e == 0);

  // Create a module and produce a code cache from it before it is evaluated.

  js_module_t *module;
  e = js_create_module(env, "test.js", -1, 0, source, NULL, NULL, &module);
  assert(e == 0);

  void *data;
  size_t len;
  e = js_create_module_code_cache(env, module, &data, &len);
  assert(e == 0);

  assert(data != NULL);
  assert(len > 0);

  e = js_delete_module(env, module);
  assert(e == 0);

  // Create a fresh module from the same source, feeding it the cache. The cache
  // must be accepted and a fresh identifier minted for it.

  bool cache_rejected;

  js_module_t *cached;
  e = js_create_module_with_code_cache(env, "test.js", -1, 0, source, data, len, &cache_rejected, NULL, NULL, &cached);
  assert(e == 0);

  assert(cache_rejected == false);

  e = js_get_module_id(env, cached, &expected_id);
  assert(e == 0);

  e = js_instantiate_module(env, cached, on_module_resolve, NULL);
  assert(e == 0);

  js_value_t *promise;
  e = js_run_module(env, cached, &promise);
  assert(e == 0);

  js_promise_state_t state;
  e = js_get_promise_state(env, promise, &state);
  assert(e == 0);

  assert(state == js_promise_fulfilled);

  e = js_delete_module(env, bar);
  assert(e == 0);

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
