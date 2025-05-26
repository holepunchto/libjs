#include <assert.h>
#include <uv.h>

#include "../include/js.h"

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_platform_limits_t limits = {.version = 0};
  e = js_get_platform_limits(platform, &limits);
  assert(e == 0);

  printf("limits.arraybuffer_length=%lu\n", limits.arraybuffer_length);
  printf("limits.string_length=%lu\n", limits.string_length);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
