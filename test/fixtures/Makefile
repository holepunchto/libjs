%.h: %
	xxd -i $^ > $@
	clang-format -i $@

all: \
	atomics-wait-timeout-notify.js.h \
	atomics-wait-timeout.js.h \
	many-large-allocs.js.h \
	many-small-allocs.js.h \
	number-object.js.h \
	promise-rejection-unhandled-deferred.js.h \
	promise-rejection-unhandled.js.h \
	promise-rejection.js.h \
	wasm-async-log.js.h \
	wasm-async.js.h \
	wasm-sync.js.h
