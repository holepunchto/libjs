#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

// Verifies that a native function survives an isolate snapshot: the producer
// binds a native `answer()` onto the global and serializes a blob; a separate
// process boots from the blob and calls `globalThis.answer()`, expecting 42.
//
// Producer and consumer run in different processes (the test re-spawns itself)
// because this build enables V8's shared read-only heap, which makes in-process
// consumption of a freshly produced blob trip a debug-only checksum CHECK.

static js_value_t *
answer(js_env_t *env, js_callback_info_t *info) {
  int e;

  // Return the callback's `data`, so the test only passes if `data` is rebound
  // after restore (it would read NULL, and return 0, without a rebind handler).
  void *data;
  e = js_get_callback_info(env, info, NULL, NULL, NULL, &data);
  assert(e == 0);

  js_value_t *result;
  e = js_create_int32(env, (int32_t) (intptr_t) data, &result);
  assert(e == 0);

  return result;
}

static void
on_wrap_finalize(js_env_t *env, void *data, void *finalize_hint) {}

// A delegate getter that returns its data for any property.
static js_value_t *
delegate_get(js_env_t *env, js_value_t *property, void *data) {
  int e;

  js_value_t *result;
  e = js_create_int32(env, (int32_t) (intptr_t) data, &result);
  assert(e == 0);

  return result;
}

static bool
rebind(js_env_t *env, js_binding_type_t type, const void *cb, js_value_t *holder, void *context, js_binding_payload_t *result) {
  if (type == js_binding_function && cb == (const void *) answer) {
    result->data = (void *) (intptr_t) 42;

    return true;
  }

  if (type == js_binding_finalizer && cb == (const void *) on_wrap_finalize) {
    result->data = (void *) (intptr_t) 99;

    return true;
  }

  if (type == js_binding_delegate && cb == (const void *) delegate_get) {
    result->data = (void *) (intptr_t) 123;

    return true;
  }

  return false;
}

static void
on_meta(js_env_t *env, js_module_t *module, js_value_t *meta, void *data) {
  int e;

  js_value_t *value;
  e = js_create_int32(env, 7, &value);
  assert(e == 0);

  e = js_set_named_property(env, meta, "answer", value);
  assert(e == 0);
}

static js_external_references_t *
references() {
  int e;

  js_external_references_t *references;
  e = js_create_external_references(&references);
  assert(e == 0);

  e = js_add_external_reference(references, (const void *) answer);
  assert(e == 0);

  e = js_add_external_reference(references, (const void *) on_wrap_finalize);
  assert(e == 0);

  e = js_add_external_reference(references, (const void *) delegate_get);
  assert(e == 0);

  return references;
}

static int exit_status = -1;

static void
on_exit(uv_process_t *process, int64_t status, int signal) {
  exit_status = (int) status;

  uv_close((uv_handle_t *) process, NULL);
}

static void
produce(uv_loop_t *loop, js_platform_t *platform) {
  int e;

  js_snapshot_options_t options = {
    .version = 0,
    .external_references = references(),
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

  js_value_t *fn;
  e = js_create_function(env, "answer", -1, answer, (void *) (intptr_t) 42, &fn);
  assert(e == 0);

  e = js_set_named_property(env, global, "answer", fn);
  assert(e == 0);

  // Wrap an object reachable from the global, with non-NULL data and a finalizer.
  // After restore its finalizer side-table slot is rebuilt and the data rebound.
  js_value_t *wrapped;
  e = js_create_object(env, &wrapped);
  assert(e == 0);

  e = js_wrap(env, wrapped, (void *) (intptr_t) 99, on_wrap_finalize, NULL, NULL);
  assert(e == 0);

  e = js_set_named_property(env, global, "wrapped", wrapped);
  assert(e == 0);

  // Create a delegate reachable from the global. After restore its internal
  // field is reconstructed and its data rebound, so property access routes
  // through the rebound getter.
  js_delegate_callbacks_t callbacks = {
    .version = 0,
    .get = delegate_get,
  };

  js_value_t *delegate;
  e = js_create_delegate(env, &callbacks, (void *) (intptr_t) 123, NULL, NULL, &delegate);
  assert(e == 0);

  e = js_set_named_property(env, global, "delegate", delegate);
  assert(e == 0);

  // Exercise import.meta during warmup: a module reads import.meta.answer
  // (populated by the meta callback) and stashes it on the global. This proves
  // the producer installs the import.meta callback, and the value rides into the
  // snapshot. The module is deleted before serializing so it leaves no global
  // handles behind.
  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "globalThis.meta = import.meta.answer", -1, &source);
  assert(e == 0);

  js_module_t *module;
  e = js_create_module(env, "warmup.js", -1, 0, source, on_meta, NULL, &module);
  assert(e == 0);

  e = js_instantiate_module(env, module, NULL, NULL);
  assert(e == 0);

  js_value_t *promise;
  e = js_run_module(env, module, &promise);
  assert(e == 0);

  js_promise_state_t state;
  e = js_get_promise_state(env, promise, &state);
  assert(e == 0);

  assert(state == js_promise_fulfilled);

  e = js_delete_module(env, module);
  assert(e == 0);

  // A module that survives the snapshot: created, instantiated, and evaluated,
  // then left in place so its compiled record is serialized. Its id is stashed on
  // the global so the consumer can look it up.
  js_value_t *survivor_source;
  e = js_create_string_utf8(env, (utf8_t *) "export const answer = 42", -1, &survivor_source);
  assert(e == 0);

  js_module_t *survivor;
  e = js_create_module(env, "survivor.js", -1, 0, survivor_source, NULL, NULL, &survivor);
  assert(e == 0);

  e = js_instantiate_module(env, survivor, NULL, NULL);
  assert(e == 0);

  js_value_t *survivor_promise;
  e = js_run_module(env, survivor, &survivor_promise);
  assert(e == 0);

  js_promise_state_t survivor_state;
  e = js_get_promise_state(env, survivor_promise, &survivor_state);
  assert(e == 0);

  assert(survivor_state == js_promise_fulfilled);

  js_value_t *survivor_id;
  e = js_get_module_id(env, survivor, &survivor_id);
  assert(e == 0);

  e = js_set_named_property(env, global, "moduleId", survivor_id);
  assert(e == 0);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  void *data;
  size_t len;
  e = js_take_snapshot(env, &data, &len);
  assert(e == 0);

  // Write the blob to a temp file the child process boots from.

  char path[4096];
  size_t path_len = sizeof(path);
  e = uv_os_tmpdir(path, &path_len);
  assert(e == 0);

  strncat(path, "/libjs-boot-from-snapshot.blob", sizeof(path) - strlen(path) - 1);

  FILE *file = fopen(path, "wb");
  assert(file != NULL);

  assert(fwrite(data, 1, len, file) == len);

  fclose(file);

  free(data);

  // Re-spawn this executable in consume mode, pointed at the blob.

  char exepath[4096];
  size_t exepath_len = sizeof(exepath);
  e = uv_exepath(exepath, &exepath_len);
  assert(e == 0);

  e = uv_os_setenv("JS_SNAPSHOT_BLOB", path);
  assert(e == 0);

  char *args[] = {exepath, NULL};

  uv_process_options_t process_options = {0};
  process_options.file = exepath;
  process_options.args = args;
  process_options.exit_cb = on_exit;

  uv_process_t process;
  e = uv_spawn(loop, &process, &process_options);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);

  remove(path);

  assert(exit_status == 0);
}

static void
consume(uv_loop_t *loop, js_platform_t *platform, const char *path) {
  int e;

  FILE *file = fopen(path, "rb");
  assert(file != NULL);

  fseek(file, 0, SEEK_END);
  long len = ftell(file);
  fseek(file, 0, SEEK_SET);

  void *data = malloc(len);
  assert(fread(data, 1, len, file) == (size_t) len);

  fclose(file);

  js_rebind_handlers_t *handlers;
  e = js_create_rebind_handlers(&handlers);
  assert(e == 0);

  e = js_add_rebind_handler(handlers, rebind, NULL);
  assert(e == 0);

  js_env_options_t options = {
    .version = 1,
    .snapshot = data,
    .snapshot_len = (size_t) len,
    .external_references = references(),
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

  js_value_t *fn;
  e = js_get_named_property(env, global, "answer", &fn);
  assert(e == 0);

  js_value_t *result;
  e = js_call_function(env, global, fn, 0, NULL, &result);
  assert(e == 0);

  int32_t value;
  e = js_get_value_int32(env, result, &value);
  assert(e == 0);

  assert(value == 42);

  // The import.meta value computed during warmup survived the snapshot.
  js_value_t *meta;
  e = js_get_named_property(env, global, "meta", &meta);
  assert(e == 0);

  int32_t meta_value;
  e = js_get_value_int32(env, meta, &meta_value);
  assert(e == 0);

  assert(meta_value == 7);

  // The wrapped object survived: its finalizer side-table slot was rebuilt and
  // the data rebound, so js_unwrap returns it.
  js_value_t *wrapped;
  e = js_get_named_property(env, global, "wrapped", &wrapped);
  assert(e == 0);

  void *wrapped_data;
  e = js_unwrap(env, wrapped, &wrapped_data);
  assert(e == 0);

  assert((int32_t) (intptr_t) wrapped_data == 99);

  // The delegate survived: its internal field was reconstructed and data rebound,
  // so property access routes through the rebound getter.
  js_value_t *delegate;
  e = js_get_named_property(env, global, "delegate", &delegate);
  assert(e == 0);

  js_value_t *delegated;
  e = js_get_named_property(env, delegate, "anything", &delegated);
  assert(e == 0);

  int32_t delegated_value;
  e = js_get_value_int32(env, delegated, &delegated_value);
  assert(e == 0);

  assert(delegated_value == 123);

  // The module graph survived: the evaluated module is rebuilt from the snapshot,
  // looked up by the id stashed on the global, and its namespace still holds the
  // evaluated export.
  js_value_t *module_id;
  e = js_get_named_property(env, global, "moduleId", &module_id);
  assert(e == 0);

  js_module_t *module;
  e = js_get_module_by_id(env, module_id, &module);
  assert(e == 0);

  js_value_t *namespace;
  e = js_get_module_namespace(env, module, &namespace);
  assert(e == 0);

  js_value_t *module_answer;
  e = js_get_named_property(env, namespace, "answer", &module_answer);
  assert(e == 0);

  int32_t module_answer_value;
  e = js_get_value_int32(env, module_answer, &module_answer_value);
  assert(e == 0);

  assert(module_answer_value == 42);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_rebind_handlers(handlers);
  assert(e == 0);

  free(data);
}

int
main() {
  int e;

  uv_loop_t *loop = uv_default_loop();

  js_platform_t *platform;
  e = js_create_platform(loop, NULL, &platform);
  assert(e == 0);

  char path[4096];
  size_t path_len = sizeof(path);

  if (uv_os_getenv("JS_SNAPSHOT_BLOB", path, &path_len) == 0) {
    consume(loop, platform, path);
  } else {
    produce(loop, platform);
  }

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
