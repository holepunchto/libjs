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

  js_value_t *fn;
  e = js_compile_function(env, NULL, 0, "test.js", -1, args, 1, 0, source, &fn);
  assert(e == 0);

  void *data;
  size_t len;
  e = js_create_function_code_cache(env, fn, &data, &len);
  assert(e == 0);

  // Consume the cache against a different source. The cache is a hint, not
  // correctness: the engine must reject the stale cache and silently recompile
  // from the new source, yielding the new value.
  //
  // The engine keys the cache on the source length (not its full text), so the
  // replacement must differ in length to be reliably rejected.

  js_value_t *other;
  e = js_create_string_utf8(env, (utf8_t *) "return n * 200", -1, &other);
  assert(e == 0);

  bool cache_rejected;

  js_value_t *cached;
  e = js_compile_function_with_code_cache(env, NULL, 0, "test.js", -1, args, 1, 0, other, data, len, &cache_rejected, &cached);
  assert(e == 0);

  assert(cache_rejected == true);

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

  assert(value == 8400);

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
