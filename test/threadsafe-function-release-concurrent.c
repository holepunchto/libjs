#include <assert.h>
#include <stdint.h>
#include <uv.h>

#include <js.h>

#define FUNCTIONS 2000

static js_threadsafe_function_t *functions[FUNCTIONS];

static uint32_t calls = 0;
static uint32_t finalized = 0;

static void
on_call(js_env_t *env, js_value_t *function, void *context, void *data) {
  calls++;
}

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  finalized++;
}

static void
on_thread(void *data) {
  int e;

  for (int i = 0; i < FUNCTIONS; i++) {
    e = js_call_threadsafe_function(functions[i], NULL, js_threadsafe_function_nonblocking);
    assert(e == 0);

    e = js_release_threadsafe_function(functions[i], js_threadsafe_function_release);
    assert(e == 0);
  }
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

  for (int i = 0; i < FUNCTIONS; i++) {
    e = js_create_threadsafe_function(env, NULL, 0, 1, on_finalize, NULL, NULL, on_call, &functions[i]);
    assert(e == 0);
  }

  uv_thread_t thread;
  e = uv_thread_create(&thread, on_thread, NULL);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  e = uv_thread_join(&thread);
  assert(e == 0);

  assert(calls == FUNCTIONS);
  assert(finalized == FUNCTIONS);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
