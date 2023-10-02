#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static js_module_t *foo;
static js_module_t *bar;

static js_module_t *
on_module_resolve (js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data) {
  int e;

  utf8_t file[PATH_MAX];
  e = js_get_value_string_utf8(env, specifier, file, PATH_MAX, NULL);
  assert(e == 0);

  js_module_t *module = NULL;

  if (strcmp((char *) file, "foo.js") == 0) {
    module = foo;
  }

  if (strcmp((char *) file, "bar.js") == 0) {
    js_value_t *source;
    e = js_create_string_utf8(env, (utf8_t *) "import foo from 'foo.js'; export default 2", -1, &source);
    assert(e == 0);

    e = js_create_module(env, "bar.js", -1, 0, source, NULL, NULL, &bar);
    assert(e == 0);

    module = bar;
  }

  assert(module != NULL);

  return module;
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

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "import bar from 'bar.js'; export default 1", -1, &source);
  assert(e == 0);

  e = js_create_module(env, "foo.js", -1, 0, source, NULL, NULL, &foo);
  assert(e == 0);

  e = js_instantiate_module(env, foo, on_module_resolve, NULL);
  assert(e == 0);

  js_value_t *promise;
  e = js_run_module(env, foo, &promise);
  assert(e == 0);

  js_promise_state_t state;
  e = js_get_promise_state(env, promise, &state);
  assert(e == 0);

  assert(state == js_promise_fulfilled);

  e = js_delete_module(env, foo);
  assert(e == 0);

  e = js_delete_module(env, bar);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
