#include <assert.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

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

  // Build a prototype object with an inherited property.
  js_value_t *prototype;
  e = js_create_object(env, &prototype);
  assert(e == 0);

  js_value_t *marker;
  e = js_create_int32(env, 42, &marker);
  assert(e == 0);

  e = js_set_named_property(env, prototype, "marker", marker);
  assert(e == 0);

  // Build the own property names and values.
  js_value_t *foo_key;
  e = js_create_string_utf8(env, (const utf8_t *) "foo", (size_t) -1, &foo_key);
  assert(e == 0);

  js_value_t *bar_key;
  e = js_create_string_utf8(env, (const utf8_t *) "bar", (size_t) -1, &bar_key);
  assert(e == 0);

  js_value_t *foo_value;
  e = js_create_int32(env, 1, &foo_value);
  assert(e == 0);

  js_value_t *bar_value;
  e = js_create_int32(env, 2, &bar_value);
  assert(e == 0);

  js_value_t *names[] = {foo_key, bar_key};
  js_value_t *values[] = {foo_value, bar_value};

  // Create an object with the given prototype and own properties.
  js_value_t *object;
  e = js_create_object_with_properties(env, prototype, names, values, 2, &object);
  assert(e == 0);

  // The own properties are present.
  js_value_t *foo;
  e = js_get_named_property(env, object, "foo", &foo);
  assert(e == 0);

  int32_t value;
  e = js_get_value_int32(env, foo, &value);
  assert(e == 0);

  assert(value == 1);

  js_value_t *bar;
  e = js_get_named_property(env, object, "bar", &bar);
  assert(e == 0);

  e = js_get_value_int32(env, bar, &value);
  assert(e == 0);

  assert(value == 2);

  // The property is inherited through the prototype chain.
  js_value_t *inherited;
  e = js_get_named_property(env, object, "marker", &inherited);
  assert(e == 0);

  e = js_get_value_int32(env, inherited, &value);
  assert(e == 0);

  assert(value == 42);

  // The object's prototype is strictly the prototype we provided.
  js_value_t *actual;
  e = js_get_prototype(env, object, &actual);
  assert(e == 0);

  bool equals;
  e = js_strict_equals(env, actual, prototype, &equals);
  assert(e == 0);

  assert(equals);

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
