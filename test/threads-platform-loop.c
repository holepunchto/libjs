#include <assert.h>
#include <uv.h>

#include "../include/js.h"

#include "fixtures/many-large-allocs.js.h"

static uv_sem_t ready;
static uv_async_t async;

static js_platform_t *platform;

static void
on_async(uv_async_t *handle) {
  uv_close((uv_handle_t *) handle, NULL);
}

static void
on_thread(void *data) {
  int e;

  uv_loop_t loop;
  e = uv_loop_init(&loop);
  assert(e == 0);

  e = js_create_platform(&loop, NULL, &platform);
  assert(e == 0);

  e = uv_async_init(&loop, &async, on_async);
  assert(e == 0);

  uv_sem_post(&ready);

  e = uv_run(&loop, UV_RUN_DEFAULT);
  assert(e == 0);

  printf("platform loop done\n");

  e = js_destroy_platform(platform);
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

  e = uv_sem_init(&ready, 0);
  assert(e == 0);

  uv_thread_t thread;
  e = uv_thread_create(&thread, on_thread, NULL);
  assert(e == 0);

  uv_sem_wait(&ready);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  // Give the environment a bunch to do.

  js_value_t *script;
  e = js_create_string_utf8(env, many_large_allocs_js, many_large_allocs_js_len, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, NULL);
  assert(e == 0);

  printf("allocation done\n");

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = uv_async_send(&async);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  uv_thread_join(&thread);
}
