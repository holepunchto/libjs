#ifndef JS_TEMPLATE_H
#define JS_TEMPLATE_H

#ifdef __cplusplus

#include <string>
#include <type_traits>
#include <utility>

#include "../js.h"

namespace {

struct js_receiver_t {
private:
  js_value_t *value;

public:
  js_receiver_t(js_value_t *value)
      : value(value) {}

  js_receiver_t(const js_receiver_t &) = delete;

  js_receiver_t(js_receiver_t &&that)
      : value(std::exchange(that.value, nullptr)) {}

  js_receiver_t &
  operator=(const js_receiver_t &) = delete;
};

struct js_arraybuffer_t {
public:
  uint8_t *data;
  size_t len;

public:
  js_arraybuffer_t() : data(nullptr), len(0) {}

  js_arraybuffer_t(uint8_t *data, size_t len) : data(data), len(len) {}

  js_arraybuffer_t(js_env_t *env, js_value_t *value) {
    int err;
    err = js_get_arraybuffer_info(env, value, reinterpret_cast<void **>(&data), &len);
    assert(err == 0);
  }
};

template <typename T>
struct js_typedarray_t {
public:
  T *data;
  size_t len;

public:
  js_typedarray_t() : data(nullptr), len(0) {}

  js_typedarray_t(T *data, size_t len) : data(data), len(len) {}

  js_typedarray_t(js_env_t *env, js_callback_info_t *, js_value_t *value) {
    int err;
    err = js_get_typedarray_info(env, value, nullptr, reinterpret_cast<void **>(&data), &len, nullptr, nullptr);
    assert(err == 0);
  }

  js_typedarray_t(const js_typedarray_t &) = delete;

  js_typedarray_t(js_typedarray_t &&that)
      : data(std::exchange(that.data, nullptr)),
        len(std::exchange(that.len, 0)) {}

  virtual ~js_typedarray_t() = default;

  js_typedarray_t &
  operator=(const js_typedarray_t &) = delete;
};

template <typename T>
struct js_typedarray_with_view_t : js_typedarray_t<T> {
private:
  js_env_t *env;
  js_typedarray_view_t *view;

public:
  js_typedarray_with_view_t(js_env_t *env, js_typed_callback_info_t *, js_value_t *value)
      : env(env) {
    int err;
    err = js_get_typedarray_view(env, value, nullptr, reinterpret_cast<void **>(&this->data), &this->len, &view);
    assert(err == 0);
  }

  js_typedarray_with_view_t(js_typedarray_with_view_t &&that)
      : env(std::exchange(that.env, nullptr)),
        view(std::exchange(that.view, nullptr)) {}

  ~js_typedarray_with_view_t() {
    if (view == nullptr) return;

    int err;
    err = js_release_typedarray_view(env, view);
    assert(err == 0);
  }
};

template <typename T>
struct js_type_container_t;

template <>
struct js_type_container_t<js_receiver_t> {
  using type = js_value_t *;

  static constexpr auto
  signature() {
    return js_object;
  }

  static auto
  unmarshall(js_env_t *, js_typed_callback_info_t *, js_value_t *value) {
    return js_receiver_t(value);
  }

  static auto
  unmarshall(js_env_t *, js_callback_info_t *, js_value_t *value) {
    return js_receiver_t(value);
  }
};

template <>
struct js_type_container_t<bool> {
  using type = bool;

  static constexpr auto
  signature() {
    return js_boolean;
  }

  static constexpr auto
  marshall(js_env_t *, js_typed_callback_info_t *, bool value) {
    return value;
  }

  static auto
  marshall(js_env_t *env, js_callback_info_t *, bool value) {
    int err;

    js_value_t *result;
    err = js_get_boolean(env, value, &result);
    assert(err == 0);

    return result;
  }

  static constexpr auto
  unmarshall(js_env_t *, js_typed_callback_info_t *, bool value) {
    return value;
  }

  static auto
  unmarshall(js_env_t *env, js_callback_info_t *, js_value_t *value) {
    int err;

    bool result;
    err = js_get_value_bool(env, value, &result);
    assert(err == 0);

    return result;
  }
};

template <>
struct js_type_container_t<int32_t> {
  using type = int32_t;

  static constexpr auto
  signature() {
    return js_int32;
  }

  static constexpr auto
  marshall(js_env_t *, js_typed_callback_info_t *, int32_t value) {
    return value;
  }

  static auto
  marshall(js_env_t *env, js_callback_info_t *, int32_t value) {
    int err;

    js_value_t *result;
    err = js_create_int32(env, value, &result);
    assert(err == 0);

    return result;
  }

  static constexpr auto
  unmarshall(js_env_t *, js_typed_callback_info_t *, int32_t value) {
    return value;
  }

  static auto
  unmarshall(js_env_t *env, js_callback_info_t *, js_value_t *value) {
    int err;

    int32_t result;
    err = js_get_value_int32(env, value, &result);
    assert(err == 0);

    return result;
  }
};

template <>
struct js_type_container_t<uint32_t> {
  using type = uint32_t;

  static constexpr auto
  signature() {
    return js_uint32;
  }

  static constexpr auto
  marshall(js_env_t *, js_typed_callback_info_t *, uint32_t value) {
    return value;
  }

  static auto
  marshall(js_env_t *env, js_callback_info_t *, uint32_t value) {
    int err;

    js_value_t *result;
    err = js_create_uint32(env, value, &result);
    assert(err == 0);

    return result;
  }

  static constexpr auto
  unmarshall(js_env_t *env, js_typed_callback_info_t *, uint32_t value) {
    return value;
  }

  static auto
  unmarshall(js_env_t *env, js_callback_info_t *, js_value_t *value) {
    int err;

    uint32_t result;
    err = js_get_value_uint32(env, value, &result);
    assert(err == 0);

    return result;
  }
};

template <>
struct js_type_container_t<int64_t> {
  using type = int64_t;

  static constexpr auto
  signature() {
    return js_int64;
  }

  static constexpr auto
  marshall(js_env_t *, js_typed_callback_info_t *, int64_t value) {
    return value;
  }

  static auto
  marshall(js_env_t *env, js_callback_info_t *, int64_t value) {
    int err;

    js_value_t *result;
    err = js_create_int64(env, value, &result);
    assert(err == 0);

    return result;
  }

  static constexpr auto
  unmarshall(js_env_t *, js_typed_callback_info_t *, int64_t value) {
    return value;
  }

  static auto
  unmarshall(js_env_t *env, js_callback_info_t *, js_value_t *value) {
    int err;

    int64_t result;
    err = js_get_value_int64(env, value, &result);
    assert(err == 0);

    return result;
  }
};

template <>
struct js_type_container_t<double> {
  using type = double;

  static constexpr auto
  signature() {
    return js_float64;
  }

  static constexpr auto
  marshall(js_env_t *, js_typed_callback_info_t *, double value) {
    return value;
  }

  static auto
  marshall(js_env_t *env, js_callback_info_t *, double value) {
    int err;

    js_value_t *result;
    err = js_create_double(env, value, &result);
    assert(err == 0);

    return result;
  }

  static constexpr auto
  unmarshall(js_env_t *, js_typed_callback_info_t *, double value) {
    return value;
  }

  static auto
  unmarshall(js_env_t *env, js_callback_info_t *, js_value_t *value) {
    int err;

    double result;
    err = js_get_value_double(env, value, &result);
    assert(err == 0);

    return result;
  }
};

template <>
struct js_type_container_t<js_arraybuffer_t> {
  using type = js_value_t *;

  static constexpr auto
  signature() {
    return js_object;
  }

  static auto
  marshall(js_env_t *env, js_arraybuffer_t value) {
    int err;

    js_value_t *result;

    void *data;
    err = js_create_arraybuffer(env, value.len, &data, &result);
    assert(err == 0);

    memcpy(data, value.data, value.len);

    return result;
  }

  static auto
  marshall(js_env_t *env, js_typed_callback_info_t *, js_arraybuffer_t value) {
    return marshall(env, value);
  }

  static auto
  marshall(js_env_t *env, js_callback_info_t *, js_arraybuffer_t value) {
    return marshall(env, value);
  }

  static auto
  unmarshall(js_env_t *env, js_typed_callback_info_t *, js_value_t *value) {
    return js_arraybuffer_t(env, value);
  }

  static auto
  unmarshall(js_env_t *env, js_callback_info_t *, js_value_t *value) {
    return js_arraybuffer_t(env, value);
  }
};

template <typename T>
struct js_type_container_t<js_typedarray_t<T>> {
  using type = js_value_t *;

  static constexpr auto
  signature() {
    return js_object;
  }

  static auto
  marshall(js_env_t *env, js_typedarray_t<T> value) {
    int err;

    js_typedarray_type_t type;
    size_t bytes_per_element = 1;

    if constexpr (std::is_same<T, uint8_t>()) {
      type = js_uint8array;
    } else {
      abort();
    }

    js_value_t *arraybuffer;

    void *data;
    err = js_create_arraybuffer(env, value.len * bytes_per_element, &data, &arraybuffer);
    assert(err == 0);

    memcpy(data, value.data, value.len * bytes_per_element);

    js_value_t *result;
    err = js_create_typedarray(env, type, value.len, arraybuffer, 0, &result);
    assert(err == 0);

    return result;
  }

  static auto
  marshall(js_env_t *env, js_typed_callback_info_t *, js_typedarray_t<T> value) {
    return marshall(env, value);
  }

  static auto
  marshall(js_env_t *env, js_callback_info_t *, js_typedarray_t<T> value) {
    return marshall(env, value);
  }

  static auto
  unmarshall(js_env_t *env, js_typed_callback_info_t *info, js_value_t *value) {
    return js_typedarray_with_view_t<T>(env, info, value);
  }

  static auto
  unmarshall(js_env_t *env, js_callback_info_t *info, js_value_t *value) {
    return js_typedarray_t<T>(env, info, value);
  }
};

template <auto fn, typename R, typename... A>
constexpr auto
js_typed_callback() {
  return +[](js_type_container_t<A>::type... args, js_typed_callback_info_t *info) -> js_type_container_t<R>::type {
    int err;

    js_env_t *env;
    err = js_get_typed_callback_info(info, &env, nullptr);
    assert(err == 0);

    auto result = fn(js_type_container_t<A>::unmarshall(env, info, args)...);

    return js_type_container_t<R>::marshall(env, info, result);
  };
}

template <auto fn, typename R, typename... A, size_t... I>
constexpr auto
js_untyped_callback(std::index_sequence<I...>) {
  return +[](js_env_t *env, js_callback_info_t *info) -> js_value_t * {
    int err;

    size_t argc = sizeof...(A);
    js_value_t *argv[sizeof...(A)];

    using head = std::tuple_element<0, std::tuple<A...>>::type;

    if constexpr (std::is_same<head, js_receiver_t>()) {
      argc--;

      err = js_get_callback_info(env, info, &argc, &argv[1], &argv[0], NULL);
      assert(err == 0);

      argc++;
    } else {
      err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
      assert(err == 0);
    }

    assert(argc == sizeof...(A));

    auto result = fn(js_type_container_t<A>::unmarshall(env, info, argv[I])...);

    return js_type_container_t<R>::marshall(env, info, result);
  };
}

template <auto fn, typename R, typename... A>
constexpr auto
js_untyped_callback() {
  return js_untyped_callback<fn, R, A...>(std::index_sequence_for<A...>());
}

template <auto fn, typename R, typename... A>
constexpr auto
js_create_typed_function(js_env_t *env, const char *name, size_t len, js_value_t **result) {
  auto typed = js_typed_callback<fn, R, A...>();

  auto untyped = js_untyped_callback<fn, R, A...>();

  js_callback_signature_t signature;

  int args[] = {
    js_type_container_t<A>::signature()...
  };

  signature.version = 0;
  signature.result = js_type_container_t<R>::signature();
  signature.args_len = sizeof...(A);
  signature.args = args;

  return js_create_typed_function(env, name, len, untyped, &signature, reinterpret_cast<const void *>(typed), nullptr, result);
}

template <auto fn, typename R, typename... A>
constexpr auto
js_create_typed_function(js_env_t *env, std::string name, js_value_t **result) {
  return js_create_typed_function<fn, R, A...>(env, name.data(), name.length(), result);
}

} // namespace

#endif

#endif // JS_TEMPLATE_H
