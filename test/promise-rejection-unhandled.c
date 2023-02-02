#include <assert.h>
#include <stdbool.h>
#include <uv.h>

#include "../include/js.h"
#include "fixtures/promise-rejection-unhandled.js.h"

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

  js_value_t *script;
  e = js_create_string_utf8(env, (char *) promise_rejection_unhandled_js, promise_rejection_unhandled_js_len, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, script, &result);
  assert(e == 0);

  assert(unhandled_called);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
