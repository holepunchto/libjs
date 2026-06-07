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

  js_value_t *name;
  e = js_create_string_utf8(env, (utf8_t *) "constant", -1, &name);
  assert(e == 0);

  js_value_t *value;
  e = js_create_uint32(env, 42, &value);
  assert(e == 0);

  js_property_descriptor_t properties[] = {
    {
      .name = name,
      .value = value,
      .attributes = js_enumerable,
    },
  };

  e = js_define_properties(env, object, properties, 1);
  assert(e == 0);

  js_value_t *result;
  e = js_get_named_property(env, object, "constant", &result);
  assert(e == 0);

  uint32_t n;
  e = js_get_value_uint32(env, result, &n);
  assert(e == 0);

  assert(n == 42);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
