#include <assert.h>
#include <utf.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that the module graph survives an isolate snapshot: the producer
// creates, instantiates, and evaluates a module, leaving it in place so its
// compiled record is serialized, and stashes its id on the global. The consumer
// rebuilds the module from the snapshot, looks it up by id, and reads its
// evaluated export from the namespace.

static void
produce(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "export const answer = 42", -1, &source);
  assert(e == 0);

  js_module_t *module;
  e = js_create_module(env, "survivor.js", -1, 0, source, NULL, NULL, &module);
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

  js_value_t *id;
  e = js_get_module_id(env, module, &id);
  assert(e == 0);

  e = js_set_named_property(env, global, "moduleId", id);
  assert(e == 0);
}

static void
consume(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *id;
  e = js_get_named_property(env, global, "moduleId", &id);
  assert(e == 0);

  js_module_t *module;
  e = js_get_module_by_id(env, id, &module);
  assert(e == 0);

  js_value_t *namespace;
  e = js_get_module_namespace(env, module, &namespace);
  assert(e == 0);

  js_value_t *answer;
  e = js_get_named_property(env, namespace, "answer", &answer);
  assert(e == 0);

  int32_t value;
  e = js_get_value_int32(env, answer, &value);
  assert(e == 0);

  assert(value == 42);
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot-module",
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
