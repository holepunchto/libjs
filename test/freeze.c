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

  js_value_t *key;
  e = js_create_string_utf8(env, (utf8_t *) "x", -1, &key);
  assert(e == 0);

  js_value_t *value;
  e = js_create_uint32(env, 1, &value);
  assert(e == 0);

  e = js_set_property(env, object, key, value);
  assert(e == 0);

  e = js_freeze(env, object);
  assert(e == 0);

  // A frozen object cannot have existing properties reconfigured or changed.
  // Some engines silently ignore the write while others throw; tolerate both
  // and assert only that the value is left unchanged.
  js_value_t *other;
  e = js_create_uint32(env, 2, &other);
  assert(e == 0);

  e = js_set_property(env, object, key, other);

  if (e != 0) {
    js_value_t *exception;
    e = js_get_and_clear_last_exception(env, &exception);
    assert(e == 0);
  }

  js_value_t *result;
  e = js_get_property(env, object, key, &result);
  assert(e == 0);

  uint32_t n;
  e = js_get_value_uint32(env, result, &n);
  assert(e == 0);

  assert(n == 1);

  // A frozen object cannot have new properties added.
  js_value_t *added;
  e = js_create_string_utf8(env, (utf8_t *) "y", -1, &added);
  assert(e == 0);

  e = js_set_property(env, object, added, value);

  if (e != 0) {
    js_value_t *exception;
    e = js_get_and_clear_last_exception(env, &exception);
    assert(e == 0);
  }

  bool has;
  e = js_has_property(env, object, added, &has);
  assert(e == 0);

  assert(!has);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
