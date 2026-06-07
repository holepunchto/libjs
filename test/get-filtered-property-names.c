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

  js_value_t *one;
  e = js_create_uint32(env, 1, &one);
  assert(e == 0);

  e = js_set_named_property(env, object, "a", one);
  assert(e == 0);

  e = js_set_named_property(env, object, "b", one);
  assert(e == 0);

  js_value_t *names;
  e = js_get_filtered_property_names(
    env,
    object,
    js_key_own_only,
    js_property_only_enumerable,
    js_index_include_indices,
    js_key_convert_to_string,
    &names
  );
  assert(e == 0);

  uint32_t len;
  e = js_get_array_length(env, names, &len);
  assert(e == 0);

  assert(len == 2);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
