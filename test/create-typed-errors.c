#include <assert.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static void
assert_error_message(js_env_t *env, js_value_t *error, const char *expected) {
  int e;

  js_value_t *message;
  e = js_get_named_property(env, error, "message", &message);
  assert(e == 0);

  utf8_t buf[64];
  size_t len;
  e = js_get_value_string_utf8(env, message, buf, sizeof(buf), &len);
  assert(e == 0);

  assert(len == strlen(expected));
  assert(memcmp(buf, expected, len) == 0);
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

  js_value_t *msg;
  e = js_create_string_utf8(env, (utf8_t *) "msg", -1, &msg);
  assert(e == 0);

  js_value_t *err;
  e = js_create_error(env, NULL, msg, &err);
  assert(e == 0);
  assert_error_message(env, err, "msg");

  js_value_t *type_err;
  e = js_create_type_error(env, NULL, msg, &type_err);
  assert(e == 0);
  assert_error_message(env, type_err, "msg");

  js_value_t *range_err;
  e = js_create_range_error(env, NULL, msg, &range_err);
  assert(e == 0);
  assert_error_message(env, range_err, "msg");

  js_value_t *syntax_err;
  e = js_create_syntax_error(env, NULL, msg, &syntax_err);
  assert(e == 0);
  assert_error_message(env, syntax_err, "msg");

  js_value_t *ref_err;
  e = js_create_reference_error(env, NULL, msg, &ref_err);
  assert(e == 0);
  assert_error_message(env, ref_err, "msg");

  bool is_error;
  e = js_is_error(env, err, &is_error);
  assert(e == 0 && is_error);
  e = js_is_error(env, type_err, &is_error);
  assert(e == 0 && is_error);
  e = js_is_error(env, range_err, &is_error);
  assert(e == 0 && is_error);
  e = js_is_error(env, syntax_err, &is_error);
  assert(e == 0 && is_error);
  e = js_is_error(env, ref_err, &is_error);
  assert(e == 0 && is_error);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
