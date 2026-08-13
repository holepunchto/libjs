#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

// An object of no particular kind is plain `js_object`, including a function,
// which is classified by `js_typeof()` instead.
static const char *sources[] = {
  "({})",
  "Object.create(null)",
  "new (function Foo () {})()",
  "(function () {})",
};

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

  for (size_t i = 0, n = sizeof(sources) / sizeof(sources[0]); i < n; i++) {
    js_value_t *script;
    e = js_create_string_utf8(env, (utf8_t *) sources[i], -1, &script);
    assert(e == 0);

    js_value_t *value;
    e = js_run_script(env, NULL, 0, 0, script, &value);
    assert(e == 0);

    js_object_type_t type;
    e = js_get_object_type(env, value, &type);
    assert(e == 0);

    assert(type == js_object);
  }

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  js_object_type_t type;
  e = js_get_object_type(env, object, &type);
  assert(e == 0);

  assert(type == js_object);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
