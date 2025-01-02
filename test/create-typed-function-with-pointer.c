#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static int typed_calls = 0;
static int untyped_calls = 0;

uint32_t
on_typed_call(js_value_t *receiver, void *ptr, js_typed_callback_info_t *info) {
  typed_calls++;

  assert((uintptr_t) ptr == 42);

  return 42;
}

js_value_t *
on_untyped_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  untyped_calls++;

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
    .trace_optimizations = true,
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
    .args_len = 2,
    .args = (int[]) {
      js_object,
      js_external,
    },
  };

  js_value_t *fn;
  e = js_create_typed_function(env, "hello", -1, on_untyped_call, &signature, on_typed_call, NULL, &fn);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  e = js_set_named_property(env, global, "hello", fn);
  assert(e == 0);

  js_value_t *external;
  e = js_create_external(env, (void *) 42, NULL, NULL, &external);
  assert(e == 0);

  e = js_set_named_property(env, global, "external", external);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "let i = 0, j; while (i++ < 200000) j = hello(external)", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  assert(untyped_calls < 200000);
  assert(typed_calls > 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
