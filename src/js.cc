#include <assert.h>
#include <v8.h>

#include "../include/js.h"
#include "platform/platform.h"

using v8::V8;

static js::Platform *platform = nullptr;

extern "C" int
js_init (const char *path) {
  assert(platform == nullptr);

  V8::InitializeICUDefaultLocation(path);
  V8::InitializeExternalStartupData(path);

  platform = new js::Platform();

  V8::InitializePlatform(platform);
  V8::Initialize();

  return 0;
}

extern "C" int
js_destroy () {
  assert(platform != nullptr);

  V8::Dispose();
  V8::ShutdownPlatform();

  delete platform;

  return 0;
}
