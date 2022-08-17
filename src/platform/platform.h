#include <v8.h>

namespace js {

class Platform : public v8::Platform {
public:
  Platform();

  int
  NumberOfWorkerThreads () override;

  std::shared_ptr<v8::TaskRunner>
  GetForegroundTaskRunner (v8::Isolate *) override;

  void
  CallOnWorkerThread (std::unique_ptr<v8::Task> task) override;

  void
  CallDelayedOnWorkerThread (std::unique_ptr<v8::Task> task, double delay_in_seconds) override;

  std::unique_ptr<v8::JobHandle>
  PostJob (v8::TaskPriority priority, std::unique_ptr<v8::JobTask> job_task) override;

  double
  MonotonicallyIncreasingTime () override;

  double
  CurrentClockTimeMillis () override;

  v8::TracingController *
  GetTracingController () override;

private:
  std::shared_ptr<v8::TaskRunner> _taskRunner;
  std::unique_ptr<v8::TracingController> _tracingController;
};

} // namespace js
