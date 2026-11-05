#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

typedef struct {
  js_ref_t *ref;
} holder_t;

static bool finalize_called = false;

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  finalize_called = true;

  free(data);
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_options_t options = {
    .expose_garbage_collection = true,
  };

  js_platform_t *platform;
  e = js_create_platform(loop, &options, &platform);
  assert(e == 0);

  js_env_t *env;
  e = js_create_env(loop, platform, NULL, &env);
  assert(e == 0);

  holder_t *holder = malloc(sizeof(holder_t));

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  e = js_add_finalizer(env, object, holder, on_finalize, NULL, NULL);
  assert(e == 0);

  e = js_create_reference(env, object, 1, &holder->ref);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  // Release the reference the way an owner embedded in the finalized memory
  // would, touching that memory after each step.
  uint32_t count;
  e = js_reference_unref(env, holder->ref, &count);
  assert(e == 0);

  assert(count == 0);
  assert(!finalize_called);

  e = js_delete_reference(env, holder->ref);
  assert(e == 0);

  holder->ref = NULL;

  assert(!finalize_called);

  e = js_request_garbage_collection(env);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  assert(finalize_called);
}
