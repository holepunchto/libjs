#include <assert.h>
#include <utf.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that import.meta works during warmup and the value it produced rides
// into the snapshot: the producer evaluates a module that reads
// import.meta.answer (populated by the meta callback) and stashes it on the
// global. The module is deleted before serializing so it leaves no global
// handles behind. The consumer reads the stashed value.

static void
on_meta(js_env_t *env, js_module_t *module, js_value_t *meta, void *data) {
  int e;

  js_value_t *value;
  e = js_create_int32(env, 7, &value);
  assert(e == 0);

  e = js_set_named_property(env, meta, "answer", value);
  assert(e == 0);
}

static void
produce(js_env_t *env, js_value_t *global) {
  int e;

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
}

static void
consume(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *meta;
  e = js_get_named_property(env, global, "meta", &meta);
  assert(e == 0);

  int32_t value;
  e = js_get_value_int32(env, meta, &value);
  assert(e == 0);

  assert(value == 7);
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot-import-meta",
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
