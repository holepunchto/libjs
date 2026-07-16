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
  e = js_create_string_utf8(env, (utf8_t *) "42", -1, &source);
  assert(e == 0);

  // Prepare a script and produce a code cache from it.

  js_script_t *script;
  e = js_prepare_script(env, "test.js", -1, 0, source, &script);
  assert(e == 0);

  void *data;
  size_t len;
  e = js_create_script_code_cache(env, script, &data, &len);
  assert(e == 0);

  assert(data != NULL);
  assert(len > 0);

  e = js_delete_script(env, script);
  assert(e == 0);

  // Prepare a fresh script from the same source, feeding it the cache. The
  // cache must be accepted and the script must run to the expected value.

  bool cache_rejected;

  js_script_t *cached;
  e = js_prepare_script_with_code_cache(env, "test.js", -1, 0, source, data, len, &cache_rejected, &cached);
  assert(e == 0);

  assert(cache_rejected == false);

  js_value_t *result;
  e = js_run_prepared_script(env, cached, &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  assert(value == 42);

  e = js_delete_script(env, cached);
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
