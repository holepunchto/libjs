#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

int unhandled_called = 0;

static void
on_uncaught_exception(js_env_t *env, js_value_t *error, void *data) {
  assert(false);
}

static void
on_unhandled_rejection(js_env_t *env, js_value_t *reason, js_value_t *promise, void *data) {
  unhandled_called++;
}

static js_value_t *
on_noop(js_env_t *env, js_callback_info_t *info) {
  return NULL;
}

// Runs as a microtask, i.e. from within a microtask checkpoint. It rejects a
// freshly created deferred and only then attaches a handler, mirroring a native
// addon that returns an already-rejected promise which its JavaScript caller
// subsequently awaits. The synchronous rejection must not be reported as
// unhandled: it is handled within the same turn, before the checkpoint drains.
static void
on_microtask(js_env_t *env, void *data) {
  int e;

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_deferred_t *deferred;
  js_value_t *promise;
  e = js_create_promise(env, &deferred, &promise);
  assert(e == 0);

  js_value_t *reason;
  e = js_create_string_utf8(env, (utf8_t *) "err", -1, &reason);
  assert(e == 0);

  e = js_reject_deferred(env, deferred, reason);
  assert(e == 0);

  // Attach a handler after the rejection, as `await` would.
  js_value_t *then;
  e = js_get_named_property(env, promise, "then", &then);
  assert(e == 0);

  js_value_t *noop;
  e = js_create_function(env, "noop", -1, on_noop, NULL, &noop);
  assert(e == 0);

  js_value_t *argv[2] = {noop, noop};
  e = js_call_function(env, promise, then, 2, argv, NULL);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);
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

  e = js_on_uncaught_exception(env, on_uncaught_exception, NULL);
  assert(e == 0);

  e = js_on_unhandled_rejection(env, on_unhandled_rejection, NULL);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  // Queueing the microtask at the top level drains it immediately. The microtask
  // therefore runs from within a checkpoint, which is where a re-entrant drain
  // would previously report the handled rejection.
  e = js_queue_microtask_with_callback(env, on_microtask, NULL);
  assert(e == 0);

  assert(unhandled_called == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
