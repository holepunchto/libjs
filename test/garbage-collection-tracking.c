#include <assert.h>
#include <uv.h>

#include "../include/js.h"

static bool gc_start_called = false;
static bool gc_end_called = false;

static void
on_start(js_garbage_collection_type_t type, void *data) {
  gc_start_called = true;

  assert((intptr_t) data == 42);
}

static void
on_end(js_garbage_collection_type_t type, void *data) {
  gc_end_called = true;

  assert((intptr_t) data == 42);
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_options_t options = {
    .expose_garbage_collection = true,
  };

  js_platform_t *platform;
  e = js_create_platform(loop, &options, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_garbage_collection_tracking_options_t gc_tracking_options = {
    .start_cb = on_start,
    .end_cb = on_end,
  };

  js_garbage_collection_tracking_t *gc_tracking;
  e = js_enable_garbage_collection_tracking(env, &gc_tracking_options, (void *) 42, &gc_tracking);
  assert(e == 0);

  e = js_request_garbage_collection(env);
  assert(e == 0);

  assert(gc_start_called);

  assert(gc_end_called);

  e = js_disable_garbage_collection_tracking(env, gc_tracking);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
