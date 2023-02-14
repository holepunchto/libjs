#include <assert.h>
#include <uv.h>

#include "../include/js.h"

bool unhandled_called = false;

static void
on_unhandled_rejection (js_env_t *env, js_value_t *reason, js_value_t *promise, void *data) {
  unhandled_called = true;
}

int
main () {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, &env);
  assert(e == 0);

  e = js_on_unhandled_rejection(env, on_unhandled_rejection, NULL);
  assert(e == 0);

  js_value_t *source;
  e = js_create_string_utf8(env, "throw 'err'", -1, &source);
  assert(e == 0);

  js_module_t *module;
  e = js_create_module(env, "test.js", -1, 0, source, &module);
  assert(e == 0);

  e = js_instantiate_module(env, module, NULL, NULL);
  assert(e == 0);

  js_value_t *promise;
  e = js_run_module(env, module, &promise);
  assert(e == 0);

  assert(unhandled_called);

  js_promise_state_t state;
  e = js_get_promise_state(env, promise, &state);
  assert(e == 0);

  assert(state == js_promise_rejected);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}