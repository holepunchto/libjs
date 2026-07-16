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

  js_value_t *args[1];

  e = js_create_string_utf8(env, (utf8_t *) "n", -1, &args[0]);
  assert(e == 0);

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "return n * 2", -1, &source);
  assert(e == 0);

  // Compile a function and produce a code cache from it.

  js_value_t *fn;
  e = js_compile_function(env, NULL, 0, "test.js", -1, args, 1, 0, source, &fn);
  assert(e == 0);

  void *data;
  size_t len;
  e = js_create_function_code_cache(env, fn, &data, &len);
  assert(e == 0);

  assert(data != NULL);
  assert(len > 0);

  // Compile a fresh function from the same source, feeding it the cache. The
  // cache must be accepted and the function must call to the expected value.

  bool cache_rejected;

  js_value_t *cached;
  e = js_compile_function_with_code_cache(env, NULL, 0, "test.js", -1, args, 1, 0, source, data, len, &cache_rejected, &cached);
  assert(e == 0);

  assert(cache_rejected == false);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *argv[1];
  e = js_create_uint32(env, 42, &argv[0]);
  assert(e == 0);

  js_value_t *result;
  e = js_call_function(env, global, cached, 1, argv, &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  assert(value == 84);

  // The identifier is re-minted on the consume side; a cache-backed function
  // carries a valid identifier of its own just as a freshly compiled one does.

  js_value_t *id;
  e = js_get_function_id(env, cached, &id);
  assert(e == 0);

  bool is_symbol;
  e = js_is_symbol(env, id, &is_symbol);
  assert(e == 0);

  assert(is_symbol);

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
