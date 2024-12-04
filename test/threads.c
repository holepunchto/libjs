#include <assert.h>
#include <uv.h>

#include "../include/js.h"

js_platform_t *platform;

void
on_thread(void *data) {
  int e;

  uv_loop_t loop;
  e = uv_loop_init(&loop);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(&loop, platform, NULL, &env);
  assert(e == 0);

  e = uv_run(&loop, UV_RUN_DEFAULT);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = uv_run(&loop, UV_RUN_DEFAULT);
  assert(e == 0);

  e = uv_loop_close(&loop);
  assert(e == 0);
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  uv_thread_t thread;
  e = uv_thread_create(&thread, on_thread, NULL);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  uv_thread_join(&thread);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
