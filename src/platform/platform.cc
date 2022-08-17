#include <v8.h>

#include "platform.h"
#include "task-runner.h"
#include "tracing-controller.h"

namespace js {

Platform::Platform() {
  _taskRunner = std::make_shared<TaskRunner>();
  _tracingController = std::make_unique<TracingController>();
}

int
Platform::NumberOfWorkerThreads() {
  return 0;
}

std::shared_ptr<v8::TaskRunner>
Platform::GetForegroundTaskRunner(v8::Isolate *) {
  return _taskRunner;
}

void
Platform::CallOnWorkerThread(std::unique_ptr<v8::Task> task) {}

void
Platform::CallDelayedOnWorkerThread(std::unique_ptr<v8::Task> task, double delay_in_seconds) {}

std::unique_ptr<v8::JobHandle>
Platform::PostJob(v8::TaskPriority priority, std::unique_ptr<v8::JobTask> job_task) {
  return nullptr;
}

double
Platform::MonotonicallyIncreasingTime() {
  return 0;
}

double
Platform::CurrentClockTimeMillis() {
  return 0;
}

v8::TracingController *
Platform::GetTracingController() {
  return _tracingController.get();
}

} // namespace js
