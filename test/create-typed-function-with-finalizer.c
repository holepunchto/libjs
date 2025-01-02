#include <assert.h>
#include <stdint.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static bool finalize_called = false;

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  finalize_called = true;
}

uint32_t
on_typed_call(js_typed_callback_info_t *info) {
  return 42;
}

js_value_t *
on_untyped_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  js_value_t *result;
  e = js_create_uint32(env, 42, &result);
  assert(e == 0);

  return result;
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_options_t options = {
    .expose_garbage_collection = true,
    .trace_garbage_collection = true,
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

  js_callback_signature_t signature = {
    .result = js_uint32,
  };

  js_value_t *fn;
  e = js_create_typed_function(env, "hello", -1, on_untyped_call, &signature, on_typed_call, NULL, &fn);
  assert(e == 0);

  e = js_add_finalizer(env, fn, NULL, on_finalize, NULL, NULL);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_request_garbage_collection(env);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  (void) finalize_called; // Might not have run
}
