# ABI stability

The interface declared in [`include/js.h`](../include/js.h) is ABI stable. A binary compiled against one release keeps working when linked against a later one, and against any of the alternative engines. This document describes the rules that make that possible.

## Opaque handles

Environments, scripts, modules, values, references, and the like are opaque pointers. Callers never see their layout, so an engine is free to change what sits behind them. Changes to internals belong behind these pointers rather than in the header.

## Functions

Once added, a function keeps its name, parameters, and return type. New behavior is added through new functions or through versioned structs rather than by changing an existing signature. The same holds for callback types.

## Enums

Enum values are never renumbered or removed. New values are appended, so callers must tolerate values they do not recognize.

## Struct versioning

Structs that cross the interface by value carry a `version` field as their first member and are documented with a `@version` tag. Each field is documented with the `@since` version in which it was added.

```c
/**
 * @version 1
 */
struct js_heap_statistics_s {
  int version;

  /**
   * @since 0
   */
  size_t total_heap_size;

  /**
   * @since 1
   */
  size_t external_memory;
};
```

The caller always sets `version`, whether the struct is passed in or filled in, to declare which fields it knows about. The implementation never reads or writes fields beyond that version, and falls back to defaults for input fields the caller did not provide.

To extend a struct:

1. Append the new fields at the end. Never reorder, resize, or remove existing fields.
2. Bump the `@version` of the struct and mark the new fields with `@since` the new version.
3. Guard every access to the new fields on the `version` set by the caller.

## Transitioning

Some changes cannot be made additively, such as changing the signature of a function or callback. These are made in three steps using a `_transitional` symbol, each step being its own release.

1. **Introduce.** Add `js_foo_transitional()` with the new signature next to the existing `js_foo()`. Callers move to `js_foo_transitional()`.
2. **Transition.** Change `js_foo()` to the new signature and keep `js_foo_transitional()` as an alias with the same signature. Callers move back to `js_foo()`.
3. **Remove.** Delete `js_foo_transitional()`.

Callers that follow along never see a symbol disappear from under them. Callers still using the old `js_foo()` after step 2, or `js_foo_transitional()` after step 3, are broken, so leave enough time between the steps for dependents to update.

The alternative engines implement the same interface, so each step must land in all of them before moving on to the next.
