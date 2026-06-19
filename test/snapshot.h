#ifndef JS_TEST_SNAPSHOT_H
#define JS_TEST_SNAPSHOT_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "../include/js.h"

// Shared harness for the snapshot round-trip tests.
//
// Each test warms up an environment in a producer process, serializes it to a
// blob, and re-spawns the same executable in consume mode to boot from the
// blob and assert the warmed-up state survived.
//
// Producer and consumer run in different processes because this build enables
// V8's shared read-only heap, which makes in-process consumption of a freshly
// produced blob trip a debug-only checksum CHECK.

// Registers any external references the test needs (native callbacks reachable
// from the snapshot). Called in both the producer and the consumer.
typedef void (*snapshot_references_cb)(js_external_references_t *references);

// Warms up the environment before it is serialized. `global` is the global
// object, already inside an open handle scope.
typedef void (*snapshot_produce_cb)(js_env_t *env, js_value_t *global);

// Asserts the warmed-up state survived. `global` is the restored global object,
// already inside an open handle scope.
typedef void (*snapshot_consume_cb)(js_env_t *env, js_value_t *global);

typedef struct {
  // Distinguishes this test's blob file from other tests running in parallel.
  const char *name;

  // Optional; registers native callbacks reachable from the snapshot.
  snapshot_references_cb references;

  // Optional; rebinds per-instance `data` after restore.
  js_rebind_cb rebind;

  snapshot_produce_cb produce;
  snapshot_consume_cb consume;
} snapshot_test_t;

static inline js_external_references_t *
snapshot__references(const snapshot_test_t *test) {
  if (test->references == NULL) return NULL;

  int e;

  js_external_references_t *references;
  e = js_create_external_references(&references);
  assert(e == 0);

  test->references(references);

  return references;
}

static inline void
snapshot__blob_path(const snapshot_test_t *test, char *path, size_t path_len) {
  snprintf(path, path_len, "test/fixtures/snapshots/%s", test->name);
}

static inline void *
snapshot__read(uv_loop_t *loop, const char *path, size_t *len) {
  int e;

  uv_fs_t req;

  int fd = uv_fs_open(loop, &req, path, UV_FS_O_RDONLY, 0, NULL);
  assert(fd >= 0);

  uv_fs_req_cleanup(&req);

  e = uv_fs_fstat(loop, &req, fd, NULL);
  assert(e == 0);

  size_t size = (size_t) req.statbuf.st_size;

  uv_fs_req_cleanup(&req);

  void *data = malloc(size);
  assert(data != NULL);

  size_t offset = 0;

  while (offset < size) {
    uv_buf_t buf = uv_buf_init((char *) data + offset, size - offset);

    e = uv_fs_read(loop, &req, fd, &buf, 1, offset, NULL);
    assert(e > 0);

    uv_fs_req_cleanup(&req);

    offset += (size_t) e;
  }

  e = uv_fs_close(loop, &req, fd, NULL);
  assert(e == 0);

  uv_fs_req_cleanup(&req);

  *len = size;

  return data;
}

static inline void
snapshot__write(uv_loop_t *loop, const char *path, void *data, size_t len) {
  int e;

  uv_fs_t req;

  int fd = uv_fs_open(loop, &req, path, UV_FS_O_WRONLY | UV_FS_O_CREAT | UV_FS_O_TRUNC, 0644, NULL);
  assert(fd >= 0);

  uv_fs_req_cleanup(&req);

  size_t offset = 0;

  while (offset < len) {
    uv_buf_t buf = uv_buf_init((char *) data + offset, len - offset);

    e = uv_fs_write(loop, &req, fd, &buf, 1, offset, NULL);
    assert(e > 0);

    uv_fs_req_cleanup(&req);

    offset += (size_t) e;
  }

  e = uv_fs_close(loop, &req, fd, NULL);
  assert(e == 0);

  uv_fs_req_cleanup(&req);
}

static int snapshot__exit_status = -1;

static inline void
snapshot__on_exit(uv_process_t *process, int64_t status, int signal) {
  snapshot__exit_status = (int) status;

  uv_close((uv_handle_t *) process, NULL);
}

static inline void
snapshot__produce(const snapshot_test_t *test, uv_loop_t *loop, js_platform_t *platform) {
  int e;

  js_snapshot_options_t options = {
    .version = 0,
    .external_references = snapshot__references(test),
  };

  js_env_t *env;
  e = js_create_snapshot(loop, platform, &options, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  test->produce(env, global);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  void *data;
  size_t len;
  e = js_take_snapshot(env, &data, &len);
  assert(e == 0);

  // Write the blob to a file the child process boots from.

  char path[4096];
  snapshot__blob_path(test, path, sizeof(path));

  snapshot__write(loop, path, data, len);

  free(data);

  // Re-spawn this executable in consume mode, pointed at the blob.

  char exepath[4096];
  size_t exepath_len = sizeof(exepath);
  e = uv_exepath(exepath, &exepath_len);
  assert(e == 0);

  e = uv_os_setenv("SNAPSHOT_BLOB", path);
  assert(e == 0);

  char *args[] = {exepath, NULL};

  uv_process_options_t process_options = {
    .file = exepath,
    .args = args,
    .exit_cb = snapshot__on_exit,
  };

  uv_process_t process;
  e = uv_spawn(loop, &process, &process_options);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  // The blob is left in place so it can be inspected after the test runs.

  assert(snapshot__exit_status == 0);
}

static inline void
snapshot__consume(const snapshot_test_t *test, uv_loop_t *loop, js_platform_t *platform, const char *path) {
  int e;

  size_t len;
  void *data = snapshot__read(loop, path, &len);

  js_rebind_handlers_t *handlers = NULL;

  if (test->rebind != NULL) {
    e = js_create_rebind_handlers(&handlers);
    assert(e == 0);

    e = js_add_rebind_handler(handlers, test->rebind, NULL);
    assert(e == 0);
  }

  js_env_options_t options = {
    .version = 1,
    .snapshot = data,
    .snapshot_len = len,
    .external_references = snapshot__references(test),
    .rebind_handlers = handlers,
  };

  js_env_t *env;
  e = js_create_env(loop, platform, &options, &env);
  assert(e == 0);

  js_handle_scope_t *scope;
  e = js_open_handle_scope(env, &scope);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  test->consume(env, global);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  if (handlers != NULL) {
    e = js_destroy_rebind_handlers(handlers);
    assert(e == 0);
  }

  free(data);
}

// Runs the producer, or the consumer if booted with `SNAPSHOT_BLOB` set
// (as the producer re-spawns itself).
static inline void
snapshot_test_run(const snapshot_test_t *test) {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  char path[4096];
  size_t path_len = sizeof(path);

  if (uv_os_getenv("SNAPSHOT_BLOB", path, &path_len) == 0) {
    snapshot__consume(test, loop, platform, path);
  } else {
    snapshot__produce(test, loop, platform);
  }

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}

#endif // JS_TEST_SNAPSHOT_H
