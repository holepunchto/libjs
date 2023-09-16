#include <assert.h>
#include <uv.h>

#include "../include/js.h"

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

  int64_t external_memory = 0;

  e = js_adjust_external_memory(env, 100, &external_memory);
  assert(e == 0);

  assert(external_memory == 100);

  e = js_adjust_external_memory(env, -50, &external_memory);
  assert(e == 0);

  assert(external_memory == 50);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
