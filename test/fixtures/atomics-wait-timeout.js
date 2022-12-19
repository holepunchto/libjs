const buffer = new SharedArrayBuffer(4)
const view = new Int32Array(buffer)

Atomics
  .waitAsync(view, 0, 0, 100)
  .value
