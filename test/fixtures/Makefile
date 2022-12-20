%.h: %
	xxd -i $^ > $@

all: \
	atomics-wait-timeout.js.h \
	atomics-wait-timeout-notify.js.h \
	wasm-async.js.h \
	wasm-sync.js.h
