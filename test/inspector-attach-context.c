#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

static int messages = 0;

static void
on_response(js_env_t *env, js_inspector_t *inspector, const char *message, size_t len, void *data) {
  messages++;
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

  js_context_t *context;
  e = js_create_context(env, &context);
  assert(e == 0);

  js_inspector_t *inspector;
  e = js_create_inspector(env, &inspector);
  assert(e == 0);

  e = js_on_inspector_response(env, inspector, on_response, NULL);
  assert(e == 0);

  e = js_connect_inspector(env, inspector);
  assert(e == 0);

  e = js_attach_context_to_inspector(env, inspector, context, "extra", 5);
  assert(e == 0);

  e = js_detach_context_from_inspector(env, inspector, context);
  assert(e == 0);

  e = js_destroy_inspector(env, inspector);
  assert(e == 0);

  e = js_destroy_context(env, context);
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
