#include <assert.h>
#include <v8.h>

#include "../include/js.h"
#include "types.h"

using v8::V8;

static js_platform_t *platform = nullptr;

extern "C" int
js_platform_init (const char *path) {
  assert(platform == nullptr);

  V8::InitializeICUDefaultLocation(path);
  V8::InitializeExternalStartupData(path);

  platform = new js_platform_t(v8::platform::NewDefaultPlatform());

  V8::InitializePlatform(platform->platform.get());
  V8::Initialize();

  return 0;
}

extern "C" int
js_platform_destroy () {
  assert(platform != nullptr);

  V8::Dispose();
  V8::ShutdownPlatform();

  delete platform;

  return 0;
}
