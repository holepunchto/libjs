#include <assert.h>
#include <uv.h>

#include "../include/js.h"

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

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_delegate_callbacks_t callbacks = {0};

  js_value_t *delegate;
  e = js_create_delegate(env, &callbacks, NULL, NULL, NULL, &delegate);
  assert(e == 0);

  js_type_tag_t tag = {1, 2};
  e = js_add_type_tag(env, delegate, &tag);
  assert(e == 0);

  bool result;
  e = js_check_type_tag(env, delegate, &tag, &result);
  assert(e == 0);

  assert(result);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
