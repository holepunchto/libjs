#include <assert.h>
#include <stdbool.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that a prepared script restored from a snapshot can still be given a
// dynamic import() handler of its own. The handler lives in the host-defined
// options the engine embedded in the script when it was compiled, so the
// consumer can only register one if the very same options array came back with
// the script; a fresh array would leave the engine consulting the original and
// fall back to the environment handler instead.
//
// The environment handler is registered too, and must never run: that is what
// tells the per-unit handler apart from the fallback.

static js_env_t *import_env;
static js_deferred_t *import_deferred;
static js_ref_t *import_namespace;
static uv_timer_t import_timer;
static bool import_resolved;
static bool unit_import_called;

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

static void
on_timer(uv_timer_t *timer) {
  int e;

  js_handle_scope_t *scope;
  e = js_open_handle_scope(import_env, &scope);
  assert(e == 0);

  js_value_t *ns;
  e = js_get_reference_value(import_env, import_namespace, &ns);
  assert(e == 0);

  e = js_resolve_deferred(import_env, import_deferred, ns);
  assert(e == 0);

  e = js_delete_reference(import_env, import_namespace);
  assert(e == 0);

  e = js_close_handle_scope(import_env, scope);
  assert(e == 0);

  import_resolved = true;

  uv_close((uv_handle_t *) timer, NULL);
}

static js_value_t *
on_unit_import(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, js_value_t *id, void *data) {
  int e;

  unit_import_called = true;

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

  js_value_t *ns;
  e = js_get_module_namespace(env, module, &ns);
  assert(e == 0);

  e = js_create_reference(env, ns, 1, &import_namespace);
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

// Registered for the environment as a whole, and only reached if the script's
// own handler was lost with its host-defined options.
static js_value_t *
on_env_import(js_env_t *env, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, js_value_t *id, void *data) {
  assert(false);

  return NULL;
}

static void
produce(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "import('foo.js').then((ns) => { globalThis.answer = ns.answer })", -1, &source);
  assert(e == 0);

  js_script_t *script;
  e = js_prepare_script(env, "importer.js", -1, 0, source, &script);
  assert(e == 0);

  js_value_t *id;
  e = js_get_script_id(env, script, &id);
  assert(e == 0);

  e = js_set_named_property(env, global, "scriptId", id);
  assert(e == 0);

  // Seeded so the consumer can tell the import ran after restore.
  js_value_t *value;
  e = js_create_uint32(env, 0, &value);
  assert(e == 0);

  e = js_set_named_property(env, global, "answer", value);
  assert(e == 0);
}

static void
consume(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *id;
  e = js_get_named_property(env, global, "scriptId", &id);
  assert(e == 0);

  js_script_t *script;
  e = js_get_script_by_id(env, id, &script);
  assert(e == 0);

  e = js_on_dynamic_import(env, on_env_import, NULL);
  assert(e == 0);

  e = js_on_script_dynamic_import(env, script, on_unit_import, NULL);
  assert(e == 0);

  js_value_t *result;
  e = js_run_prepared_script(env, script, &result);
  assert(e == 0);

  uv_loop_t *loop;
  e = js_get_env_loop(env, &loop);
  assert(e == 0);

  while (!import_resolved) {
    e = uv_run(loop, UV_RUN_ONCE);
    assert(e >= 0);
  }

  assert(unit_import_called);

  js_value_t *answer;
  e = js_get_named_property(env, global, "answer", &answer);
  assert(e == 0);

  uint32_t value;
  e = js_get_value_uint32(env, answer, &value);
  assert(e == 0);

  assert(value == 42);

  e = js_delete_script(env, script);
  assert(e == 0);
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot-script-dynamic-import",
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
