#ifndef JS_H
#define JS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <utf.h>
#include <uv.h>

typedef struct js_platform_s js_platform_t;
typedef struct js_platform_options_s js_platform_options_t;
typedef struct js_platform_limits_s js_platform_limits_t;
typedef struct js_env_s js_env_t;
typedef struct js_env_options_s js_env_options_t;
typedef struct js_handle_scope_s js_handle_scope_t;
typedef struct js_escapable_handle_scope_s js_escapable_handle_scope_t;
typedef struct js_context_s js_context_t;
typedef struct js_module_s js_module_t;
typedef struct js_value_s js_value_t;
typedef struct js_ref_s js_ref_t;
typedef struct js_property_descriptor_s js_property_descriptor_t;
typedef struct js_delegate_callbacks_s js_delegate_callbacks_t;
typedef struct js_type_tag_s js_type_tag_t;
typedef struct js_deferred_s js_deferred_t;
typedef struct js_string_view_s js_string_view_t;
typedef struct js_callback_info_s js_callback_info_t;
typedef struct js_typed_callback_info_s js_typed_callback_info_t;
typedef struct js_callback_signature_s js_callback_signature_t;
typedef struct js_arraybuffer_backing_store_s js_arraybuffer_backing_store_t;
typedef struct js_threadsafe_function_s js_threadsafe_function_t;
typedef struct js_deferred_teardown_s js_deferred_teardown_t;
typedef struct js_heap_statistics_s js_heap_statistics_t;
typedef struct js_heap_space_statistics_s js_heap_space_statistics_t;
typedef struct js_error_location_s js_error_location_t;
typedef struct js_inspector_s js_inspector_t;

typedef js_value_t *(*js_function_cb)(js_env_t *, js_callback_info_t *);
typedef void (*js_finalize_cb)(js_env_t *, void *data, void *finalize_hint);
typedef js_value_t *(*js_delegate_get_cb)(js_env_t *, js_value_t *property, void *data);
typedef bool (*js_delegate_has_cb)(js_env_t *, js_value_t *property, void *data);
typedef bool (*js_delegate_set_cb)(js_env_t *, js_value_t *property, js_value_t *value, void *data);
typedef bool (*js_delegate_delete_property_cb)(js_env_t *, js_value_t *property, void *data);
typedef js_value_t *(*js_delegate_own_keys_cb)(js_env_t *, void *data);
typedef js_module_t *(*js_module_resolve_cb)(js_env_t *, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data);
typedef void (*js_module_meta_cb)(js_env_t *, js_module_t *module, js_value_t *meta, void *data);
typedef void (*js_module_evaluate_cb)(js_env_t *, js_module_t *module, void *data);
typedef void (*js_uncaught_exception_cb)(js_env_t *, js_value_t *error, void *data);
typedef void (*js_unhandled_rejection_cb)(js_env_t *, js_value_t *reason, js_value_t *promise, void *data);
typedef js_module_t *(*js_dynamic_import_cb)(js_env_t *, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, void *data);
typedef js_value_t *(*js_dynamic_import_transitional_cb)(js_env_t *, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, void *data);
typedef void (*js_threadsafe_function_cb)(js_env_t *, js_value_t *function, void *context, void *data);
typedef void (*js_teardown_cb)(void *data);
typedef void (*js_deferred_teardown_cb)(js_deferred_teardown_t *, void *data);
typedef void (*js_inspector_message_cb)(js_env_t *, js_inspector_t *, js_value_t *message, void *data);
typedef void (*js_inspector_message_transitional_cb)(js_env_t *, js_inspector_t *, const char *message, size_t len, void *data);
typedef bool (*js_inspector_paused_cb)(js_env_t *, js_inspector_t *, void *data);

enum {
  /**
   * There's a pending exception that, unless handled, will be propagated up the
   * JavaScript execution stack.
   */
  js_pending_exception = -1,

  /**
   * There was an uncaught exception that could not be propagated as the JavaScript
   * execution stack is empty.
   */
  js_uncaught_exception = -2,
};

typedef enum {
  js_undefined = 0,
  js_null = 1,
  js_boolean = 2,
  js_number = 3,
  js_string = 4,
  js_symbol = 5,
  js_object = 6,
  js_function = 7,
  js_external = 8,
  js_bigint = 9,
} js_value_type_t;

typedef enum {
  js_int8array = 0,
  js_uint8array = 1,
  js_uint8clampedarray = 2,
  js_int16array = 3,
  js_uint16array = 4,
  js_int32array = 5,
  js_uint32array = 6,
  js_float16array = 11,
  js_float32array = 7,
  js_float64array = 8,
  js_bigint64array = 9,
  js_biguint64array = 10,
} js_typedarray_type_t;

enum {
  // Numeric types

  js_int8 = 1 << 8 | js_number,
  js_uint8 = 2 << 8 | js_number,
  js_int16 = 3 << 8 | js_number,
  js_uint16 = 4 << 8 | js_number,
  js_int32 = 5 << 8 | js_number,
  js_uint32 = 6 << 8 | js_number,
  js_int64 = 7 << 8 | js_number,
  js_uint64 = 8 << 8 | js_number,
  js_float16 = 11 << 8 | js_number,
  js_float32 = 9 << 8 | js_number,
  js_float64 = 10 << 8 | js_number,

  js_bigint64 = 1 << 8 | js_bigint,
  js_biguint64 = 2 << 8 | js_bigint,
};

typedef enum {
  js_promise_pending = 0,
  js_promise_fulfilled = 1,
  js_promise_rejected = 2,
} js_promise_state_t;

enum {
  js_writable = 1,
  js_enumerable = 1 << 1,
  js_configurable = 1 << 2,
  js_static = 1 << 10,
};

typedef enum {
  js_key_include_prototypes = 0,
  js_key_own_only = 1,
} js_key_collection_mode_t;

typedef enum {
  js_key_convert_to_string = 0,
  js_key_keep_numbers = 1,
} js_key_conversion_mode_t;

typedef enum {
  js_property_all_properties = 0,
  js_property_only_writable = js_writable,
  js_property_only_enumerable = js_enumerable,
  js_property_only_configurable = js_configurable,
  js_property_skip_strings = 1 << 3,
  js_property_skip_symbols = 1 << 4,
} js_property_filter_t;

typedef enum {
  js_index_include_indices = 0,
  js_index_skip_indices = 1,
} js_index_filter_t;

typedef enum {
  js_utf8 = 1,
  js_utf16le = 2,
  js_latin1 = 3,
} js_string_encoding_t;

typedef enum {
  js_threadsafe_function_release = 0,
  js_threadsafe_function_abort = 1
} js_threadsafe_function_release_mode_t;

typedef enum {
  js_threadsafe_function_nonblocking = 0,
  js_threadsafe_function_blocking = 1
} js_threadsafe_function_call_mode_t;

/** @version 1 */
struct js_platform_options_s {
  int version;

  /**
   * Expose garbage collection APIs, which are otherwise not available as they
   * negatively impact performance.
   *
   * @since 0
   */
  bool expose_garbage_collection;

  /**
   * Trace invocations of the garbage collector.
   *
   * @since 0
   */
  bool trace_garbage_collection;

  /**
   * Disable the optimizing compiler, such as TurboFan on V8.
   *
   * @since 0
   */
  bool disable_optimizing_compiler;

  /**
   * Trace optimizations made by the optimizing compiler based on type feedback.
   *
   * Requires that the optimizing compiler is enabled and supports tracing.
   *
   * @since 0
   */
  bool trace_optimizations;

  /**
   * Trace deoptimizations made by the optimizing compiler based on type feddback.
   *
   * Requires that the optimizing compiler is enabled and supports tracing.
   *
   * @since 0
   */
  bool trace_deoptimizations;

  /**
   * Enable the sampling profiler if supported.
   *
   * @since 0
   */
  bool enable_sampling_profiler;

  /**
   * The interval between stack traces in microseconds.
   *
   * @since 0
   */
  int sampling_profiler_interval;

  /**
   * Enable trade-off of performance for memory if supported.
   *
   * @since 1
   */
  bool optimize_for_memory;
};

/** @version 0 */
struct js_platform_limits_s {
  int version;

  /**
   * The maximum length of `ArrayBuffer` objects in bytes.
   *
   * @since 0
   */
  size_t arraybuffer_length;

  /**
   * The maximum length of `String` objects in UTF-16 code units.
   *
   * @since 0
   */
  size_t string_length;
};

/** @version 0 */
struct js_env_options_s {
  int version;

  /**
   * The memory limit of the JavaScript heap. By default, the limit will be
   * inferred based on the amount of physical memory of the device.
   *
   * @since 0
   */
  size_t memory_limit;
};

/** @version 0 */
struct js_property_descriptor_s {
  int version;

  /** @since 0 */
  js_value_t *name;

  /** @since 0 */
  void *data;

  /** @since 0 */
  int attributes;

  // One of:

  // Method

  /** @since 0 */
  js_function_cb method;

  // Accessor

  /** @since 0 */
  js_function_cb getter;

  /** @since 0 */
  js_function_cb setter;

  // Value

  /** @since 0 */
  js_value_t *value;
};

/** @version 0 */
struct js_delegate_callbacks_s {
  int version;

  /** @since 0 */
  js_delegate_get_cb get;

  /** @since 0 */
  js_delegate_has_cb has;

  /** @since 0 */
  js_delegate_set_cb set;

  /** @since 0 */
  js_delegate_delete_property_cb delete_property;

  /** @since 0 */
  js_delegate_own_keys_cb own_keys;
};

/** @version 0 */
struct js_type_tag_s {
  /** @since 0 */
  uint64_t lower;

  /** @since 0 */
  uint64_t upper;
};

/** @version 0 */
struct js_callback_signature_s {
  int version;

  /** @since 0 */
  int result;

  /** @since 0 */
  size_t args_len;

  /** @since 0 */
  int *args;
};

/** @version 1 */
struct js_heap_statistics_s {
  int version;

  /**
   * The amount of memory currently committed for the heap.
   *
   * @since 0
   */
  size_t total_heap_size;

  /**
   * The size of all objects residing in the heap.
   *
   * @since 0
   */
  size_t used_heap_size;

  /**
   * The size of the backing store, i.e. array buffers and external strings.
   *
   * @since 1
   */
  size_t external_memory;
};

/** @version 0 */
struct js_heap_space_statistics_s {
  int version;

  /** @since 0 */
  const char *space_name;

  /** @since 0 */
  size_t space_size;

  /** @since 0 */
  size_t space_used_size;

  /** @since 0 */
  size_t space_available_size;
};

/** @version 0 */
struct js_error_location_s {
  int version;

  /** @since 0 */
  js_value_t *name;

  /** @since 0 */
  js_value_t *source;

  /** @since 0 */
  int64_t line;

  /** @since 0 */
  int64_t column_start;

  /** @since 0 */
  int64_t column_end;
};

int
js_create_platform(uv_loop_t *loop, const js_platform_options_t *options, js_platform_t **result);

int
js_destroy_platform(js_platform_t *platform);

int
js_get_platform_identifier(js_platform_t *platform, const char **result);

int
js_get_platform_version(js_platform_t *platform, const char **result);

int
js_get_platform_limits(js_platform_t *platform, js_platform_limits_t *result);

int
js_get_platform_loop(js_platform_t *platform, uv_loop_t **result);

int
js_create_env(uv_loop_t *loop, js_platform_t *platform, const js_env_options_t *options, js_env_t **result);

int
js_destroy_env(js_env_t *env);

/**
 * Add a callback for uncaught exceptions. By default, uncaught exceptions are
 * swallowed and do not affect JavaScript execution.
 *
 * An exception is considered uncaught if the JavaScript execution stack is
 * emptied without the exception being caught.
 */
int
js_on_uncaught_exception(js_env_t *env, js_uncaught_exception_cb cb, void *data);

/**
 * Add a callback for unhandled promise rejections. By default, unhandled
 * promise rejections are swallowed and do not affect JavaScript execution.
 *
 * A promise rejection is considered unhandled if the rejection has not been
 * caught after performing a microtask checkpoint.
 */
int
js_on_unhandled_rejection(js_env_t *env, js_unhandled_rejection_cb cb, void *data);

/**
 * Add a callback for dynamic `import()` statements. By default, a dynamic
 * import will result in either an uncaught exception or an unhandled promise
 * rejection during script or module evaluation.
 */
int
js_on_dynamic_import(js_env_t *env, js_dynamic_import_cb cb, void *data);

/**
 * Add a callback for dynamic `import()` statements with deferred resolution.
 * By default, a dynamic import will result in either an uncaught exception or
 * an unhandled promise rejection during script or module evaluation.
 */
int
js_on_dynamic_import_transitional(js_env_t *env, js_dynamic_import_transitional_cb cb, void *data);

int
js_get_env_loop(js_env_t *env, uv_loop_t **result);

int
js_get_env_platform(js_env_t *env, js_platform_t **result);

/**
 * Unless otherwise stated, the following functions will rethrow any pending
 * exception set on the associated JavaScript environment. To handle a pending
 * exception, call `js_get_and_clear_last_exception()`. If the exception cannot
 * be handled it may be rethrown with `js_throw()`.
 */

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_open_handle_scope(js_env_t *env, js_handle_scope_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_close_handle_scope(js_env_t *env, js_handle_scope_t *scope);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_open_escapable_handle_scope(js_env_t *env, js_escapable_handle_scope_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_close_escapable_handle_scope(js_env_t *env, js_escapable_handle_scope_t *scope);

/**
 * Promote an escapee to the outer handle scope. The behavior is undefined if
 * called more than once for the same handle scope.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_escape_handle(js_env_t *env, js_escapable_handle_scope_t *scope, js_value_t *escapee, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_context(js_env_t *env, js_context_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_destroy_context(js_env_t *env, js_context_t *context);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_enter_context(js_env_t *env, js_context_t *context);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_exit_context(js_env_t *env, js_context_t *context);

/**
 * Get the platform specific bindings object for the specified environment.
 *
 * Platform implementations can use this to export any additional functionality
 * by setting properties on the object and may also import any properties set
 * by embedders.
 */
int
js_get_bindings(js_env_t *env, js_value_t **result);

int
js_run_script(js_env_t *env, const char *file, size_t len, int offset, js_value_t *source, js_value_t **result);

int
js_create_module(js_env_t *env, const char *name, size_t len, int offset, js_value_t *source, js_module_meta_cb cb, void *data, js_module_t **result);

int
js_create_synthetic_module(js_env_t *env, const char *name, size_t len, js_value_t *const export_names[], size_t export_names_len, js_module_evaluate_cb cb, void *data, js_module_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_delete_module(js_env_t *env, js_module_t *module);

/**
 * Get the name of the module as specified when the module was created. The
 * name remains valid until the module is deleted.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_module_name(js_env_t *env, js_module_t *module, const char **result);

/**
 * Get the namespace object of the module. The behavior is undefined if the
 * module is not yet instantiated.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_module_namespace(js_env_t *env, js_module_t *module, js_value_t **result);

int
js_set_module_export(js_env_t *env, js_module_t *module, js_value_t *name, js_value_t *value);

int
js_instantiate_module(js_env_t *env, js_module_t *module, js_module_resolve_cb cb, void *data);

int
js_run_module(js_env_t *env, js_module_t *module, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_reference(js_env_t *env, js_value_t *value, uint32_t count, js_ref_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_delete_reference(js_env_t *env, js_ref_t *reference);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_reference_ref(js_env_t *env, js_ref_t *reference, uint32_t *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_reference_unref(js_env_t *env, js_ref_t *reference, uint32_t *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_reference_value(js_env_t *env, js_ref_t *reference, js_value_t **result);

int
js_define_class(js_env_t *env, const char *name, size_t len, js_function_cb constructor, void *data, js_property_descriptor_t const properties[], size_t properties_len, js_value_t **result);

int
js_define_properties(js_env_t *env, js_value_t *object, js_property_descriptor_t const properties[], size_t properties_len);

int
js_wrap(js_env_t *env, js_value_t *object, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_ref_t **result);

int
js_unwrap(js_env_t *env, js_value_t *object, void **result);

int
js_remove_wrap(js_env_t *env, js_value_t *object, void **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_delegate(js_env_t *env, const js_delegate_callbacks_t *callbacks, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_add_finalizer(js_env_t *env, js_value_t *object, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_ref_t **result);

int
js_add_type_tag(js_env_t *env, js_value_t *object, const js_type_tag_t *tag);

int
js_check_type_tag(js_env_t *env, js_value_t *object, const js_type_tag_t *tag, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_int32(js_env_t *env, int32_t value, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_uint32(js_env_t *env, uint32_t value, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_int64(js_env_t *env, int64_t value, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_double(js_env_t *env, double value, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_bigint_int64(js_env_t *env, int64_t value, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_bigint_uint64(js_env_t *env, uint64_t value, js_value_t **result);

int
js_create_string_utf8(js_env_t *env, const utf8_t *str, size_t len, js_value_t **result);

int
js_create_string_utf16le(js_env_t *env, const utf16_t *str, size_t len, js_value_t **result);

int
js_create_string_latin1(js_env_t *env, const latin1_t *str, size_t len, js_value_t **result);

/**
 * Create a string value from a UTF8 encoded C string. If the string is not
 * copied it must remain valid until the finalize callback is invoked. The
 * finalize callback may be omitted if the string is guaranteed to outlive the
 * JavaScript environment.
 */
int
js_create_external_string_utf8(js_env_t *env, utf8_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result, bool *copied);

/**
 * Create a string value from a UTF16LE encoded C string. If the string is not
 * copied it must remain valid until the finalize callback is invoked. The
 * finalize callback may be omitted if the string is guaranteed to outlive the
 * JavaScript environment.
 */
int
js_create_external_string_utf16le(js_env_t *env, utf16_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result, bool *copied);

/**
 * Create a string value from a Latin1 encoded C string. If the string is not
 * copied it must remain valid until the finalize callback is invoked. The
 * finalize callback may be omitted if the string is guaranteed to outlive the
 * JavaScript environment.
 */
int
js_create_external_string_latin1(js_env_t *env, latin1_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result, bool *copied);

int
js_create_property_key_utf8(js_env_t *env, const utf8_t *str, size_t len, js_value_t **result);

int
js_create_property_key_utf16le(js_env_t *env, const utf16_t *str, size_t len, js_value_t **result);

int
js_create_property_key_latin1(js_env_t *env, const latin1_t *str, size_t len, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_symbol(js_env_t *env, js_value_t *description, js_value_t **result);

int
js_symbol_for(js_env_t *env, const char *description, size_t len, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_object(js_env_t *env, js_value_t **result);

int
js_create_function(js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data, js_value_t **result);

int
js_create_function_with_source(js_env_t *env, const char *name, size_t name_len, const char *file, size_t file_len, js_value_t *const args[], size_t args_len, int offset, js_value_t *source, js_value_t **result);

int
js_create_typed_function(js_env_t *env, const char *name, size_t len, js_function_cb cb, const js_callback_signature_t *signature, const void *address, void *data, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_array(js_env_t *env, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_array_with_length(js_env_t *env, size_t len, js_value_t **result);

/**
 * Create an external value from a pointer. The pointer must remain valid until
 * the finalize callback is invoked. The finalize callback may be omitted if the
 * pointer is guaranteed to outlive the JavaScript environment.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_external(js_env_t *env, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_date(js_env_t *env, double time, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_type_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_range_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_syntax_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_reference_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_error_location(js_env_t *env, js_value_t *error, js_error_location_t *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_promise(js_env_t *env, js_deferred_t **deferred, js_value_t **promise);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_resolve_deferred(js_env_t *env, js_deferred_t *deferred, js_value_t *resolution);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_reject_deferred(js_env_t *env, js_deferred_t *deferred, js_value_t *resolution);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_promise_state(js_env_t *env, js_value_t *promise, js_promise_state_t *result);

/**
 * Get the result of the promise. The behavior is undefined if the promise is
 * still pending.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_promise_result(js_env_t *env, js_value_t *promise, js_value_t **result);

/**
 * Create an `ArrayBuffer` with 0-initialized data.
 */
int
js_create_arraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result);

/**
 * Create an `ArrayBuffer` with the contents of an existing backing store.
 */
int
js_create_arraybuffer_with_backing_store(js_env_t *env, js_arraybuffer_backing_store_t *backing_store, void **data, size_t *len, js_value_t **result);

/**
 * Create an `ArrayBuffer` with uninitialized data.
 */
int
js_create_unsafe_arraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result);

/**
 * Create an `ArrayBuffer` with externally managed data. The data must remain
 * valid until either the finalize callback is invoked or the `ArrayBuffer` is
 * detached. The finalize callback may be omitted if the data is either
 * guaranteed to outlive the JavaScript environment or if the `ArrayBuffer` is
 * manually detached prior to the data becoming invalid.
 */
int
js_create_external_arraybuffer(js_env_t *env, void *data, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_detach_arraybuffer(js_env_t *env, js_value_t *arraybuffer);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_arraybuffer_backing_store(js_env_t *env, js_value_t *arraybuffer, js_arraybuffer_backing_store_t **result);

/**
 * Create a `SharedArrayBuffer` with 0-initialized data.
 */
int
js_create_sharedarraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result);

/**
 * Create a `SharedArrayBuffer` with the contents of an existing backing store.
 */
int
js_create_sharedarraybuffer_with_backing_store(js_env_t *env, js_arraybuffer_backing_store_t *backing_store, void **data, size_t *len, js_value_t **result);

/**
 * Create a `SharedArrayBuffer` with uninitialized data.
 */
int
js_create_unsafe_sharedarraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result);

/**
 * Create a `SharedArrayBuffer` with externally managed data. The data must
 * remain valid until the finalize callback is invoked. The finalize callback
 * may be omitted if the data is guaranteed to outlive the JavaScript
 * environment.
 *
 * The finalize callback may be invoked from another thread and so it is not
 * safe to assume that it will be invoked from the same thread on which the
 * `SharedArrayBuffer` was created.
 */
int
js_create_external_sharedarraybuffer(js_env_t *env, void *data, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_sharedarraybuffer_backing_store(js_env_t *env, js_value_t *sharedarraybuffer, js_arraybuffer_backing_store_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_release_arraybuffer_backing_store(js_env_t *env, js_arraybuffer_backing_store_t *backing_store);

int
js_create_typedarray(js_env_t *env, js_typedarray_type_t type, size_t len, js_value_t *arraybuffer, size_t offset, js_value_t **result);

int
js_create_dataview(js_env_t *env, size_t len, js_value_t *arraybuffer, size_t offset, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_coerce_to_boolean(js_env_t *env, js_value_t *value, js_value_t **result);

int
js_coerce_to_number(js_env_t *env, js_value_t *value, js_value_t **result);

int
js_coerce_to_string(js_env_t *env, js_value_t *value, js_value_t **result);

int
js_coerce_to_object(js_env_t *env, js_value_t *value, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_typeof(js_env_t *env, js_value_t *value, js_value_type_t *result);

int
js_instanceof(js_env_t *env, js_value_t *object, js_value_t *constructor, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_undefined(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_null(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_boolean(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_number(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_int32(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_uint32(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_string(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_symbol(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_object(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_function(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_async_function(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_generator_function(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_generator(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_arguments(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_array(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_external(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_wrapped(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_delegate(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_bigint(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_date(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_regexp(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_error(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_promise(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_proxy(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_map(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_map_iterator(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_set(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_set_iterator(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_weak_map(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_weak_set(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_weak_ref(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_arraybuffer(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_detached_arraybuffer(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_sharedarraybuffer(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_typedarray(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_int8array(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_uint8array(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_uint8clampedarray(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_int16array(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_uint16array(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_int32array(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_uint32array(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_float16array(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_float32array(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_float64array(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_bigint64array(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_biguint64array(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_dataview(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_module_namespace(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_strict_equals(js_env_t *env, js_value_t *a, js_value_t *b, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_global(js_env_t *env, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_undefined(js_env_t *env, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_null(js_env_t *env, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_boolean(js_env_t *env, bool value, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_bool(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_int32(js_env_t *env, js_value_t *value, int32_t *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_uint32(js_env_t *env, js_value_t *value, uint32_t *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_int64(js_env_t *env, js_value_t *value, int64_t *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_double(js_env_t *env, js_value_t *value, double *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_bigint_int64(js_env_t *env, js_value_t *value, int64_t *result, bool *lossless);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_bigint_uint64(js_env_t *env, js_value_t *value, uint64_t *result, bool *lossless);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_string_utf8(js_env_t *env, js_value_t *value, utf8_t *str, size_t len, size_t *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_string_utf16le(js_env_t *env, js_value_t *value, utf16_t *str, size_t len, size_t *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_string_latin1(js_env_t *env, js_value_t *value, latin1_t *str, size_t len, size_t *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_external(js_env_t *env, js_value_t *value, void **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_date(js_env_t *env, js_value_t *value, double *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_array_length(js_env_t *env, js_value_t *array, uint32_t *result);

int
js_get_array_elements(js_env_t *env, js_value_t *array, js_value_t *elements[], size_t len, size_t offset, uint32_t *result);

int
js_set_array_elements(js_env_t *env, js_value_t *array, const js_value_t *elements[], size_t len, size_t offset);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_prototype(js_env_t *env, js_value_t *object, js_value_t **result);

int
js_get_property_names(js_env_t *env, js_value_t *object, js_value_t **result);

int
js_get_filtered_property_names(js_env_t *env, js_value_t *object, js_key_collection_mode_t mode, js_property_filter_t property_filter, js_index_filter_t index_filter, js_key_conversion_mode_t key_conversion, js_value_t **result);

int
js_get_property(js_env_t *env, js_value_t *object, js_value_t *key, js_value_t **result);

int
js_has_property(js_env_t *env, js_value_t *object, js_value_t *key, bool *result);

int
js_has_own_property(js_env_t *env, js_value_t *object, js_value_t *key, bool *result);

int
js_set_property(js_env_t *env, js_value_t *object, js_value_t *key, js_value_t *value);

int
js_delete_property(js_env_t *env, js_value_t *object, js_value_t *key, bool *result);

int
js_get_named_property(js_env_t *env, js_value_t *object, const char *name, js_value_t **result);

int
js_has_named_property(js_env_t *env, js_value_t *object, const char *name, bool *result);

int
js_set_named_property(js_env_t *env, js_value_t *object, const char *name, js_value_t *value);

int
js_delete_named_property(js_env_t *env, js_value_t *object, const char *name, bool *result);

int
js_get_element(js_env_t *env, js_value_t *object, uint32_t index, js_value_t **result);

int
js_has_element(js_env_t *env, js_value_t *object, uint32_t index, bool *result);

int
js_set_element(js_env_t *env, js_value_t *object, uint32_t index, js_value_t *value);

int
js_delete_element(js_env_t *env, js_value_t *object, uint32_t index, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_string_view(js_env_t *env, js_value_t *string, js_string_encoding_t *encoding, const void **str, size_t *len, js_string_view_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_release_string_view(js_env_t *env, js_string_view_t *view);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_callback_info(js_env_t *env, const js_callback_info_t *info, size_t *argc, js_value_t *argv[], js_value_t **receiver, void **data);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_typed_callback_info(const js_typed_callback_info_t *info, js_env_t **env, void **data);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_new_target(js_env_t *env, const js_callback_info_t *info, js_value_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_arraybuffer_info(js_env_t *env, js_value_t *arraybuffer, void **data, size_t *len);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_sharedarraybuffer_info(js_env_t *env, js_value_t *sharedarraybuffer, void **data, size_t *len);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_typedarray_info(js_env_t *env, js_value_t *typedarray, js_typedarray_type_t *type, void **data, size_t *len, js_value_t **arraybuffer, size_t *offset);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_dataview_info(js_env_t *env, js_value_t *dataview, void **data, size_t *len, js_value_t **arraybuffer, size_t *offset);

/**
 * Call a JavaScript function from native code.
 *
 * When there is no JavaScript already executing on the stack, such as when the
 * native code making the call was invoked as the result of I/O, a microtask
 * checkpoint is performed before returning to native code.
 *
 * If there is JavaScript already executing on the stack, such as when the
 * native code making the call was invoked from JavaScript, no microtask
 * checkpoint is performed before returning to native code.
 */
int
js_call_function(js_env_t *env, js_value_t *receiver, js_value_t *function, size_t argc, js_value_t *const argv[], js_value_t **result);

/**
 * Call a JavaScript function from native code and perform a microtask
 * checkpoint.
 *
 * THIS FUNCTION MUST ONLY BE USED WHEN THERE IS NO JAVASCRIPT ALREADY
 * EXECUTING ON THE STACK. If in doubt, use `js_call_function()` instead which
 * automatically performs microtask checkpoints as needed.
 */
int
js_call_function_with_checkpoint(js_env_t *env, js_value_t *receiver, js_value_t *function, size_t argc, js_value_t *const argv[], js_value_t **result);

int
js_new_instance(js_env_t *env, js_value_t *constructor, size_t argc, js_value_t *const argv[], js_value_t **result);

int
js_create_threadsafe_function(js_env_t *env, js_value_t *function, size_t queue_limit, size_t initial_thread_count, js_finalize_cb finalize_cb, void *finalize_hint, void *context, js_threadsafe_function_cb cb, js_threadsafe_function_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_threadsafe_function_context(js_threadsafe_function_t *function, void **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_call_threadsafe_function(js_threadsafe_function_t *function, void *data, js_threadsafe_function_call_mode_t mode);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_acquire_threadsafe_function(js_threadsafe_function_t *function);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_release_threadsafe_function(js_threadsafe_function_t *function, js_threadsafe_function_release_mode_t mode);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_ref_threadsafe_function(js_env_t *env, js_threadsafe_function_t *function);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_unref_threadsafe_function(js_env_t *env, js_threadsafe_function_t *function);

int
js_add_teardown_callback(js_env_t *env, js_teardown_cb callback, void *data);

int
js_remove_teardown_callback(js_env_t *env, js_teardown_cb callback, void *data);

int
js_add_deferred_teardown_callback(js_env_t *env, js_deferred_teardown_cb callback, void *data, js_deferred_teardown_t **result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_finish_deferred_teardown_callback(js_deferred_teardown_t *handle);

int
js_throw(js_env_t *env, js_value_t *error);

int
js_throw_error(js_env_t *env, const char *code, const char *message);

int
js_throw_verrorf(js_env_t *env, const char *code, const char *message, va_list args);

int
js_throw_errorf(js_env_t *env, const char *code, const char *message, ...);

int
js_throw_type_error(js_env_t *env, const char *code, const char *message);

int
js_throw_type_verrorf(js_env_t *env, const char *code, const char *message, va_list args);

int
js_throw_type_errorf(js_env_t *env, const char *code, const char *message, ...);

int
js_throw_range_error(js_env_t *env, const char *code, const char *message);

int
js_throw_range_verrorf(js_env_t *env, const char *code, const char *message, va_list args);

int
js_throw_range_errorf(js_env_t *env, const char *code, const char *message, ...);

int
js_throw_syntax_error(js_env_t *env, const char *code, const char *message);

int
js_throw_syntax_verrorf(js_env_t *env, const char *code, const char *message, va_list args);

int
js_throw_syntax_errorf(js_env_t *env, const char *code, const char *message, ...);

int
js_throw_reference_error(js_env_t *env, const char *code, const char *message);

int
js_throw_reference_verrorf(js_env_t *env, const char *code, const char *message, va_list args);

int
js_throw_reference_errorf(js_env_t *env, const char *code, const char *message, ...);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_exception_pending(js_env_t *env, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_and_clear_last_exception(js_env_t *env, js_value_t **result);

/**
 * Trigger an uncaught exception. If no uncaught exception handler is installed
 * the function has no effect and execution will continue normally.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_fatal_exception(js_env_t *env, js_value_t *error);

/**
 * Terminate JavaScript execution at the next possible opportunity, discarding
 * the remainder of the execution stack.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_terminate_execution(js_env_t *env);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_adjust_external_memory(js_env_t *env, int64_t change_in_bytes, int64_t *result);

/**
 * Request that the garbage collector be run. This should only be used for
 * testing as it will negatively impact performance. Unless garbage collection
 * APIs have been exposed using the `expose_garbage_collection` option the
 * function does nothing.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_request_garbage_collection(js_env_t *env);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_heap_statistics(js_env_t *env, js_heap_statistics_t *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_heap_space_statistics(js_env_t *env, js_heap_space_statistics_t statistics[], size_t len, size_t offset, size_t *result);

int
js_create_inspector(js_env_t *env, js_inspector_t **result);

int
js_destroy_inspector(js_env_t *env, js_inspector_t *inspector);

int
js_on_inspector_response(js_env_t *env, js_inspector_t *inspector, js_inspector_message_cb cb, void *data);

int
js_on_inspector_response_transitional(js_env_t *env, js_inspector_t *inspector, js_inspector_message_transitional_cb cb, void *data);

int
js_on_inspector_paused(js_env_t *env, js_inspector_t *inspector, js_inspector_paused_cb cb, void *data);

int
js_connect_inspector(js_env_t *env, js_inspector_t *inspector);

int
js_send_inspector_request(js_env_t *env, js_inspector_t *inspector, js_value_t *message);

int
js_send_inspector_request_transitional(js_env_t *env, js_inspector_t *inspector, const char *message, size_t len);

int
js_attach_context_to_inspector(js_env_t *env, js_inspector_t *inspector, js_context_t *context, const char *name, size_t len);

int
js_detach_context_from_inspector(js_env_t *env, js_inspector_t *inspector, js_context_t *context);

#ifdef __cplusplus
}
#endif

#endif // JS_H
