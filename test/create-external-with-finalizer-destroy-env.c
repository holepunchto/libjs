#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static bool teardown_called = false;
static int finalize_calls = 0;

static void
on_teardown(void *data) {
  teardown_called = true;
}

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  finalize_calls++;

  assert(teardown_called);
  assert((intptr_t) data == 42);
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

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *external;
  e = js_create_external(env, (void *) 42, on_finalize, NULL, &external);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  e = js_set_named_property(env, global, "retained", external);
  assert(e == 0);

  e = js_add_teardown_callback(env, on_teardown, NULL);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  assert(finalize_calls == 1);
}
