#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

#define len 16

static int typed_calls = 0;

uint32_t
on_typed_call(js_value_t *receiver, js_typed_callback_info_t *info) {
  typed_calls++;

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

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_callback_signature_t signature = {
    .result = js_uint32,
    .args_len = 1,
    .args = (int[]){
      js_object,
    },
  };

  // Every one of these is left outstanding, so the environment has to carry a
  // whole arena of signatures across the teardown of the isolate.
  for (int i = 0; i < len; i++) {
    char name[16];
    snprintf(name, sizeof(name), "hello%d", i);

    js_value_t *fn;
    e = js_create_typed_function(env, name, -1, on_untyped_call, &signature, on_typed_call, NULL, &fn);
    assert(e == 0);

    e = js_set_named_property(env, global, name, fn);
    assert(e == 0);
  }

  // Enough calls for the engine to optimize and take the typed path, which is
  // what makes it read the signatures from a compiler thread.
  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "var i = 0, j; while (i++ < 200000) j = hello0()", -1, &source);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, source, &result);
  assert(e == 0);

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
