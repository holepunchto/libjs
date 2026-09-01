#include <assert.h>
#include <stdlib.h>
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

  js_platform_limits_t limits = {
    .version = 0,
  };

  e = js_get_platform_limits(platform, &limits);
  assert(e == 0);

  size_t len = limits.string_length + 1;

  // Only the first byte is ever read, as the length is rejected before the
  // string is constructed.
  latin1_t *data = malloc(1);
  assert(data != NULL);

  data[0] = 'a';

  js_value_t *string;
  e = js_create_string_latin1(env, data, len, &string);
  assert(e == js_pending_exception);

  js_value_t *error;
  e = js_get_and_clear_last_exception(env, &error);
  assert(e == 0);

  bool is_error;
  e = js_is_error(env, error, &is_error);
  assert(e == 0);
  assert(is_error);

  free(data);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
