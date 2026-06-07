#include <assert.h>
#include <string.h>
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

  void *data;
  js_value_t *sharedarraybuffer;
  e = js_create_sharedarraybuffer(env, 32, &data, &sharedarraybuffer);
  assert(e == 0);

  memset(data, 0xab, 32);

  void *info_data;
  size_t info_len;
  e = js_get_sharedarraybuffer_info(env, sharedarraybuffer, &info_data, &info_len);
  assert(e == 0);

  assert(info_data == data);
  assert(info_len == 32);
  assert(((uint8_t *) info_data)[0] == 0xab);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
