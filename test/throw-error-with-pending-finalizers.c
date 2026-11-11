#include <assert.h>
#include <stdint.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_ref_t *error;

static int finalize_calls = 0;

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  finalize_calls++;
}

static js_value_t *
on_create(js_env_t *env, js_callback_info_t *info) {
  int e;

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  e = js_add_finalizer(env, object, NULL, on_finalize, NULL, NULL);
  assert(e == 0);

  return NULL;
}

static js_value_t *
on_throw(js_env_t *env, js_callback_info_t *info) {
  int e;

  js_value_t *value;
  e = js_get_reference_value(env, error, &value);
  assert(e == 0);

  e = js_throw(env, value);
  assert(e == 0);

  return NULL;
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

  js_value_t *value;
  e = js_create_string_utf8(env, (utf8_t *) "err", -1, &value);
  assert(e == 0);

  e = js_create_error(env, NULL, value, &value);
  assert(e == 0);

  e = js_create_reference(env, value, 1, &error);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *fn;
  e = js_create_function(env, "create", -1, on_create, NULL, &fn);
  assert(e == 0);

  e = js_set_named_property(env, global, "create", fn);
  assert(e == 0);

  e = js_create_function(env, "fail", -1, on_throw, NULL, &fn);
  assert(e == 0);

  e = js_set_named_property(env, global, "fail", fn);
  assert(e == 0);

  // Garbage collections that happen while the error is thrown run the second
  // pass of the finalizers left behind by the earlier calls, and none of them
  // may make the error get lost.

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "var thrown = 0; for (var i = 0; i < 200000; i++) { create(); try { fail() } catch (err) { thrown++ } } thrown", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  int32_t thrown;
  e = js_get_value_int32(env, result, &thrown);
  assert(e == 0);

  assert(finalize_calls > 0);

  assert(thrown == 200000);

  e = js_delete_reference(env, error);
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
