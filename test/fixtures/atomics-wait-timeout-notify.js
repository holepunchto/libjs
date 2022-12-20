const buffer = new SharedArrayBuffer(4)
const view = new Int32Array(buffer)

const value = Atomics
  .waitAsync(view, 0, 0, 10000 /* Noticeable timeout */)
  .value

Atomics.notify(view, 0, 1);

value
