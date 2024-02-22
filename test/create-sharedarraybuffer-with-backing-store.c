
#include <assert.h>
#include <utf.h>
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

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  uint8_t *data_a, *data_b;

  js_arraybuffer_backing_store_t *backing_store;

  {
    js_value_t *sharedarraybuffer;
    e = js_create_sharedarraybuffer(env, 1, (void **) &data_a, &sharedarraybuffer);
    assert(e == 0);

    e = js_get_sharedarraybuffer_backing_store(env, sharedarraybuffer, &backing_store);
    assert(e == 0);
  }

  {
    js_value_t *sharedarraybuffer;
    e = js_create_sharedarraybuffer_with_backing_store(env, backing_store, (void **) &data_b, NULL, &sharedarraybuffer);
    assert(e == 0);
  }

  data_a[0] = 42;

  assert(data_b[0] == 42);

  e = js_release_arraybuffer_backing_store(env, backing_store);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
