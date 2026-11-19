#include <assert.h>
#include <stdint.h>
#include <uv.h>

#include <js.h>

#define CALLS 100

static intptr_t next = 0;

static void
on_call(js_env_t *env, js_value_t *function, void *context, void *data) {
  assert((intptr_t) data == next);

  next++;
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

  js_threadsafe_function_t *function;
  e = js_create_threadsafe_function(env, NULL, 0, 1, NULL, NULL, NULL, on_call, &function);
  assert(e == 0);

  for (intptr_t i = 0; i < CALLS; i++) {
    e = js_call_threadsafe_function(function, (void *) i, js_threadsafe_function_nonblocking);
    assert(e == 0);
  }

  e = js_release_threadsafe_function(function, js_threadsafe_function_release);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  assert(next == CALLS);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
