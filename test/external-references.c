#include <assert.h>
#include <stdint.h>
#include <uv.h>

#include "../include/js.h"

static void
fn_a() {}

static void
fn_b() {}

int
main() {
  int e;

  js_external_references_t *references;
  e = js_create_external_references(&references);
  assert(e == 0);

  e = js_add_external_reference(references, (const void *) fn_a);
  assert(e == 0);

  e = js_add_external_reference(references, (const void *) fn_b);
  assert(e == 0);

  const intptr_t *data;
  size_t len;
  e = js_get_external_references(references, &data, &len);
  assert(e == 0);

  assert(len == 2);

  assert(data[0] == (intptr_t) fn_a);
  assert(data[1] == (intptr_t) fn_b);

  // The array handed to V8 must be null-terminated.
  assert(data[len] == 0);

  // Adding after a read stays valid; the terminator is not left dangling in the
  // middle of the array.
  e = js_add_external_reference(references, (const void *) main);
  assert(e == 0);

  e = js_get_external_references(references, &data, &len);
  assert(e == 0);

  assert(len == 3);
  assert(data[2] == (intptr_t) main);
  assert(data[len] == 0);

  e = js_destroy_external_references(references);
  assert(e == 0);

  return 0;
}
