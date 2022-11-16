# libjs

A library for embedding the V8 JavaScript engine using a C API closely resembling Node-API. It tightly integrates with libuv, using a dedicated V8 platform implementation for scheduling platform and isolate tasks on libuv event loops.

## API

See [`includes/js.h`](include/js.h) for the public API.

## License

ISC
