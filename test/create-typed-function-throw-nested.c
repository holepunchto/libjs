#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static int typed_calls = 0;
static bool typed_throw = false;

uint32_t
on_typed_call(js_value_t *receiver, int32_t i, js_typed_callback_info_t *info) {
  int e;

  typed_calls++;

  if (i == 199999) {
    typed_throw = true;

    js_env_t *env;
    e = js_get_typed_callback_info(info, &env, NULL);
    assert(e == 0);

    e = js_throw_error(env, NULL, "err");
    assert(e == 0);
  }

  return 42;
}

js_value_t *
on_untyped_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  size_t argc = 1;
  js_value_t *argv[1];
  e = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(e == 0);

  int32_t i;
  e = js_get_value_int32(env, argv[0], &i);
  assert(e == 0);

  if (i == 199999) {
    e = js_throw_error(env, NULL, "err");
    assert(e == 0);

    return NULL;
  }

  js_value_t *result;
  e = js_create_uint32(env, 42, &result);
  assert(e == 0);

  return result;
}

js_value_t *
on_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  size_t argc = 1;
  js_value_t *argv[1];
  e = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *caught;
  e = js_call_function(env, global, argv[0], 0, NULL, &caught);
  assert(e == 0);

  // Back in the callback after JavaScript caught the error thrown in a fast
  // call: nothing may be pending here, or it would be thrown again when the
  // callback returns.

  bool has_exception;
  e = js_is_exception_pending(env, &has_exception);
  assert(e == 0);

  assert(!has_exception);

  return caught;
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
    .args = (int[]){
      js_object,
      js_int32,
    },
  };

  js_value_t *hello;
  e = js_create_typed_function(env, "hello", -1, on_untyped_call, &signature, on_typed_call, NULL, &hello);
  assert(e == 0);

  js_value_t *run;
  e = js_create_function(env, "run", -1, on_call, NULL, &run);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  e = js_set_named_property(env, global, "hello", hello);
  assert(e == 0);

  e = js_set_named_property(env, global, "run", run);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "run(() => { let i = 0, caught = false; while (i < 200000) { try { hello(i++) } catch (err) { caught = err.message === 'err' } } return caught })", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  bool caught;
  e = js_get_value_bool(env, result, &caught);
  assert(e == 0);

  assert(caught);

  assert(typed_calls > 0);
  assert(typed_throw);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
