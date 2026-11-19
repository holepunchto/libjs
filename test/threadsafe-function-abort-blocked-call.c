#include <assert.h>
#include <stdint.h>
#include <uv.h>

#include <js.h>

static js_threadsafe_function_t *function;

static intptr_t received = 0;
static int calls = 0;
static int finalize_calls = 0;

static void
on_call(js_env_t *env, js_value_t *function, void *context, void *data) {
  assert(env != NULL);

  received = (intptr_t) data;
  calls++;
}

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  finalize_calls++;
}

static void
on_thread(void *data) {
  int e;

  e = js_call_threadsafe_function(function, (void *) 2, js_threadsafe_function_blocking);
  assert(e == js_closing);
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

  e = js_create_threadsafe_function(env, NULL, 1, 2, on_finalize, NULL, NULL, on_call, &function);
  assert(e == 0);

  e = js_call_threadsafe_function(function, (void *) 1, js_threadsafe_function_nonblocking);
  assert(e == 0);

  uv_thread_t thread;
  e = uv_thread_create(&thread, on_thread, NULL);
  assert(e == 0);

  e = js_release_threadsafe_function(function, js_threadsafe_function_abort);
  assert(e == 0);

  e = uv_thread_join(&thread);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  assert(calls == 1);
  assert(received == 1);
  assert(finalize_calls == 1);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
