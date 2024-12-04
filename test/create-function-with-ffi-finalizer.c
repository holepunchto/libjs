#include <assert.h>
#include <stdint.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"
#include "../include/js/ffi.h"

bool finalize_called = false;

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  finalize_called = true;
}

uint32_t
on_fast_call(void) {
  return 42;
}

js_value_t *
on_slow_call(js_env_t *env, js_callback_info_t *info) {
  js_value_t *result;
  int e = js_create_uint32(env, 42, &result);
  assert(e == 0);

  return result;
}

int
main() {
  int e;

  js_ffi_type_info_t *return_info;
  e = js_ffi_create_type_info(js_ffi_uint32, &return_info);
  assert(e == 0);

  js_ffi_type_info_t *arg_info;
  e = js_ffi_create_type_info(js_ffi_void, &arg_info);
  assert(e == 0);

  js_ffi_function_info_t *function_info;
  e = js_ffi_create_function_info(return_info, &arg_info, 1, &function_info);
  assert(e == 0);

  js_ffi_function_t *ffi;
  e = js_ffi_create_function(on_fast_call, function_info, &ffi);
  assert(e == 0);

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

  js_value_t *fn;
  e = js_create_function_with_ffi(env, "fn", -1, on_slow_call, NULL, ffi, &fn);
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

  assert(finalize_called);
}
