#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static int typed_calls = 0;
static int untyped_calls = 0;

uint32_t
on_typed_call(js_value_t *receiver, js_value_t *dataview, js_typed_callback_info_t *info) {
  int e;

  typed_calls++;

  js_env_t *env;
  e = js_get_typed_callback_info(info, &env, NULL);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  uint8_t *data;
  size_t len;
  e = js_get_dataview_info(env, dataview, (void **) &data, &len, NULL, NULL);
  assert(e == 0);

  assert(len == 4);
  assert(data[0] == 1);
  assert(data[1] == 2);
  assert(data[2] == 3);
  assert(data[3] == 4);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

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
      js_object,
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

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "let i = 0, j; while (i++ < 200000) j = hello(new DataView(Uint8Array.from([1, 2, 3, 4]).buffer))", -1, &script);
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
