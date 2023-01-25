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
  e = js_create_env(loop, platform, &env);
  assert(e == 0);

  uint8_t *data;

  js_value_t *arraybuffer;
  e = js_create_unsafe_arraybuffer(env, 4, (void **) &data, &arraybuffer);
  assert(e == 0);

  for (int i = 0; i < 4; i++) {
    printf("0x%02X\n", data[i]);
  }

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
