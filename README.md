# libjs

Simple and ABI stable C bindings to V8 built on libuv. It tightly integrates with libuv, using a dedicated V8 platform implementation for scheduling platform and isolate tasks on libuv event loops.

## Alternatives

The API is designed in a way that minimises its coupling to V8, making it possible to implement ABI compatible alternatives for cases where V8 might not be ideal. We maintain two such alternatives that might be more appropriate for your use case:

- <https://github.com/holepunchto/libjsc>  
  Based on the builtin JavaScriptCore framework on Darwin. It implements a somewhat limited subset of the functionality available in the V8 version and is mostly suitable for iOS and iPadOS.

- <https://github.com/holepunchto/libqjs>  
  Based on QuickJS. It implements all of the functionality also available in the V8 version, but in a fraction of the size. The performance and memory footprint is comparable to [V8 Lite](https://v8.dev/blog/v8-lite).

## API

See [`include/js.h`](include/js.h) for the public API.

### Exceptions

JavaScript exceptions are used for all error handling. A negative return value therefore indicates that an exception is pending. Furthermore, certain functions will immediately return an error value if an exception is already pending when the function was called. To handle a pending exception in native code, rather than defer it to JavaScript, use the `js_get_and_clear_last_exception()` function. If the exception cannot be handled, it may be rethrown with `js_throw()`.

## License

ISC
