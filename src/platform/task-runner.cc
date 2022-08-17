#include <v8.h>

#include "task-runner.h"

namespace js {

void
TaskRunner::PostTask(std::unique_ptr<v8::Task> task) {}

void
TaskRunner::PostDelayedTask(std::unique_ptr<v8::Task> task, double delay_in_seconds) {}

void
TaskRunner::PostIdleTask(std::unique_ptr<v8::IdleTask> task) {}

bool
TaskRunner::IdleTasksEnabled() {
  return true;
}

bool
TaskRunner::NonNestableTasksEnabled() const {
  return true;
}

} // namespace js
