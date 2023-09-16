#include <assert.h>
#include <uv.h>

#include "../include/js.h"

#include "fixtures/many-large-allocs.js.h"

int
main () {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, many_large_allocs_js, many_large_allocs_js_len, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, NULL);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
