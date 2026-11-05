#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

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

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  e = js_set_named_property(env, global, "object", object);
  assert(e == 0);

  js_ref_t *ref;
  e = js_create_reference(env, object, 0, &ref);
  assert(e == 0);

  for (int i = 0; i < 4; i++) {
    e = js_reference_ref(env, ref, NULL);
    assert(e == 0);

    e = js_reference_unref(env, ref, NULL);
    assert(e == 0);
  }

  e = js_reference_ref(env, ref, NULL);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "Reflect.ownKeys(object).length", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  uint32_t length;
  e = js_get_value_uint32(env, result, &length);
  assert(e == 0);

  assert(length == 0);

  e = js_delete_reference(env, ref);
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
