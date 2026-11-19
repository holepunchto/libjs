#include <assert.h>
#include <stdint.h>
#include <utf.h>

#include "../include/js.h"
#include "snapshot.h"

// Verifies that a promise reaction created while warming up keeps the
// continuation snapshot it captured: the producer creates a pending reaction
// with a continuation variable set, and the consumer settles the promise and
// asserts that the reaction sees the variable while the top level does not.

static uint32_t seen = 0;

static js_value_t *
get_key(js_env_t *env) {
  int e;

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  js_value_t *key;
  e = js_get_named_property(env, global, "key", &key);
  assert(e == 0);

  return key;
}

static js_value_t *
on_fulfilled(js_env_t *env, js_callback_info_t *info) {
  int e;

  js_value_t *result;
  e = js_get_continuation_variable(env, get_key(env), &result);
  assert(e == 0);

  e = js_get_value_uint32(env, result, &seen);
  assert(e == 0);

  return NULL;
}

static void
references(js_external_references_t *references) {
  int e;

  e = js_add_external_reference(references, (const void *) on_fulfilled);
  assert(e == 0);
}

static void
produce(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *key;
  e = js_create_symbol(env, NULL, &key);
  assert(e == 0);

  e = js_set_named_property(env, global, "key", key);
  assert(e == 0);

  js_value_t *value;
  e = js_create_uint32(env, 42, &value);
  assert(e == 0);

  js_continuation_scope_t *scope;
  e = js_open_continuation_variable_scope(env, key, value, &scope);
  assert(e == 0);

  js_value_t *source;
  e = js_create_string_utf8(env, (utf8_t *) "new Promise(function (resolve) { globalThis.resolve = resolve })", -1, &source);
  assert(e == 0);

  js_value_t *promise;
  e = js_run_script(env, NULL, 0, 0, source, &promise);
  assert(e == 0);

  js_value_t *callback;
  e = js_create_function(env, "onFulfilled", -1, on_fulfilled, NULL, &callback);
  assert(e == 0);

  js_value_t *then;
  e = js_get_named_property(env, promise, "then", &then);
  assert(e == 0);

  e = js_call_function(env, promise, then, 1, &callback, NULL);
  assert(e == 0);

  e = js_close_continuation_scope(env, scope);
  assert(e == 0);
}

static void
consume(js_env_t *env, js_value_t *global) {
  int e;

  js_value_t *value;
  e = js_get_continuation_variable(env, get_key(env), &value);
  assert(e == 0);

  bool is_undefined;
  e = js_is_undefined(env, value, &is_undefined);
  assert(e == 0);

  assert(is_undefined);

  js_value_t *resolve;
  e = js_get_named_property(env, global, "resolve", &resolve);
  assert(e == 0);

  e = js_call_function(env, global, resolve, 0, NULL, NULL);
  assert(e == 0);

  assert(seen == 42);
}

int
main() {
  snapshot_test_t test = {
    .name = "run-from-snapshot-continuation-variable",
    .references = references,
    .produce = produce,
    .consume = consume,
  };

  snapshot_test_run(&test);
}
