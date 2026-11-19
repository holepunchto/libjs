#include <assert.h>
#include <stdint.h>
#include <uv.h>

#include <js.h>

static uint8_t data[1] = {42};

static int finalize_calls = 0;

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  finalize_calls++;
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env_a;
  e = js_create_env(loop, platform, NULL, &env_a);
  assert(e == 0);

  js_env_t *env_b;
  e = js_create_env(loop, platform, NULL, &env_b);
  assert(e == 0);

  js_arraybuffer_backing_store_t *backing_store;

  {
    js_handle_scope_t *scope;
    e = js_open_handle_scope(env_a, &scope);
    assert(e == 0);

    js_value_t *arraybuffer;
    e = js_create_external_arraybuffer(env_a, data, sizeof(data), on_finalize, NULL, &arraybuffer);
    assert(e == 0);

    e = js_get_arraybuffer_backing_store(env_a, arraybuffer, &backing_store);
    assert(e == 0);

    e = js_close_handle_scope(env_a, scope);
    assert(e == 0);
  }

  e = js_destroy_env(env_a);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  assert(finalize_calls == 0);

  e = js_release_arraybuffer_backing_store(env_b, backing_store);
  assert(e == 0);

  e = js_destroy_env(env_b);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  assert(finalize_calls == 1);
}
