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

  js_value_t *value;
  e = js_create_uint32(env, 1, &value);
  assert(e == 0);

  js_value_t *key_utf8;
  e = js_create_property_key_utf8(env, (utf8_t *) "alpha", -1, &key_utf8);
  assert(e == 0);

  e = js_set_property(env, object, key_utf8, value);
  assert(e == 0);

  utf16_t beta[] = {'b', 'e', 't', 'a'};

  js_value_t *key_utf16;
  e = js_create_property_key_utf16le(env, beta, 4, &key_utf16);
  assert(e == 0);

  e = js_set_property(env, object, key_utf16, value);
  assert(e == 0);

  latin1_t gamma[] = {'g', 'a', 'm', 'm', 'a'};

  js_value_t *key_latin1;
  e = js_create_property_key_latin1(env, gamma, 5, &key_latin1);
  assert(e == 0);

  e = js_set_property(env, object, key_latin1, value);
  assert(e == 0);

  bool has;
  e = js_has_named_property(env, object, "alpha", &has);
  assert(e == 0);
  assert(has);

  e = js_has_named_property(env, object, "beta", &has);
  assert(e == 0);
  assert(has);

  e = js_has_named_property(env, object, "gamma", &has);
  assert(e == 0);
  assert(has);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
