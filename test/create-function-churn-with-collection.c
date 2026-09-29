#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <uv.h>

#include "../include/js.h"

#define rounds 20

#define len 500

#define survivors 10

static int finalized = 0;

static int survivor_data[survivors];

static void
on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  finalized++;
}

static js_value_t *
on_call(js_env_t *env, js_callback_info_t *info) {
  int e;

  int *data;
  e = js_get_callback_info(env, info, NULL, NULL, NULL, (void **) &data);
  assert(e == 0);

  js_value_t *result;
  e = js_create_int32(env, *data, &result);
  assert(e == 0);

  return result;
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

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  // A few of each are kept reachable from the start, so that they sit at the
  // bottom of their arrays while everything above them is collected, freed and
  // allocated again.
  for (int i = 0; i < survivors; i++) {
    survivor_data[i] = i;

    char name[16];
    snprintf(name, sizeof(name), "survivor%d", i);

    js_value_t *fn;
    e = js_create_function(env, name, -1, on_call, &survivor_data[i], &fn);
    assert(e == 0);

    e = js_wrap(env, fn, &survivor_data[i], on_finalize, NULL, NULL);
    assert(e == 0);

    e = js_add_finalizer(env, fn, &survivor_data[i], on_finalize, NULL, NULL);
    assert(e == 0);

    e = js_set_named_property(env, global, name, fn);
    assert(e == 0);
  }

  // Collecting between rounds frees slots from weak callbacks and shrinks the
  // arrays, while the next round allocates into whatever was left behind.
  for (int round = 0; round < rounds; round++) {
    js_handle_scope_t *scope;
    e = js_open_handle_scope(env, &scope);
    assert(e == 0);

    for (int i = 0; i < len; i++) {
      js_value_t *fn;
      e = js_create_function(env, NULL, 0, on_call, &survivor_data[0], &fn);
      assert(e == 0);

      js_value_t *object;
      e = js_create_object(env, &object);
      assert(e == 0);

      e = js_wrap(env, object, NULL, on_finalize, NULL, NULL);
      assert(e == 0);

      e = js_add_finalizer(env, object, NULL, on_finalize, NULL, NULL);
      assert(e == 0);
    }

    e = js_close_handle_scope(env, scope);
    assert(e == 0);

    e = js_request_garbage_collection(env);
    assert(e == 0);
  }

  assert(finalized > 0);

  for (int i = 0; i < survivors; i++) {
    char name[16];
    snprintf(name, sizeof(name), "survivor%d", i);

    js_value_t *fn;
    e = js_get_named_property(env, global, name, &fn);
    assert(e == 0);

    js_value_t *result;
    e = js_call_function(env, global, fn, 0, NULL, &result);
    assert(e == 0);

    int32_t actual;
    e = js_get_value_int32(env, result, &actual);
    assert(e == 0);

    assert(actual == i);

    int *data;
    e = js_unwrap(env, fn, (void **) &data);
    assert(e == 0);

    assert(data == &survivor_data[i]);
  }

  // The last round is left for the environment to sweep, along with the
  // survivors.
  for (int i = 0; i < len; i++) {
    js_value_t *fn;
    e = js_create_function(env, NULL, 0, on_call, &survivor_data[0], &fn);
    assert(e == 0);

    js_value_t *object;
    e = js_create_object(env, &object);
    assert(e == 0);

    e = js_wrap(env, object, NULL, on_finalize, NULL, NULL);
    assert(e == 0);

    e = js_add_finalizer(env, object, NULL, on_finalize, NULL, NULL);
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
