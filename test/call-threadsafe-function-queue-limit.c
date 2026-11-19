#include <assert.h>
#include <stdint.h>
#include <uv.h>

#include <js.h>

static intptr_t next;

static void
on_call(js_env_t *env, js_value_t *function, void *context, void *data) {
  assert((intptr_t) data == next);

  next++;
}

static void
check_limit(uv_loop_t *loop, js_env_t *env, size_t limit) {
  int e;

  next = 0;

  js_threadsafe_function_t *function;
  e = js_create_threadsafe_function(env, NULL, limit, 1, NULL, NULL, NULL, on_call, &function);
  assert(e == 0);

  for (size_t i = 0; i < limit; i++) {
    e = js_call_threadsafe_function(function, (void *) (intptr_t) i, js_threadsafe_function_nonblocking);
    assert(e == 0);
  }

  e = js_call_threadsafe_function(function, (void *) (intptr_t) limit, js_threadsafe_function_nonblocking);
  assert(e == js_queue_full);

  uv_run(loop, UV_RUN_NOWAIT);

  assert(next == (intptr_t) limit);

  e = js_call_threadsafe_function(function, (void *) (intptr_t) limit, js_threadsafe_function_nonblocking);
  assert(e == 0);

  e = js_release_threadsafe_function(function, js_threadsafe_function_release);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  assert(next == (intptr_t) limit + 1);
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

  check_limit(loop, env, 1);
  check_limit(loop, env, 2);
  check_limit(loop, env, 4);
  check_limit(loop, env, 5);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
