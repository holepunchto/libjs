#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

// Regression test: a response callback that destroys the inspector sessions
// while queued messages are still being delivered must not crash the runtime.

static js_inspector_t *a = NULL;
static js_inspector_t *b = NULL;

static bool destroyed = false;

static void
on_response(js_env_t *env, js_inspector_t *inspector, const char *message, size_t len, void *data) {
  int e;

  if (destroyed) return;

  if (strstr(message, "addHeapSnapshotChunk") != NULL) {
    destroyed = true;

    e = js_destroy_inspector(env, a);
    assert(e == 0);

    e = js_destroy_inspector(env, b);
    assert(e == 0);
  }
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

  e = js_create_inspector(env, &a);
  assert(e == 0);

  e = js_on_inspector_response(env, a, on_response, NULL);
  assert(e == 0);

  e = js_connect_inspector(env, a);
  assert(e == 0);

  e = js_create_inspector(env, &b);
  assert(e == 0);

  e = js_on_inspector_response(env, b, on_response, NULL);
  assert(e == 0);

  e = js_connect_inspector(env, b);
  assert(e == 0);

  const char *enable = "{ \"id\": 1, \"method\": \"HeapProfiler.enable\" }";

  e = js_send_inspector_request(env, a, enable, -1);
  assert(e == 0);

  // reportProgress schedules the snapshot as a task, so its messages are
  // delivered from the macrotask flush point, where the first chunk callback
  // destroys both sessions while more messages are still queued.
  const char *snapshot = "{ \"id\": 2, \"method\": \"HeapProfiler.takeHeapSnapshot\", \"params\": { \"reportProgress\": true } }";

  e = js_send_inspector_request(env, a, snapshot, -1);
  assert(e == 0);

  for (int i = 0; i < 10000 && !destroyed; i++) {
    uv_run(loop, UV_RUN_NOWAIT);
  }

  assert(destroyed);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
