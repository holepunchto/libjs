%.h: %
	xxd -i $^ > $@

all: \
	atomics-wait-timeout.js.h \
	atomics-wait-timeout-notify.js.h \
	many-large-allocs.js.h \
	many-small-allocs.js.h \
	promise-rejection.js.h \
	promise-rejection-unhandled.js.h \
	wasm-async.js.h \
	wasm-async-log.js.h \
	wasm-sync.js.h
