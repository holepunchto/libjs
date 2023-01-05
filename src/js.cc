#include <atomic>
#include <bit>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <uv.h>

#include <v8-fast-api-calls.h>
#include <v8.h>

#include "../include/js.h"
#include "../include/js/ffi.h"

using namespace v8;

typedef struct js_env_scope_s js_env_scope_t;
typedef struct js_callback_s js_callback_t;
typedef struct js_finalizer_s js_finalizer_t;
typedef struct js_tracing_controller_s js_tracing_controller_t;
typedef struct js_task_s js_task_t;
typedef struct js_task_handle_s js_task_handle_t;
typedef struct js_delayed_task_handle_s js_delayed_task_handle_t;
typedef struct js_idle_task_handle_s js_idle_task_handle_t;
typedef struct js_task_runner_s js_task_runner_t;
typedef struct js_task_scope_s js_task_scope_t;
typedef struct js_job_state_s js_job_state_t;
typedef struct js_job_delegate_s js_job_delegate_t;
typedef struct js_job_handle_s js_job_handle_t;
typedef struct js_worker_s js_worker_t;

typedef enum {
  js_context_environment = 1,
} js_context_index_t;

typedef enum {
  js_task_nestable,
  js_task_non_nestable,
} js_task_nestability_t;

template <typename T>
static inline Local<T>
to_local (Persistent<T> &persistent) {
  return *reinterpret_cast<Local<T> *>(&persistent);
}

template <typename T = Value>
static inline Local<T>
to_local (js_value_t *value) {
  return *reinterpret_cast<Local<T> *>(&value);
}

template <typename T>
static inline js_value_t *
from_local (Local<T> local) {
  return reinterpret_cast<js_value_t *>(*local);
}

struct js_tracing_controller_s : public TracingController {
private: // V8 embedder API
};

struct js_task_s : public Task {
  js_env_t *env;
  js_task_cb cb;
  void *data;

  js_task_s(js_env_t *env, js_task_cb cb, void *data)
      : env(env),
        cb(cb),
        data(data) {}

  inline void
  run () {
    cb(env, data);
  }

private: // V8 embedder API
  void
  Run () override {
    run();
  }
};

using js_task_completion_cb = std::function<void()>;

struct js_task_handle_s {
  std::unique_ptr<Task> task;
  js_task_nestability_t nestability;
  js_task_completion_cb on_completion;

  js_task_handle_s(std::unique_ptr<Task> task, js_task_nestability_t nestability)
      : task(std::move(task)),
        nestability(nestability),
        on_completion(nullptr) {}

  inline void
  run () {
    task->Run();

    if (on_completion) on_completion();
  }
};

struct js_delayed_task_handle_s : js_task_handle_t {
  uint64_t expiry;

  js_delayed_task_handle_s(std::unique_ptr<Task> task, js_task_nestability_t nestability, uint64_t expiry)
      : js_task_handle_t(std::move(task), nestability),
        expiry(expiry) {}

  friend bool
  operator<(const js_delayed_task_handle_t &a, const js_delayed_task_handle_t &b) {
    return a.expiry > b.expiry;
  }
};

struct js_idle_task_handle_s {
  std::unique_ptr<IdleTask> task;
  js_task_completion_cb on_completion;

  js_idle_task_handle_s(std::unique_ptr<IdleTask> task)
      : task(std::move(task)) {}

  void
  run (double deadline) {
    task->Run(deadline);

    if (on_completion) on_completion();
  }
};

struct js_task_runner_s : public TaskRunner {
  uv_loop_t *loop;
  uv_timer_t timer;
  std::deque<js_task_handle_t> tasks;
  std::priority_queue<js_delayed_task_handle_t> delayed_tasks;
  std::queue<js_idle_task_handle_t> idle_tasks;
  std::recursive_mutex lock;
  uint32_t depth;
  uint32_t outstanding;
  uint32_t disposable;
  std::condition_variable_any available;
  std::condition_variable_any drained;

  js_task_runner_s(uv_loop_t *loop)
      : loop(loop),
        timer(),
        tasks(),
        delayed_tasks(),
        idle_tasks(),
        lock(),
        depth(0),
        outstanding(0),
        disposable(0),
        available(),
        drained() {
    uv_timer_init(loop, &timer);
    timer.data = this;
  }

  ~js_task_runner_s() {
    terminate();
  }

  inline uint64_t
  now () {
    return uv_now(loop);
  }

  inline bool
  empty () {
    std::scoped_lock guard(lock);

    return tasks.empty() && delayed_tasks.empty() && idle_tasks.empty();
  }

  inline size_t
  size () {
    std::scoped_lock guard(lock);

    return tasks.size() + delayed_tasks.size() + idle_tasks.size();
  }

  inline void
  push_task (js_task_handle_t &&task) {
    std::scoped_lock guard(lock);

    outstanding++;

    task.on_completion = [this] { on_completion(); };

    tasks.push_back(std::move(task));

    available.notify_one();
  }

  inline void
  push_task (js_delayed_task_handle_t &&task) {
    std::scoped_lock guard(lock);

    outstanding++;

    // Nestable delayed tasks are not allowed to execute JavaScript and should
    // therefore be safe to dispose if all other tasks have finished.
    bool is_disposable = task.nestability == js_task_nestable;

    if (is_disposable) disposable++;

    task.on_completion = [this, is_disposable] { on_completion(is_disposable); };

    delayed_tasks.push(std::move(task));
  }

  inline void
  push_task (js_idle_task_handle_t &&task) {
    std::scoped_lock guard(lock);

    outstanding++;

    task.on_completion = [this] { on_completion(); };

    idle_tasks.push(std::move(task));
  }

  inline std::optional<js_task_handle_t>
  pop_task () {
    std::scoped_lock guard(lock);

    auto task = tasks.begin();

    while (task != tasks.end()) {
      if (depth == 0 || task->nestability == js_task_nestable) break;
      else task++;
    }

    if (task == tasks.end()) {
      // TODO: Check if we're idling and return an idle task if available.

      return std::nullopt;
    }

    auto value = std::move(const_cast<js_task_handle_t &>(*task));

    tasks.erase(task);

    return value;
  }

  inline std::optional<js_task_handle_t>
  pop_task_wait () {
    std::unique_lock guard(lock);

    auto task = pop_task();

    if (task) return task;

    available.wait(guard);

    return pop_task();
  }

  void
  move_expired_tasks () {
    std::scoped_lock guard(lock);

    while (!delayed_tasks.empty()) {
      js_delayed_task_handle_t const &task = delayed_tasks.top();

      if (task.expiry > now()) break;

      tasks.push_back(std::move(const_cast<js_delayed_task_handle_t &>(task)));

      delayed_tasks.pop();

      available.notify_one();
    }

    adjust_timer();
  }

  inline void
  drain () {
    std::unique_lock guard(lock);

    while (outstanding > 0) {
      drained.wait(guard);
    }
  }

  inline void
  terminate () {
    std::scoped_lock guard(lock);

    // TODO: Clear and cancel outstanding tasks and notify threads waiting for
    // the outstanding tasks to drain.

    available.notify_all();
  }

private:
  inline void
  on_completion (bool is_disposable = false) {
    std::scoped_lock guard(lock);

    if (is_disposable) disposable--;

    if (--outstanding == 0) {
      drained.notify_all();
    }
  }

  static void
  on_timer (uv_timer_t *handle) {
    auto tasks = reinterpret_cast<js_task_runner_t *>(handle->data);

    tasks->move_expired_tasks();
  }

  void
  adjust_timer () {
    std::scoped_lock guard(lock);

    if (delayed_tasks.empty()) {
      uv_timer_stop(&timer);
    } else {
      js_delayed_task_handle_t const &task = delayed_tasks.top();

      uint64_t timeout = task.expiry - now();

      uv_timer_start(&timer, on_timer, timeout, 0);

      // Don't let the timer keep the loop alive if all outstanding tasks are
      // disposable.
      if (outstanding == disposable) {
        uv_unref(reinterpret_cast<uv_handle_t *>(&timer));
      }
    }
  }

private: // V8 embedder API
  void
  PostTask (std::unique_ptr<Task> task) override {
    push_task(js_task_handle_t(std::move(task), js_task_nestable));
  }

  void
  PostNonNestableTask (std::unique_ptr<Task> task) override {
    push_task(js_task_handle_t(std::move(task), js_task_non_nestable));
  }

  void
  PostDelayedTask (std::unique_ptr<Task> task, double delay) override {
    push_task(js_delayed_task_handle_t(std::move(task), js_task_nestable, now() + (delay * 1000)));
  }

  void
  PostNonNestableDelayedTask (std::unique_ptr<Task> task, double delay) override {
    push_task(js_delayed_task_handle_t(std::move(task), js_task_non_nestable, now() + (delay * 1000)));
  }

  void
  PostIdleTask (std::unique_ptr<IdleTask> task) override {
    push_task(js_idle_task_handle_t(std::move(task)));
  }

  bool
  IdleTasksEnabled () override {
    return true;
  }

  bool
  NonNestableTasksEnabled () const override {
    return true;
  }

  bool
  NonNestableDelayedTasksEnabled () const override {
    return true;
  }
};

struct js_task_scope_s {
  std::shared_ptr<js_task_runner_t> tasks;

  js_task_scope_s(std::shared_ptr<js_task_runner_t> tasks)
      : tasks(tasks) {
    tasks->depth++;
  }

  js_task_scope_s(const js_task_scope_s &) = delete;

  ~js_task_scope_s() {
    tasks->depth--;
  }

  js_task_scope_s &
  operator=(const js_task_scope_s &) = delete;
};

static const auto js_invalid_task_id = uint8_t(-1);

struct js_job_state_s {
  uint8_t available_parallelism;
  std::atomic<uint64_t> task_ids;

  js_job_state_s(uint8_t available_parallelism)
      : available_parallelism(available_parallelism < 64 ? available_parallelism : 64),
        task_ids(0) {}

  inline uint8_t
  acquire_task_id () {
    uint64_t task_ids = this->task_ids.load(std::memory_order_relaxed);

    uint8_t task_id;
    bool ok;

    do {
      task_id = std::countr_one(task_ids);

      ok = this->task_ids.compare_exchange_weak(
        task_ids,
        task_ids | (uint64_t(1) << task_id),
        std::memory_order_acquire,
        std::memory_order_relaxed
      );
    } while (!ok);

    return task_id;
  }

  inline void
  release_task_id (uint8_t task_id) {
    task_ids.fetch_and(~(uint64_t(1) << task_id), std::memory_order_release);
  }
};

struct js_job_delegate_s : public JobDelegate {
  std::shared_ptr<js_job_state_t> state;
  js_job_handle_t *handle;
  uint8_t task_id;
  bool is_joining_thread;

  js_job_delegate_s(std::shared_ptr<js_job_state_t> state, js_job_handle_t *handle, bool is_joining_thread)
      : state(state),
        handle(handle),
        task_id(js_invalid_task_id),
        is_joining_thread(is_joining_thread) {}

  ~js_job_delegate_s() {
    if (task_id != js_invalid_task_id) state->release_task_id(task_id);
  }

private: // V8 embedder API
  bool
  ShouldYield () override {
    return false;
  }

  void
  NotifyConcurrencyIncrease () override {}

  uint8_t
  GetTaskId () override {
    if (task_id == js_invalid_task_id) task_id = state->acquire_task_id();
    return task_id;
  }

  bool
  IsJoiningThread () const override {
    return is_joining_thread;
  }
};

struct js_job_handle_s : public JobHandle {
  TaskPriority priority;
  std::unique_ptr<JobTask> task;
  std::shared_ptr<js_job_state_t> state;
  std::atomic<bool> done;

  js_job_handle_s(TaskPriority priority, std::unique_ptr<JobTask> task, std::shared_ptr<js_job_state_t> state)
      : priority(priority),
        task(std::move(task)),
        state(state),
        done(false) {}

  void
  join () {
    auto delegate = js_job_delegate_t(state, this, true);

    while (task->GetMaxConcurrency(0) > 0) {
      task->Run(&delegate);
    }

    done = true;
  }

  void
  cancel () {
    done = true;
  }

private: // V8 embedder API
  void
  NotifyConcurrencyIncrease () override {
    join();
  }

  void
  Join () override {
    join();
  }

  void
  Cancel () override {
    cancel();
  }

  void
  CancelAndDetach () override {
    cancel();
  }

  bool
  IsActive () override {
    return !done;
  }

  bool
  IsValid () override {
    return !done;
  }
};

struct js_worker_s {
  std::shared_ptr<js_task_runner_t> tasks;
  std::thread thread;

  js_worker_s(std::shared_ptr<js_task_runner_t> tasks)
      : tasks(tasks),
        thread(&js_worker_t::on_thread, this) {}

  ~js_worker_s() {
    if (thread.joinable()) join();
  }

  inline void
  join () {
    thread.join();
  }

private:
  void
  on_thread () {
    while (auto task = tasks->pop_task_wait()) {
      task->run();
    }
  }
};

struct js_platform_s : public Platform {
  js_platform_options_t options;
  uv_loop_t *loop;
  uv_prepare_t prepare;
  uv_check_t check;
  std::map<Isolate *, std::shared_ptr<js_task_runner_t>> foreground;
  std::shared_ptr<js_task_runner_t> background;
  std::shared_ptr<js_job_state_t> state;
  std::vector<std::shared_ptr<js_worker_t>> workers;
  std::unique_ptr<js_tracing_controller_t> trace;

  js_platform_s(js_platform_options_t options, uv_loop_t *loop)
      : options(options),
        loop(loop),
        prepare(),
        check(),
        foreground(),
        background(new js_task_runner_t(loop)),
        state(new js_job_state_s(uv_available_parallelism() - 1 /* main thread */)),
        workers(),
        trace(new js_tracing_controller_t()) {
    uv_prepare_init(loop, &prepare);
    uv_prepare_start(&prepare, on_prepare);
    prepare.data = this;

    uv_check_init(loop, &check);
    uv_check_start(&check, on_check);
    check.data = this;

    // The check handle should not on its own keep the loop alive; it's simply
    // used for running any outstanding tasks that might cause additional work
    // to be queued.
    uv_unref(reinterpret_cast<uv_handle_t *>(&check));

    start_workers();
  }

  ~js_platform_s() {
    background->terminate();
  }

  inline uint64_t
  now () {
    return uv_now(loop);
  }

  inline void
  idle () {
    // TODO: This should wait until either the platform drains completely or a
    // task is made available.
    drain();
  }

  inline void
  drain () {
    background->drain();
  }

private:
  inline void
  run_macrotasks () {
    background->move_expired_tasks();

    while (auto task = background->pop_task()) {
      task->run();
    }
  }

  inline void
  check_liveness () {
    if (background->empty()) {
      uv_prepare_stop(&prepare);
    } else {
      uv_prepare_start(&prepare, on_prepare);
    }
  }

  static void
  on_prepare (uv_prepare_t *handle) {
    auto platform = reinterpret_cast<js_platform_t *>(handle->data);

    platform->run_macrotasks();

    platform->check_liveness();
  }

  static void
  on_check (uv_check_t *handle) {
    auto platform = reinterpret_cast<js_platform_t *>(handle->data);

    if (uv_loop_alive(platform->loop)) return;

    platform->idle();

    platform->check_liveness();
  }

  inline void
  start_workers () {
    workers.reserve(state->available_parallelism);

    while (workers.size() < workers.capacity()) {
      workers.emplace_back(new js_worker_t(background));
    }
  }

private: // V8 embedder API
  PageAllocator *
  GetPageAllocator () override {
    return nullptr;
  }

  int
  NumberOfWorkerThreads () override {
    return workers.size();
  }

  std::shared_ptr<TaskRunner>
  GetForegroundTaskRunner (Isolate *isolate) override {
    return foreground[isolate];
  }

  void
  CallOnWorkerThread (std::unique_ptr<Task> task) override {
    background->push_task(js_task_handle_t(std::move(task), js_task_nestable));
  }

  void
  CallDelayedOnWorkerThread (std::unique_ptr<Task> task, double delay) override {
    background->push_task(js_delayed_task_handle_t(std::move(task), js_task_nestable, now() + (delay * 1000)));
  }

  std::unique_ptr<JobHandle>
  CreateJob (TaskPriority priority, std::unique_ptr<JobTask> task) override {
    return std::make_unique<js_job_handle_t>(priority, std::move(task), state);
  }

  double
  MonotonicallyIncreasingTime () override {
    return now();
  }

  double
  CurrentClockTimeMillis () override {
    return SystemClockTimeMillis();
  }

  TracingController *
  GetTracingController () override {
    return trace.get();
  }
};

struct js_env_s {
  uv_loop_t *loop;
  uv_prepare_t prepare;
  uv_check_t check;
  js_platform_t *platform;
  std::shared_ptr<js_task_runner_t> tasks;
  Isolate *isolate;
  ArrayBuffer::Allocator *allocator;
  HandleScope scope;
  Persistent<Context> context;
  Persistent<Value> exception;
  std::multimap<size_t, js_module_t *> modules;
  std::vector<Global<Promise>> unhandled_promises;
  js_uncaught_exception_cb on_uncaught_exception;
  void *uncaught_exception_data;
  js_unhandled_rejection_cb on_unhandled_rejection;
  void *unhandled_rejection_data;

  js_env_s(uv_loop_t *loop, js_platform_t *platform, Isolate *isolate, ArrayBuffer::Allocator *allocator)
      : loop(loop),
        prepare(),
        check(),
        platform(platform),
        tasks(platform->foreground[isolate]),
        isolate(isolate),
        allocator(allocator),
        scope(isolate),
        context(isolate, Context::New(isolate)),
        modules(),
        unhandled_promises(),
        on_uncaught_exception(nullptr),
        uncaught_exception_data(nullptr),
        on_unhandled_rejection(nullptr),
        unhandled_rejection_data(nullptr) {
    uv_prepare_init(loop, &prepare);
    uv_prepare_start(&prepare, on_prepare);
    prepare.data = this;

    uv_check_init(loop, &check);
    uv_check_start(&check, on_check);
    check.data = this;

    // The check handle should not on its own keep the loop alive; it's simply
    // used for running any outstanding tasks that might cause additional work
    // to be queued.
    uv_unref(reinterpret_cast<uv_handle_t *>(&check));

    to_local(this->context)->Enter();
  }

  inline uint64_t
  now () {
    return uv_now(loop);
  }

  inline void
  enter () {
    isolate->Enter();
  }

  inline void
  exit () {
    isolate->Exit();
  }

  inline void
  idle () {
    // TODO: This should wait until either the platform drains completely or a
    // task is made available for the isolate.
    platform->drain();
  }

  inline void
  run_microtasks () {
    auto context = to_local(this->context);

    isolate->PerformMicrotaskCheckpoint();

    for (auto &promise : unhandled_promises) {
      if (on_unhandled_rejection) {
        on_unhandled_rejection(this, from_local(promise.Get(isolate)), unhandled_rejection_data);
      }
    }

    unhandled_promises.clear();
  }

  inline void
  run_macrotasks () {
    tasks->move_expired_tasks();

    while (auto task = tasks->pop_task()) {
      auto scope = js_task_scope_t(tasks);

      task->run();

      run_microtasks();
    }
  }

  inline void
  set_exception (Local<Value> exception) {
    this->exception.Reset(isolate, exception);
  }

private:
  inline void
  check_liveness () {
    tasks->move_expired_tasks();

    if (tasks->empty() || tasks->outstanding == tasks->disposable) {
      uv_prepare_stop(&prepare);
    } else {
      uv_prepare_start(&prepare, on_prepare);
    }
  }

  static void
  on_prepare (uv_prepare_t *handle) {
    auto env = reinterpret_cast<js_env_t *>(handle->data);

    env->run_macrotasks();

    env->check_liveness();
  }

  static void
  on_check (uv_check_t *handle) {
    auto env = reinterpret_cast<js_env_t *>(handle->data);

    env->run_microtasks();

    if (uv_loop_alive(env->loop)) return;

    env->idle();

    env->check_liveness();
  }
};

struct js_env_scope_s {
  js_env_t *env;

  js_env_scope_s(js_env_t *env)
      : env(env) {
    env->enter();
  }

  js_env_scope_s(const js_env_scope_s &) = delete;

  ~js_env_scope_s() {
    env->exit();
  }

  js_env_scope_s &
  operator=(const js_env_scope_s &) = delete;
};

struct js_handle_scope_s {
  HandleScope scope;

  js_handle_scope_s(Isolate *isolate)
      : scope(isolate) {}
};

struct js_escapable_handle_scope_s {
  EscapableHandleScope scope;
  bool escaped;

  js_escapable_handle_scope_s(Isolate *isolate)
      : scope(isolate),
        escaped(false) {}
};

struct js_module_s {
  Local<Module> module;
  js_module_cb resolve;
  js_synthetic_module_cb evaluate;
  void *data;

  js_module_s(Local<Module> module, void *data)
      : module(module),
        resolve(nullptr),
        evaluate(nullptr),
        data(data) {}
};

struct js_ref_s {
  Persistent<Value> value;
  uint32_t count;

  js_ref_s(Isolate *isolate, Local<Value> value, uint32_t count)
      : value(isolate, value),
        count(count) {}
};

struct js_deferred_s {
  Persistent<Promise::Resolver> resolver;

  js_deferred_s(Isolate *isolate, Local<Promise::Resolver> resolver)
      : resolver(isolate, resolver) {}
};

struct js_callback_s {
  js_env_t *env;
  js_function_cb cb;
  void *data;

  js_callback_s(js_env_t *env, js_function_cb cb, void *data)
      : env(env),
        cb(cb),
        data(data) {}
};

struct js_finalizer_s {
  Persistent<Value> value;
  js_env_t *env;
  void *data;
  js_finalize_cb cb;
  void *hint;

  js_finalizer_s(js_env_t *env, Local<Value> value, void *data, js_finalize_cb cb, void *hint)
      : value(env->isolate, value),
        data(data),
        cb(cb),
        hint(hint) {}
};

struct js_ffi_type_info_s {
  CTypeInfo type_info;

  js_ffi_type_info_s(CTypeInfo::Type type, CTypeInfo::SequenceType sequence_type, CTypeInfo::Flags flags)
      : type_info(type, sequence_type, flags) {}
};

struct js_ffi_function_info_s {
  CTypeInfo return_info;
  std::vector<CTypeInfo> arg_info;
  CFunctionInfo function_info;

  js_ffi_function_info_s(CTypeInfo return_info, std::vector<CTypeInfo> &&arg_info)
      : return_info(return_info),
        arg_info(std::move(arg_info)),
        function_info(this->return_info, this->arg_info.size(), this->arg_info.data()) {}
};

struct js_ffi_function_s {
  CFunction function;

  js_ffi_function_s(const void *fn, const CFunctionInfo *function_info)
      : function(fn, function_info) {}
};

static inline js_env_t *
get_env (Local<Context> context) {
  return reinterpret_cast<js_env_t *>(context->GetAlignedPointerFromEmbedderData(js_context_environment));
}

static inline js_module_t *
get_module (Local<Context> context, Local<Module> referrer) {
  auto env = get_env(context);

  auto range = env->modules.equal_range(referrer->GetIdentityHash());

  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->module == referrer) {
      return it->second;
    }
  }

  return nullptr;
}

extern "C" int
js_create_platform (uv_loop_t *loop, const js_platform_options_t *options, js_platform_t **result) {
  if (options) {
    auto flags = std::string();

    if (options->expose_garbage_collection) {
      flags += " --expose-gc";
    }

    if (options->disable_optimizing_compiler) {
      flags += " --jitless --noexpose-wasm";
    }

    V8::SetFlagsFromString(flags.c_str());
  }

  *result = new js_platform_t(options ? *options : js_platform_options_t(), loop);

  V8::InitializePlatform(*result);
  V8::Initialize();

  return 0;
}

extern "C" int
js_destroy_platform (js_platform_t *platform) {
  V8::Dispose();
  V8::DisposePlatform();

  delete platform;

  return 0;
}

extern "C" int
js_get_platform_loop (js_platform_t *platform, uv_loop_t **result) {
  *result = platform->loop;

  return 0;
}

static void
on_uncaught_exception (Local<Message> message, Local<Value> error) {
  auto isolate = message->GetIsolate();

  auto context = isolate->GetCurrentContext();

  auto env = get_env(context);

  if (env->on_uncaught_exception) {
    env->on_uncaught_exception(env, from_local(error), env->uncaught_exception_data);
  }
}

static void
on_promise_reject (PromiseRejectMessage message) {
  auto promise = message.GetPromise();

  auto isolate = promise->GetIsolate();
  auto context = isolate->GetCurrentContext();

  auto env = get_env(context);

  switch (message.GetEvent()) {
  case kPromiseRejectAfterResolved:
  case kPromiseResolveAfterResolved:
    return;

  case kPromiseRejectWithNoHandler:
    env->unhandled_promises.push_back(Global<Promise>(isolate, promise));
    break;

  case kPromiseHandlerAddedAfterReject:
    for (auto it = env->unhandled_promises.begin(); it != env->unhandled_promises.end(); it++) {
      auto unhandled_promise = it->Get(isolate);

      if (unhandled_promise == promise) {
        *it = std::move(env->unhandled_promises.back());

        env->unhandled_promises.pop_back();

        break;
      }
    }
  }
}

extern "C" int
js_create_env (uv_loop_t *loop, js_platform_t *platform, js_env_t **result) {
  auto allocator = ArrayBuffer::Allocator::NewDefaultAllocator();

  Isolate::CreateParams params;
  params.array_buffer_allocator = allocator;
  params.allow_atomics_wait = false;

  auto isolate = Isolate::Allocate();

  auto tasks = new js_task_runner_t(loop);

  platform->foreground.emplace(isolate, std::move(tasks));

  Isolate::Initialize(isolate, params);

  isolate->SetMicrotasksPolicy(MicrotasksPolicy::kExplicit);

  isolate->AddMessageListener(on_uncaught_exception);

  isolate->SetPromiseRejectCallback(on_promise_reject);

  auto env = new js_env_s(loop, platform, isolate, allocator);

  env->enter();

  auto context = to_local(env->context);

  context->SetAlignedPointerInEmbedderData(js_context_environment, env);

  *result = env;

  return 0;
}

extern "C" int
js_destroy_env (js_env_t *env) {
  auto isolate = env->isolate;
  auto allocator = env->allocator;

  env->exit();

  env->platform->foreground.erase(isolate);

  delete env;

  isolate->Dispose();

  delete allocator;

  return 0;
}

extern "C" int
js_on_uncaught_exception (js_env_t *env, js_uncaught_exception_cb cb, void *data) {
  env->on_uncaught_exception = cb;
  env->uncaught_exception_data = data;

  return 0;
}

extern "C" int
js_on_unhandled_rejection (js_env_t *env, js_unhandled_rejection_cb cb, void *data) {
  env->on_unhandled_rejection = cb;
  env->unhandled_rejection_data = data;

  return 0;
}

extern "C" int
js_get_env_loop (js_env_t *env, uv_loop_t **result) {
  *result = env->loop;

  return 0;
}

extern "C" int
js_open_handle_scope (js_env_t *env, js_handle_scope_t **result) {
  *result = new js_handle_scope_s(env->isolate);

  return 0;
}

extern "C" int
js_close_handle_scope (js_env_t *env, js_handle_scope_t *scope) {
  delete scope;

  return 0;
}

extern "C" int
js_open_escapable_handle_scope (js_env_t *env, js_escapable_handle_scope_t **result) {
  *result = new js_escapable_handle_scope_s(env->isolate);

  return 0;
}

extern "C" int
js_close_escapable_handle_scope (js_env_t *env, js_escapable_handle_scope_t *scope) {
  delete scope;

  return 0;
}

extern "C" int
js_escape_handle (js_env_t *env, js_escapable_handle_scope_t *scope, js_value_t *escapee, js_value_t **result) {
  if (scope->escaped) {
    env->set_exception(Exception::Error(String::NewFromUtf8Literal(env->isolate, "Scope has already been escaped")));

    return -1;
  }

  scope->escaped = true;

  auto local = to_local(escapee);

  *result = from_local(scope->scope.Escape(local));

  return 0;
}

extern "C" int
js_run_script (js_env_t *env, js_value_t *source, js_value_t **result) {
  auto context = to_local(env->context);

  auto local_source = to_local<String>(source);

  auto v8_source = ScriptCompiler::Source(local_source);

  auto compiled = ScriptCompiler::Compile(context, &v8_source).ToLocalChecked();

  TryCatch try_catch(env->isolate);

  auto local = compiled->Run(context);

  if (try_catch.HasCaught()) {
    env->set_exception(try_catch.Exception());

    return -1;
  }

  if (result != nullptr) {
    *result = from_local(local.ToLocalChecked());
  }

  return 0;
}

static MaybeLocal<Module>
on_resolve_module (Local<Context> context, Local<String> specifier, Local<FixedArray> assertions, Local<Module> referrer) {
  auto env = get_env(context);

  auto module = get_module(context, referrer);

  auto result = module->resolve(
    env,
    from_local(specifier),
    from_local(assertions),
    module,
    module->data
  );

  if (result == nullptr) return MaybeLocal<Module>();

  return result->module;
}

extern "C" int
js_create_module (js_env_t *env, const char *name, size_t len, js_value_t *source, js_module_cb cb, void *data, js_module_t **result) {
  auto context = to_local(env->context);

  auto local_source = to_local<String>(source);

  MaybeLocal<String> local_name;

  if (len == size_t(-1)) {
    local_name = String::NewFromUtf8(env->isolate, name);
  } else {
    local_name = String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, len);
  }

  if (local_name.IsEmpty()) {
    env->set_exception(Exception::RangeError(String::NewFromUtf8Literal(env->isolate, "Invalid string length")));

    return -1;
  }

  auto origin = ScriptOrigin(
    env->isolate,
    local_name.ToLocalChecked(),
    0,
    0,
    false,
    -1,
    Local<Value>(),
    false,
    false,
    true
  );

  auto v8_source = ScriptCompiler::Source(local_source, origin);

  auto compiled = ScriptCompiler::CompileModule(env->isolate, &v8_source).ToLocalChecked();

  auto module = new js_module_t(compiled, data);

  module->resolve = cb;

  env->modules.emplace(compiled->GetIdentityHash(), module);

  compiled->InstantiateModule(context, on_resolve_module).Check();

  *result = module;

  return 0;
}

static MaybeLocal<Value>
on_evaluate_module (Local<Context> context, Local<Module> referrer) {
  auto env = get_env(context);

  auto module = get_module(context, referrer);

  module->evaluate(env, module, module->data);

  return Undefined(env->isolate);
}

extern "C" int
js_create_synthetic_module (js_env_t *env, const char *name, size_t len, js_value_t *const export_names[], size_t names_len, js_synthetic_module_cb cb, void *data, js_module_t **result) {
  auto context = to_local(env->context);

  auto local = reinterpret_cast<Local<String> *>(const_cast<js_value_t **>(export_names));

  auto names = std::vector<Local<String>>(local, local + names_len);

  MaybeLocal<String> local_name;

  if (len == size_t(-1)) {
    local_name = String::NewFromUtf8(env->isolate, name);
  } else {
    local_name = String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, len);
  }

  if (local_name.IsEmpty()) {
    env->set_exception(Exception::RangeError(String::NewFromUtf8Literal(env->isolate, "Invalid string length")));

    return -1;
  }

  auto compiled = Module::CreateSyntheticModule(
    env->isolate,
    local_name.ToLocalChecked(),
    names,
    on_evaluate_module
  );

  auto module = new js_module_t(compiled, data);

  module->evaluate = cb;

  env->modules.emplace(compiled->GetIdentityHash(), module);

  *result = module;

  return 0;
}

extern "C" int
js_delete_module (js_env_t *env, js_module_t *module) {
  delete module;

  return 0;
}

extern "C" int
js_set_module_export (js_env_t *env, js_module_t *module, js_value_t *name, js_value_t *value) {
  auto local = module->module;

  local->SetSyntheticModuleExport(env->isolate, to_local<String>(name), to_local(value)).Check();

  return 0;
}

extern "C" int
js_run_module (js_env_t *env, js_module_t *module, js_value_t **result) {
  auto local = module->module;

  auto context = to_local(env->context);

  *result = from_local(local->Evaluate(context).ToLocalChecked());

  return 0;
}

static void
on_reference_finalize (const WeakCallbackInfo<js_ref_t> &info) {
  auto reference = info.GetParameter();

  reference->value.Reset();
}

static inline void
js_set_weak_reference (js_env_t *env, js_ref_t *reference) {
  reference->value.SetWeak(reference, on_reference_finalize, WeakCallbackType::kParameter);
}

static inline void
js_clear_weak_reference (js_env_t *env, js_ref_t *reference) {
  reference->value.ClearWeak();
}

extern "C" int
js_create_reference (js_env_t *env, js_value_t *value, uint32_t count, js_ref_t **result) {
  auto reference = new js_ref_t(env->isolate, to_local(value), count);

  if (reference->count == 0) js_set_weak_reference(env, reference);

  *result = reference;

  return 0;
}

extern "C" int
js_delete_reference (js_env_t *env, js_ref_t *reference) {
  delete reference;

  return 0;
}

extern "C" int
js_reference_ref (js_env_t *env, js_ref_t *reference, uint32_t *result) {
  reference->count++;

  if (reference->count == 1) js_clear_weak_reference(env, reference);

  if (result != nullptr) {
    *result = reference->count;
  }

  return 0;
}

extern "C" int
js_reference_unref (js_env_t *env, js_ref_t *reference, uint32_t *result) {
  if (reference->count == 0) {
    env->set_exception(Exception::Error(String::NewFromUtf8Literal(env->isolate, "Cannot decrease reference count")));

    return -1;
  }

  reference->count--;

  if (reference->count == 0) js_set_weak_reference(env, reference);

  if (result != nullptr) {
    *result = reference->count;
  }

  return 0;
}

extern "C" int
js_get_reference_value (js_env_t *env, js_ref_t *reference, js_value_t **result) {
  if (reference->value.IsEmpty()) {
    *result = nullptr;
  } else {
    *result = from_local(reference->value.Get(env->isolate));
  }

  return 0;
}

extern "C" int
js_create_int32 (js_env_t *env, int32_t value, js_value_t **result) {
  auto uint = Integer::New(env->isolate, value);

  *result = from_local(uint);

  return 0;
}

extern "C" int
js_create_uint32 (js_env_t *env, uint32_t value, js_value_t **result) {
  auto uint = Integer::NewFromUnsigned(env->isolate, value);

  *result = from_local(uint);

  return 0;
}

extern "C" int
js_create_string_utf8 (js_env_t *env, const char *value, size_t len, js_value_t **result) {
  MaybeLocal<String> string;

  if (len == size_t(-1)) {
    string = String::NewFromUtf8(env->isolate, value);
  } else {
    string = String::NewFromUtf8(env->isolate, value, NewStringType::kNormal, len);
  }

  if (string.IsEmpty()) {
    env->set_exception(Exception::RangeError(String::NewFromUtf8Literal(env->isolate, "Invalid string length")));

    return -1;
  }

  *result = from_local(string.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_object (js_env_t *env, js_value_t **result) {
  auto context = to_local(env->context);

  auto object = Object::New(env->isolate);

  *result = from_local(object);

  return 0;
}

static void
on_function_call (const FunctionCallbackInfo<Value> &info) {
  auto callback = reinterpret_cast<js_callback_t *>(info.Data().As<External>()->Value());

  auto env = callback->env;

  auto result = callback->cb(env, reinterpret_cast<js_callback_info_t *>(const_cast<FunctionCallbackInfo<Value> *>(&info)));

  if (env->exception.IsEmpty()) {
    if (result != nullptr) {
      info.GetReturnValue().Set(to_local(result));
    }
  } else {
    env->isolate->ThrowException(Local<Value>::New(env->isolate, env->exception));

    env->exception.Reset();
  }
}

extern "C" int
js_create_function (js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data, js_value_t **result) {
  auto scope = EscapableHandleScope(env->isolate);

  auto context = to_local(env->context);

  auto callback = new js_callback_t(env, cb, data);

  auto external = External::New(env->isolate, callback);

  auto fn = Function::New(context, on_function_call, external).ToLocalChecked();

  if (name != nullptr) {
    MaybeLocal<String> string;

    if (len == size_t(-1)) {
      string = String::NewFromUtf8(env->isolate, name);
    } else {
      string = String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, len);
    }

    if (string.IsEmpty()) {
      env->set_exception(Exception::RangeError(String::NewFromUtf8Literal(env->isolate, "Invalid string length")));

      return -1;
    }

    fn->SetName(string.ToLocalChecked());
  }

  *result = from_local(scope.Escape(fn));

  return 0;
}

extern "C" int
js_create_function_with_ffi (js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data, js_ffi_function_t *ffi, js_value_t **result) {
  auto scope = EscapableHandleScope(env->isolate);

  auto context = to_local(env->context);

  auto callback = new js_callback_t(env, cb, data);

  auto external = External::New(env->isolate, callback);

  auto tpl = FunctionTemplate::New(
    env->isolate,
    on_function_call,
    external,
    Local<Signature>(),
    0,
    ConstructorBehavior::kThrow,
    SideEffectType::kHasNoSideEffect,
    &ffi->function
  );

  auto fn = tpl->GetFunction(context).ToLocalChecked();

  if (name != nullptr) {
    MaybeLocal<String> string;

    if (len == size_t(-1)) {
      string = String::NewFromUtf8(env->isolate, name);
    } else {
      string = String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, len);
    }

    if (string.IsEmpty()) {
      env->set_exception(Exception::RangeError(String::NewFromUtf8Literal(env->isolate, "Invalid string length")));

      return -1;
    }

    fn->SetName(string.ToLocalChecked());
  }

  *result = from_local(scope.Escape(fn));

  return 0;
}

static void
on_external_finalize (const WeakCallbackInfo<js_finalizer_t> &info) {
  auto finalizer = info.GetParameter();

  auto external = to_local(finalizer->value).As<External>();

  finalizer->cb(finalizer->env, finalizer->data, finalizer->hint);

  finalizer->value.Reset();

  delete finalizer;
}

extern "C" int
js_create_external (js_env_t *env, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result) {
  auto external = External::New(env->isolate, data);

  if (finalize_cb) {
    auto finalizer = new js_finalizer_t(env, external, data, finalize_cb, finalize_hint);

    finalizer->value.SetWeak(finalizer, on_external_finalize, WeakCallbackType::kParameter);
  }

  *result = from_local(external);

  return 0;
}

extern "C" int
js_create_promise (js_env_t *env, js_deferred_t **deferred, js_value_t **promise) {
  auto context = to_local(env->context);

  auto resolver = Promise::Resolver::New(context).ToLocalChecked();

  *deferred = new js_deferred_t(env->isolate, resolver);

  *promise = from_local(resolver->GetPromise());

  return 0;
}

static inline int
on_conclude_deferred (js_env_t *env, js_deferred_t *deferred, js_value_t *resolution, bool resolved) {
  auto context = to_local(env->context);

  auto resolver = Local<Promise::Resolver>::New(env->isolate, deferred->resolver);

  auto local = to_local(resolution);

  if (resolved) resolver->Resolve(context, local).Check();
  else resolver->Reject(context, local).Check();

  delete deferred;

  return 0;
}

extern "C" int
js_resolve_deferred (js_env_t *env, js_deferred_t *deferred, js_value_t *resolution) {
  return on_conclude_deferred(env, deferred, resolution, true);
}

extern "C" int
js_reject_deferred (js_env_t *env, js_deferred_t *deferred, js_value_t *resolution) {
  return on_conclude_deferred(env, deferred, resolution, false);
}

extern "C" int
js_get_promise_state (js_env_t *env, js_value_t *promise, js_promise_state_t *result) {
  auto local = to_local<Promise>(promise);

  switch (local->State()) {
  case Promise::PromiseState::kPending:
    *result = js_promise_pending;
    break;
  case Promise::PromiseState::kFulfilled:
    *result = js_promise_fulfilled;
    break;
  case Promise::PromiseState::kRejected:
    *result = js_promise_rejected;
    break;
  }

  return 0;
}

extern "C" int
js_get_promise_result (js_env_t *env, js_value_t *promise, js_value_t **result) {
  auto local = to_local<Promise>(promise);

  if (local->State() == Promise::PromiseState::kPending) {
    env->set_exception(Exception::Error(String::NewFromUtf8Literal(env->isolate, "Promise is pending")));

    return -1;
  }

  *result = from_local(local->Result());

  return 0;
}

extern "C" int
js_create_error (js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  auto context = to_local(env->context);

  auto error = Exception::Error(to_local<String>(message)).As<Object>();

  if (code != nullptr) {
    error->Set(context, String::NewFromUtf8Literal(env->isolate, "code"), to_local(code)).Check();
  }

  *result = from_local(error);

  return 0;
}

extern "C" int
js_typeof (js_env_t *env, js_value_t *value, js_value_type_t *result) {
  auto local = to_local(value);

  if (local->IsNumber()) {
    *result = js_number;
  } else if (local->IsBigInt()) {
    *result = js_bigint;
  } else if (local->IsString()) {
    *result = js_string;
  } else if (local->IsFunction()) {
    *result = js_function;
  } else if (local->IsExternal()) {
    *result = js_external;
  } else if (local->IsObject()) {
    *result = js_object;
  } else if (local->IsBoolean()) {
    *result = js_boolean;
  } else if (local->IsUndefined()) {
    *result = js_undefined;
  } else if (local->IsSymbol()) {
    *result = js_symbol;
  } else if (local->IsNull()) {
    *result = js_null;
  }

  return 0;
}

extern "C" int
js_is_undefined (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsUndefined();

  return 0;
}

extern "C" int
js_is_null (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsNull();

  return 0;
}

extern "C" int
js_is_boolean (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsBoolean();

  return 0;
}

extern "C" int
js_is_number (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsNumber();

  return 0;
}

extern "C" int
js_is_string (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsString();

  return 0;
}

extern "C" int
js_is_symbol (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsSymbol();

  return 0;
}

extern "C" int
js_is_object (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsObject();

  return 0;
}

extern "C" int
js_is_function (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsFunction();

  return 0;
}

extern "C" int
js_is_array (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsArray();

  return 0;
}

extern "C" int
js_is_external (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsExternal();

  return 0;
}

extern "C" int
js_is_bigint (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsBigInt();

  return 0;
}

extern "C" int
js_is_date (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsDate();

  return 0;
}

extern "C" int
js_is_error (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsNativeError();

  return 0;
}

extern "C" int
js_is_promise (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsPromise();

  return 0;
}

extern "C" int
js_is_arraybuffer (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsArrayBuffer();

  return 0;
}

extern "C" int
js_is_typedarray (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsTypedArray();

  return 0;
}

extern "C" int
js_is_dataview (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsDataView();

  return 0;
}

extern "C" int
js_strict_equals (js_env_t *env, js_value_t *a, js_value_t *b, bool *result) {
  *result = to_local(a)->StrictEquals(to_local(b));

  return 0;
}

extern "C" int
js_get_global (js_env_t *env, js_value_t **result) {
  auto context = to_local(env->context);

  *result = from_local(context->Global());

  return 0;
}

extern "C" int
js_get_undefined (js_env_t *env, js_value_t **result) {
  *result = from_local(Undefined(env->isolate));

  return 0;
}

extern "C" int
js_get_null (js_env_t *env, js_value_t **result) {
  *result = from_local(Null(env->isolate));

  return 0;
}

extern "C" int
js_get_boolean (js_env_t *env, bool value, js_value_t **result) {
  if (value) {
    *result = from_local(True(env->isolate));
  } else {
    *result = from_local(False(env->isolate));
  }

  return 0;
}

extern "C" int
js_get_value_int32 (js_env_t *env, js_value_t *value, int32_t *result) {
  auto local = to_local<Int32>(value);

  *result = local->Value();

  return 0;
}

extern "C" int
js_get_value_uint32 (js_env_t *env, js_value_t *value, uint32_t *result) {
  auto local = to_local<Uint32>(value);

  *result = local->Value();

  return 0;
}

extern "C" int
js_get_value_string_utf8 (js_env_t *env, js_value_t *value, char *str, size_t len, size_t *result) {
  auto local = to_local<String>(value);

  if (str == nullptr) {
    *result = local->Utf8Length(env->isolate);
  } else if (len != 0) {
    len = local->WriteUtf8(
      env->isolate,
      str,
      len - 1,
      nullptr,
      String::REPLACE_INVALID_UTF8 | String::NO_NULL_TERMINATION
    );

    str[len] = '\0';

    if (result != nullptr) {
      *result = len;
    }
  } else if (result != nullptr) {
    *result = 0;
  }

  return 0;
}

extern "C" int
js_get_value_external (js_env_t *env, js_value_t *value, void **result) {
  auto local = to_local<External>(value);

  *result = local->Value();

  return 0;
}

extern "C" int
js_get_named_property (js_env_t *env, js_value_t *object, const char *name, js_value_t **result) {
  auto context = to_local(env->context);

  auto key = String::NewFromUtf8(env->isolate, name);

  if (key.IsEmpty()) {
    env->set_exception(Exception::RangeError(String::NewFromUtf8Literal(env->isolate, "Invalid string length")));

    return -1;
  }

  auto local = to_local<Object>(object)->Get(context, key.ToLocalChecked());

  *result = from_local(local.ToLocalChecked());

  return 0;
}

extern "C" int
js_set_named_property (js_env_t *env, js_value_t *object, const char *name, js_value_t *value) {
  auto context = to_local(env->context);

  auto key = String::NewFromUtf8(env->isolate, name);

  if (key.IsEmpty()) {
    env->set_exception(Exception::RangeError(String::NewFromUtf8Literal(env->isolate, "Invalid string length")));

    return -1;
  }

  auto local = to_local(value);

  to_local<Object>(object)->Set(context, key.ToLocalChecked(), local).Check();

  return 0;
}

extern "C" int
js_call_function (js_env_t *env, js_value_t *receiver, js_value_t *fn, size_t argc, js_value_t *const argv[], js_value_t **result) {
  auto context = to_local(env->context);

  auto local_receiver = to_local(receiver);

  auto local_fn = to_local<Function>(fn);

  TryCatch try_catch(env->isolate);

  auto local = local_fn->Call(
    context,
    local_receiver,
    argc,
    reinterpret_cast<Local<Value> *>(const_cast<js_value_t **>(argv))
  );

  if (try_catch.HasCaught()) {
    env->set_exception(try_catch.Exception());

    return -1;
  }

  if (result != nullptr) {
    *result = from_local(local.ToLocalChecked());
  }

  return 0;
}

extern "C" int
js_make_callback (js_env_t *env, js_value_t *receiver, js_value_t *fn, size_t argc, js_value_t *const argv[], js_value_t **result) {
  int err = js_call_function(env, receiver, fn, argc, argv, result);

  env->run_microtasks();

  return err;
}

extern "C" int
js_get_callback_info (js_env_t *env, const js_callback_info_t *info, size_t *argc, js_value_t *argv[], js_value_t **self, void **data) {
  auto v8_info = reinterpret_cast<const FunctionCallbackInfo<Value> &>(*info);

  if (argv != nullptr) {
    size_t i = 0, n = v8_info.Length() < *argc ? v8_info.Length() : *argc;

    for (; i < n; i++) {
      argv[i] = from_local(v8_info[i]);
    }

    n = *argc;

    if (i < n) {
      auto undefined = from_local(Undefined(env->isolate));

      for (; i < n; i++) {
        argv[i] = undefined;
      }
    }
  }

  if (argc != nullptr) {
    *argc = v8_info.Length();
  }

  if (self != nullptr) {
    *self = from_local(v8_info.This());
  }

  if (data != nullptr) {
    *data = reinterpret_cast<js_callback_t *>(v8_info.Data().As<External>()->Value())->data;
  }

  return 0;
}

extern "C" int
js_get_arraybuffer_info (js_env_t *env, js_value_t *arraybuffer, void **data, size_t *len) {
  auto local = to_local(arraybuffer).As<ArrayBuffer>();

  if (data != nullptr) {
    *data = local->Data();
  }

  if (len != nullptr) {
    *len = local->ByteLength();
  }

  return 0;
}

extern "C" int
js_get_typedarray_info (js_env_t *env, js_value_t *typedarray, js_typedarray_type_t *type, void **data, size_t *len, js_value_t **arraybuffer, size_t *offset) {
  auto local = to_local(typedarray).As<TypedArray>();

  if (type != nullptr) {
    if (local->IsInt8Array()) {
      *type = js_int8_array;
    } else if (local->IsUint8Array()) {
      *type = js_uint8_array;
    } else if (local->IsUint8ClampedArray()) {
      *type = js_uint8_clamped_array;
    } else if (local->IsInt16Array()) {
      *type = js_int16_array;
    } else if (local->IsUint16Array()) {
      *type = js_uint16_array;
    } else if (local->IsInt32Array()) {
      *type = js_int32_array;
    } else if (local->IsUint32Array()) {
      *type = js_uint32_array;
    } else if (local->IsFloat32Array()) {
      *type = js_float32_array;
    } else if (local->IsFloat64Array()) {
      *type = js_float64_array;
    } else if (local->IsBigInt64Array()) {
      *type = js_bigint64_array;
    } else if (local->IsBigUint64Array()) {
      *type = js_biguint64_array;
    }
  }

  if (len != nullptr) {
    *len = local->Length();
  }

  Local<ArrayBuffer> buffer;

  if (data != nullptr || arraybuffer != nullptr) {
    buffer = local->Buffer();
  }

  if (data != nullptr) {
    *data = static_cast<uint8_t *>(buffer->Data()) + local->ByteOffset();
  }

  if (arraybuffer != nullptr) {
    *arraybuffer = from_local(buffer);
  }

  if (offset != nullptr) {
    *offset = local->ByteOffset();
  }

  return 0;
}

extern "C" int
js_get_dataview_info (js_env_t *env, js_value_t *dataview, void **data, size_t *len, js_value_t **arraybuffer, size_t *offset) {
  auto local = to_local(dataview).As<DataView>();

  if (len != nullptr) {
    *len = local->ByteLength();
  }

  Local<ArrayBuffer> buffer;

  if (data != nullptr || arraybuffer != nullptr) {
    buffer = local->Buffer();
  }

  if (data != nullptr) {
    *data = static_cast<uint8_t *>(buffer->Data()) + local->ByteOffset();
  }

  if (arraybuffer != nullptr) {
    *arraybuffer = from_local(buffer);
  }

  if (offset != nullptr) {
    *offset = local->ByteOffset();
  }

  return 0;
}

extern "C" int
js_throw (js_env_t *env, js_value_t *error) {
  env->isolate->ThrowException(to_local(error));

  return 0;
}

extern "C" int
js_throw_error (js_env_t *env, const char *code, const char *message) {
  auto context = to_local(env->context);

  auto local = String::NewFromUtf8(env->isolate, message);

  if (local.IsEmpty()) {
    env->set_exception(Exception::RangeError(String::NewFromUtf8Literal(env->isolate, "Invalid string length")));

    return -1;
  }

  auto error = Exception::Error(local.ToLocalChecked()).As<Object>();

  if (code != nullptr) {
    auto local = String::NewFromUtf8(env->isolate, code);

    if (local.IsEmpty()) {
      env->set_exception(Exception::RangeError(String::NewFromUtf8Literal(env->isolate, "Invalid string length")));

      return -1;
    }

    error->Set(context, String::NewFromUtf8Literal(env->isolate, "code"), local.ToLocalChecked()).Check();
  }

  env->isolate->ThrowException(error);

  return 0;
}

extern "C" int
js_is_exception_pending (js_env_t *env, bool *result) {
  *result = !env->exception.IsEmpty();

  return 0;
}

extern "C" int
js_get_and_clear_last_exception (js_env_t *env, js_value_t **result) {
  if (env->exception.IsEmpty()) return js_get_undefined(env, result);

  *result = from_local(Local<Value>::New(env->isolate, env->exception));

  env->exception.Reset();

  return 0;
}

extern "C" int
js_fatal_exception (js_env_t *env, js_value_t *error) {
  auto message = Exception::CreateMessage(env->isolate, to_local(error));

  on_uncaught_exception(message, to_local(error));

  return 0;
}

static void
on_microtask (void *data) {
  auto task = reinterpret_cast<js_task_t *>(data);

  task->cb(task->env, task->data);

  delete task;
}

extern "C" int
js_queue_microtask (js_env_t *env, js_task_cb cb, void *data) {
  auto task = new js_task_t(env, cb, data);

  auto context = to_local(env->context);

  env->isolate->EnqueueMicrotask(on_microtask, task);

  return 0;
}

extern "C" int
js_queue_macrotask (js_env_t *env, js_task_cb cb, void *data, uint64_t delay) {
  auto task = std::make_unique<js_task_t>(env, cb, data);

  auto tasks = env->tasks;

  if (delay) {
    tasks->push_task(js_delayed_task_handle_t(std::move(task), js_task_non_nestable, env->now() + delay));
  } else {
    tasks->push_task(js_task_handle_t(std::move(task), js_task_non_nestable));
  }

  return 0;
}

extern "C" int
js_request_garbage_collection (js_env_t *env) {
  if (!env->platform->options.expose_garbage_collection) {
    env->set_exception(Exception::Error(String::NewFromUtf8Literal(env->isolate, "Garbage collection is unavailable")));

    return -1;
  }

  env->isolate->RequestGarbageCollectionForTesting(Isolate::GarbageCollectionType::kFullGarbageCollection);

  return 0;
}

extern "C" int
js_ffi_create_type_info (js_ffi_type_t type, js_ffi_type_info_t **result) {
  CTypeInfo::Type v8_type;
  CTypeInfo::SequenceType v8_sequence_type = CTypeInfo::SequenceType::kScalar;
  CTypeInfo::Flags v8_flags = CTypeInfo::Flags::kNone;

  switch (type) {
  case js_ffi_receiver:
    v8_type = CTypeInfo::Type::kV8Value;
    break;
  case js_ffi_void:
    v8_type = CTypeInfo::Type::kVoid;
    break;
  case js_ffi_bool:
    v8_type = CTypeInfo::Type::kBool;
    break;
  case js_ffi_uint32:
    v8_type = CTypeInfo::Type::kUint32;
    break;
  case js_ffi_uint64:
    v8_type = CTypeInfo::Type::kUint64;
    break;
  case js_ffi_int32:
    v8_type = CTypeInfo::Type::kInt32;
    break;
  case js_ffi_int64:
    v8_type = CTypeInfo::Type::kInt64;
    break;
  case js_ffi_float32:
    v8_type = CTypeInfo::Type::kFloat32;
    break;
  case js_ffi_float64:
    v8_type = CTypeInfo::Type::kFloat64;
    break;
  case js_ffi_uint8array:
    v8_type = CTypeInfo::Type::kUint8;
    v8_sequence_type = CTypeInfo::SequenceType::kIsTypedArray;
    break;
  }

  auto type_info = new js_ffi_type_info_t(v8_type, v8_sequence_type, v8_flags);

  *result = type_info;

  return 0;
}

extern "C" int
js_ffi_create_function_info (const js_ffi_type_info_t *return_info, js_ffi_type_info_t *const arg_info[], unsigned int arg_len, js_ffi_function_info_t **result) {
  auto v8_return_info = return_info->type_info;

  auto v8_arg_info = std::vector<CTypeInfo>();

  v8_arg_info.reserve(arg_len);

  for (unsigned int i = 0; i < arg_len; i++) {
    v8_arg_info.push_back(arg_info[i]->type_info);
  }

  auto function_info = new js_ffi_function_info_t(v8_return_info, std::move(v8_arg_info));

  *result = function_info;

  return 0;
}

extern "C" int
js_ffi_create_function (const void *fn, const js_ffi_function_info_t *type_info, js_ffi_function_t **result) {
  auto v8_type_info = &type_info->function_info;

  auto function = new js_ffi_function_t(fn, v8_type_info);

  *result = function;

  return 0;
}
