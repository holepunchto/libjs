#include <assert.h>
#include <libplatform/libplatform.h>
#include <v8.h>

#include "../include/js.h"
#include "platform.h"

using v8::V8;

extern "C" int
js_platform_init (const char *path) {
  assert(js::platform == nullptr);

  V8::InitializeICUDefaultLocation(path);
  V8::InitializeExternalStartupData(path);

  js::platform = v8::platform::NewDefaultPlatform().release();

  V8::InitializePlatform(js::platform);
  V8::Initialize();

  return 0;
}

extern "C" int
js_platform_destroy () {
  assert(js::platform != nullptr);

  V8::Dispose();
  V8::ShutdownPlatform();

  delete js::platform;

  return 0;
}
