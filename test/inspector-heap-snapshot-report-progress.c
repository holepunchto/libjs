#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

// Regression test for a crash when taking a heap snapshot with progress
// reporting enabled. `HeapProfiler.takeHeapSnapshot` with `reportProgress: true`
// causes V8 to emit `HeapProfiler.reportHeapSnapshotProgress` notifications from
// within snapshot generation, while the stack is being scanned and JavaScript
// execution is disallowed. If the inspector channel delivers those messages by
// synchronously re-entering JavaScript (as a real embedder's callback does),
// the process crashes. The runtime must instead defer delivery to a safe point.

static js_ref_t *fn = NULL;

static int progress_called = 0;
static int chunk_called = 0;
static int response_called = 0;

static js_value_t *
noop(js_env_t *env, js_callback_info_t *info) {
  return NULL;
}

static void
on_response(js_env_t *env, js_inspector_t *inspector, const char *message, size_t len, void *data) {
  int e;

  if (strstr(message, "reportHeapSnapshotProgress") != NULL) {
    progress_called++;
  } else if (strstr(message, "addHeapSnapshotChunk") != NULL) {
    chunk_called++;
  } else if (strstr(message, "\"id\":2") != NULL) {
    response_called++;
  }

  // Re-enter JavaScript from the callback, exactly as an embedder that forwards
  // inspector messages into a JS session does. On an unfixed runtime this call
  // happens while snapshot generation forbids execution and crashes.
  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *callback;
  e = js_get_reference_value(env, fn, &callback);
  assert(e == 0);

  js_value_t *result;
  e = js_call_function(env, global, callback, 0, NULL, &result);
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

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *callback;
  e = js_create_function(env, "noop", -1, noop, NULL, &callback);
  assert(e == 0);

  e = js_create_reference(env, callback, 1, &fn);
  assert(e == 0);

  js_inspector_t *inspector;
  e = js_create_inspector(env, &inspector);
  assert(e == 0);

  e = js_on_inspector_response(env, inspector, on_response, NULL);
  assert(e == 0);

  e = js_connect_inspector(env, inspector);
  assert(e == 0);

  const char *enable = "{ \"id\": 1, \"method\": \"HeapProfiler.enable\" }";

  e = js_send_inspector_request(env, inspector, enable, -1);
  assert(e == 0);

  const char *snapshot = "{ \"id\": 2, \"method\": \"HeapProfiler.takeHeapSnapshot\", \"params\": { \"reportProgress\": true } }";

  e = js_send_inspector_request(env, inspector, snapshot, -1);
  assert(e == 0);

  // Drive the loop so the scheduled snapshot task runs and its queued messages
  // are delivered.
  for (int i = 0; i < 10000 && response_called == 0; i++) {
    uv_run(loop, UV_RUN_NOWAIT);
  }

  assert(progress_called > 0);
  assert(chunk_called > 0);
  assert(response_called == 1);

  e = js_delete_reference(env, fn);
  assert(e == 0);

  e = js_destroy_inspector(env, inspector);
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
