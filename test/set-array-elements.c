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

  js_value_t *array;
  e = js_create_array_with_length(env, 3, &array);
  assert(e == 0);

  js_value_t *values[3];
  for (uint32_t i = 0; i < 3; i++) {
    e = js_create_uint32(env, i + 1, &values[i]);
    assert(e == 0);
  }

  e = js_set_array_elements(env, array, (const js_value_t **) values, 3, 0);
  assert(e == 0);

  for (uint32_t i = 0; i < 3; i++) {
    js_value_t *element;
    e = js_get_element(env, array, i, &element);
    assert(e == 0);

    uint32_t n;
    e = js_get_value_uint32(env, element, &n);
    assert(e == 0);

    assert(n == i + 1);
  }

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
