#include <v8.h>

namespace js {

class TaskRunner : public v8::TaskRunner {
public:
  void
  PostTask (std::unique_ptr<v8::Task> task) override;

  void
  PostDelayedTask (std::unique_ptr<v8::Task> task, double delay_in_seconds) override;

  void
  PostIdleTask (std::unique_ptr<v8::IdleTask> task) override;

  bool
  IdleTasksEnabled () override;

  bool
  NonNestableTasksEnabled () const override;
};

} // namespace js
