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
  e = js_create_string_utf8(env, (utf8_t *) "return 1", -1, &source);
  assert(e == 0);

  // The engine rejects an argument name that is not an identifier without
  // throwing, so the failure has to come with an exception of our own.

  const char *names[] = {"", "a b", "a) { return 1 } function b("};

  for (size_t i = 0; i < 3; i++) {
    js_value_t *args[1];
    e = js_create_string_utf8(env, (utf8_t *) names[i], -1, &args[0]);
    assert(e == 0);

    js_value_t *fn = (js_value_t *) 42;
    e = js_compile_function(env, NULL, 0, "test.js", -1, args, 1, 0, source, &fn);
    assert(e == js_pending_exception);

    assert(fn == (js_value_t *) 42);

    bool has_exception;
    e = js_is_exception_pending(env, &has_exception);
    assert(e == 0);

    assert(has_exception);

    js_value_t *error;
    e = js_get_and_clear_last_exception(env, &error);
    assert(e == 0);
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
