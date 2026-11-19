#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <uv.h>

#include <js.h>

#define CALLS 1000

static js_threadsafe_function_t *function;

static intptr_t next = 0;
static bool finalize_called = false;

static void
on_call(js_env_t *env, js_value_t *function, void *context, void *data) {
  assert((intptr_t) data == next);

  next++;
}

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  finalize_called = true;
}

static void
on_thread(void *data) {
  int e;

  for (intptr_t i = 0; i < CALLS; i++) {
    e = js_call_threadsafe_function(function, (void *) i, js_threadsafe_function_blocking);
    assert(e == 0);
  }

  e = js_release_threadsafe_function(function, js_threadsafe_function_release);
  assert(e == 0);
}

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

  e = js_create_threadsafe_function(env, NULL, 1, 1, on_finalize, NULL, NULL, on_call, &function);
  assert(e == 0);

  uv_thread_t thread;
  e = uv_thread_create(&thread, on_thread, NULL);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  e = uv_thread_join(&thread);
  assert(e == 0);

  assert(next == CALLS);
  assert(finalize_called);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
