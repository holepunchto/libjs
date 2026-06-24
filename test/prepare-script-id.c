#include <assert.h>
#include <stdbool.h>
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

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "null", -1, &source);
  assert(e == 0);

  js_script_t *first;
  e = js_prepare_script(env, "first.js", -1, 0, source, &first);
  assert(e == 0);

  js_script_t *second;
  e = js_prepare_script(env, "second.js", -1, 0, source, &second);
  assert(e == 0);

  // The identifier is stable across repeated lookups of the same script.

  js_value_t *first_id;
  e = js_get_script_id(env, first, &first_id);
  assert(e == 0);

  js_value_t *first_id_again;
  e = js_get_script_id(env, first, &first_id_again);
  assert(e == 0);

  bool equals;
  e = js_strict_equals(env, first_id, first_id_again, &equals);
  assert(e == 0);

  assert(equals);

  // Distinct scripts carry distinct identifiers.

  js_value_t *second_id;
  e = js_get_script_id(env, second, &second_id);
  assert(e == 0);

  e = js_strict_equals(env, first_id, second_id, &equals);
  assert(e == 0);

  assert(equals == false);

  e = js_delete_script(env, first);
  assert(e == 0);

  e = js_delete_script(env, second);
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
