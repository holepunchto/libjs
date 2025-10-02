#include <assert.h>
#include <string.h>
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

  size_t len;
  e = js_get_heap_space_statistics(env, NULL, 0, 0, &len);
  assert(e == 0);

  assert(len > 0);

  js_heap_space_statistics_t *statistics = malloc(len * sizeof(js_heap_space_statistics_t *));
  e = js_get_heap_space_statistics(env, statistics, len, 0, NULL);
  assert(e == 0);

  assert(strlen(statistics[0].space_name) > 0);

  free(statistics);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
