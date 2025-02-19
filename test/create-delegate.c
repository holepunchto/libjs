#include <assert.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include "../include/js.h"

bool get_called = false;
bool has_called = false;
bool set_called = false;
bool delete_called = false;
bool keys_called = false;

js_value_t *
get(js_env_t *env, js_value_t *property, void *data) {
  int e;

  get_called = true;

  utf8_t name[4];
  e = js_get_value_string_utf8(env, property, name, 4, NULL);
  assert(e == 0);

  assert(strcmp((char *) name, "foo") == 0);

  js_value_t *result;
  e = js_create_uint32(env, 42, &result);
  assert(e == 0);

  return result;
}

bool
has(js_env_t *env, js_value_t *property, void *data) {
  int e;

  has_called = true;

  utf8_t name[4];
  e = js_get_value_string_utf8(env, property, name, 4, NULL);
  assert(e == 0);

  assert(strcmp((char *) name, "foo") == 0);

  return true;
}

bool
set(js_env_t *env, js_value_t *property, js_value_t *value, void *data) {
  int e;

  set_called = true;

  utf8_t name[4];
  e = js_get_value_string_utf8(env, property, name, 4, NULL);
  assert(e == 0);

  assert(strcmp((char *) name, "foo") == 0);

  uint32_t result;
  e = js_get_value_uint32(env, value, &result);
  assert(e == 0);

  assert(result == 42);

  return true;
}

bool
delete_property(js_env_t *env, js_value_t *property, void *data) {
  int e;

  delete_called = true;

  utf8_t name[4];
  e = js_get_value_string_utf8(env, property, name, 4, NULL);
  assert(e == 0);

  assert(strcmp((char *) name, "foo") == 0);

  return true;
}

js_value_t *
own_keys(js_env_t *env, void *data) {
  int e;

  keys_called = true;

  js_value_t *result;
  e = js_create_array_with_length(env, 1, &result);
  assert(e == 0);

  js_value_t *property;
  e = js_create_string_utf8(env, (utf8_t *) "foo", -1, &property);
  assert(e == 0);

  e = js_set_element(env, result, 0, property);
  assert(e == 0);

  return result;
}

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

  js_delegate_callbacks_t callbacks = {
    .get = get,
    .has = has,
    .set = set,
    .delete_property = delete_property,
    .own_keys = own_keys,
  };

  js_value_t *delegate;
  e = js_create_delegate(env, &callbacks, NULL, NULL, NULL, &delegate);
  assert(e == 0);

  bool is_delegate;
  e = js_is_delegate(env, delegate, &is_delegate);
  assert(e == 0);

  js_value_t *global;
  e = js_get_global(env, &global);
  assert(e == 0);

  e = js_set_named_property(env, global, "delegate", delegate);
  assert(e == 0);

  js_value_t *script, *result;

  e = js_create_string_utf8(env, (utf8_t *) "delegate.foo", -1, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  assert(get_called);

  {
    uint32_t value;
    e = js_get_value_uint32(env, result, &value);
    assert(e == 0);

    assert(value == 42);
  }

  e = js_create_string_utf8(env, (utf8_t *) "'foo' in delegate", -1, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  assert(has_called);

  {
    bool value;
    e = js_get_value_bool(env, result, &value);
    assert(e == 0);

    assert(value == true);
  }

  e = js_create_string_utf8(env, (utf8_t *) "delegate.foo = 42", -1, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  assert(set_called);

  {
    uint32_t value;
    e = js_get_value_uint32(env, result, &value);
    assert(e == 0);

    assert(value == 42);
  }

  e = js_create_string_utf8(env, (utf8_t *) "delete delegate.foo", -1, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  assert(delete_called);

  {
    bool value;
    e = js_get_value_bool(env, result, &value);
    assert(e == 0);

    assert(value == true);
  }

  e = js_create_string_utf8(env, (utf8_t *) "Object.getOwnPropertyNames(delegate)", -1, &script);
  assert(e == 0);

  e = js_run_script(env, NULL, 0, 0, script, &result);
  assert(e == 0);

  assert(keys_called);

  {
    bool is_array;
    e = js_is_array(env, result, &is_array);
    assert(e == 0);

    assert(is_array);

    uint32_t len;
    e = js_get_array_length(env, result, &len);
    assert(e == 0);

    assert(len == 1);

    for (size_t i = 0; i < len; i++) {
      js_value_t *value;
      e = js_get_element(env, result, i, &value);
      assert(e == 0);

      bool is_string;
      e = js_is_string(env, value, &is_string);
      assert(e == 0);

      assert(is_string);
    }
  }

  e = js_close_handle_scope(env, scope);
  assert(e == 0);

  e = js_destroy_env(env);
  assert(e == 0);

  e = js_destroy_platform(platform);
  assert(e == 0);

  e = uv_run(loop, UV_RUN_DEFAULT);
  assert(e == 0);
}
