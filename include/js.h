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

/**
 * An ABI stable interface to an embedded JavaScript engine. The interface is
 * engine agnostic, so anything said below about values and their behavior is
 * that of the ECMAScript specification rather than of any one engine, and
 * anything an engine is free to decide for itself is called out as such.
 */

/**
 * All functions report success by returning `0` and failure by returning a
 * negative value.
 *
 * A function taking a `js_env_t` reports a failure as `js_pending_exception`
 * if an exception is pending on the environment, or as `js_uncaught_exception`
 * if the exception could not be made pending as the JavaScript execution stack
 * is empty. A function taking no environment has nowhere to report an
 * exception, as it may be called without one, and so reports a failure as a
 * negative value alone.
 */

/**
 * The interface performs no type checking. A function that documents a
 * parameter as a value of a particular JavaScript type, such as a string or an
 * object, may do anything at all when passed a value of any other type, up to
 * and including crashing. Test a value of unknown provenance with
 * `js_typeof()`, `js_get_object_type()`, or one of the `js_is_*()` functions
 * before passing it on.
 *
 * The same holds for the handles the interface hands out, such as
 * environments, scripts and references: using one after it has been destroyed,
 * or together with an environment other than the one that created it, is
 * undefined behavior. Individual functions call out the cases that are easiest
 * to get wrong, but the absence of such a note is not a promise that misuse is
 * caught.
 */

/**
 * Native text is passed as a pointer and a length in code units of its
 * encoding, the length being `(size_t) -1` if the text is NUL terminated
 * instead. Text is copied unless otherwise stated and so need not remain valid
 * beyond the call it is passed to.
 */

/**
 * Structs carrying a `@version` are extended over time, each of their fields
 * declaring the `@since` version in which it was added. The `version` field
 * says how much of the struct is there and is always set by the caller,
 * whether the struct is passed in or filled in. Fields belonging to a later
 * version than the one declared are neither read nor written.
 */

typedef struct js_platform_s js_platform_t;
typedef struct js_platform_options_s js_platform_options_t;
typedef struct js_platform_limits_s js_platform_limits_t;
typedef struct js_env_s js_env_t;
typedef struct js_env_options_s js_env_options_t;
typedef struct js_handle_scope_s js_handle_scope_t;
typedef struct js_escapable_handle_scope_s js_escapable_handle_scope_t;
typedef struct js_context_s js_context_t;
typedef struct js_module_s js_module_t;
typedef struct js_script_s js_script_t;
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
typedef struct js_garbage_collection_tracking_s js_garbage_collection_tracking_t;
typedef struct js_garbage_collection_tracking_options_s js_garbage_collection_tracking_options_t;

enum {
  /**
   * There's a pending exception that, unless handled, will be propagated up the
   * JavaScript execution stack.
   */
  js_pending_exception = -1,

  /**
   * There was an uncaught exception that could not be propagated as the
   * JavaScript execution stack is empty.
   */
  js_uncaught_exception = -2,
};

/**
 * The type of a value, as reported by `js_typeof()`. It mirrors the `typeof`
 * operator, except that `null` has a type of its own rather than being
 * reported as an object, and that external values are reported as
 * `js_external`.
 */
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

/**
 * The type of an object, as reported by `js_get_object_type()`. The low byte is
 * the value type, `js_object`, and the second byte the kind of object, the
 * kinds being mutually exclusive and declared in order of precedence.
 *
 * An object of no particular kind is plain `js_object`. An external is likewise
 * plain `js_external`, needing no kind of its own.
 *
 * The boxed types, `js_boolean_object` through `js_bigint_object`, are the
 * wrapper objects reported by `js_is_boolean_object()` and its siblings, not
 * the primitives themselves, which are not objects.
 *
 * The set of kinds is open and may be extended, so treat an unrecognized kind
 * as `js_object`.
 */
typedef enum {
  js_array = 1 << 8 | js_object,
  js_arguments = 2 << 8 | js_object,
  js_date = 3 << 8 | js_object,
  js_regexp = 4 << 8 | js_object,
  js_error = 5 << 8 | js_object,
  js_promise = 6 << 8 | js_object,
  js_proxy = 7 << 8 | js_object,
  js_generator = 8 << 8 | js_object,
  js_map = 9 << 8 | js_object,
  js_set = 10 << 8 | js_object,
  js_map_iterator = 11 << 8 | js_object,
  js_set_iterator = 12 << 8 | js_object,
  js_weak_map = 13 << 8 | js_object,
  js_weak_set = 14 << 8 | js_object,
  js_weak_ref = 15 << 8 | js_object,
  js_arraybuffer = 16 << 8 | js_object,
  js_sharedarraybuffer = 17 << 8 | js_object,
  js_typedarray = 18 << 8 | js_object,
  js_dataview = 19 << 8 | js_object,
  js_module_namespace = 20 << 8 | js_object,
  js_boolean_object = 21 << 8 | js_object,
  js_number_object = 22 << 8 | js_object,
  js_string_object = 23 << 8 | js_object,
  js_symbol_object = 24 << 8 | js_object,
  js_bigint_object = 25 << 8 | js_object,
} js_object_type_t;

/**
 * The element type of a typed array. The type determines both how the elements
 * of the array are interpreted and how many bytes each of them occupies.
 */
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

/**
 * The native types that a value may be passed to and from a callback as, used
 * to describe the signature of a function created with
 * `js_create_typed_function()`. The low byte is the value type the native type
 * refines, so `js_int32` is a number and `js_bigint64` a bigint.
 *
 * A value type may also be used on its own, in which case the value is passed
 * as an unconverted `js_value_t`.
 *
 * Which of the types an engine supports is up to the engine; a signature it
 * cannot represent is rejected as a whole rather than in part.
 */
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

/**
 * The state of a promise, as reported by `js_get_promise_state()`. A promise
 * starts out pending and settles at most once, after which its state no longer
 * changes.
 */
typedef enum {
  js_promise_pending = 0,
  js_promise_fulfilled = 1,
  js_promise_rejected = 2,
} js_promise_state_t;

/**
 * The attributes of a property, as declared by a `js_property_descriptor_t`.
 * An attribute that is not set is off, so a descriptor with no attributes at
 * all describes a property that is non-writable, non-enumerable and
 * non-configurable.
 *
 * `js_static` is only meaningful to `js_define_class()`, where it moves a
 * property from the prototype of the class to the constructor itself.
 */
enum {
  js_writable = 1,
  js_enumerable = 1 << 1,
  js_configurable = 1 << 2,
  js_static = 1 << 10,
};

/**
 * Whether the properties collected by `js_get_filtered_property_names()`
 * include those inherited from the prototype chain of the object.
 */
typedef enum {
  js_key_include_prototypes = 0,
  js_key_own_only = 1,
} js_key_collection_mode_t;

/**
 * Whether the array indices collected by `js_get_filtered_property_names()`
 * are converted to strings or kept as numbers.
 */
typedef enum {
  js_key_convert_to_string = 0,
  js_key_keep_numbers = 1,
} js_key_conversion_mode_t;

/**
 * Which properties `js_get_filtered_property_names()` collects, the values
 * being combined as a bit set. Each filter narrows the selection further, so a
 * filter of `js_property_only_enumerable | js_property_skip_symbols` collects
 * the enumerable string-keyed properties alone.
 */
typedef enum {
  js_property_all_properties = 0,
  js_property_only_writable = js_writable,
  js_property_only_enumerable = js_enumerable,
  js_property_only_configurable = js_configurable,
  js_property_skip_strings = 1 << 3,
  js_property_skip_symbols = 1 << 4,
} js_property_filter_t;

/**
 * Whether the properties collected by `js_get_filtered_property_names()`
 * include array indices.
 */
typedef enum {
  js_index_include_indices = 0,
  js_index_skip_indices = 1,
} js_index_filter_t;

/**
 * The encoding of text, such as the contents of a string borrowed with
 * `js_get_string_view()`.
 */
typedef enum {
  js_utf8 = 1,
  js_utf16le = 2,
  js_latin1 = 3,
} js_string_encoding_t;

/**
 * How a threadsafe function is released. Releasing it in
 * `js_threadsafe_function_release` mode gives up the claim of the calling
 * thread alone, whereas `js_threadsafe_function_abort` releases the function
 * outright, regardless of the threads that still hold it.
 */
typedef enum {
  js_threadsafe_function_release = 0,
  js_threadsafe_function_abort = 1
} js_threadsafe_function_release_mode_t;

/**
 * What a call to a threadsafe function does when its queue is full: a blocking
 * call waits for room, whereas a non-blocking call fails at once.
 */
typedef enum {
  js_threadsafe_function_nonblocking = 0,
  js_threadsafe_function_blocking = 1
} js_threadsafe_function_call_mode_t;

/**
 * The kind of garbage collection being performed. A generational collection
 * only visits the most recently allocated objects and is comparatively cheap,
 * whereas a mark-compact collection visits the whole heap.
 */
typedef enum {
  js_garbage_collection_type_mark_compact = 1,
  js_garbage_collection_type_generational = 2
} js_garbage_collection_type_t;

/**
 * Called when a function created with `js_create_function()` and its siblings
 * is invoked. The arguments of the call are read with
 * `js_get_callback_info()`.
 *
 * The value returned becomes the return value of the call, `NULL` standing in
 * for `undefined`. To throw instead, return `NULL` after making an exception
 * pending on the environment.
 */
typedef js_value_t *(*js_function_cb)(js_env_t *, js_callback_info_t *);

/**
 * Called when a value that native data was attached to has been collected, or
 * when the data is otherwise released, and is passed the data and the hint it
 * was attached with.
 *
 * As collection is at the discretion of the engine, a finalize callback is not
 * guaranteed to run at all. It is also called at a point where the engine is
 * not necessarily able to run JavaScript, so it should do no more than release
 * the data it is given, and must not assume that the environment it is passed,
 * which may be `NULL`, is usable.
 */
typedef void (*js_finalize_cb)(js_env_t *, void *data, void *finalize_hint);

/**
 * Called to read a property of an object created with `js_create_delegate()`.
 * The value returned becomes the value of the property, and returning `NULL`
 * leaves the read to the object itself.
 */
typedef js_value_t *(*js_delegate_get_cb)(js_env_t *, js_value_t *property, void *data);

/**
 * Called to test for a property of an object created with
 * `js_create_delegate()`. Returning `false` makes the property absent, in
 * which case reading it yields `undefined` without the get callback being
 * consulted.
 */
typedef bool (*js_delegate_has_cb)(js_env_t *, js_value_t *property, void *data);

/**
 * Called to write a property of an object created with `js_create_delegate()`.
 * Returning `true` reports the write as done, and returning `false` leaves it
 * to the object itself.
 */
typedef bool (*js_delegate_set_cb)(js_env_t *, js_value_t *property, js_value_t *value, void *data);

/**
 * Called to delete a property of an object created with
 * `js_create_delegate()`. Returning `true` reports the deletion as done, and
 * returning `false` leaves it to the object itself.
 */
typedef bool (*js_delegate_delete_property_cb)(js_env_t *, js_value_t *property, void *data);

/**
 * Called to enumerate the own properties of an object created with
 * `js_create_delegate()`. The value returned must be an array of property
 * keys, and returning `NULL` leaves the enumeration to the object itself.
 */
typedef js_value_t *(*js_delegate_own_keys_cb)(js_env_t *, void *data);

/**
 * Called once for every import declared by a module being instantiated with
 * `js_instantiate_module()`, and is passed the specifier of the import, its
 * import attributes as an object, and the module the import appears in.
 *
 * The module returned is linked as the target of the import. To fail the
 * instantiation instead, return `NULL` after making an exception pending on
 * the environment.
 */
typedef js_module_t *(*js_module_resolve_cb)(js_env_t *, js_value_t *specifier, js_value_t *assertions, js_module_t *referrer, void *data);

/**
 * Called the first time the `import.meta` object of a module is accessed, and
 * is passed the module and the object, which the callback is free to populate.
 */
typedef void (*js_module_meta_cb)(js_env_t *, js_module_t *module, js_value_t *meta, void *data);

/**
 * Called to evaluate a module created with `js_create_synthetic_module()`,
 * which must set each of the exports declared by the module with
 * `js_set_module_export()`.
 */
typedef void (*js_module_evaluate_cb)(js_env_t *, js_module_t *module, void *data);

/**
 * Called when an exception reaches the bottom of the JavaScript execution
 * stack without being caught, and is passed the value that was thrown.
 */
typedef void (*js_uncaught_exception_cb)(js_env_t *, js_value_t *error, void *data);

/**
 * Called when a promise is still rejected after a microtask checkpoint without
 * its rejection having been handled, and is passed the reason and the promise.
 */
typedef void (*js_unhandled_rejection_cb)(js_env_t *, js_value_t *reason, js_value_t *promise, void *data);

/**
 * Called when a dynamic `import()` is evaluated, and is passed the specifier
 * of the import, its import attributes as an object, the referrer that
 * initiated it, and the identifier of the compilation unit the referrer
 * belongs to.
 *
 * The value returned is the result of the import: either the namespace of the
 * imported module, or a promise that is fulfilled with it once the module has
 * been loaded. To fail the import instead, return `NULL` after making an
 * exception pending on the environment.
 */
typedef js_value_t *(*js_dynamic_import_cb)(js_env_t *, js_value_t *specifier, js_value_t *assertions, js_value_t *referrer, js_value_t *id, void *data);

/**
 * Called on the loop of the environment for every call made with
 * `js_call_threadsafe_function()`, and is passed the function the threadsafe
 * function was created with, which may be `NULL`, its context, and the data of
 * the call.
 */
typedef void (*js_threadsafe_function_cb)(js_env_t *, js_value_t *function, void *context, void *data);

/**
 * Called when a task queued against the environment, such as a microtask
 * queued with `js_queue_microtask_with_callback()`, is run.
 */
typedef void (*js_task_cb)(js_env_t *, void *data);

/**
 * Called when the environment is being destroyed, and is passed the data the
 * callback was registered with. The environment must not be used from within
 * the callback.
 */
typedef void (*js_teardown_cb)(void *data);

/**
 * Called when the environment is being destroyed, and is passed the handle of
 * the callback and the data it was registered with. The environment is not
 * destroyed until the handle has been finished with
 * `js_finish_deferred_teardown_callback()`, which the callback need not do
 * itself.
 */
typedef void (*js_deferred_teardown_cb)(js_deferred_teardown_t *, void *data);

/**
 * Called with every message an inspector session sends. The message is UTF-8
 * encoded JSON that is only valid for the duration of the call.
 */
typedef void (*js_inspector_message_cb)(js_env_t *, js_inspector_t *, const char *message, size_t len, void *data);

/**
 * Called repeatedly for as long as execution is paused by an inspector
 * session. Return `true` to remain paused or `false` to give up waiting and
 * let execution continue.
 */
typedef bool (*js_inspector_paused_cb)(js_env_t *, js_inspector_t *, void *data);

/**
 * Called before and after each garbage collection, and is passed the type of
 * the collection. The callback must not re-enter the environment.
 */
typedef void (*js_garbage_collection_cb)(js_garbage_collection_type_t, void *data);

/**
 * The options an engine is configured with when a platform is created. As the
 * engine is configured globally, the options apply to every environment
 * created from the platform.
 *
 * @version 1
 */
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
   * Disable the optimizing compiler, if the engine has one.
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
   * Trace deoptimizations made by the optimizing compiler based on type feedback.
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

/**
 * The limits an engine imposes on the values that may be created within an
 * environment, as reported by `js_get_platform_limits()`.
 *
 * @version 0
 */
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

/**
 * The options an environment is created with.
 *
 * @version 0
 */
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

/**
 * A property to be defined on an object with `js_define_properties()` or on a
 * class with `js_define_class()`.
 *
 * @version 0
 */
struct js_property_descriptor_s {
  int version;

  /**
   * The key of the property, which must be a string or a symbol.
   *
   * @since 0
   */
  js_value_t *name;

  /**
   * The data passed to the `method`, `getter` and `setter` of the property.
   *
   * @since 0
   */
  void *data;

  /**
   * The attributes of the property, combined as a bit set.
   *
   * @since 0
   */
  int attributes;

  // One of:

  // Method

  /**
   * A function to define the property as, which is otherwise an ordinary data
   * property.
   *
   * @since 0
   */
  js_function_cb method;

  // Accessor

  /**
   * The getter of an accessor property. Either the getter or the setter may be
   * omitted, in which case the property cannot be read or written
   * respectively.
   *
   * @since 0
   */
  js_function_cb getter;

  /**
   * The setter of an accessor property.
   *
   * @since 0
   */
  js_function_cb setter;

  // Value

  /**
   * The value of a data property.
   *
   * @since 0
   */
  js_value_t *value;
};

/**
 * The callbacks that the property access of an object created with
 * `js_create_delegate()` is delegated to. Any callback may be omitted, in
 * which case the operation it covers is left to the object itself.
 *
 * @version 0
 */
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

/**
 * A 128-bit value identifying the native type of an object, as added with
 * `js_add_type_tag()`. Any two tags that are not meant to identify the same
 * type must differ, so a tag is best written down as a literal generated at
 * random rather than derived from anything at runtime.
 *
 * @version 0
 */
struct js_type_tag_s {
  /**
   * The low 64 bits of the tag.
   *
   * @since 0
   */
  uint64_t lower;

  /**
   * The high 64 bits of the tag.
   *
   * @since 0
   */
  uint64_t upper;
};

/**
 * The signature of the unwrapped entry point of a function created with
 * `js_create_typed_function()`, given in terms of the native types.
 *
 * @version 0
 */
struct js_callback_signature_s {
  int version;

  /**
   * The type of the value returned by the entry point.
   *
   * @since 0
   */
  int result;

  /**
   * The number of parameter types, including that of the receiver.
   *
   * @since 0
   */
  size_t args_len;

  /**
   * The types of the parameters of the entry point, the first of which is
   * always that of the receiver.
   *
   * @since 0
   */
  int *args;
};

/**
 * Statistics for the JavaScript heap of an environment, as reported by
 * `js_get_heap_statistics()`.
 *
 * @version 1
 */
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

/**
 * Statistics for one of the spaces that make up the JavaScript heap, as
 * reported by `js_get_heap_space_statistics()`.
 *
 * @version 0
 */
struct js_heap_space_statistics_s {
  int version;

  /**
   * The name of the space, which is engine specific. The string is owned by
   * the engine and remains valid for the lifetime of the environment.
   *
   * @since 0
   */
  const char *space_name;

  /**
   * The amount of memory currently committed for the space.
   *
   * @since 0
   */
  size_t space_size;

  /**
   * The size of all objects residing in the space.
   *
   * @since 0
   */
  size_t space_used_size;

  /**
   * The amount of memory that the space may still grow by before it has to be
   * collected.
   *
   * @since 0
   */
  size_t space_available_size;
};

/**
 * The location in the source at which an error was created, as reported by
 * `js_get_error_location()`.
 *
 * @version 0
 */
struct js_error_location_s {
  int version;

  /**
   * The name of the script the error was created in, as given when the script
   * was compiled, or `undefined` if it is not known.
   *
   * @since 0
   */
  js_value_t *name;

  /**
   * The source of the script the error was created in, or `undefined` if it is
   * not known.
   *
   * @since 0
   */
  js_value_t *source;

  /**
   * The line the error was created on, counting from 1.
   *
   * @since 0
   */
  int64_t line;

  /**
   * The column the error was created on, counting from 0.
   *
   * @since 0
   */
  int64_t column_start;

  /**
   * The column just past the end of the offending source text.
   *
   * @since 0
   */
  int64_t column_end;
};

/**
 * The callbacks to invoke around each garbage collection, as registered with
 * `js_enable_garbage_collection_tracking()`.
 *
 * @version 0
 */
struct js_garbage_collection_tracking_options_s {
  int version;

  /**
   * Called before a collection begins.
   *
   * @since 0
   */
  js_garbage_collection_cb start;

  /**
   * Called after a collection has finished.
   *
   * @since 0
   */
  js_garbage_collection_cb end;
};

/**
 * Create a platform for running JavaScript environments, scheduling any tasks
 * it needs to perform on the given loop.
 *
 * The options may be `NULL`, in which case the defaults are used, and are only
 * read for the duration of the call. As the engine is initialized and
 * configured globally, at most one platform may exist at a time within a
 * process.
 */
int
js_create_platform(uv_loop_t *loop, const js_platform_options_t *options, js_platform_t **result);

/**
 * Destroy a platform. The platform must not be used after this function is
 * called, and the behavior is undefined if it is.
 *
 * The resources held by the platform are not released until every environment
 * created from it has been destroyed and the loop has been run long enough for
 * the handles of the platform to close.
 */
int
js_destroy_platform(js_platform_t *platform);

/**
 * Get the identifier of the engine backing the platform, such as `v8`. The
 * string is statically allocated and remains valid for the lifetime of the
 * process.
 */
int
js_get_platform_identifier(js_platform_t *platform, const char **result);

/**
 * Get the version of the engine backing the platform. The string is statically
 * allocated and remains valid for the lifetime of the process. Its format is
 * engine specific and should not be parsed.
 */
int
js_get_platform_version(js_platform_t *platform, const char **result);

/**
 * Get the limits imposed by the engine on the values that may be created
 * within an environment. The caller must initialize the `version` field of the
 * result before the call to declare which fields it knows about.
 */
int
js_get_platform_limits(js_platform_t *platform, js_platform_limits_t *result);

/**
 * Get the loop that the platform was created with.
 */
int
js_get_platform_loop(js_platform_t *platform, uv_loop_t **result);

/**
 * Create a JavaScript environment, scheduling its tasks on the given loop. The
 * options may be `NULL`, in which case the defaults are used, and are only
 * read for the duration of the call.
 *
 * An environment owns an isolated JavaScript heap and is created with a
 * default context already entered. Values, and anything derived from them,
 * belong to the environment that created them and may never be passed to
 * another. Several environments may be created from the same platform and run
 * on separate loops and threads, but an environment may only be used from the
 * thread that its loop runs on.
 */
int
js_create_env(uv_loop_t *loop, js_platform_t *platform, const js_env_options_t *options, js_env_t **result);

/**
 * Destroy an environment, running any teardown callbacks registered with
 * `js_add_teardown_callback()` in the reverse order of their registration. The
 * environment must not be used after this function is called, and the behavior
 * is undefined if it is, with the exception of the deferred teardown callbacks
 * registered with `js_add_deferred_teardown_callback()`.
 *
 * If any deferred teardown callbacks are outstanding, the resources held by
 * the environment are not released until the last of them has been finished
 * with `js_finish_deferred_teardown_callback()` and the loop has been run
 * again.
 */
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
 * Add a callback for dynamic `import()` statements with deferred resolution.
 * By default, a dynamic import will result in either an uncaught exception or
 * an unhandled promise rejection during script or module evaluation.
 */
int
js_on_dynamic_import(js_env_t *env, js_dynamic_import_cb cb, void *data);

/**
 * Get the loop that the environment was created with.
 */
int
js_get_env_loop(js_env_t *env, uv_loop_t **result);

/**
 * Get the platform that the environment was created from.
 */
int
js_get_env_platform(js_env_t *env, js_platform_t **result);

/**
 * Unless otherwise stated, the following functions will rethrow any pending
 * exception set on the associated JavaScript environment. To handle a pending
 * exception, call `js_get_and_clear_last_exception()`. If the exception cannot
 * be handled it may be rethrown with `js_throw()`.
 */

/**
 * Open a handle scope. Values created while the scope is open are owned by it
 * and remain valid until it is closed.
 *
 * Handle scopes form a stack and must be closed in the reverse order in which
 * they were opened. The behavior is undefined if a scope is closed out of
 * order or more than once, or if a value is used after the scope that owns it
 * has been closed.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_open_handle_scope(js_env_t *env, js_handle_scope_t **result);

/**
 * Close a handle scope, releasing the values it owns. The scope must be the
 * innermost scope that is currently open.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_close_handle_scope(js_env_t *env, js_handle_scope_t *scope);

/**
 * Open a handle scope from which a single value may be promoted to the
 * enclosing scope with `js_escape_handle()`. The scope behaves as one opened
 * with `js_open_handle_scope()` in all other respects.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_open_escapable_handle_scope(js_env_t *env, js_escapable_handle_scope_t **result);

/**
 * Close an escapable handle scope, releasing the values it owns. The scope
 * must be the innermost scope that is currently open.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_close_escapable_handle_scope(js_env_t *env, js_escapable_handle_scope_t *scope);

/**
 * Promote a value to the handle scope enclosing the given escapable handle
 * scope, letting it outlive the scope in which it was created. The scope must
 * still be open, and the behavior is undefined if a value has already been
 * escaped from it.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_escape_handle(js_env_t *env, js_escapable_handle_scope_t *scope, js_value_t *escapee, js_value_t **result);

/**
 * Create a context with a global object of its own. Values may be passed
 * freely between the contexts of an environment, but not between environments.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_context(js_env_t *env, js_context_t **result);

/**
 * Destroy a context. The behavior is undefined if the context is entered when
 * it is destroyed, or if it is used afterwards.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_destroy_context(js_env_t *env, js_context_t *context);

/**
 * Enter a context, making it the context that subsequent operations are
 * performed in. Contexts form a stack and must be exited in the reverse order
 * in which they were entered.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_enter_context(js_env_t *env, js_context_t *context);

/**
 * Exit a context, restoring the context that was entered before it. The
 * context must be the innermost context that is currently entered.
 *
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

/**
 * Compile and run a script in the current context, yielding the completion
 * value of the script.
 *
 * The `file` names the script in stack traces and may be `NULL` if `len` is
 * `0`, and `offset` is the line within the file at which the source begins.
 * The `result` may be `NULL` if the completion value is not needed.
 *
 * A script run with this function carries no identifier of its own and is
 * instead attributed to the identifier returned by
 * `js_get_default_module_id()`, which it shares with every other such
 * compilation unit. Use `js_prepare_script()` if the script must be told apart
 * from them.
 */
int
js_run_script(js_env_t *env, const char *file, size_t len, int offset, js_value_t *source, js_value_t **result);

/**
 * Compile a script without running it, yielding a reusable handle that can be
 * run with `js_run_prepared_script()`. Unlike `js_run_script()`, a prepared
 * script carries a unique identifier of its own, allowing dynamic `import()`
 * calls it initiates to be attributed to it specifically.
 *
 * The `file` names the script in stack traces and may be `NULL` if `len` is
 * `0`, and `offset` is the line within the file at which the source begins.
 * The script must eventually be deleted with `js_delete_script()`.
 */
int
js_prepare_script(js_env_t *env, const char *file, size_t len, int offset, js_value_t *source, js_script_t **result);

/**
 * As `js_prepare_script()`, but with a code cache previously produced by
 * `js_create_script_code_cache()` supplied up front. If the cache is accepted
 * the engine grafts the cached bytecode onto a freshly minted identifier and
 * skips the parse and compile step; if it is rejected the engine silently
 * recompiles from `source`, so a rejected cache is a missed optimization, never
 * a failure.
 *
 * The `source` text and origin must still be supplied, as the engine validates
 * the cache against them. On return, `*cache_rejected` reports whether the cache
 * was usable; it may be `NULL` if the caller does not care.
 */
int
js_prepare_script_with_code_cache(js_env_t *env, const char *file, size_t len, int offset, js_value_t *source, const void *cached_data, size_t cached_data_len, bool *cache_rejected, js_script_t **result);

/**
 * Extract the compiled bytecode of a script prepared with `js_prepare_script()`
 * as a code cache. On success, `*data` is a newly allocated buffer of `*len`
 * bytes owned by the caller, to be released with `free()`. The bytes may be
 * persisted (e.g. to disk) and later handed back to
 * `js_prepare_script_with_code_cache()` to skip recompilation.
 *
 * The code cache stores compiled bytecode only; the script's identifier is not
 * part of the cache and is re-minted on the consume side.
 */
int
js_create_script_code_cache(js_env_t *env, js_script_t *script, void **data, size_t *len);

/**
 * Run a script previously compiled with `js_prepare_script()`. The script may
 * be run more than once and in any context entered when it is run. The
 * `result` may be `NULL` if the completion value is not needed.
 */
int
js_run_prepared_script(js_env_t *env, js_script_t *script, js_value_t **result);

/**
 * Delete a script previously compiled with `js_prepare_script()`, releasing the
 * resources held by its handle.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_delete_script(js_env_t *env, js_script_t *script);

/**
 * Get the name of a script compiled with `js_prepare_script()`, as passed to
 * `js_prepare_script()` when the script was compiled.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_script_name(js_env_t *env, js_script_t *script, const char **result);

/**
 * Get the unique identifier of a script compiled with `js_prepare_script()`.
 * The identifier is a `Symbol` owned by the engine that is stable for the
 * lifetime of the script and matches the `id` passed to the dynamic `import()`
 * callback when the script is the referrer.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_script_id(js_env_t *env, js_script_t *script, js_value_t **result);

/**
 * Add a callback for dynamic `import()` statements appearing in a single
 * compiled unit, taking precedence over the callback added with
 * `js_on_dynamic_import()`. A unit with a callback of its own may import even
 * if the environment has none.
 *
 * A unit takes at most one callback, and registering a second one throws. The
 * callback is not part of any code cache and must be added again for every
 * load, as the identifier of the unit must.
 *
 * A unit must have an identifier of its own to take a callback. Code compiled
 * without one, such as scripts run with `js_run_script()`, shares the
 * identifier returned by `js_get_default_module_id()`, cannot be told apart
 * from any other such code, and so is refused; it always falls back to the
 * callback added with `js_on_dynamic_import()`.
 */
int
js_on_script_dynamic_import(js_env_t *env, js_script_t *script, js_dynamic_import_cb cb, void *data);

/**
 * Compile a source text module. The `name` names the module in stack traces
 * and is the name reported by `js_get_module_name()`, and `offset` is the line
 * within the file at which the source begins.
 *
 * The `cb` is invoked to populate the `import.meta` object of the module the
 * first time it is accessed during evaluation and may be `NULL`.
 *
 * The module must be instantiated with `js_instantiate_module()` before it can
 * be run, and must eventually be deleted with `js_delete_module()`.
 */
int
js_create_module(js_env_t *env, const char *name, size_t len, int offset, js_value_t *source, js_module_meta_cb cb, void *data, js_module_t **result);

/**
 * As `js_create_module()`, but with a code cache previously produced by
 * `js_create_module_code_cache()` supplied up front. Behaves exactly as
 * `js_prepare_script_with_code_cache()` with respect to the cache: it is a hint,
 * validated against `source` and the origin, and `*cache_rejected` reports
 * whether it was usable (`NULL` if the caller does not care).
 */
int
js_create_module_with_code_cache(js_env_t *env, const char *name, size_t len, int offset, js_value_t *source, const void *cached_data, size_t cached_data_len, bool *cache_rejected, js_module_meta_cb cb, void *data, js_module_t **result);

/**
 * Extract the compiled bytecode of a source-text module created with
 * `js_create_module()` as a code cache. On success, `*data` is a newly
 * allocated buffer of `*len` bytes owned by the caller, to be released with
 * `free()`.
 *
 * The module must be unevaluated: produce the cache after `js_create_module()`
 * (or after `js_instantiate_module()`) but before `js_run_module()`. Synthetic
 * modules have no source and are not supported.
 */
int
js_create_module_code_cache(js_env_t *env, js_module_t *module, void **data, size_t *len);

/**
 * Create a module whose exports are provided by native code rather than by
 * source text. The `export_names` are the names of the exports the module
 * declares, all of which must be set with `js_set_module_export()` when the
 * `cb` is invoked to evaluate the module.
 *
 * A synthetic module has no source, and so cannot report an `import.meta`
 * object, cannot be the referrer of a dynamic `import()`, and cannot be
 * serialized to a code cache. It must still be instantiated with
 * `js_instantiate_module()` before it can be run, and must eventually be
 * deleted with `js_delete_module()`.
 */
int
js_create_synthetic_module(js_env_t *env, const char *name, size_t len, js_value_t *const export_names[], size_t export_names_len, js_module_evaluate_cb cb, void *data, js_module_t **result);

/**
 * Delete a module, releasing the resources held by its handle. The behavior is
 * undefined if the module is used afterwards, or if it is deleted while still
 * part of a module graph that may yet be instantiated or evaluated, such as
 * one belonging to a module that imported it.
 *
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
 * Get the unique identifier of the module. The identifier is a `Symbol` owned
 * by the engine that is stable for the lifetime of the module and matches the
 * `id` passed to the dynamic `import()` callback when the module is the
 * referrer.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_module_id(js_env_t *env, js_module_t *module, js_value_t **result);

/**
 * Get the identifier shared by all compilation units that do not carry one of
 * their own, such as scripts run with `js_run_script()`. This is the `id`
 * passed to the dynamic `import()` callback when the referrer is one of those
 * units, allowing the caller to correlate the import with its context.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_default_module_id(js_env_t *env, js_value_t **result);

/**
 * Get the namespace object of the module. The module must have been
 * instantiated with `js_instantiate_module()`, as the namespace does not exist
 * before then.
 */
int
js_get_module_namespace(js_env_t *env, js_module_t *module, js_value_t **result);

/**
 * Set the value of one of the exports of a synthetic module. The module must
 * have been created with `js_create_synthetic_module()` and the name must be
 * one of the export names declared when it was created.
 */
int
js_set_module_export(js_env_t *env, js_module_t *module, js_value_t *name, js_value_t *value);

/**
 * Resolve and link the imports of a module, preparing it to be run with
 * `js_run_module()`.
 *
 * The `cb` is invoked once for every import declared by the module and must
 * return the module that the specifier resolves to, or throw and return
 * `NULL`. The callback is inherited by every module it returns that does not
 * already have one of its own, so a whole module graph can be linked with a
 * single callback.
 *
 * Instantiation is a no-op for a module that has already been instantiated,
 * but throws for one that is currently being instantiated or evaluated.
 */
int
js_instantiate_module(js_env_t *env, js_module_t *module, js_module_resolve_cb cb, void *data);

/**
 * Evaluate a module and the modules it imports, yielding a promise that is
 * fulfilled once evaluation completes and rejected if any of the modules
 * throws. The `result` may be `NULL` if the promise is not needed.
 *
 * The module must first have been instantiated with `js_instantiate_module()`;
 * running an uninstantiated or currently evaluating module throws. Running a
 * module that has already been evaluated yields the promise of that evaluation
 * without evaluating it again.
 */
int
js_run_module(js_env_t *env, js_module_t *module, js_value_t **result);

/**
 * Add a callback for dynamic `import()` statements appearing in a single
 * compiled unit, taking precedence over the callback added with
 * `js_on_dynamic_import()`. A unit with a callback of its own may import even
 * if the environment has none.
 *
 * A unit takes at most one callback, and registering a second one throws. The
 * callback is not part of any code cache and must be added again for every
 * load, as the identifier of the unit must.
 *
 * Synthetic modules have no source and so can never be the referrer of a
 * dynamic `import()`; registering against one throws.
 */
int
js_on_module_dynamic_import(js_env_t *env, js_module_t *module, js_dynamic_import_cb cb, void *data);

/**
 * Create a reference to a value that is independent of any handle scope and so
 * may outlive it. The reference must eventually be deleted with
 * `js_delete_reference()`.
 *
 * The `count` is the initial reference count. A reference with a count greater
 * than `0` is strong and keeps the value alive; one with a count of `0` is
 * weak and does not, in which case `js_get_reference_value()` yields `NULL`
 * once the value has been collected. Weak references are only meaningful for
 * objects, functions and symbols, it being left to the engine whether a value
 * of any other type is ever reported as collected.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_reference(js_env_t *env, js_value_t *value, uint32_t count, js_ref_t **result);

/**
 * Delete a reference, releasing the value it holds. The behavior is undefined
 * if the reference is used afterwards.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_delete_reference(js_env_t *env, js_ref_t *reference);

/**
 * Increment the reference count of a reference, making it strong if it was
 * weak, and yield the new count. The `result` may be `NULL` if the count is
 * not needed.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_reference_ref(js_env_t *env, js_ref_t *reference, uint32_t *result);

/**
 * Decrement the reference count of a reference, making it weak if the count
 * reaches `0`, and yield the new count. Decrementing a count that is already
 * `0` has no effect. The `result` may be `NULL` if the count is not needed.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_reference_unref(js_env_t *env, js_ref_t *reference, uint32_t *result);

/**
 * Get the value held by a reference as a value owned by the current handle
 * scope. If the reference is weak and its value has been collected, `NULL` is
 * yielded instead.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_reference_value(js_env_t *env, js_ref_t *reference, js_value_t **result);

/**
 * Define a class, yielding its constructor. The `constructor` callback is
 * invoked when the class is constructed with `js_new_instance()` or `new`, and
 * the `data` is passed to it and to every method and accessor declared by the
 * `properties` that does not carry data of its own.
 *
 * Properties declared with the `js_static` attribute are defined on the
 * constructor itself and the rest on its prototype. Methods defined on the
 * prototype throw if called with a receiver that is not an instance of the
 * class.
 *
 * The `name` may be `NULL`, in which case the class is anonymous.
 */
int
js_define_class(js_env_t *env, const char *name, size_t len, js_function_cb constructor, void *data, js_property_descriptor_t const properties[], size_t properties_len, js_value_t **result);

/**
 * Define properties on an object. Each descriptor must specify exactly one of
 * a `method`, a `getter` and `setter` pair, or a `value`, and is defined with
 * the property attributes given by its `attributes`. The `js_static` attribute
 * is only meaningful to `js_define_class()` and is ignored here.
 *
 * The `data` of a descriptor is passed to its method, getter, and setter.
 */
int
js_define_properties(js_env_t *env, js_value_t *object, js_property_descriptor_t const properties[], size_t properties_len);

/**
 * Associate a native pointer with an object, to be retrieved with
 * `js_unwrap()`. An object may be wrapped at most once, and wrapping one that
 * is already wrapped throws.
 *
 * The `finalize_cb` is invoked with the `data` and the `finalize_hint` once
 * the object has been collected and may be `NULL`. As collection is at the
 * discretion of the engine, the callback may never be invoked at all, in
 * particular for an object that is still alive when the environment is
 * destroyed; use a teardown callback for cleanup that must happen.
 *
 * If `result` is not `NULL` it is set to a weak reference to the object, which
 * the caller must eventually delete with `js_delete_reference()`.
 */
int
js_wrap(js_env_t *env, js_value_t *object, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_ref_t **result);

/**
 * Get the native pointer associated with an object by `js_wrap()`. Unwrapping
 * an object that is not wrapped throws.
 *
 * The wrapper is not inherited, so an object whose prototype is wrapped is not
 * itself wrapped.
 */
int
js_unwrap(js_env_t *env, js_value_t *object, void **result);

/**
 * Remove the association made by `js_wrap()`, yielding the native pointer and
 * detaching its finalize callback, which will not be invoked. The `result` may
 * be `NULL` if the pointer is not needed. Removing the wrapper of an object
 * that is not wrapped throws.
 */
int
js_remove_wrap(js_env_t *env, js_value_t *object, void **result);

/**
 * Create an object whose property access is delegated to a set of native
 * callbacks. Any callback may be `NULL`, in which case the corresponding
 * operation is not intercepted and is instead performed on the object itself.
 *
 * The `finalize_cb` is invoked with the `data` and the `finalize_hint` once
 * the object has been collected and may be `NULL`, with the same caveats as
 * the callback passed to `js_wrap()`.
 */
int
js_create_delegate(js_env_t *env, const js_delegate_callbacks_t *callbacks, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result);

/**
 * Add a finalize callback to an object, invoked with the `data` and the
 * `finalize_hint` once the object has been collected. Unlike `js_wrap()`, no
 * pointer is associated with the object and any number of callbacks may be
 * added to it.
 *
 * As collection is at the discretion of the engine, the callback may never be
 * invoked at all, in particular for an object that is still alive when the
 * environment is destroyed; use a teardown callback for cleanup that must
 * happen.
 *
 * If `result` is not `NULL` it is set to a weak reference to the object, which
 * the caller must eventually delete with `js_delete_reference()`.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_add_finalizer(js_env_t *env, js_value_t *object, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_ref_t **result);

/**
 * Tag an object with a value that identifies its native type, to be checked
 * with `js_check_type_tag()`. An object may be tagged at most once, and
 * tagging one that is already tagged throws.
 *
 * The tag is copied and need not remain valid after the call.
 */
int
js_add_type_tag(js_env_t *env, js_value_t *object, const js_type_tag_t *tag);

/**
 * Check whether an object was tagged with the given tag by
 * `js_add_type_tag()`, which is the case only if the tags match exactly. An
 * object that was never tagged reports `false` rather than throwing.
 *
 * The tag is not inherited, so an object whose prototype is tagged is not
 * itself tagged.
 */
int
js_check_type_tag(js_env_t *env, js_value_t *object, const js_type_tag_t *tag, bool *result);

/**
 * Create a `Number` from a signed 32-bit integer.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_int32(js_env_t *env, int32_t value, js_value_t **result);

/**
 * Create a `Number` from an unsigned 32-bit integer.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_uint32(js_env_t *env, uint32_t value, js_value_t **result);

/**
 * Create a `Number` from a signed 64-bit integer. As numbers are represented
 * as doubles, an integer outside the safe integer range is rounded to the
 * nearest representable value.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_int64(js_env_t *env, int64_t value, js_value_t **result);

/**
 * Create a `Number` from a double.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_double(js_env_t *env, double value, js_value_t **result);

/**
 * Create a `BigInt` from a signed 64-bit integer.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_bigint_int64(js_env_t *env, int64_t value, js_value_t **result);

/**
 * Create a `BigInt` from an unsigned 64-bit integer.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_bigint_uint64(js_env_t *env, uint64_t value, js_value_t **result);

/**
 * Create a `BigInt` from an array of 64-bit words holding its magnitude,
 * ordered from least to most significant. The `sign` is `0` for a positive
 * value and `1` for a negative one, and `len` is the number of words, which
 * must all be readable.
 *
 * Throws if the resulting value is larger than the engine supports.
 */
int
js_create_bigint_words(js_env_t *env, int sign, const uint64_t *words, size_t len, js_value_t **result);

/**
 * Create a `String` from UTF-8 encoded text. The `len` is the length of the
 * text in code units, or `(size_t) -1` if it is NUL terminated, and the text
 * is copied and need not remain valid after the call.
 *
 * Throws if the resulting string would be longer than
 * `js_platform_limits_t::string_length`.
 */
int
js_create_string_utf8(js_env_t *env, const utf8_t *str, size_t len, js_value_t **result);

/**
 * Create a `String` from UTF-16LE encoded text. The `len` is the length of the
 * text in code units, or `(size_t) -1` if it is NUL terminated, and the text
 * is copied and need not remain valid after the call.
 *
 * Throws if the resulting string would be longer than
 * `js_platform_limits_t::string_length`.
 */
int
js_create_string_utf16le(js_env_t *env, const utf16_t *str, size_t len, js_value_t **result);

/**
 * Create a `String` from Latin-1 encoded text. The `len` is the length of the
 * text in code units, or `(size_t) -1` if it is NUL terminated, and the text
 * is copied and need not remain valid after the call.
 *
 * Throws if the resulting string would be longer than
 * `js_platform_limits_t::string_length`.
 */
int
js_create_string_latin1(js_env_t *env, const latin1_t *str, size_t len, js_value_t **result);

/**
 * Create a `String` that borrows UTF-8 encoded text rather than copying it.
 * The `len` is the length of the text in code units, or `(size_t) -1` if it is
 * NUL terminated.
 *
 * Whether the text can be borrowed is up to the engine. On return, `copied`
 * reports whether it had to be copied after all and may be `NULL` if the
 * caller does not care. If the text was copied the finalize callback is
 * invoked before this function returns; otherwise the text must remain valid
 * until the finalize callback is invoked, which may be at any point after the
 * string has been collected.
 *
 * The finalize callback may be omitted if the text is guaranteed to outlive
 * the JavaScript environment.
 */
int
js_create_external_string_utf8(js_env_t *env, utf8_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result, bool *copied);

/**
 * Create a `String` that borrows UTF-16LE encoded text rather than copying it.
 * The `len` is the length of the text in code units, or `(size_t) -1` if it is
 * NUL terminated.
 *
 * Whether the text can be borrowed is up to the engine. On return, `copied`
 * reports whether it had to be copied after all and may be `NULL` if the
 * caller does not care. If the text was copied the finalize callback is
 * invoked before this function returns; otherwise the text must remain valid
 * until the finalize callback is invoked, which may be at any point after the
 * string has been collected.
 *
 * The finalize callback may be omitted if the text is guaranteed to outlive
 * the JavaScript environment.
 */
int
js_create_external_string_utf16le(js_env_t *env, utf16_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result, bool *copied);

/**
 * Create a `String` that borrows Latin-1 encoded text rather than copying it.
 * The `len` is the length of the text in code units, or `(size_t) -1` if it is
 * NUL terminated.
 *
 * Whether the text can be borrowed is up to the engine. On return, `copied`
 * reports whether it had to be copied after all and may be `NULL` if the
 * caller does not care. If the text was copied the finalize callback is
 * invoked before this function returns; otherwise the text must remain valid
 * until the finalize callback is invoked, which may be at any point after the
 * string has been collected.
 *
 * The finalize callback may be omitted if the text is guaranteed to outlive
 * the JavaScript environment.
 */
int
js_create_external_string_latin1(js_env_t *env, latin1_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result, bool *copied);

/**
 * Create a `String` intended for use as a property key. The string is interned
 * by the engine, making repeated property lookups with it cheaper at the cost
 * of a more expensive creation. It is in all other respects a string as
 * created by `js_create_string_utf8()`.
 */
int
js_create_property_key_utf8(js_env_t *env, const utf8_t *str, size_t len, js_value_t **result);

/**
 * Create a `String` intended for use as a property key. The string is interned
 * by the engine, making repeated property lookups with it cheaper at the cost
 * of a more expensive creation. It is in all other respects a string as
 * created by `js_create_string_utf16le()`.
 */
int
js_create_property_key_utf16le(js_env_t *env, const utf16_t *str, size_t len, js_value_t **result);

/**
 * Create a `String` intended for use as a property key. The string is interned
 * by the engine, making repeated property lookups with it cheaper at the cost
 * of a more expensive creation. It is in all other respects a string as
 * created by `js_create_string_latin1()`.
 */
int
js_create_property_key_latin1(js_env_t *env, const latin1_t *str, size_t len, js_value_t **result);

/**
 * Create a unique `Symbol`. The `description` is a string used when the symbol
 * is printed and may be `NULL`. It does not affect the identity of the symbol,
 * two symbols created with the same description being distinct.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_symbol(js_env_t *env, js_value_t *description, js_value_t **result);

/**
 * Get the `Symbol` with the given description from the global symbol registry,
 * creating it if it is not already registered, as `Symbol.for()` does. The
 * `len` is the length of the description in bytes, or `(size_t) -1` if it is
 * NUL terminated.
 */
int
js_symbol_for(js_env_t *env, const char *description, size_t len, js_value_t **result);

/**
 * Create an empty object with the default `Object` prototype.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_object(js_env_t *env, js_value_t **result);

/**
 * Create an empty object with the given prototype, which must be an object or
 * the `null` value.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_object_with_prototype(js_env_t *env, js_value_t *prototype, js_value_t **result);

/**
 * Create an object with the given prototype and properties, which is cheaper
 * than creating the object and then setting each property in turn. The
 * prototype must be an object or the `null` value, and both arrays must hold
 * `property_count` entries.
 *
 * The properties are defined as writable, enumerable and configurable data
 * properties, without invoking any setters on the prototype chain.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_object_with_properties(js_env_t *env, js_value_t *prototype, js_value_t *const property_names[], js_value_t *const property_values[], size_t property_count, js_value_t **result);

/**
 * Create a function that invokes a native callback, passing it the `data`. The
 * `name` is the name reported by the `name` property of the function and may
 * be `NULL`, in which case the function is anonymous.
 */
int
js_create_function(js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data, js_value_t **result);

/**
 * Compile a function from source text, binding the given argument names, and
 * return the resulting callable. Like `js_prepare_script()`, the function
 * carries a unique identifier of its own, allowing dynamic `import()` calls it
 * initiates to be attributed to it specifically.
 *
 * The `source` is the body of the function alone and the `args` the names of
 * its parameters, given as strings. The `name` is the name reported by the
 * `name` property of the function and may be `NULL`, and the `file` names the
 * function in stack traces, with `offset` the line within the file at which
 * the body begins.
 *
 * Throws if the source or any of the argument names cannot be compiled, an
 * argument name that is not an identifier included.
 */
int
js_compile_function(js_env_t *env, const char *name, size_t name_len, const char *file, size_t file_len, js_value_t *const args[], size_t args_len, int offset, js_value_t *source, js_value_t **result);

/**
 * As `js_compile_function()`, but with a code cache previously produced by
 * `js_create_function_code_cache()` supplied up front. Behaves exactly as
 * `js_prepare_script_with_code_cache()` with respect to the cache: it is a hint,
 * validated against `source` and the origin, and `*cache_rejected` reports
 * whether it was usable (`NULL` if the caller does not care). On a rejected
 * cache the engine silently recompiles from `source`.
 */
int
js_compile_function_with_code_cache(js_env_t *env, const char *name, size_t name_len, const char *file, size_t file_len, js_value_t *const args[], size_t args_len, int offset, js_value_t *source, const void *cached_data, size_t cached_data_len, bool *cache_rejected, js_value_t **result);

/**
 * Extract the compiled bytecode of a function compiled with
 * `js_compile_function()` as a code cache. On success, `*data` is a newly
 * allocated buffer of `*len` bytes owned by the caller, to be released with
 * `free()`. The bytes may be persisted and later handed back to
 * `js_compile_function_with_code_cache()` to skip recompilation.
 *
 * Only a function returned by `js_compile_function()` carries the serializable
 * compiled form; passing any other function is an error. The code cache stores
 * compiled bytecode only; the function's identifier is not part of the cache
 * and is re-minted on the consume side.
 */
int
js_create_function_code_cache(js_env_t *env, js_value_t *function, void **data, size_t *len);

/**
 * @deprecated Use `js_compile_function()`, of which this is an alias.
 */
int
js_create_function_with_source(js_env_t *env, const char *name, size_t name_len, const char *file, size_t file_len, js_value_t *const args[], size_t args_len, int offset, js_value_t *source, js_value_t **result);

/**
 * Create a function that invokes a native callback, additionally offering the
 * engine an unwrapped entry point at `address` that it may call directly from
 * optimized code.
 *
 * The `signature` describes the entry point: `result` is the type of its
 * return value and `args` the types of its parameters, the first of which is
 * always the receiver. The entry point is passed the receiver, the arguments,
 * and a `js_typed_callback_info_t` as its last argument.
 *
 * The unwrapped entry point is an optimization and is never guaranteed to be
 * used. If the engine does not support it, or does not support the signature,
 * the function silently falls back to a function created with
 * `js_create_function()`. The `cb` must therefore always be provided and must
 * be indistinguishable in behavior from the entry point.
 */
int
js_create_typed_function(js_env_t *env, const char *name, size_t len, js_function_cb cb, const js_callback_signature_t *signature, const void *address, void *data, js_value_t **result);

/**
 * Get the unique identifier of a function compiled with `js_compile_function()`.
 * The identifier is a `Symbol` owned by the engine that is stable for the
 * lifetime of the function and matches the `id` passed to the dynamic `import()`
 * callback when the function is the referrer.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_function_id(js_env_t *env, js_value_t *function, js_value_t **result);

/**
 * Add a callback for dynamic `import()` statements appearing in a single
 * compiled unit, taking precedence over the callback added with
 * `js_on_dynamic_import()`. A unit with a callback of its own may import even
 * if the environment has none.
 *
 * A unit takes at most one callback, and registering a second one throws. The
 * callback is not part of any code cache and must be added again for every
 * load, as the identifier of the unit must.
 *
 * The callback belongs to the unit that contains the function, which for a
 * function compiled with `js_compile_function()` is the function itself. A
 * function defined inside a script or module carries that unit instead, and
 * registering against it registers for the whole of it. One carrying no unit,
 * or one attributed to the default identifier, is refused.
 */
int
js_on_function_dynamic_import(js_env_t *env, js_value_t *function, js_dynamic_import_cb cb, void *data);

/**
 * Create an empty `Array`.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_array(js_env_t *env, js_value_t **result);

/**
 * Create an `Array` of the given length with all of its elements empty. Throws
 * if the length is larger than the engine supports.
 */
int
js_create_array_with_length(js_env_t *env, size_t len, js_value_t **result);

/**
 * Create an `Array` with the given elements, which is cheaper than creating
 * the array and then setting each element in turn. The array must hold
 * `element_count` entries.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_array_with_elements(js_env_t *env, js_value_t *const elements[], size_t element_count, js_value_t **result);

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
 * Create a `Date` from a number of milliseconds since the Unix epoch. A time
 * that is `NaN` or outside the range representable by `Date` yields an invalid
 * date, whose value is `NaN`.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_date(js_env_t *env, double time, js_value_t **result);

/**
 * Create an `Error` with the given message, which must be a string. If `code`
 * is not `NULL` it is set as the `code` property of the error.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

/**
 * Create a `TypeError` with the given message, which must be a string. If
 * `code` is not `NULL` it is set as the `code` property of the error.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_type_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

/**
 * Create a `RangeError` with the given message, which must be a string. If
 * `code` is not `NULL` it is set as the `code` property of the error.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_range_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

/**
 * Create a `SyntaxError` with the given message, which must be a string. If
 * `code` is not `NULL` it is set as the `code` property of the error.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_syntax_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

/**
 * Create a `ReferenceError` with the given message, which must be a string. If
 * `code` is not `NULL` it is set as the `code` property of the error.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_reference_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result);

/**
 * Get the location in the source at which an error was created. The caller
 * must initialize the `version` field of the result before the call to declare
 * which fields it knows about.
 *
 * The location is where the error was created rather than where it was thrown,
 * and may be empty for an error that was created outside of any script.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_error_location(js_env_t *env, js_value_t *error, js_error_location_t *result);

/**
 * Create a pending promise together with the deferred used to settle it. The
 * deferred must eventually be settled with either `js_resolve_deferred()` or
 * `js_reject_deferred()`, exactly one of which may be called and only once.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_create_promise(js_env_t *env, js_deferred_t **deferred, js_value_t **promise);

/**
 * Resolve the promise associated with a deferred. The deferred is released and
 * must not be used afterwards.
 *
 * If no JavaScript is executing on the stack a microtask checkpoint is
 * performed before returning, running any reactions the promise has
 * registered.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_resolve_deferred(js_env_t *env, js_deferred_t *deferred, js_value_t *resolution);

/**
 * Reject the promise associated with a deferred. The deferred is released and
 * must not be used afterwards.
 *
 * If no JavaScript is executing on the stack a microtask checkpoint is
 * performed before returning, running any reactions the promise has
 * registered.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_reject_deferred(js_env_t *env, js_deferred_t *deferred, js_value_t *resolution);

/**
 * Get the state of a promise. The state of a promise only ever advances from
 * pending to either fulfilled or rejected.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_promise_state(js_env_t *env, js_value_t *promise, js_promise_state_t *result);

/**
 * Get the value a promise was fulfilled with, or the reason it was rejected
 * with. The behavior is undefined if the promise is still pending.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_promise_result(js_env_t *env, js_value_t *promise, js_value_t **result);

/**
 * Create an `ArrayBuffer` with 0-initialized data. If `data` is not `NULL` it
 * is set to a pointer to the contents of the buffer, which remains valid until
 * the buffer is detached or collected.
 *
 * Throws if the length exceeds `js_platform_limits_t::arraybuffer_length` or
 * the memory could not be allocated.
 */
int
js_create_arraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result);

/**
 * Create an `ArrayBuffer` that shares the contents of an existing backing
 * store, which remains owned by the caller and must still be released with
 * `js_release_arraybuffer_backing_store()`. Both `data` and `len` may be
 * `NULL` if the contents and their length are not needed.
 */
int
js_create_arraybuffer_with_backing_store(js_env_t *env, js_arraybuffer_backing_store_t *backing_store, void **data, size_t *len, js_value_t **result);

/**
 * Create an `ArrayBuffer` with uninitialized data, which is cheaper than
 * 0-initializing it but leaves the contents of the buffer unspecified until
 * they have been written. If `data` is not `NULL` it is set to a pointer to
 * the contents of the buffer, which remains valid until the buffer is detached
 * or collected.
 *
 * Throws if the length exceeds `js_platform_limits_t::arraybuffer_length` or
 * the memory could not be allocated.
 */
int
js_create_unsafe_arraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result);

/**
 * Create an `ArrayBuffer` with externally managed data. The data must remain
 * valid until either the finalize callback is invoked or the `ArrayBuffer` is
 * detached. The finalize callback may be omitted if the data is either
 * guaranteed to outlive the JavaScript environment or if the `ArrayBuffer` is
 * manually detached prior to the data becoming invalid.
 *
 * The finalize callback may be invoked without an environment and must not
 * assume that the one it is passed is usable.
 */
int
js_create_external_arraybuffer(js_env_t *env, void *data, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result);

/**
 * Detach an `ArrayBuffer` from its data, leaving it 0 bytes long and releasing
 * the data if it is owned by the engine. Detaching a buffer that is already
 * detached has no effect.
 *
 * The behavior is undefined if the buffer cannot be detached, whether that is
 * the case being up to the engine.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_detach_arraybuffer(js_env_t *env, js_value_t *arraybuffer);

/**
 * Get a reference to the memory backing an `ArrayBuffer`, keeping it alive
 * independently of the buffer it was taken from. The backing store must
 * eventually be released with `js_release_arraybuffer_backing_store()`, and
 * the behavior is undefined if it is used afterwards.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_arraybuffer_backing_store(js_env_t *env, js_value_t *arraybuffer, js_arraybuffer_backing_store_t **result);

/**
 * Create a `SharedArrayBuffer` with 0-initialized data. If `data` is not
 * `NULL` it is set to a pointer to the contents of the buffer, which remains
 * valid until the buffer, and every other buffer sharing its backing store,
 * has been collected.
 */
int
js_create_sharedarraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result);

/**
 * Create a `SharedArrayBuffer` that shares the contents of an existing backing
 * store, which remains owned by the caller and must still be released with
 * `js_release_arraybuffer_backing_store()`. Both `data` and `len` may be
 * `NULL` if the contents and their length are not needed.
 *
 * This is how memory is shared between environments: take the backing store of
 * a `SharedArrayBuffer` in one environment and create a `SharedArrayBuffer`
 * from it in another.
 */
int
js_create_sharedarraybuffer_with_backing_store(js_env_t *env, js_arraybuffer_backing_store_t *backing_store, void **data, size_t *len, js_value_t **result);

/**
 * Create a `SharedArrayBuffer` with uninitialized data, which is cheaper than
 * 0-initializing it but leaves the contents of the buffer unspecified until
 * they have been written. If `data` is not `NULL` it is set to a pointer to
 * the contents of the buffer.
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
 * Get a reference to the memory backing a `SharedArrayBuffer`, keeping it
 * alive independently of the buffer it was taken from. The backing store must
 * eventually be released with `js_release_arraybuffer_backing_store()`, and
 * the behavior is undefined if it is used afterwards.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_sharedarraybuffer_backing_store(js_env_t *env, js_value_t *sharedarraybuffer, js_arraybuffer_backing_store_t **result);

/**
 * Release a backing store previously obtained with
 * `js_get_arraybuffer_backing_store()` or
 * `js_get_sharedarraybuffer_backing_store()`. The memory itself is only
 * released once every buffer sharing it has also been collected.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_release_arraybuffer_backing_store(js_env_t *env, js_arraybuffer_backing_store_t *backing_store);

/**
 * Create a typed array of `len` elements of the given type over an
 * `ArrayBuffer` or `SharedArrayBuffer`, starting `offset` bytes into it.
 *
 * The offset must be aligned to the size of the element type and the view must
 * lie entirely within the buffer; the behavior is undefined otherwise.
 */
int
js_create_typedarray(js_env_t *env, js_typedarray_type_t type, size_t len, js_value_t *arraybuffer, size_t offset, js_value_t **result);

/**
 * Create a `DataView` of `len` bytes over an `ArrayBuffer` or
 * `SharedArrayBuffer`, starting `offset` bytes into it.
 *
 * The view must lie entirely within the buffer; the behavior is undefined
 * otherwise.
 */
int
js_create_dataview(js_env_t *env, size_t len, js_value_t *arraybuffer, size_t offset, js_value_t **result);

/**
 * Coerce a value to a `Boolean` as the `!!` operator would. The coercion
 * cannot observably fail and never runs user code.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_coerce_to_boolean(js_env_t *env, js_value_t *value, js_value_t **result);

/**
 * Coerce a value to a `Number` as the unary `+` operator would. The coercion
 * may run user code, such as a `valueOf()` method, and may therefore throw.
 */
int
js_coerce_to_number(js_env_t *env, js_value_t *value, js_value_t **result);

/**
 * Coerce a value to a `String` as the `String()` function would. The coercion
 * may run user code, such as a `toString()` method, and may therefore throw.
 * Note that a symbol cannot be coerced to a string and always throws.
 */
int
js_coerce_to_string(js_env_t *env, js_value_t *value, js_value_t **result);

/**
 * Coerce a value to an object as the `Object()` function would, wrapping a
 * primitive in its corresponding object type. Neither `null` nor `undefined`
 * can be coerced to an object and both throw.
 */
int
js_coerce_to_object(js_env_t *env, js_value_t *value, js_value_t **result);

/**
 * Get the type of a value. The result mirrors the `typeof` operator, except
 * that `null` is reported as `js_null` rather than as an object and that
 * external values are reported as `js_external`.
 *
 * For a finer classification of an object, use `js_get_object_type()`.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_typeof(js_env_t *env, js_value_t *value, js_value_type_t *result);

/**
 * Check whether a value is an instance of a constructor, as the `instanceof`
 * operator would. The constructor must be a function, and the check may run
 * user code, such as a `Symbol.hasInstance` method, and may therefore throw.
 */
int
js_instanceof(js_env_t *env, js_value_t *object, js_value_t *constructor, bool *result);

/**
 * The following functions test the type of a value. Unlike the rest of the
 * interface, they accept a value of any type, reporting `false` for one of a
 * type they do not cover, and so are the way to make a value of unknown
 * provenance safe to pass on. None of them coerce the value or run any user
 * code, and none of them throw.
 */

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
 * Check whether a value is a `Boolean` wrapper object, as created by `new
 * Boolean()`. A boolean primitive is not a wrapper object.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_boolean_object(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_number(js_env_t *env, js_value_t *value, bool *result);

/**
 * Check whether a value is a `Number` wrapper object, as created by `new
 * Number()`. A number primitive is not a wrapper object.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_number_object(js_env_t *env, js_value_t *value, bool *result);

/**
 * Check whether a value is a number that is exactly representable as a signed
 * 32-bit integer, and so may be read with `js_get_value_int32()`.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_int32(js_env_t *env, js_value_t *value, bool *result);

/**
 * Check whether a value is a number that is exactly representable as an
 * unsigned 32-bit integer, and so may be read with `js_get_value_uint32()`.
 *
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
 * Check whether a value is a `String` wrapper object, as created by `new
 * String()`. A string primitive is not a wrapper object.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_string_object(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_symbol(js_env_t *env, js_value_t *value, bool *result);

/**
 * Check whether a value is a `Symbol` wrapper object, as created by
 * `Object(Symbol())`. A symbol primitive is not a wrapper object.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_symbol_object(js_env_t *env, js_value_t *value, bool *result);

/**
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_object(js_env_t *env, js_value_t *value, bool *result);

/**
 * Check whether a value is callable. This includes classes, which are callable
 * but throw when called without `new`.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_function(js_env_t *env, js_value_t *value, bool *result);

/**
 * Check whether a value is a function declared `async`. Such a function is
 * also a function as far as `js_is_function()` is concerned.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_async_function(js_env_t *env, js_value_t *value, bool *result);

/**
 * Check whether a value is a generator function, i.e. the function that
 * produces a generator rather than the generator itself.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_generator_function(js_env_t *env, js_value_t *value, bool *result);

/**
 * Check whether a value is a generator, i.e. the object returned by calling a
 * generator function.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_generator(js_env_t *env, js_value_t *value, bool *result);

/**
 * Check whether a value is an `arguments` object.
 *
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
 * Check whether a value is an external created with `js_create_external()`. An
 * external is opaque to JavaScript and is not an object.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_external(js_env_t *env, js_value_t *value, bool *result);

/**
 * Check whether a value is an object with a native pointer associated with it
 * by `js_wrap()`. The wrapper is not inherited, so an object whose prototype
 * is wrapped is not itself wrapped.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_wrapped(js_env_t *env, js_value_t *value, bool *result);

/**
 * Check whether a value is an object created with `js_create_delegate()`.
 *
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
 * Check whether a value is a `BigInt` wrapper object, as created by
 * `Object(0n)`. A bigint primitive is not a wrapper object.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_bigint_object(js_env_t *env, js_value_t *value, bool *result);

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
 * Check whether a value is an error object, such as one created by the `Error`
 * constructor or one of its built-in subclasses. An object that merely has an
 * error on its prototype chain is not itself an error.
 *
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
 * Check whether a value is an `ArrayBuffer` that has been detached from its
 * data, either with `js_detach_arraybuffer()` or by being transferred away.
 *
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
 * Check whether a value is the namespace object of a module, as returned by
 * `js_get_module_namespace()`.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_module_namespace(js_env_t *env, js_value_t *value, bool *result);

/**
 * The behavior is undefined if the value is not an object.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_object_type(js_env_t *env, js_value_t *value, js_object_type_t *result);

/**
 * Check whether two values are equal as the `===` operator would, without
 * coercing either of them. Note that `NaN` is not equal to itself and that `0`
 * and `-0` are equal to one another.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_strict_equals(js_env_t *env, js_value_t *a, js_value_t *b, bool *result);

/**
 * Get the global object of the current context.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_global(js_env_t *env, js_value_t **result);

/**
 * Get the `undefined` value.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_undefined(js_env_t *env, js_value_t **result);

/**
 * Get the `null` value.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_null(js_env_t *env, js_value_t **result);

/**
 * Get one of the two `Boolean` values.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_boolean(js_env_t *env, bool value, js_value_t **result);

/**
 * Get the value of a `Boolean`. The value is not coerced; use
 * `js_coerce_to_boolean()` first if it may be of any other type.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_bool(js_env_t *env, js_value_t *value, bool *result);

/**
 * Get the value of a number as a signed 32-bit integer. The number must be
 * exactly representable as one, as reported by `js_is_int32()`, and the
 * behavior is undefined otherwise.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_int32(js_env_t *env, js_value_t *value, int32_t *result);

/**
 * Get the value of a number as an unsigned 32-bit integer. The number must be
 * exactly representable as one, as reported by `js_is_uint32()`, and the
 * behavior is undefined otherwise.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_uint32(js_env_t *env, js_value_t *value, uint32_t *result);

/**
 * Get the value of a number as a signed 64-bit integer, truncating towards
 * zero. A number that lies outside the range of the type is clamped to it, and
 * `NaN` and the infinities are reported as `0`.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_int64(js_env_t *env, js_value_t *value, int64_t *result);

/**
 * Get the value of a number as a double, which is lossless as numbers are
 * themselves doubles.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_double(js_env_t *env, js_value_t *value, double *result);

/**
 * Get the low 64 bits of a `BigInt` as a signed integer. If `lossless` is not
 * `NULL` it reports whether the value was representable without truncation.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_bigint_int64(js_env_t *env, js_value_t *value, int64_t *result, bool *lossless);

/**
 * Get the low 64 bits of a `BigInt` as an unsigned integer. If `lossless` is
 * not `NULL` it reports whether the value was representable without
 * truncation.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_bigint_uint64(js_env_t *env, js_value_t *value, uint64_t *result, bool *lossless);

/**
 * Get the magnitude of a `BigInt` as an array of 64-bit words, ordered from
 * least to most significant, along with its sign, which is `0` if positive and
 * `1` if negative.
 *
 * If both `sign` and `words` are `NULL`, `result` is set to the number of
 * words needed to hold the magnitude and must itself not be `NULL`. Otherwise,
 * both must be given, at most `len` words are written to `words`, and
 * `result`, if not `NULL`, is set to the number of words written.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_bigint_words(js_env_t *env, js_value_t *value, int *sign, uint64_t *words, size_t len, size_t *result);

/**
 * Get the contents of a string as UTF-8 encoded text.
 *
 * If `str` is `NULL`, `result` is set to the length of the text in bytes,
 * excluding the terminating NUL, and must itself not be `NULL`. Otherwise, at
 * most `len` bytes are written to `str` and `result`, if not `NULL`, is set to
 * the number of bytes written, again excluding the terminating NUL.
 *
 * The text is only NUL terminated if there is room left for it, and text that
 * does not fit is truncated rather than reported as an error. Any code unit
 * that cannot be represented, such as an unpaired surrogate, is replaced.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_string_utf8(js_env_t *env, js_value_t *value, utf8_t *str, size_t len, size_t *result);

/**
 * Get the contents of a string as UTF-16LE encoded text, which is lossless as
 * strings are themselves sequences of UTF-16 code units.
 *
 * If `str` is `NULL`, `result` is set to the length of the text in code units,
 * excluding the terminating NUL, and must itself not be `NULL`. Otherwise, at
 * most `len` code units are written to `str` and `result`, if not `NULL`, is
 * set to the number of code units written, again excluding the terminating
 * NUL.
 *
 * The text is only NUL terminated if there is room left for it, and text that
 * does not fit is truncated rather than reported as an error.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_string_utf16le(js_env_t *env, js_value_t *value, utf16_t *str, size_t len, size_t *result);

/**
 * Get the contents of a string as Latin-1 encoded text, truncating each code
 * unit to its low byte.
 *
 * If `str` is `NULL`, `result` is set to the length of the text in code units,
 * excluding the terminating NUL, and must itself not be `NULL`. Otherwise, at
 * most `len` code units are written to `str` and `result`, if not `NULL`, is
 * set to the number of code units written, again excluding the terminating
 * NUL.
 *
 * The text is only NUL terminated if there is room left for it, and text that
 * does not fit is truncated rather than reported as an error.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_string_latin1(js_env_t *env, js_value_t *value, latin1_t *str, size_t len, size_t *result);

/**
 * Get the pointer held by an external created with `js_create_external()`.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_external(js_env_t *env, js_value_t *value, void **result);

/**
 * Get the value of a `Date` as a number of milliseconds since the Unix epoch,
 * or `NaN` if the date is invalid.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_value_date(js_env_t *env, js_value_t *value, double *result);

/**
 * Get the length of an array, which is the index of its last element plus one
 * and so may be larger than the number of elements it actually holds.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_array_length(js_env_t *env, js_value_t *array, uint32_t *result);

/**
 * Get up to `len` elements of an array, starting at `offset`, writing them to
 * `elements`, which must have room for `len` of them. On return, `result`, if
 * not `NULL`, is set to the number of elements written, which is fewer than
 * `len` if the array ends first.
 *
 * The elements are read as ordinary property accesses and may therefore run
 * user code and throw.
 */
int
js_get_array_elements(js_env_t *env, js_value_t *array, js_value_t *elements[], size_t len, size_t offset, uint32_t *result);

/**
 * Set `len` elements of an array, starting at `offset`, from `elements`, which
 * must hold `len` of them. The array is grown as needed.
 *
 * The elements are written as ordinary property accesses and may therefore run
 * user code and throw.
 */
int
js_set_array_elements(js_env_t *env, js_value_t *array, js_value_t *const elements[], size_t len, size_t offset);

/**
 * Get the prototype of an object, which is either an object or the `null`
 * value.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_prototype(js_env_t *env, js_value_t *object, js_value_t **result);

/**
 * Set the prototype of an object, which must itself be an object or the `null`
 * value. Throws if the object is not extensible or if the assignment would
 * create a cycle in the prototype chain.
 */
int
js_set_prototype(js_env_t *env, js_value_t *object, js_value_t *prototype);

/**
 * Seal an object, marking it as not extensible and making all of its own
 * properties non-configurable. The values of its properties may still be
 * changed.
 */
int
js_seal(js_env_t *env, js_value_t *object);

/**
 * Freeze an object, marking it as not extensible and making all of its own
 * properties non-configurable and non-writable.
 */
int
js_freeze(js_env_t *env, js_value_t *object);

/**
 * Get the enumerable string-keyed properties of an object and of its prototype
 * chain as an array of strings, with array indices converted to strings. Use
 * `js_get_filtered_property_names()` for any other selection of properties.
 */
int
js_get_property_names(js_env_t *env, js_value_t *object, js_value_t **result);

/**
 * Get the properties of an object as an array of property keys.
 *
 * The `mode` selects whether the prototype chain of the object is included,
 * the `property_filter` which properties are collected, the `index_filter`
 * whether array indices are included, and the `key_conversion` whether array
 * indices are converted to strings or kept as numbers.
 */
int
js_get_filtered_property_names(js_env_t *env, js_value_t *object, js_key_collection_mode_t mode, js_property_filter_t property_filter, js_index_filter_t index_filter, js_key_conversion_mode_t key_conversion, js_value_t **result);

/**
 * Get a property of an object, or `undefined` if it has no such property. The
 * `key` must be a string or a symbol. The lookup may run user code, such as a
 * getter, and may therefore throw.
 */
int
js_get_property(js_env_t *env, js_value_t *object, js_value_t *key, js_value_t **result);

/**
 * Check whether an object or its prototype chain has a property, as the `in`
 * operator would. The `key` must be a string or a symbol.
 */
int
js_has_property(js_env_t *env, js_value_t *object, js_value_t *key, bool *result);

/**
 * Check whether an object itself has a property, disregarding its prototype
 * chain. The `key` must be a string or a symbol.
 */
int
js_has_own_property(js_env_t *env, js_value_t *object, js_value_t *key, bool *result);

/**
 * Set a property of an object. The `key` must be a string or a symbol.
 *
 * The assignment may run user code, such as a setter, and may therefore throw.
 * Whether an assignment that merely fails, such as one to a non-writable
 * property, is reported as an error is left to the engine, so do not rely on
 * either outcome.
 */
int
js_set_property(js_env_t *env, js_value_t *object, js_value_t *key, js_value_t *value);

/**
 * Delete a property of an object, as the `delete` operator would, reporting
 * whether the property is gone as a result. The `key` must be a string or a
 * symbol, and `result` may be `NULL` if the outcome is not needed.
 */
int
js_delete_property(js_env_t *env, js_value_t *object, js_value_t *key, bool *result);

/**
 * Get a property of an object by a NUL terminated name, which is equivalent to
 * `js_get_property()` with the name as a string.
 */
int
js_get_named_property(js_env_t *env, js_value_t *object, const char *name, js_value_t **result);

/**
 * Check whether an object or its prototype chain has a property with a NUL
 * terminated name, which is equivalent to `js_has_property()` with the name as
 * a string.
 */
int
js_has_named_property(js_env_t *env, js_value_t *object, const char *name, bool *result);

/**
 * Set a property of an object by a NUL terminated name, which is equivalent to
 * `js_set_property()` with the name as a string.
 */
int
js_set_named_property(js_env_t *env, js_value_t *object, const char *name, js_value_t *value);

/**
 * Delete a property of an object by a NUL terminated name, which is equivalent
 * to `js_delete_property()` with the name as a string.
 */
int
js_delete_named_property(js_env_t *env, js_value_t *object, const char *name, bool *result);

/**
 * Get an element of an object by index, which is equivalent to
 * `js_get_property()` with the index as a key.
 */
int
js_get_element(js_env_t *env, js_value_t *object, uint32_t index, js_value_t **result);

/**
 * Check whether an object or its prototype chain has an element at an index,
 * which is equivalent to `js_has_property()` with the index as a key.
 */
int
js_has_element(js_env_t *env, js_value_t *object, uint32_t index, bool *result);

/**
 * Set an element of an object by index, which is equivalent to
 * `js_set_property()` with the index as a key.
 */
int
js_set_element(js_env_t *env, js_value_t *object, uint32_t index, js_value_t *value);

/**
 * Delete an element of an object by index, which is equivalent to
 * `js_delete_property()` with the index as a key.
 */
int
js_delete_element(js_env_t *env, js_value_t *object, uint32_t index, bool *result);

/**
 * Borrow the contents of a string without copying or transcoding them. How a
 * string is stored is up to the engine, so `encoding` reports which encoding
 * the contents are already in and the caller must handle any of them. `str`
 * points into the string's own storage and `len` is its length in code units of
 * that encoding, both valid until the view is released with
 * `js_release_string_view()`.
 *
 * As the storage belongs to the engine, no other API may be called with `env`
 * while the view is open, as anything that allocates may move the string and
 * invalidate `str`. Release the view first, or copy out what must outlive it.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_string_view(js_env_t *env, js_value_t *string, js_string_encoding_t *encoding, const void **str, size_t *len, js_string_view_t **result);

/**
 * Release a view returned by `js_get_string_view()`, invalidating its `str` and
 * lifting the restriction on calling other APIs with `env`.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_release_string_view(js_env_t *env, js_string_view_t *view);

/**
 * Get the arguments and receiver of a call from within a native callback.
 *
 * If `argv` is not `NULL`, `argc` must point to the number of arguments it has
 * room for. At most that many arguments are copied into it and any entries
 * left over are filled with `undefined`, so a callback may always read the
 * number of arguments it expects. On return, `argc`, if not `NULL`, is set to
 * the number of arguments actually passed, which may be more than were copied.
 *
 * The `receiver` and `data` may be `NULL` if the `this` value of the call and
 * the data the function was created with are not needed.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_callback_info(js_env_t *env, const js_callback_info_t *info, size_t *argc, js_value_t *argv[], js_value_t **receiver, void **data);

/**
 * Get the environment and data associated with a function from within its
 * unwrapped entry point, given the `js_typed_callback_info_t` passed as the
 * last argument of the entry point. Either output may be `NULL`.
 *
 * Unlike an ordinary callback, an unwrapped entry point is not passed an
 * environment and must recover it this way if it needs one.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_typed_callback_info(const js_typed_callback_info_t *info, js_env_t **env, void **data);

/**
 * Get the `new.target` of a call from within a native callback, which is the
 * constructor being invoked when the function is called with `new` and
 * `undefined` when it is called as an ordinary function.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_new_target(js_env_t *env, const js_callback_info_t *info, js_value_t **result);

/**
 * Get the contents and length in bytes of an `ArrayBuffer`, either of which
 * may be `NULL` if it is not needed. The contents remain valid until the
 * buffer is detached or collected; a detached buffer has a length of `0`.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_arraybuffer_info(js_env_t *env, js_value_t *arraybuffer, void **data, size_t *len);

/**
 * Get the contents and length in bytes of a `SharedArrayBuffer`, either of
 * which may be `NULL` if it is not needed. The contents remain valid until
 * every buffer sharing them has been collected.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_sharedarraybuffer_info(js_env_t *env, js_value_t *sharedarraybuffer, void **data, size_t *len);

/**
 * Get the type, contents, length, backing buffer, and byte offset of a typed
 * array, any of which may be `NULL` if it is not needed.
 *
 * The `data` points at the first element of the view rather than at the start
 * of the buffer, and `len` counts elements rather than bytes. Asking for the
 * `arraybuffer` may force the engine to materialize a buffer for a view that
 * does not yet have one.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_typedarray_info(js_env_t *env, js_value_t *typedarray, js_typedarray_type_t *type, void **data, size_t *len, js_value_t **arraybuffer, size_t *offset);

/**
 * Get the contents, length in bytes, backing buffer, and byte offset of a
 * `DataView`, any of which may be `NULL` if it is not needed.
 *
 * The `data` points at the start of the view rather than at the start of the
 * buffer. Asking for the `arraybuffer` may force the engine to materialize a
 * buffer for a view that does not yet have one.
 *
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

/**
 * Enqueue a JavaScript function to be called with no arguments on the next
 * microtask checkpoint.
 *
 * If no JavaScript is executing on the stack a checkpoint is performed before
 * returning, in which case the function is called before this function
 * returns.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_queue_microtask(js_env_t *env, js_value_t *function);

/**
 * Enqueue a native callback to be called on the next microtask checkpoint.
 *
 * If no JavaScript is executing on the stack a checkpoint is performed before
 * returning, in which case the callback is called before this function
 * returns.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_queue_microtask_with_callback(js_env_t *env, js_task_cb cb, void *data);

/**
 * Construct an object by calling a function as a constructor, as the `new`
 * operator would.
 *
 * As with `js_call_function()`, a microtask checkpoint is performed before
 * returning if no JavaScript was already executing on the stack.
 */
int
js_new_instance(js_env_t *env, js_value_t *constructor, size_t argc, js_value_t *const argv[], js_value_t **result);

/**
 * Create a handle that may be used to call into the environment from any
 * thread.
 *
 * Either a `function` or a `cb` must be given. A call made with
 * `js_call_threadsafe_function()` is queued and dispatched on the loop of the
 * environment: if a `cb` was given it is invoked with the function, the
 * `context` and the data of the call, and otherwise the function is called
 * with no arguments.
 *
 * The `queue_limit` is the maximum number of calls that may be outstanding at
 * once, or `0` for no limit. The `initial_thread_count` is the number of
 * threads that hold the handle to begin with and must be greater than `0`.
 * Every thread that later acquires the handle with
 * `js_acquire_threadsafe_function()` must release it again with
 * `js_release_threadsafe_function()`, and the handle is released once the last
 * thread holding it has let go.
 *
 * The `finalize_cb` is invoked with the `context` and the `finalize_hint` on
 * the loop of the environment once the handle has been released.
 */
int
js_create_threadsafe_function(js_env_t *env, js_value_t *function, size_t queue_limit, size_t initial_thread_count, js_finalize_cb finalize_cb, void *finalize_hint, void *context, js_threadsafe_function_cb cb, js_threadsafe_function_t **result);

/**
 * Get the context that a threadsafe function was created with. May be called
 * from any thread.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_threadsafe_function_context(js_threadsafe_function_t *function, void **result);

/**
 * Call a threadsafe function, queueing the `data` to be dispatched on the loop
 * of the environment. May be called from any thread.
 *
 * The `mode` selects what happens when the queue is full: a blocking call
 * waits for room, whereas a non-blocking call fails immediately. A call also
 * fails if the handle has already been released. Taking no environment, the
 * function reports a failure as a negative value alone.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_call_threadsafe_function(js_threadsafe_function_t *function, void *data, js_threadsafe_function_call_mode_t mode);

/**
 * Acquire a threadsafe function on behalf of the calling thread, so that the
 * handle is not released while the thread still holds it. Fails if the handle
 * has already been released. May be called from any thread.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_acquire_threadsafe_function(js_threadsafe_function_t *function);

/**
 * Release a threadsafe function on behalf of the calling thread. May be called
 * from any thread.
 *
 * In `js_threadsafe_function_release` mode the handle is released once every
 * thread holding it has let go, whereas in `js_threadsafe_function_abort` mode
 * it is released at once, regardless of the other threads. Any call made after
 * the handle has been released fails.
 *
 * The handle itself stays valid until its finalize callback has been invoked
 * on the loop of the environment, and the behavior is undefined if it is used
 * after that.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_release_threadsafe_function(js_threadsafe_function_t *function, js_threadsafe_function_release_mode_t mode);

/**
 * Reference a threadsafe function, making it keep the loop of the environment
 * alive for as long as the handle is held. A threadsafe function is referenced
 * when it is created.
 *
 * Must be called from the thread that the loop of the environment runs on.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_ref_threadsafe_function(js_env_t *env, js_threadsafe_function_t *function);

/**
 * Unreference a threadsafe function, letting the loop of the environment exit
 * even though the handle is still held. Calls that are already queued are
 * still dispatched if the loop keeps running for other reasons.
 *
 * Must be called from the thread that the loop of the environment runs on.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_unref_threadsafe_function(js_env_t *env, js_threadsafe_function_t *function);

/**
 * Add a callback to be invoked with the `data` when the environment is
 * destroyed, in order to release resources that outlive it. Callbacks are
 * invoked in the reverse order of their registration.
 *
 * Throws if the same callback and data pair is already registered, or if the
 * environment is already being destroyed.
 */
int
js_add_teardown_callback(js_env_t *env, js_teardown_cb callback, void *data);

/**
 * Remove a teardown callback added with `js_add_teardown_callback()`,
 * identified by the same callback and data pair. Throws if the pair is not
 * registered.
 *
 * It is safe to call while the callbacks are being run, in which case it has
 * no effect.
 */
int
js_remove_teardown_callback(js_env_t *env, js_teardown_cb callback, void *data);

/**
 * Add a callback to be invoked with the `data` when the environment is
 * destroyed, deferring the destruction of the environment until the callback
 * has been finished with `js_finish_deferred_teardown_callback()`. This allows
 * outstanding asynchronous work to be wound down before the environment goes
 * away.
 *
 * The handle yielded in `result`, which may be `NULL` if it is only needed
 * from within the callback, is also passed to the callback and must eventually
 * be finished whether or not the callback has been invoked.
 *
 * Throws if the same callback and data pair is already registered, or if the
 * environment is already being destroyed.
 */
int
js_add_deferred_teardown_callback(js_env_t *env, js_deferred_teardown_cb callback, void *data, js_deferred_teardown_t **result);

/**
 * Finish a deferred teardown callback, letting the environment be destroyed
 * once every other deferred callback has also been finished. The behavior is
 * undefined if the handle is used afterwards.
 *
 * This may be called before the callback has been invoked, in which case the
 * callback is removed and will not be invoked at all.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_finish_deferred_teardown_callback(js_deferred_teardown_t *handle);

/**
 * Throw a value as an exception, making it pending on the environment. The
 * value need not be an error.
 *
 * The exception propagates up the JavaScript execution stack when control
 * returns to it, and can be handled in native code with
 * `js_get_and_clear_last_exception()`.
 */
int
js_throw(js_env_t *env, js_value_t *error);

/**
 * Throw an `Error` with the given NUL terminated message. If `code` is not
 * `NULL` it is set as the `code` property of the error.
 */
int
js_throw_error(js_env_t *env, const char *code, const char *message);

/**
 * As `js_throw_errorf()`, but taking a `va_list`.
 */
int
js_throw_verrorf(js_env_t *env, const char *code, const char *message, va_list args);

/**
 * As `js_throw_error()`, but formatting the message as `printf()` does.
 */
int
js_throw_errorf(js_env_t *env, const char *code, const char *message, ...);

/**
 * Throw a `TypeError` with the given NUL terminated message. If `code` is not
 * `NULL` it is set as the `code` property of the error.
 */
int
js_throw_type_error(js_env_t *env, const char *code, const char *message);

/**
 * As `js_throw_type_errorf()`, but taking a `va_list`.
 */
int
js_throw_type_verrorf(js_env_t *env, const char *code, const char *message, va_list args);

/**
 * As `js_throw_type_error()`, but formatting the message as `printf()` does.
 */
int
js_throw_type_errorf(js_env_t *env, const char *code, const char *message, ...);

/**
 * Throw a `RangeError` with the given NUL terminated message. If `code` is not
 * `NULL` it is set as the `code` property of the error.
 */
int
js_throw_range_error(js_env_t *env, const char *code, const char *message);

/**
 * As `js_throw_range_errorf()`, but taking a `va_list`.
 */
int
js_throw_range_verrorf(js_env_t *env, const char *code, const char *message, va_list args);

/**
 * As `js_throw_range_error()`, but formatting the message as `printf()` does.
 */
int
js_throw_range_errorf(js_env_t *env, const char *code, const char *message, ...);

/**
 * Throw a `SyntaxError` with the given NUL terminated message. If `code` is
 * not `NULL` it is set as the `code` property of the error.
 */
int
js_throw_syntax_error(js_env_t *env, const char *code, const char *message);

/**
 * As `js_throw_syntax_errorf()`, but taking a `va_list`.
 */
int
js_throw_syntax_verrorf(js_env_t *env, const char *code, const char *message, va_list args);

/**
 * As `js_throw_syntax_error()`, but formatting the message as `printf()` does.
 */
int
js_throw_syntax_errorf(js_env_t *env, const char *code, const char *message, ...);

/**
 * Throw a `ReferenceError` with the given NUL terminated message. If `code` is
 * not `NULL` it is set as the `code` property of the error.
 */
int
js_throw_reference_error(js_env_t *env, const char *code, const char *message);

/**
 * As `js_throw_reference_errorf()`, but taking a `va_list`.
 */
int
js_throw_reference_verrorf(js_env_t *env, const char *code, const char *message, va_list args);

/**
 * As `js_throw_reference_error()`, but formatting the message as `printf()`
 * does.
 */
int
js_throw_reference_errorf(js_env_t *env, const char *code, const char *message, ...);

/**
 * Check whether an exception is pending on the environment.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_is_exception_pending(js_env_t *env, bool *result);

/**
 * Take the pending exception of the environment, clearing it so that execution
 * may continue. Yields `undefined` if no exception is pending.
 *
 * If the exception cannot be handled it may be made pending again with
 * `js_throw()`.
 *
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
 * Report a change in the amount of memory that is held outside the JavaScript
 * heap but kept alive by objects within it, letting the engine take it into
 * account when deciding to collect. Yields the new total, and `result` may be
 * `NULL` if it is not needed.
 *
 * Every reported allocation must eventually be balanced by a report of its
 * release.
 *
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
 * Add callbacks to be invoked before and after each garbage collection.
 * Tracking must eventually be disabled with
 * `js_disable_garbage_collection_tracking()`.
 *
 * The callbacks are invoked while the engine is collecting and must not
 * allocate, run JavaScript, or otherwise re-enter the environment.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_enable_garbage_collection_tracking(js_env_t *env, const js_garbage_collection_tracking_options_t *options, void *data, js_garbage_collection_tracking_t **result);

/**
 * Remove the callbacks added with `js_enable_garbage_collection_tracking()`.
 * The behavior is undefined if the handle is used afterwards.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_disable_garbage_collection_tracking(js_env_t *env, js_garbage_collection_tracking_t *tracking);

/**
 * Get statistics for the JavaScript heap of the environment. The caller must
 * initialize the `version` field of the result before the call to declare
 * which fields it knows about.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_heap_statistics(js_env_t *env, js_heap_statistics_t *result);

/**
 * Get statistics for the individual spaces that make up the JavaScript heap of
 * the environment. The set of spaces and their names is engine specific.
 *
 * If `statistics` is `NULL`, `result` is set to the number of spaces and must
 * itself not be `NULL`. Otherwise, at most `len` entries are written, starting
 * with the space at `offset`, and `result`, if not `NULL`, is set to the
 * number of entries written.
 *
 * This function can be called even if there is a pending JavaScript exception.
 */
int
js_get_heap_space_statistics(js_env_t *env, js_heap_space_statistics_t statistics[], size_t len, size_t offset, size_t *result);

/**
 * Create an inspector session for the environment, allowing it to be debugged
 * over the Chrome DevTools Protocol. The session must eventually be destroyed
 * with `js_destroy_inspector()`.
 *
 * Several sessions may be created for the same environment. A session does
 * nothing until it has been connected with `js_connect_inspector()`.
 *
 * Inspector support is optional, and an engine that does not offer it throws.
 */
int
js_create_inspector(js_env_t *env, js_inspector_t **result);

/**
 * Destroy an inspector session. The behavior is undefined if it is used
 * afterwards.
 */
int
js_destroy_inspector(js_env_t *env, js_inspector_t *inspector);

/**
 * Add a callback for the messages that an inspector session sends, which are
 * either responses to requests or notifications raised on their own. The
 * message is UTF-8 encoded JSON that is only valid for the duration of the
 * call.
 */
int
js_on_inspector_response(js_env_t *env, js_inspector_t *inspector, js_inspector_message_cb cb, void *data);

/**
 * Add a callback for when execution is paused, such as at a breakpoint. The
 * callback is invoked repeatedly for as long as execution remains paused,
 * giving the embedder the opportunity to poll for and forward further
 * inspector requests, and must return `true` to remain paused or `false` to
 * give up waiting and let execution continue.
 *
 * While paused, the tasks of the environment continue to be run so that the
 * inspector remains responsive.
 */
int
js_on_inspector_paused(js_env_t *env, js_inspector_t *inspector, js_inspector_paused_cb cb, void *data);

/**
 * Connect an inspector session, after which requests may be sent to it with
 * `js_send_inspector_request()` and its messages will be delivered to the
 * callback added with `js_on_inspector_response()`.
 */
int
js_connect_inspector(js_env_t *env, js_inspector_t *inspector);

/**
 * Send a Chrome DevTools Protocol request to a connected inspector session.
 * The message is UTF-8 encoded JSON, and `len` may be `(size_t) -1` if it is
 * NUL terminated.
 */
int
js_send_inspector_request(js_env_t *env, js_inspector_t *inspector, const char *message, size_t len);

/**
 * Make a context visible to an inspector session under the given name, which
 * may be `NULL` if the context is to be anonymous.
 *
 * The context that the environment was created with is attached automatically.
 */
int
js_attach_context_to_inspector(js_env_t *env, js_inspector_t *inspector, js_context_t *context, const char *name, size_t len);

/**
 * Remove a context from an inspector session, after which it can no longer be
 * inspected.
 */
int
js_detach_context_from_inspector(js_env_t *env, js_inspector_t *inspector, js_context_t *context);

#ifdef __cplusplus
}
#endif

#endif // JS_H
