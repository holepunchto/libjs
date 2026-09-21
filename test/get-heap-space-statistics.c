#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
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

  if (len > 0) {
    js_heap_space_statistics_t *statistics = calloc(len, sizeof(js_heap_space_statistics_t));
    e = js_get_heap_space_statistics(env, statistics, len, 0, NULL);
    assert(e == 0);

    assert(strlen(statistics[0].space_name) > 0);

    // A space can never hold more than has been committed for it, and at least
    // one of them will have room to spare. The guard catches a conversion that
    // reports the used size as the committed size.
    bool has_spare = false;

    for (size_t i = 0; i < len; i++) {
      assert(statistics[i].space_size >= statistics[i].space_used_size);

      if (statistics[i].space_size > statistics[i].space_used_size) {
        has_spare = true;
      }
    }

    assert(has_spare);

    free(statistics);
  }

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
