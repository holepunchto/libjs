#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_value_t *
on_construct (js_env_t *env, js_callback_info_t *info) {
  return NULL;
}

int
main () {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  js_value_t *property;
  e = js_create_uint32(env, 42, &property);
  assert(e == 0);

  js_property_descriptor_t properties[] = {
    {
      .name = "foo",
      .value = property,
    },
  };

  js_value_t *class;
  e = js_define_class(env, "Foo", -1, on_construct, NULL, properties, 1, &class);
  assert(e == 0);

  js_value_t *instance;
  e = js_new_instance(env, class, 0, NULL, &instance);
  assert(e == 0);

  js_value_t *result;
  e = js_get_named_property(env, instance, "foo", &result);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, result, &value);
  assert(e == 0);

  assert(value == 42);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
