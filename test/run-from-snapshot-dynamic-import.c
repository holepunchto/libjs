#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that dynamic import() works after booting from a snapshot: the
// producer warms up a plain global, and the consumer registers a dynamic import
// handler and runs a script that imports a module. The import is resolved
// asynchronously from a timer, so it only completes once the event loop runs,
// proving the restored environment drives dynamic import end to end.
//
// The import handler is set at runtime with js_on_dynamic_import() rather than
// serialized, so the consumer reinstates it after restore.

static js_env_t *import_env;
static js_deferred_t *import_deferred;
static js_value_t *import_namespace;
static uv_timer_t import_timer;
static bool import_resolved;

static void
on_module_evaluate(js_env_t *env, js_module_t *module, void *data) {
  int e;

  js_value_t *name;
  e = js_create_string_utf8(env, (utf8_t *) "answer", -1, &name);
  assert(e == 0);

  js_value_t *value;
  e = js_create_uint32(env, 42, &value);
  assert(e == 0);

  e = js_set_module_export(env, module, name, value);
  assert(e == 0);
}

// Fires from the event loop and resolves the pending dynamic import, so the
// import only completes once the loop has run.
static void
on_timer(uv_timer_t *timer) {
  int e;

  e = js_resolve_deferred(import_env, import_deferred, import_namespace);
  assert(e == 0);

  import_resolved = true;

  uv_close((uv_handle_t *) timer, NULL);
}

// Builds and evaluates the requested module synchronously, but defers handing
// back its namespace until a timer fires on the event loop.
static js_value_t *
on_import(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, js_value_t *id, void *data) {
  int e;

  js_value_t *export_names[1];
  e = js_create_string_utf8(env, (utf8_t *) "answer", -1, &export_names[0]);
  assert(e == 0);

  js_module_t *module;
  e = js_create_synthetic_module(env, "synthetic", -1, export_names, 1, on_module_evaluate, NULL, &module);
  assert(e == 0);

  e = js_instantiate_module(env, module, NULL, NULL);
  assert(e == 0);

  js_value_t *evaluated;
  e = js_run_module(env, module, &evaluated);
  assert(e == 0);

  e = js_get_module_namespace(env, module, &import_namespace);
  assert(e == 0);

  js_value_t *promise;
  e = js_create_promise(env, &import_deferred, &promise);
  assert(e == 0);

  import_env = env;

  uv_loop_t *loop;
  e = js_get_env_loop(env, &loop);
  assert(e == 0);

  e = uv_timer_init(loop, &import_timer);
  assert(e == 0);

  e = uv_timer_start(&import_timer, on_timer, 0, 0);
  assert(e == 0);

  return promise;
}

static void
produce(js_env_t *env, js_value_t *global) {
  int e;

  // Seed the slot the consumer overwrites once the import resolves, to prove the
  // import ran after restore rather than during warmup.
  js_value_t *value;
  e = js_create_uint32(env, 0, &value);
  assert(e == 0);

  e = js_set_named_property(env, global, "answer", value);
  assert(e == 0);
}

static void
consume(js_env_t *env, js_value_t *global) {
  int e;

  e = js_on_dynamic_import(env, on_import, NULL);
  assert(e == 0);

  js_value_t *script;
  e = js_create_string_utf8(env, (utf8_t *) "import('foo.js').then((ns) => { globalThis.answer = ns.answer })", -1, &script);
  assert(e == 0);

  js_value_t *result;
  e = js_run_script(env, "test.js", -1, 0, script, &result);
  assert(e == 0);

  // The import is still pending; it resolves only once the timer fires.
  js_value_t *answer;
  e = js_get_named_property(env, global, "answer", &answer);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, answer, &value);
  assert(e == 0);

  assert(value == 0);

  // Run the loop until the import resolves.
  uv_loop_t *loop;
  e = js_get_env_loop(env, &loop);
  assert(e == 0);

  while (!import_resolved) {
    e = uv_run(loop, UV_RUN_ONCE);
    assert(e >= 0);
  }

  e = js_get_named_property(env, global, "answer", &answer);
  assert(e == 0);

  e = js_get_value_uint32(env, answer, &value);
  assert(e == 0);

  assert(value == 42);
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot-dynamic-import",
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
