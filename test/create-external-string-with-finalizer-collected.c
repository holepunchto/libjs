#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

// Creates external strings across several segments of the environment's
// storage and checks that each is finalized once its string is collected, and
// that an empty one is finalized right away as the engine never keeps it. The
// engine may release the strings on a worker thread, but the finalizers must
// still run on the thread of the environment and be able to call into it.

static uv_thread_t main_thread;

static uint32_t finalize_count = 0;

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  int e;

  uv_thread_t thread = uv_thread_self();

  assert(uv_thread_equal(&thread, &main_thread));

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *object;
  e = js_create_object(env, &object);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  finalize_count++;
}

static latin1_t latin1[] = {'f', 'o', 'o'};
static utf16_t utf16le[] = {'f', 'o', 'o'};

static void
create_strings(js_env_t *env, uint32_t n) {
  int e;

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  for (uint32_t i = 0; i < n; i++) {
    js_value_t *string;

    e = js_create_external_string_latin1(env, latin1, 3, on_finalize, NULL, &string, NULL);
    assert(e == 0);

    e = js_create_external_string_utf16le(env, utf16le, 3, on_finalize, NULL, &string, NULL);
    assert(e == 0);
  }

  e = js_close_handle_scope(env, scope);
  assert(e == 0);
}

int
main() {
  int e;

  main_thread = uv_thread_self();

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

  create_strings(env, 100);

  e = js_request_garbage_collection(env);
  assert(e == 0);

  assert(finalize_count == 200);

  // A second round allocates from the storage the first one freed.
  create_strings(env, 100);

  e = js_request_garbage_collection(env);
  assert(e == 0);

  assert(finalize_count == 400);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *string;

  e = js_create_external_string_latin1(env, latin1, 0, on_finalize, NULL, &string, NULL);
  assert(e == 0);

  e = js_create_external_string_utf16le(env, utf16le, 0, on_finalize, NULL, &string, NULL);
  assert(e == 0);

  assert(finalize_count == 402);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
