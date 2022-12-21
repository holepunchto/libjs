#include <assert.h>
#include <stdint.h>
#include <uv.h>

#include "../include/js.h"
#include "../include/js/ffi.h"

static int fast_calls = 0;
static int slow_calls = 0;

uint32_t
on_fast_call (uint32_t arg) {
  fast_calls++;

  return 42;
}

js_value_t *
on_slow_call (js_env_t *env, const js_callback_info_t *info) {
  slow_calls++;

  js_value_t *result;
  int e = js_create_uint32(env, 42, &result);
  assert(e == 0);

  return result;
}

int
main () {
  int e;

  js_ffi_type_info_t *return_info;
  e = js_ffi_create_type_info(js_ffi_uint32, js_ffi_scalar, &return_info);
  assert(e == 0);

  js_ffi_type_info_t *arg_info;
  e = js_ffi_create_type_info(js_ffi_uint32, js_ffi_scalar, &arg_info);
  assert(e == 0);

  js_ffi_function_info_t *function_info;
  e = js_ffi_create_function_info(return_info, &arg_info, 1, &function_info);
  assert(e == 0);

  js_ffi_function_t *ffi;
  e = js_ffi_create_function(on_fast_call, function_info, &ffi);
  assert(e == 0);

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, &env);
  assert(e == 0);

  js_value_t *fn;
  js_create_function_with_ffi(env, "hello", -1, on_slow_call, NULL, ffi, &fn);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  e = js_set_named_property(env, global, "hello", fn);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, "let i = 0, j; while (i++ < 20000) j = hello(42)", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, script, &result);
  assert(e == 0);

  printf("slow calls: %d\n", slow_calls);
  printf("fast calls: %d\n", fast_calls);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);
}
