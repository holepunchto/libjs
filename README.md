# libjs

Simple and ABI stable C bindings to V8 built on libuv. It tightly integrates with libuv, using a dedicated V8 platform implementation for scheduling platform and isolate tasks on libuv event loops.

## API

See [`include/js.h`](include/js.h) for the public API.

### Exceptions

JavaScript exceptions are used for all error handling. A negative return value therefore indicates that an exception is pending. Furthermore, certain functions will immediately return an error value if an exception is already pending when the function was called. To handle a pending exception in native code, rather than defer it to JavaScript, use the `js_get_and_clear_last_exception()` function. If the exception cannot be handled, it may be rethrown with `js_throw()`.

## Building

By default, the library is compiled with static V8 prebuilds generated by <https://github.com/holepunchto/chromium-prebuilds>. You can bring along your own build of V8 by defining the `v8` target in your build definition. If you rely on the default V8 prebuilds, make sure to first checkout <https://github.com/holepunchto/chromium-prebuilds> and follow the instructions on how to make a prebuild.

## Alternatives

The API is designed in a way that minimises its coupling to V8, making it possible to implement ABI compatible alternatives for cases where V8 might not be ideal. We maintain two such alternatives that might be more appropriate for your use case:

- <https://github.com/holepunchto/libjsc>  
  Based on the builtin JavaScriptCore framework on Darwin. It implements most of the functionality available in the V8 version and is mostly suitable for iOS and iPadOS.

- <https://github.com/holepunchto/libqjs>  
  Based on QuickJS. It implements all of the functionality also available in the V8 version, but in a fraction of the size. The performance and memory footprint is comparable to [V8 Lite](https://v8.dev/blog/v8-lite).

## License

Apache-2.0
