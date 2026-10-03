#include <assert.h>
#include <stdint.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that a native function survives an isolate snapshot: the producer
// binds a native `answer()` onto the global, and the consumer calls it. The
// function returns its `data`, which is NULL until rebound, so the test only
// passes if the rebind handler reinstates it after restore.

static js_value_t *
answer(js_env_t *env, js_callback_info_t *info) {
  int e;

  void *data;
  e = js_get_callback_info(env, info, NULL, NULL, NULL, &data);
  assert(e == 0);

  js_value_t *result;
  e = js_create_int32(env, (int32_t) (intptr_t) data, &result);
  assert(e == 0);

  return result;
}

static bool
rebind(js_env_t *env, const js_rebind_info_t *info, void *context, js_binding_payload_t *result) {
  if (info->type == js_binding_function && info->function.cb == answer) {
    result->data = (void *) (intptr_t) 42;

    return true;
  }

  return false;
}

static void
references(js_external_references_t *references) {
  int e;

  e = js_add_external_reference(references, (const void *) answer);
  assert(e == 0);
}

static void
produce(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *fn;
  e = js_create_function(env, "answer", -1, answer, (void *) (intptr_t) 42, &fn);
  assert(e == 0);

  e = js_set_named_property(env, global, "answer", fn);
  assert(e == 0);
}

static void
consume(js_env_t *env, js_value_t *global) {
  int e;

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
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot-function",
    .references = references,
    .rebind = rebind,
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
