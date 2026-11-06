#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static bool typed_throw = false;
static int typed_calls_after_throw = 0;
static bool untyped_throw = false;

uint32_t
on_typed_call(js_value_t *receiver, js_typed_callback_info_t *info) {
  int e;

  js_env_t *env;
  e = js_get_typed_callback_info(info, &env, NULL);
  assert(e == 0);

  bool has_exception;
  e = js_is_exception_pending(env, &has_exception);
  assert(e == 0);

  // A fast call made after JavaScript caught the error thrown by an earlier
  // one must not see that error as pending.

  assert(!has_exception);

  if (typed_throw) {
    typed_calls_after_throw++;

    return 42;
  }

  typed_throw = true;

  e = js_throw_error(env, NULL, "err");
  assert(e == 0);

  e = js_is_exception_pending(env, &has_exception);
  assert(e == 0);

  assert(has_exception);

  e = js_throw_error(env, NULL, "again");
  assert(e == js_pending_exception);

  return 42;
}

js_value_t *
on_untyped_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  if (!untyped_throw) {
    untyped_throw = true;

    e = js_throw_error(env, NULL, "err");
    assert(e == 0);

    return NULL;
  }

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
    .args_len = 1,
    .args = (int[]){
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
  e = js_create_string_utf8(env, (utf8_t *) "(() => { let i = 0, caught = 0; while (i++ < 200000) { try { hello() } catch (err) { if (err.message === 'err') caught++ } } return caught })()", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  uint32_t caught;
  e = js_get_value_uint32(env, result, &caught);
  assert(e == 0);

  assert(caught == 2);

  assert(typed_throw);
  assert(typed_calls_after_throw > 0);

  // The error was thrown in a fast call and caught in JavaScript: nothing may
  // be left pending once the script is done.

  bool has_exception;
  e = js_is_exception_pending(env, &has_exception);
  assert(e == 0);

  assert(!has_exception);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
