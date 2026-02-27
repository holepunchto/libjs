#include <algorithm>
#include <atomic>
#include <bit>
#include <condition_variable>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <thread>
#include <vector>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include <v8-external-memory-accounter.h>
#include <v8-fast-api-calls.h>
#include <v8-inspector.h>
#include <v8.h>

#include "../include/js.h"

using namespace v8;
using namespace v8_inspector;

typedef struct js_callback_s js_callback_t;
typedef struct js_typed_callback_s js_typed_callback_t;
typedef struct js_finalizer_s js_finalizer_t;
typedef struct js_delegate_s js_delegate_t;
typedef struct js_external_string_utf16le_s js_external_string_utf16le_t;
typedef struct js_external_string_latin1_s js_external_string_latin1_t;
typedef struct js_tracing_controller_s js_tracing_controller_t;
typedef struct js_task_handle_s js_task_handle_t;
typedef struct js_delayed_task_handle_s js_delayed_task_handle_t;
typedef struct js_idle_task_handle_s js_idle_task_handle_t;
typedef struct js_task_runner_s js_task_runner_t;
typedef struct js_job_state_s js_job_state_t;
typedef struct js_job_handle_s js_job_handle_t;
typedef struct js_worker_s js_worker_t;
typedef struct js_heap_s js_heap_t;
typedef struct js_allocator_s js_allocator_t;
typedef struct js_teardown_queue_s js_teardown_queue_t;
typedef struct js_threadsafe_queue_s js_threadsafe_queue_t;
typedef struct js_threadsafe_unbounded_queue_s js_threadsafe_unbounded_queue_t;
typedef struct js_threadsafe_bounded_queue_s js_threadsafe_bounded_queue_t;
typedef struct js_inspector_client_s js_inspector_client_t;
typedef struct js_inspector_channel_s js_inspector_channel_t;

typedef enum {
  js_task_nestable,
  js_task_non_nestable,
} js_task_nestability_t;

typedef enum {
  js_return_type,
  js_argument_type,
} js_type_position_t;

namespace {

// As V8 local handles are, by design, just a pointer to an allocation, we can
// treat them as pointers to the opaque `js_value_t` type.

static_assert(sizeof(Local<Value>) == sizeof(js_value_t *));

static inline Local<Value>
js_to_local(js_value_t *value) {
  return *reinterpret_cast<Local<Value> *>(&value);
}

static inline Local<Value>
js_to_local(const js_value_t *value) {
  return *reinterpret_cast<Local<Value> *>(&value);
}

template <typename T>
static inline Local<T>
js_to_local(js_value_t *value) {
  return js_to_local(value).As<T>();
}

template <typename T>
static inline Local<T>
js_to_local(const js_value_t *value) {
  return js_to_local(value).As<T>();
}

static inline js_value_t *
js_from_local(Local<Value> local) {
  return reinterpret_cast<js_value_t *>(*local);
}

} // namespace

namespace {

static const ExternalPointerTypeTag js_callback_info_type_tag = 1;
static const ExternalPointerTypeTag js_delegate_type_tag = 2;
static const ExternalPointerTypeTag js_finalizer_type_tag = 3;
static const ExternalPointerTypeTag js_external_type_tag = 4;

} // namespace

struct js_tracing_controller_s : public TracingController {
private: // V8 embedder API
};

using js_task_completion_cb = std::function<void()>;

struct js_task_handle_s {
  TaskPriority priority;
  std::unique_ptr<Task> task;
  js_task_nestability_t nestability;
  js_task_completion_cb on_completion;

  js_task_handle_s(TaskPriority priority, std::unique_ptr<Task> task, js_task_nestability_t nestability)
      : priority(priority),
        task(std::move(task)),
        nestability(nestability),
        on_completion() {}

  js_task_handle_s(const js_task_handle_s &) = delete;

  js_task_handle_s(js_task_handle_s &&) = default;

  js_task_handle_s &
  operator=(const js_task_handle_s &) = delete;

  js_task_handle_s &
  operator=(js_task_handle_s &&) = default;

  void
  run() {
    task->Run();

    if (on_completion) on_completion();
  }
};

struct js_delayed_task_handle_s : js_task_handle_t {
  uint64_t expiry;

  js_delayed_task_handle_s(TaskPriority priority, std::unique_ptr<Task> task, js_task_nestability_t nestability, uint64_t expiry)
      : js_task_handle_t(priority, std::move(task), nestability),
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
      : task(std::move(task)),
        on_completion() {}

  js_idle_task_handle_s(const js_idle_task_handle_s &) = delete;

  js_idle_task_handle_s(js_idle_task_handle_s &&) = default;

  js_idle_task_handle_s &
  operator=(const js_idle_task_handle_s &) = delete;

  js_idle_task_handle_s &
  operator=(js_idle_task_handle_s &&) = default;

  void
  run(double deadline) {
    task->Run(deadline);

    if (on_completion) on_completion();
  }
};

struct js_task_runner_s : public TaskRunner {
  uv_loop_t *loop;
  uv_timer_t timer;
  uv_async_t async;

  int active_handles;

  // Keep a cyclic reference to the task runner itself that we'll only reset
  // once its handles have fully closed.
  std::shared_ptr<js_task_runner_t> self;

  std::deque<js_task_handle_t> tasks;
  std::priority_queue<js_delayed_task_handle_t> delayed_tasks;
  std::queue<js_idle_task_handle_t> idle_tasks;

  std::recursive_mutex lock;

  uint32_t depth;
  uint32_t outstanding;
  uint32_t disposable;

  bool closed;

  std::condition_variable_any available;
  std::condition_variable_any drained;

  js_task_runner_s(uv_loop_t *loop)
      : loop(loop),
        timer(),
        async(),
        active_handles(2),
        self(),
        tasks(),
        delayed_tasks(),
        idle_tasks(),
        lock(),
        depth(0),
        outstanding(0),
        disposable(0),
        closed(false),
        available(),
        drained() {
    int err;

    err = uv_timer_init(loop, &timer);
    assert(err == 0);

    timer.data = this;

    err = uv_async_init(loop, &async, on_async);
    assert(err == 0);

    async.data = this;

    uv_unref(reinterpret_cast<uv_handle_t *>(&async));
  }

  js_task_runner_s(const js_task_runner_s &) = delete;

  js_task_runner_s &
  operator=(const js_task_runner_s &) = delete;

  void
  close() {
    std::unique_lock guard(lock);

    closed = true;

    // TODO: Clear and cancel outstanding tasks and notify threads waiting for
    // the outstanding tasks to drain.

    available.notify_all();

    uv_close(reinterpret_cast<uv_handle_t *>(&timer), on_handle_close);

    uv_close(reinterpret_cast<uv_handle_t *>(&async), on_handle_close);
  }

  uint64_t
  now() {
    return uv_hrtime();
  }

  bool
  empty() {
    std::unique_lock guard(lock);

    return tasks.empty() && delayed_tasks.empty() && idle_tasks.empty();
  }

  size_t
  size() {
    std::unique_lock guard(lock);

    return tasks.size() + delayed_tasks.size() + idle_tasks.size();
  }

  bool
  inactive() {
    std::unique_lock guard(lock);

    return empty() || outstanding == disposable;
  }

  void
  push_task(js_task_handle_t &&task) {
    int err;

    std::unique_lock guard(lock);

    if (closed) return;

    outstanding++;

    task.on_completion = [this] { on_completion(); };

    tasks.push_back(std::move(task));

    available.notify_one();

    err = uv_async_send(&async);
    assert(err == 0);
  }

  void
  push_task(js_delayed_task_handle_t &&task) {
    std::unique_lock guard(lock);

    if (closed) return;

    outstanding++;

    // Tasks that are not user blocking are safe to dispose if all other tasks
    // have finished.
    auto is_disposable = task.priority != TaskPriority::kUserBlocking;

    if (is_disposable) disposable++;

    task.on_completion = [this, is_disposable] { on_completion(is_disposable); };

    delayed_tasks.push(std::move(task));
  }

  void
  push_task(js_idle_task_handle_t &&task) {
    std::unique_lock guard(lock);

    if (closed) return;

    outstanding++;

    task.on_completion = [this] { on_completion(); };

    idle_tasks.push(std::move(task));
  }

  bool
  can_pop_task() {
    std::unique_lock guard(lock);

    if (depth == 0) return !tasks.empty();

    for (auto task = tasks.begin(); task != tasks.end(); task++) {
      if (task->nestability == js_task_nestable) return true;
    }

    return false;
  }

  std::optional<js_task_handle_t>
  pop_task() {
    std::unique_lock guard(lock);

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

    return std::move(value);
  }

  std::optional<js_task_handle_t>
  pop_task_wait() {
    std::unique_lock guard(lock);

    auto task = pop_task();

    if (task) return task;

    while (!closed && !can_pop_task()) {
      available.wait(guard);
    }

    return pop_task();
  }

  void
  move_expired_tasks() {
    int err;

    std::unique_lock guard(lock);

    while (!delayed_tasks.empty()) {
      auto const &task = delayed_tasks.top();

      if (task.expiry > now()) break;

      tasks.push_back(std::move(const_cast<js_delayed_task_handle_t &>(task)));

      delayed_tasks.pop();

      available.notify_one();

      if (closed) continue;

      err = uv_async_send(&async);
      assert(err == 0);
    }

    adjust_timer();
  }

  void
  drain() {
    std::unique_lock guard(lock);

    if (closed) return;

    while (outstanding > disposable) {
      drained.wait(guard);
    }
  }

private:
  void
  adjust_timer() {
    int err;

    std::unique_lock guard(lock);

    if (closed) return;

    if (delayed_tasks.empty()) {
      err = uv_timer_stop(&timer);
      assert(err == 0);
    } else {
      auto const &task = delayed_tasks.top();

      auto n = now();

      auto timeout = task.expiry > n ? (task.expiry - n) / 1000000 : 0;

      err = uv_timer_start(&timer, on_timer, timeout, 0);
      assert(err == 0);

      // Don't let the timer keep the loop alive if all outstanding tasks are
      // disposable.
      if (outstanding == disposable) {
        uv_unref(reinterpret_cast<uv_handle_t *>(&timer));
      }
    }
  }

  void
  on_completion(bool is_disposable = false) {
    std::unique_lock guard(lock);

    if (is_disposable) disposable--;

    if (--outstanding <= disposable) {
      drained.notify_all();
    }
  }

  static void
  on_timer(uv_timer_t *handle) {
    auto tasks = reinterpret_cast<js_task_runner_t *>(handle->data);

    tasks->move_expired_tasks();
  }

  static void
  on_async(uv_async_t *handle) {}

  static void
  on_handle_close(uv_handle_t *handle) {
    auto tasks = reinterpret_cast<js_task_runner_t *>(handle->data);

    if (--tasks->active_handles == 0) {
      tasks->self.reset();
    }
  }

private: // V8 embedder API
  void
  PostTaskImpl(std::unique_ptr<Task> task, const SourceLocation &location = SourceLocation::Current()) override {
    push_task(js_task_handle_t(TaskPriority::kBestEffort, std::move(task), js_task_nestable));
  }

  void
  PostNonNestableTaskImpl(std::unique_ptr<Task> task, const SourceLocation &location = SourceLocation::Current()) override {
    push_task(js_task_handle_t(TaskPriority::kUserBlocking, std::move(task), js_task_non_nestable));
  }

  void
  PostDelayedTaskImpl(std::unique_ptr<Task> task, double delay_in_seconds, const SourceLocation &location = SourceLocation::Current()) override {
    auto expiry = now() + uint64_t(delay_in_seconds * 1000000000);

    push_task(js_delayed_task_handle_t(TaskPriority::kBestEffort, std::move(task), js_task_nestable, expiry));
  }

  void
  PostNonNestableDelayedTaskImpl(std::unique_ptr<Task> task, double delay_in_seconds, const SourceLocation &location = SourceLocation::Current()) override {
    auto expiry = now() + uint64_t(delay_in_seconds * 1000000000);

    push_task(js_delayed_task_handle_t(TaskPriority::kUserBlocking, std::move(task), js_task_non_nestable, expiry));
  }

  void
  PostIdleTaskImpl(std::unique_ptr<IdleTask> task, const SourceLocation &location = SourceLocation::Current()) override {
    push_task(js_idle_task_handle_t(std::move(task)));
  }

  bool
  IdleTasksEnabled() override {
    return true;
  }

  bool
  NonNestableTasksEnabled() const override {
    return true;
  }

  bool
  NonNestableDelayedTasksEnabled() const override {
    return true;
  }
};

namespace {

static const auto js_invalid_task_id = uint8_t(-1);

} // namespace

struct js_job_state_s : std::enable_shared_from_this<js_job_state_s> {
  TaskPriority priority;
  std::unique_ptr<JobTask> task;
  std::shared_ptr<js_task_runner_t> task_runner;

  uint8_t available_parallelism;

  uint8_t active_workers;
  uint8_t pending_workers;

  std::atomic<uint64_t> task_ids;

  std::atomic<bool> cancelled;

  std::recursive_mutex lock;
  std::condition_variable_any worker_released;

  js_job_state_s(TaskPriority priority, std::unique_ptr<JobTask> task, std::shared_ptr<js_task_runner_t> task_runner, uint8_t available_parallelism)
      : priority(priority),
        task(std::move(task)),
        task_runner(std::move(task_runner)),
        available_parallelism(std::min<uint8_t>(available_parallelism, 64)),
        active_workers(0),
        pending_workers(0),
        task_ids(0),
        cancelled(false),
        lock(),
        worker_released() {}

  js_job_state_s(const js_job_state_s &) = delete;

  js_job_state_s &
  operator=(const js_job_state_s &) = delete;

  uint8_t
  acquire_task_id() {
    auto task_ids = this->task_ids.load(std::memory_order_relaxed);

    uint8_t task_id;
    bool ok;

    do {
      task_id = std::countr_one(task_ids) & 0xff;

      ok = this->task_ids.compare_exchange_weak(
        task_ids,
        task_ids | (uint64_t(1) << task_id),
        std::memory_order_acquire,
        std::memory_order_relaxed
      );
    } while (!ok);

    return task_id;
  }

  void
  release_task_id(uint8_t task_id) {
    task_ids.fetch_and(~(uint64_t(1) << task_id), std::memory_order_release);
  }

  void
  create_workers() {
    if (cancelled.load(std::memory_order_relaxed)) return;

    std::unique_lock guard(lock);

    auto concurrency = max_concurrency();

    if (concurrency > active_workers + pending_workers) {
      concurrency -= active_workers + pending_workers;

      for (auto i = 0; i < concurrency; i++, pending_workers++) {
        schedule_run();
      }
    }
  }

  void
  join() {
    std::unique_lock guard(lock);

    update_priority(TaskPriority::kUserBlocking);

    active_workers++; // Reserved for the joining thread

    auto wait_for_concurrency = [this, &guard]() {
      auto concurrency = max_concurrency(-1);

      while (active_workers > concurrency && active_workers > 1) {
        worker_released.wait(guard);

        concurrency = max_concurrency(-1);
      }

      if (concurrency == 0) cancelled.store(true, std::memory_order_relaxed);

      return concurrency;
    };

    auto concurrency = wait_for_concurrency();

    if (concurrency == 0) return;

    if (concurrency > active_workers + pending_workers) {
      concurrency -= active_workers + pending_workers;

      for (auto i = 0; i < concurrency; i++, pending_workers++) {
        schedule_run();
      }
    }

    do {
      run(true);
    } while (wait_for_concurrency());

    active_workers--;
  }

  void
  cancel() {
    cancelled.store(true, std::memory_order_relaxed);
  }

  void
  cancel_and_wait() {
    std::unique_lock guard(lock);

    cancel();

    while (active_workers) {
      worker_released.wait(guard);
    }
  }

  bool
  is_active() {
    std::unique_lock guard(lock);

    return max_concurrency() != 0 || active_workers != 0;
  }

  void
  update_priority(TaskPriority priority) {
    std::unique_lock guard(lock);

    this->priority = priority;
  }

private:
  uint8_t
  max_concurrency(int8_t delta = 0) {
    std::unique_lock guard(lock);

    auto worker_count = active_workers + delta;

    if (worker_count < 0) worker_count = 0;

    auto max_concurrency = task->GetMaxConcurrency(size_t(worker_count));

    if (max_concurrency > available_parallelism) return available_parallelism;

    return uint8_t(max_concurrency);
  }

  void
  run(bool is_joining_thread = false) {
    js_job_delegate_s delegate(shared_from_this(), is_joining_thread);

    task->Run(&delegate);
  }

  void
  schedule_run() {
    auto task = std::make_unique<js_job_worker_s>(shared_from_this());

    task_runner->push_task(js_task_handle_t(TaskPriority::kBestEffort, std::move(task), js_task_nestable));
  }

  bool
  should_start_task() {
    std::unique_lock guard(lock);

    pending_workers--;

    if (cancelled.load(std::memory_order_relaxed) || active_workers > max_concurrency(-1)) {
      return false;
    }

    active_workers++;

    return true;
  }

  bool
  should_continue_task() {
    std::unique_lock guard(lock);

    if (cancelled.load(std::memory_order_relaxed) || active_workers > max_concurrency(-1)) {
      active_workers--;

      worker_released.notify_one();

      return false;
    }

    return true;
  }

  struct js_job_worker_s : public Task {
    std::weak_ptr<js_job_state_t> state;

    js_job_worker_s(std::weak_ptr<js_job_state_t> state)
        : state(std::move(state)) {}

    js_job_worker_s(const js_job_worker_s &) = delete;

    js_job_worker_s &
    operator=(const js_job_worker_s &) = delete;

    void
    run() {
      auto state = this->state.lock();

      if (!state) return;
      if (!state->should_start_task()) return;

      do {
        state->run();
      } while (state->should_continue_task());
    }

  private: // V8 embedder API
    void
    Run() {
      run();
    }
  };

  struct js_job_delegate_s : public JobDelegate {
    std::shared_ptr<js_job_state_t> state;
    uint8_t task_id;
    bool is_joining_thread;
    bool cancelled;

    js_job_delegate_s(std::shared_ptr<js_job_state_t> state, bool is_joining_thread = false)
        : state(std::move(state)),
          task_id(js_invalid_task_id),
          is_joining_thread(is_joining_thread),
          cancelled(false) {}

    ~js_job_delegate_s() {
      release_task_id();
    }

    js_job_delegate_s(const js_job_delegate_s &) = delete;

    js_job_delegate_s &
    operator=(const js_job_delegate_s &) = delete;

  private:
    uint8_t
    acquire_task_id() {
      if (task_id == js_invalid_task_id) task_id = state->acquire_task_id();
      return task_id;
    }

    void
    release_task_id() {
      if (task_id != js_invalid_task_id) state->release_task_id(task_id);
    }

  private: // V8 embedder API
    bool
    ShouldYield() override {
      cancelled |= state->cancelled.load(std::memory_order_relaxed);
      return cancelled;
    }

    void
    NotifyConcurrencyIncrease() override {
      return state->create_workers();
    }

    uint8_t
    GetTaskId() override {
      return acquire_task_id();
    }

    bool
    IsJoiningThread() const override {
      return is_joining_thread;
    }
  };
};

struct js_job_handle_s : public JobHandle {
  std::shared_ptr<js_job_state_t> state;

  js_job_handle_s(TaskPriority priority, std::unique_ptr<JobTask> task, std::shared_ptr<js_task_runner_t> task_runner, uint8_t available_parallelism)
      : state(new js_job_state_t(priority, std::move(task), std::move(task_runner), available_parallelism)) {}

  js_job_handle_s(const js_job_handle_s &) = delete;

  js_job_handle_s &
  operator=(const js_job_handle_s &) = delete;

private: // V8 embedder API
  void
  NotifyConcurrencyIncrease() override {
    state->create_workers();
  }

  void
  Join() override {
    state->join();
    state = nullptr;
  }

  void
  Cancel() override {
    state->cancel_and_wait();
    state = nullptr;
  }

  void
  CancelAndDetach() override {
    state->cancel();
    state = nullptr;
  }

  bool
  IsActive() override {
    return state->is_active();
  }

  bool
  IsValid() override {
    return state != nullptr;
  }

  bool
  UpdatePriorityEnabled() const override {
    return true;
  }

  void
  UpdatePriority(TaskPriority priority) override {
    state->update_priority(priority);
  }
};

struct js_worker_s {
  std::shared_ptr<js_task_runner_t> tasks;
  std::thread thread;

  js_worker_s(std::shared_ptr<js_task_runner_t> tasks)
      : tasks(std::move(tasks)),
        thread(&js_worker_t::on_thread, this) {}

  js_worker_s(const js_worker_s &) = delete;

  ~js_worker_s() {
    if (thread.joinable()) join();
  }

  js_worker_s &
  operator=(const js_worker_s &) = delete;

  void
  join() {
    thread.join();
  }

private:
  void
  on_thread() {
    while (auto task = tasks->pop_task_wait()) {
      task->run();
    }
  }
};

struct js_heap_s {
  js_heap_s() {}

  js_heap_s(const js_heap_s &) = delete;

  js_heap_s &
  operator=(const js_heap_s &) = delete;

  void *
  alloc(size_t size) {
    void *ptr = alloc_unsafe(size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
  }

  void *
  alloc_unsafe(size_t size) {
    return ::malloc(size);
  }

  void
  free(void *ptr) {
    ::free(ptr);
  }
};

struct js_allocator_s : public ArrayBuffer::Allocator {
  js_heap_t heap;

  js_allocator_s() = default;

  js_allocator_s(const js_allocator_s &) = delete;

  js_allocator_s &
  operator=(const js_allocator_s &) = delete;

private: // V8 embedder API
  void *
  Allocate(size_t length) override {
    return heap.alloc(length);
  }

  void *
  AllocateUninitialized(size_t length) override {
    return heap.alloc_unsafe(length);
  }

  void
  Free(void *data, size_t length) override {
    heap.free(data);
  }
};

struct js_teardown_queue_s {
  using js_teardown_handle = std::pair<js_teardown_cb, void *>;
  using js_teardown_list = std::list<js_teardown_handle>;
  using js_teardown_index = std::map<js_teardown_handle, js_teardown_list::iterator>;

  enum class status {
    success = 0,
    drained = 1,
    already_registered = 2,
    not_registered = 3,
  };

  js_teardown_list handles;
  js_teardown_index index;
  bool drained;

  js_teardown_queue_s()
      : handles(),
        index(),
        drained(false) {}

  auto
  begin() {
    return handles.begin();
  }

  auto
  end() {
    return handles.end();
  }

  status
  push(js_teardown_cb cb, void *data) {
    if (drained) return status::drained;

    js_teardown_handle handle = std::make_pair(cb, data);

    if (index.contains(handle)) return status::already_registered;

    index[handle] = handles.insert(handles.begin(), handle);

    return status::success;
  }

  status
  pop(js_teardown_cb cb, void *data) {
    js_teardown_handle handle = std::make_pair(cb, data);

    if (!index.contains(handle)) return status::not_registered;

    if (drained) return status::success;

    handles.erase(index[handle]);
    index.erase(handle);

    return status::success;
  }

  status
  drain(js_env_t *env) {
    if (drained) return status::drained;

    drained = true;

    for (auto const &[cb, data] : handles) {
      cb(data);
    }

    handles.clear();

    return status::success;
  }
};

struct js_platform_s : public Platform {
  js_platform_options_t options;

  uv_loop_t *loop;
  uv_prepare_t prepare;
  uv_check_t check;

  int active_handles;

  std::set<js_env_t *> environments;

  std::map<Isolate *, std::shared_ptr<js_task_runner_t>> foreground;
  std::shared_ptr<js_task_runner_t> background;

  std::vector<std::shared_ptr<js_worker_t>> workers;
  std::unique_ptr<js_tracing_controller_t> trace;

  std::mutex lock;

  js_platform_s(js_platform_options_t options, uv_loop_t *loop)
      : options(std::move(options)),
        loop(loop),
        prepare(),
        check(),
        active_handles(2),
        environments(),
        foreground(),
        background(new js_task_runner_t(loop)),
        workers(),
        trace(new js_tracing_controller_t()),
        lock() {
    int err;

    bool success;

    success = V8::InitializeICU();
    assert(success);

    V8::InitializePlatform(this);

    success = V8::Initialize();
    assert(success);

    background->self = background;

    err = uv_prepare_init(loop, &prepare);
    assert(err == 0);

    err = uv_prepare_start(&prepare, on_prepare);
    assert(err == 0);

    prepare.data = this;

    err = uv_check_init(loop, &check);
    assert(err == 0);

    err = uv_check_start(&check, on_check);
    assert(err == 0);

    check.data = this;

    // The check handle should not on its own keep the loop alive; it's simply
    // used for running any outstanding tasks that might cause additional work
    // to be queued.
    uv_unref(reinterpret_cast<uv_handle_t *>(&check));

    workers.reserve(std::max<size_t>(uv_available_parallelism() - 1 /* main thread */, 1));

    while (workers.size() < workers.capacity()) {
      workers.emplace_back(new js_worker_t(background));
    }
  }

  js_platform_s(const js_platform_s &) = delete;

  js_platform_s &
  operator=(const js_platform_s &) = delete;

  void
  close() {
    background->close();

    for (auto &worker : workers) {
      worker->join();
    }

    uv_close(reinterpret_cast<uv_handle_t *>(&prepare), on_handle_close);

    uv_close(reinterpret_cast<uv_handle_t *>(&check), on_handle_close);
  }

  uint64_t
  now() {
    return uv_hrtime();
  }

  void
  idle() {
    // TODO: This should wait until either the platform drains completely or a
    // task is made available.
    drain();
  }

  void
  drain() {
    background->drain();
  }

  void
  attach(js_env_t *env, std::unique_lock<std::mutex> &guard) {
    environments.insert(env);
  }

  void
  detach(js_env_t *env, std::unique_lock<std::mutex> &guard) {
    environments.erase(env);

    dispose_maybe(guard);
  }

private:
  void
  dispose_maybe(std::unique_lock<std::mutex> &lock) {
    if (active_handles == 0 && environments.empty()) {
      V8::Dispose();
      V8::DisposePlatform();

      lock.unlock();

      delete this;
    }
  }

  void
  run_tasks() {
    background->move_expired_tasks();

    while (auto task = background->pop_task()) {
      task->run();
    }
  }

  void
  check_liveness() {
    int err;

    if (background->inactive()) {
      err = uv_prepare_stop(&prepare);
    } else {
      err = uv_prepare_start(&prepare, on_prepare);
    }

    assert(err == 0);
  }

  static void
  on_prepare(uv_prepare_t *handle) {
    auto platform = reinterpret_cast<js_platform_t *>(handle->data);

    platform->run_tasks();

    platform->check_liveness();
  }

  static void
  on_check(uv_check_t *handle) {
    int err;

    auto platform = reinterpret_cast<js_platform_t *>(handle->data);

    if (uv_loop_alive(platform->loop)) {
      err = uv_prepare_start(&platform->prepare, on_prepare);
      assert(err == 0);

      return;
    }

    platform->idle();

    platform->check_liveness();
  }

  static void
  on_handle_close(uv_handle_t *handle) {
    auto platform = reinterpret_cast<js_platform_t *>(handle->data);

    std::unique_lock guard(platform->lock);

    platform->active_handles--;

    platform->dispose_maybe(guard);
  }

private: // V8 embedder API
  PageAllocator *
  GetPageAllocator() override {
    return nullptr;
  }

  int
  NumberOfWorkerThreads() override {
    return int(workers.size());
  }

  std::shared_ptr<TaskRunner>
  GetForegroundTaskRunner(Isolate *isolate, TaskPriority priority) override {
    std::unique_lock guard(lock);

    return foreground[isolate];
  }

  std::unique_ptr<JobHandle>
  CreateJobImpl(TaskPriority priority, std::unique_ptr<JobTask> task, const SourceLocation &location) override {
    return std::make_unique<js_job_handle_t>(priority, std::move(task), background, workers.size());
  }

  void
  PostTaskOnWorkerThreadImpl(TaskPriority priority, std::unique_ptr<Task> task, const SourceLocation &location) override {
    background->push_task(js_task_handle_t(priority, std::move(task), js_task_nestable));
  }

  void
  PostDelayedTaskOnWorkerThreadImpl(TaskPriority priority, std::unique_ptr<Task> task, double delay_in_seconds, const SourceLocation &location) override {
    auto expiry = background->now() + uint64_t(delay_in_seconds * 1000000000);

    background->push_task(js_delayed_task_handle_t(priority, std::move(task), js_task_nestable, expiry));
  }

  double
  MonotonicallyIncreasingTime() override {
    return double(now()) / 1000000000;
  }

  double
  CurrentClockTimeMillis() override {
    return SystemClockTimeMillis();
  }

  TracingController *
  GetTracingController() override {
    return trace.get();
  }
};

struct js_env_s {
  uv_loop_t *loop;
  uv_prepare_t prepare;
  uv_check_t check;
  uv_async_t teardown;

  int active_handles;

  uint32_t refs;

  js_platform_t *platform;

  std::shared_ptr<js_task_runner_t> tasks;

  uint32_t depth;

  Isolate *isolate;

  Global<Context> context;

  Global<Private> wrapper;
  Global<Private> delegate;
  Global<Private> tag;

  Global<Value> exception;

  ExternalMemoryAccounter memory;

  std::multimap<int, js_module_t *> modules;

  std::list<Global<Promise>> unhandled_promises;

  js_teardown_queue_t teardown_queue;

  std::shared_ptr<js_inspector_client_t> inspector;

  struct {
    js_uncaught_exception_cb uncaught_exception;
    void *uncaught_exception_data;

    js_unhandled_rejection_cb unhandled_rejection;
    void *unhandled_rejection_data;

    js_dynamic_import_cb dynamic_import;
    js_dynamic_import_transitional_cb dynamic_import_transitional;
    void *dynamic_import_data;
  } callbacks;

  js_env_s(uv_loop_t *loop, js_platform_t *platform, Isolate *isolate)
      : loop(loop),
        prepare(),
        check(),
        teardown(),
        active_handles(3),
        refs(0),
        platform(platform),
        tasks(),
        depth(0),
        isolate(isolate),
        context(),
        wrapper(),
        delegate(),
        tag(),
        exception(),
        modules(),
        unhandled_promises(),
        teardown_queue(),
        inspector(),
        callbacks() {
    int err;

    std::unique_lock guard(platform->lock);

    tasks = platform->foreground[isolate];

    platform->attach(this, guard);

    guard.unlock();

    tasks->self = tasks;

    err = uv_prepare_init(loop, &prepare);
    assert(err == 0);

    err = uv_prepare_start(&prepare, on_prepare);
    assert(err == 0);

    prepare.data = this;

    err = uv_check_init(loop, &check);
    assert(err == 0);

    err = uv_check_start(&check, on_check);
    assert(err == 0);

    check.data = this;

    // The check handle should not on its own keep the loop alive; it's simply
    // used for running any outstanding tasks that might cause additional work
    // to be queued.
    uv_unref(reinterpret_cast<uv_handle_t *>(&check));

    err = uv_async_init(loop, &teardown, on_teardown);
    assert(err == 0);

    teardown.data = this;

    uv_unref(reinterpret_cast<uv_handle_t *>(&teardown));

    isolate->SetData(0, this);

    auto scope = HandleScope(isolate);

    context.Reset(isolate, Context::New(isolate));
    context.Get(isolate)->Enter();

    wrapper.Reset(isolate, Private::New(isolate));
    delegate.Reset(isolate, Private::New(isolate));
    tag.Reset(isolate, Private::New(isolate));
  }

  ~js_env_s() {
    if (inspector) inspector.reset();

    {
      auto scope = HandleScope(isolate);

      wrapper.Reset();
      delegate.Reset();
      tag.Reset();
      exception.Reset();

      context.Get(isolate)->Exit();
      context.Reset();
    }

    isolate->Exit();
    isolate->Dispose();

    std::unique_lock guard(platform->lock);

    platform->foreground.erase(isolate);

    platform->detach(this, guard);
  }

  js_env_s(const js_env_s &) = delete;

  js_env_s &
  operator=(const js_env_s &) = delete;

  static inline js_env_t *
  from(Isolate *isolate) {
    return reinterpret_cast<js_env_t *>(isolate->GetData(0));
  }

  bool
  ref() {
    refs++;

    return true;
  }

  bool
  unref() {
    int err;

    if (refs == 0) return false;

    if (--refs == 0 && teardown_queue.drained) {
      err = uv_async_send(&teardown);
      assert(err == 0);
    }

    return true;
  }

  void
  close_maybe() {
    teardown_queue.drain(this);

    if (refs == 0) close();
    else {
      uv_ref(reinterpret_cast<uv_handle_t *>(&teardown));
    }
  }

  uint64_t
  now() {
    return uv_hrtime();
  }

  void
  idle() {
    // TODO: This should wait until either the platform drains completely or a
    // task is made available for the isolate.
    platform->drain();
  }

  void
  run_microtasks() {
    if (isolate->IsExecutionTerminating()) return;

    isolate->PerformMicrotaskCheckpoint();

    if (callbacks.unhandled_rejection == nullptr) {
      return unhandled_promises.clear();
    }

    while (!unhandled_promises.empty()) {
      auto promise = std::move(unhandled_promises.front());

      unhandled_promises.pop_front();

      auto scope = HandleScope(isolate);

      auto local = promise.Get(isolate);

      callbacks.unhandled_rejection(
        this,
        js_from_local(local->Result()),
        js_from_local(local),
        callbacks.unhandled_rejection_data
      );

      if (isolate->IsExecutionTerminating()) return;

      isolate->PerformMicrotaskCheckpoint();
    }
  }

  void
  run_macrotasks() {
    tasks->move_expired_tasks();

    while (auto task = tasks->pop_task()) {
      tasks->depth++;

      task->run();

      run_microtasks();

      tasks->depth--;
    }
  }

  bool
  is_exception_pending() {
    return !exception.IsEmpty();
  }

  void
  uncaught_exception(Local<Value> error) {
    if (callbacks.uncaught_exception) {
      callbacks.uncaught_exception(this, js_from_local(error), callbacks.uncaught_exception_data);
    } else {
      exception.Reset(isolate, error);
    }
  }

  Local<Context>
  current_context() {
    return isolate->GetCurrentContext();
  }

  template <typename T>
  T
  try_catch(const std::function<T()> &fn) {
    auto try_catch = TryCatch(isolate);

    auto result = fn();

    if (try_catch.HasCaught() && try_catch.CanContinue()) {
      auto error = try_catch.Exception();

      if (depth == 0) uncaught_exception(error);
      else {
        exception.Reset(isolate, error);

        try_catch.ReThrow();
      }
    }

    return std::move(result);
  }

  template <typename T>
  Maybe<T>
  try_catch(const std::function<Maybe<T>()> &fn) {
    return try_catch<Maybe<T>>(fn);
  }

  template <typename T>
  MaybeLocal<T>
  try_catch(const std::function<MaybeLocal<T>()> &fn) {
    return try_catch<MaybeLocal<T>>(fn);
  }

  template <typename T>
  T
  call_into_javascript(const std::function<T()> &fn, bool always_checkpoint = false) {
    return try_catch<T>(
      [&] {
        depth++;

        auto result = fn();

        if (depth == 1 || always_checkpoint) run_microtasks();

        depth--;

        return result;
      }
    );
  }

  template <typename T>
  Maybe<T>
  call_into_javascript(const std::function<Maybe<T>()> &fn, bool always_checkpoint = false) {
    return call_into_javascript<Maybe<T>>(fn, always_checkpoint);
  }

  template <typename T>
  MaybeLocal<T>
  call_into_javascript(const std::function<MaybeLocal<T>()> &fn, bool always_checkpoint = false) {
    return call_into_javascript<MaybeLocal<T>>(fn, always_checkpoint);
  }

  auto
  add_teardown_callback(js_teardown_cb cb, void *data) {
    return teardown_queue.push(cb, data);
  }

  auto
  remove_teardown_callback(js_teardown_cb cb, void *data) {
    return teardown_queue.pop(cb, data);
  }

  static void
  on_uncaught_exception(Local<Message> message, Local<Value> error) {
    auto env = js_env_t::from(Isolate::GetCurrent());

    env->uncaught_exception(error);
  }

  static void
  on_fatal_error(const char *location, const char *message) {
    abort();
  }

  static void
  on_out_of_memory_error(const char *location, const OOMDetails &details) {
    abort();
  }

  static void
  on_promise_reject(PromiseRejectMessage message) {
    auto promise = message.GetPromise();

    auto isolate = Isolate::GetCurrent();

    auto env = js_env_t::from(isolate);

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
          env->unhandled_promises.erase(it);

          break;
        }
      }
    }
  }

private:
  void
  close() {
    tasks->close();

    uv_close(reinterpret_cast<uv_handle_t *>(&prepare), on_handle_close);

    uv_close(reinterpret_cast<uv_handle_t *>(&check), on_handle_close);

    uv_close(reinterpret_cast<uv_handle_t *>(&teardown), on_handle_close);
  }

  void
  dispose() {
    delete this;
  }

  void
  dispose_maybe() {
    if (active_handles == 0) dispose();
  }

  void
  check_liveness() {
    int err;

    tasks->move_expired_tasks();

    if (tasks->inactive()) {
      err = uv_prepare_stop(&prepare);
    } else {
      err = uv_prepare_start(&prepare, on_prepare);
    }

    assert(err == 0);
  }

  static void
  on_prepare(uv_prepare_t *handle) {
    auto env = reinterpret_cast<js_env_t *>(handle->data);

    env->run_macrotasks();

    env->check_liveness();
  }

  static void
  on_check(uv_check_t *handle) {
    int err;

    auto env = reinterpret_cast<js_env_t *>(handle->data);

    if (uv_loop_alive(env->loop)) {
      err = uv_prepare_start(&env->prepare, on_prepare);
      assert(err == 0);

      return;
    }

    env->idle();

    env->check_liveness();
  }

  static void
  on_teardown(uv_async_t *handle) {
    auto env = reinterpret_cast<js_env_t *>(handle->data);

    if (env->refs == 0) env->close();
  }

  static void
  on_handle_close(uv_handle_t *handle) {
    auto env = reinterpret_cast<js_env_t *>(handle->data);

    env->active_handles--;

    env->dispose_maybe();
  }
};

struct js_context_s {
  Global<Context> context;

  js_context_s(js_env_t *env)
      : context() {
    auto parent_context = env->context.Get(env->isolate);

    context.Reset(env->isolate, Context::New(env->isolate));

    context.Get(env->isolate)->SetSecurityToken(parent_context->GetSecurityToken());
  }

  js_context_s(const js_context_s &) = delete;

  js_context_s &
  operator=(const js_context_s &) = delete;
};

struct js_handle_scope_s {
  HandleScope scope;

  js_handle_scope_s(Isolate *isolate)
      : scope(isolate) {}

  js_handle_scope_s(const js_handle_scope_s &) = delete;

  js_handle_scope_s &
  operator=(const js_handle_scope_s &) = delete;
};

struct js_escapable_handle_scope_s {
  EscapableHandleScope scope;

  js_escapable_handle_scope_s(Isolate *isolate)
      : scope(isolate) {}

  js_escapable_handle_scope_s(const js_escapable_handle_scope_s &) = delete;

  js_escapable_handle_scope_s &
  operator=(const js_escapable_handle_scope_s &) = delete;
};

struct js_module_s {
  Global<Module> module;

  std::string name;

  struct {
    js_module_resolve_cb resolve;
    void *resolve_data;

    js_module_meta_cb meta;
    void *meta_data;

    js_module_evaluate_cb evaluate;
    void *evaluate_data;
  } callbacks;

  js_module_s(Isolate *isolate, Local<Module> module, std::string name)
      : module(isolate, module),
        name(std::move(name)),
        callbacks() {}

  js_module_s(const js_module_s &) = delete;

  js_module_s &
  operator=(const js_module_s &) = delete;

  static inline js_module_t *
  from_local(Local<Context> context, Local<Module> local) {
    auto env = js_env_t::from(Isolate::GetCurrent());

    auto range = env->modules.equal_range(local->GetIdentityHash());

    for (auto it = range.first; it != range.second; ++it) {
      if (it->second->module == local) {
        return it->second;
      }
    }

    return nullptr;
  }

  static MaybeLocal<Module>
  on_resolve(Local<Context> context, Local<String> specifier, Local<FixedArray> raw_assertions, Local<Module> referrer) {
    auto env = js_env_t::from(Isolate::GetCurrent());

    auto module = js_module_t::from_local(context, referrer);

    auto assertions = Object::New(env->isolate, Null(env->isolate), nullptr, nullptr, 0);

    for (int i = 0; i < raw_assertions->Length(); i += 3) {
      assertions
        ->Set(
          context,
          raw_assertions->Get(i).As<String>(),
          raw_assertions->Get(i + 1).As<Value>()
        )
        .Check();
    }

    auto result = module->callbacks.resolve(
      env,
      js_from_local(specifier),
      js_from_local(assertions),
      module,
      module->callbacks.resolve_data
    );

    if (env->exception.IsEmpty()) {
      if (result->callbacks.resolve == nullptr) {
        result->callbacks.resolve = module->callbacks.resolve;
        result->callbacks.resolve_data = module->callbacks.resolve_data;
      }

      return result->module.Get(env->isolate);
    }

    auto error = env->exception.Get(env->isolate);

    env->exception.Reset();

    env->isolate->ThrowException(error);

    return MaybeLocal<Module>();
  }

  static MaybeLocal<Value>
  on_evaluate(Local<Context> context, Local<Module> referrer) {
    auto env = js_env_t::from(Isolate::GetCurrent());

    auto module = js_module_t::from_local(context, referrer);

    module->callbacks.evaluate(env, module, module->callbacks.evaluate_data);

    if (env->exception.IsEmpty()) {
      auto resolver = Promise::Resolver::New(context).ToLocalChecked();

      auto success = resolver->Resolve(context, Undefined(env->isolate));

      success.Check();

      return resolver->GetPromise();
    }

    auto error = env->exception.Get(env->isolate);

    env->exception.Reset();

    env->isolate->ThrowException(error);

    return MaybeLocal<Value>();
  }

  static MaybeLocal<Promise>
  on_dynamic_import(Local<Context> context, Local<Data> data, Local<Value> referrer, Local<String> specifier, Local<FixedArray> raw_assertions) {
    int err;

    auto env = js_env_t::from(Isolate::GetCurrent());

    if (env->callbacks.dynamic_import == nullptr && env->callbacks.dynamic_import_transitional == nullptr) {
      err = js_throw_error(env, nullptr, "Dynamic import() is not supported");
      assert(err == 0);

      return MaybeLocal<Promise>();
    }

    auto assertions = Object::New(env->isolate, Null(env->isolate), nullptr, nullptr, 0);

    for (int i = 0; i < raw_assertions->Length(); i += 3) {
      assertions
        ->Set(
          context,
          raw_assertions->Get(i).As<String>(),
          raw_assertions->Get(i + 1).As<Value>()
        )
        .Check();
    }

    if (env->callbacks.dynamic_import) {
      auto result = env->callbacks.dynamic_import(
        env,
        js_from_local(specifier),
        js_from_local(assertions),
        js_from_local(referrer),
        env->callbacks.dynamic_import_data
      );

      if (env->exception.IsEmpty()) {
        auto module = result->module.Get(env->isolate);

        auto resolver = Promise::Resolver::New(context).ToLocalChecked();

        auto success = resolver->Resolve(context, module->GetModuleNamespace());

        success.Check();

        return resolver->GetPromise();
      }
    } else {
      auto result = env->callbacks.dynamic_import_transitional(
        env,
        js_from_local(specifier),
        js_from_local(assertions),
        js_from_local(referrer),
        env->callbacks.dynamic_import_data
      );

      if (env->exception.IsEmpty()) {
        auto local = js_to_local(result);

        if (local->IsPromise()) return local.As<Promise>();

        auto resolver = Promise::Resolver::New(context).ToLocalChecked();

        auto success = resolver->Resolve(context, local);

        success.Check();

        return resolver->GetPromise();
      }
    }

    auto error = env->exception.Get(env->isolate);

    env->exception.Reset();

    env->isolate->ThrowException(error);

    return MaybeLocal<Promise>();
  }

  static void
  on_import_meta(Local<Context> context, Local<Module> local, Local<Object> meta) {
    auto env = js_env_t::from(Isolate::GetCurrent());

    auto module = js_module_t::from_local(context, local);

    if (module->callbacks.meta == nullptr) return;

    module->callbacks.meta(
      env,
      module,
      js_from_local(meta),
      module->callbacks.meta_data
    );

    if (env->exception.IsEmpty()) return;

    auto error = env->exception.Get(env->isolate);

    env->exception.Reset();

    env->isolate->ThrowException(error);
  }
};

struct js_ref_s {
  Global<Value> value;
  uint32_t count;

  js_ref_s(Isolate *isolate, Local<Value> value, uint32_t count)
      : value(isolate, value),
        count(count) {}

  js_ref_s(const js_ref_s &) = delete;

  js_ref_s &
  operator=(const js_ref_s &) = delete;

  void
  set_weak() {
    value.SetWeak(this, on_finalize, WeakCallbackType::kParameter);
  }

  void
  clear_weak() {
    value.ClearWeak<js_ref_t>();
  }

private:
  static void
  on_finalize(const WeakCallbackInfo<js_ref_t> &info) {
    auto reference = info.GetParameter();

    reference->value.Reset();
  }
};

struct js_deferred_s {
  Global<Promise::Resolver> resolver;

  js_deferred_s(Isolate *isolate, Local<Promise::Resolver> resolver)
      : resolver(isolate, resolver) {}

  js_deferred_s(const js_deferred_s &) = delete;

  js_deferred_s &
  operator=(const js_deferred_s &) = delete;
};

struct js_callback_s {
  Global<External> external;
  js_env_t *env;
  js_function_cb cb;
  void *data;

  js_callback_s(js_env_t *env, js_function_cb cb, void *data)
      : external(env->isolate, External::New(env->isolate, this, js_callback_info_type_tag)),
        env(env),
        cb(cb),
        data(data) {
    external.SetWeak(this, on_finalize, WeakCallbackType::kParameter);
  }

  js_callback_s(const js_callback_s &) = delete;

  virtual ~js_callback_s() = default;

  js_callback_s &
  operator=(const js_callback_s &) = delete;

  MaybeLocal<Function>
  to_function(Isolate *isolate, Local<Context> context) {
    return Function::New(
      context,
      on_call,
      external.Get(isolate),
      0,
      ConstructorBehavior::kAllow,
      SideEffectType::kHasSideEffect
    );
  }

  Local<FunctionTemplate>
  to_function_template(Isolate *isolate, Local<Signature> signature = Local<Signature>()) {
    return FunctionTemplate::New(
      isolate,
      on_call,
      external.Get(isolate),
      signature,
      0,
      ConstructorBehavior::kAllow,
      SideEffectType::kHasSideEffect
    );
  }

protected:
  static void
  on_call(const FunctionCallbackInfo<Value> &info) {
    auto callback = reinterpret_cast<js_callback_t *>(info.Data().As<External>()->Value(js_callback_info_type_tag));

    auto env = callback->env;

    auto result = callback->cb(env, reinterpret_cast<js_callback_info_t *>(const_cast<FunctionCallbackInfo<Value> *>(&info)));

    if (env->exception.IsEmpty()) {
      if (result) {
        info.GetReturnValue().Set(js_to_local(result));
      }
    } else {
      auto error = env->exception.Get(env->isolate);

      env->exception.Reset();

      env->isolate->ThrowException(error);
    }
  }

  static void
  on_finalize(const WeakCallbackInfo<js_callback_t> &info) {
    auto callback = info.GetParameter();

    delete callback;
  }
};

struct js_typed_callback_s : js_callback_t {
  CTypeInfo result;
  std::vector<CTypeInfo> args;
  CFunctionInfo type;
  const void *address;

  js_typed_callback_s(js_env_t *env, js_function_cb cb, void *data, CTypeInfo result, std::vector<CTypeInfo> args, const void *address, CFunctionInfo::Int64Representation integer_representation)
      : js_callback_t(env, cb, data),
        result(std::move(result)),
        args(std::move(args)),
        type(this->result, uint16_t(this->args.size()), this->args.data(), integer_representation),
        address(address) {}

  js_typed_callback_s(const js_typed_callback_s &) = delete;

  js_typed_callback_s &
  operator=(const js_typed_callback_s &) = delete;

  Local<FunctionTemplate>
  to_function_template(Isolate *isolate, Local<Signature> signature = Local<Signature>()) {
    auto function = CFunction(address, &type);

    return FunctionTemplate::New(
      isolate,
      on_call,
      external.Get(isolate),
      signature,
      0,
      ConstructorBehavior::kThrow,
      SideEffectType::kHasNoSideEffect,
      &function
    );
  }
};

struct js_finalizer_s {
  Global<Value> value;
  js_env_t *env;
  void *data;
  js_finalize_cb finalize_cb;
  void *finalize_hint;

  js_finalizer_s(js_env_t *env, void *data, js_finalize_cb finalize_cb, void *finalize_hint)
      : value(),
        env(env),
        data(data),
        finalize_cb(finalize_cb),
        finalize_hint(finalize_hint) {}

  js_finalizer_s(const js_finalizer_s &) = delete;

  virtual ~js_finalizer_s() = default;

  js_finalizer_s &
  operator=(const js_finalizer_s &) = delete;

  void
  attach_to(Isolate *isolate, Local<Value> local) {
    value.Reset(isolate, local);

    value.SetWeak(this, on_finalize, WeakCallbackType::kParameter);
  }

  void
  detach() {
    value.ClearWeak<js_finalizer_t>();
  }

private:
  static void
  on_finalize(const WeakCallbackInfo<js_finalizer_t> &info) {
    auto finalizer = info.GetParameter();

    finalizer->value.Reset();

    if (finalizer->finalize_cb) {
      info.SetSecondPassCallback(on_second_pass_finalize);
    } else {
      delete finalizer;
    }
  }

  static void
  on_second_pass_finalize(const WeakCallbackInfo<js_finalizer_t> &info) {
    auto finalizer = info.GetParameter();

    finalizer->finalize_cb(finalizer->env, finalizer->data, finalizer->finalize_hint);

    delete finalizer;
  }
};

struct js_delegate_s : js_finalizer_t {
  js_delegate_callbacks_t callbacks;

  js_delegate_s(js_env_t *env, js_delegate_callbacks_t callbacks, void *data, js_finalize_cb finalize_cb, void *finalize_hint)
      : js_finalizer_t(env, data, finalize_cb, finalize_hint),
        callbacks(std::move(callbacks)) {}

  js_delegate_s(const js_delegate_s &) = delete;

  js_delegate_s &
  operator=(const js_delegate_s &) = delete;

  Local<ObjectTemplate>
  to_object_template(Isolate *isolate) {
    auto external = External::New(isolate, this, js_delegate_type_tag);

    auto tpl = ObjectTemplate::New(isolate);

    tpl->SetHandler(NamedPropertyHandlerConfiguration(
      on_get,
      on_set,
      nullptr,
      on_delete,
      on_enumerate,
      nullptr,
      nullptr,
      external
    ));

    tpl->SetHandler(IndexedPropertyHandlerConfiguration(
      on_get,
      on_set,
      nullptr,
      on_delete,
      nullptr,
      nullptr,
      nullptr,
      external
    ));

    return tpl;
  }

private:
  template <typename T>
  static Intercepted
  on_get(Local<T> property, const PropertyCallbackInfo<Value> &info) {
    auto env = js_env_t::from(info.GetIsolate());

    auto delegate = static_cast<js_delegate_t *>(info.Data().As<External>()->Value(js_delegate_type_tag));

    if (delegate->callbacks.has) {
      auto exists = delegate->callbacks.has(env, js_from_local(property), delegate->data);

      if (env->is_exception_pending()) return Intercepted::kNo;

      if (!exists) return Intercepted::kYes;
    }

    if (delegate->callbacks.get) {
      auto result = delegate->callbacks.get(env, js_from_local(property), delegate->data);

      if (env->is_exception_pending()) return Intercepted::kNo;

      if (result) {
        info.GetReturnValue().Set(js_to_local(result));

        return Intercepted::kYes;
      }
    }

    return Intercepted::kNo;
  }

  static Intercepted
  on_get(uint32_t index, const PropertyCallbackInfo<Value> &info) {
    auto isolate = info.GetIsolate();

    auto property = Int32::NewFromUnsigned(isolate, index);

    return on_get(property, info);
  }

  template <typename T>
  static Intercepted
  on_set(Local<T> property, Local<Value> value, const PropertyCallbackInfo<void> &info) {
    auto env = js_env_t::from(info.GetIsolate());

    auto delegate = static_cast<js_delegate_t *>(info.Data().As<External>()->Value(js_delegate_type_tag));

    if (delegate->callbacks.set) {
      auto result = delegate->callbacks.set(env, js_from_local(property), js_from_local(value), delegate->data);

      if (env->is_exception_pending()) return Intercepted::kNo;

      if (result) return Intercepted::kYes;
    }

    return Intercepted::kNo;
  }

  static Intercepted
  on_set(uint32_t index, Local<Value> value, const PropertyCallbackInfo<void> &info) {
    auto isolate = info.GetIsolate();

    auto property = Int32::NewFromUnsigned(isolate, index);

    return on_set(property, value, info);
  }

  template <typename T>
  static Intercepted
  on_delete(Local<T> property, const PropertyCallbackInfo<Boolean> &info) {
    auto env = js_env_t::from(info.GetIsolate());

    auto delegate = static_cast<js_delegate_t *>(info.Data().As<External>()->Value(js_delegate_type_tag));

    if (delegate->callbacks.delete_property) {
      auto result = delegate->callbacks.delete_property(env, js_from_local(property), delegate->data);

      if (env->is_exception_pending()) return Intercepted::kNo;

      if (result) {
        info.GetReturnValue().Set(true);

        return Intercepted::kYes;
      }
    }

    return Intercepted::kNo;
  }

  static Intercepted
  on_delete(uint32_t index, const PropertyCallbackInfo<Boolean> &info) {
    auto isolate = info.GetIsolate();

    auto property = Int32::NewFromUnsigned(isolate, index);

    return on_delete(property, info);
  }

  static void
  on_enumerate(const PropertyCallbackInfo<Array> &info) {
    auto env = js_env_t::from(info.GetIsolate());

    auto delegate = static_cast<js_delegate_t *>(info.Data().As<External>()->Value(js_delegate_type_tag));

    if (delegate->callbacks.own_keys) {
      auto result = delegate->callbacks.own_keys(env, delegate->data);

      if (env->is_exception_pending()) return;

      if (result) {
        info.GetReturnValue().Set(js_to_local<Array>(result));
      }
    }
  }
};

struct js_external_string_utf16le_s : String::ExternalStringResource {
  js_env_t *env;

  utf16_t *str;
  size_t len;

  js_finalize_cb finalize_cb;
  void *finalize_hint;

  js_external_string_utf16le_s(js_env_t *env, utf16_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint)
      : env(env),
        str(str),
        len(len),
        finalize_cb(finalize_cb),
        finalize_hint(finalize_hint) {}

  js_external_string_utf16le_s(const js_external_string_utf16le_s &) = delete;

  ~js_external_string_utf16le_s() {
    if (finalize_cb) finalize_cb(env, str, finalize_hint);
  }

  js_external_string_utf16le_s &
  operator=(const js_external_string_utf16le_s &) = delete;

  const uint16_t *
  data() const override {
    return str;
  };

  size_t
  length() const override {
    return len;
  }
};

struct js_external_string_latin1_s : String::ExternalOneByteStringResource {
  js_env_t *env;

  latin1_t *str;
  size_t len;

  js_finalize_cb finalize_cb;
  void *finalize_hint;

  js_external_string_latin1_s(js_env_t *env, latin1_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint)
      : env(env),
        str(str),
        len(len),
        finalize_cb(finalize_cb),
        finalize_hint(finalize_hint) {}

  js_external_string_latin1_s(const js_external_string_latin1_s &) = delete;

  ~js_external_string_latin1_s() {
    if (finalize_cb) finalize_cb(env, str, finalize_hint);
  }

  js_external_string_latin1_s &
  operator=(const js_external_string_latin1_s &) = delete;

  const char *
  data() const override {
    return reinterpret_cast<char *>(str);
  };

  size_t
  length() const override {
    return len;
  }
};

struct js_arraybuffer_backing_store_s {
  std::shared_ptr<BackingStore> backing_store;

  js_arraybuffer_backing_store_s(std::shared_ptr<BackingStore> backing_store)
      : backing_store(std::move(backing_store)) {}

  js_arraybuffer_backing_store_s(const js_arraybuffer_backing_store_s &) = delete;

  js_arraybuffer_backing_store_s &
  operator=(const js_arraybuffer_backing_store_s &) = delete;
};

struct js_threadsafe_queue_s {
  virtual ~js_threadsafe_queue_s() = default;

  virtual bool
  push(void *data, js_threadsafe_function_call_mode_t mode) = 0;

  virtual std::optional<void *>
  pop() = 0;

  virtual void
  close() = 0;
};

struct js_threadsafe_unbounded_queue_s : js_threadsafe_queue_t {
  std::queue<void *> queue;

  bool closed;

  std::mutex lock;

  js_threadsafe_unbounded_queue_s()
      : queue(),
        closed(false),
        lock() {}

  bool
  push(void *data, js_threadsafe_function_call_mode_t mode) override {
    std::unique_lock guard(lock);

    if (closed) return false;

    queue.push(data);

    return true;
  }

  std::optional<void *>
  pop() override {
    std::unique_lock guard(lock);

    if (queue.empty()) return std::nullopt;

    auto data = queue.front();

    queue.pop();

    return data;
  }

  void
  close() override {
    std::unique_lock guard(lock);

    closed = true;
  }
};

struct js_threadsafe_bounded_queue_s : js_threadsafe_queue_t {
  std::vector<void *> queue;

  const size_t mask;

  size_t read;
  size_t write;

  bool closed;

  std::mutex lock;
  std::condition_variable available;

  js_threadsafe_bounded_queue_s(size_t queue_limit)
      : queue(queue_limit),
        mask(queue_limit - 1),
        read(0),
        write(0),
        closed(false),
        lock(),
        available() {
    assert(std::has_single_bit(queue_limit));
  }

  bool
  push(void *data, js_threadsafe_function_call_mode_t mode) override {
    std::unique_lock guard(lock);

    while (true) {
      if (closed) return false;

      auto next = (write + 1) & mask;

      if (next != read) {
        queue[write] = data;
        write = next;

        return true;
      }

      if (mode == js_threadsafe_function_nonblocking) {
        return false;
      }

      available.wait(guard);
    }
  }

  std::optional<void *>
  pop() override {
    std::unique_lock guard(lock);

    if (read == write) return std::nullopt;

    auto data = queue[read];

    read = (read + 1) & mask;

    available.notify_one();

    return data;
  }

  void
  close() override {
    std::unique_lock guard(lock);

    closed = true;

    available.notify_all();
  }
};

namespace {

static const uint8_t js_threadsafe_function_idle = 0x0;
static const uint8_t js_threadsafe_function_running = 0x1;
static const uint8_t js_threadsafe_function_pending = 0x2;

} // namespace

struct js_threadsafe_function_s {
  Global<Value> function;
  js_env_t *env;

  uv_async_t async;

  std::unique_ptr<js_threadsafe_queue_t> queue;

  std::atomic<uint8_t> state;
  std::atomic<size_t> thread_count;

  void *context;
  js_finalize_cb finalize_cb;
  void *finalize_hint;

  js_threadsafe_function_cb cb;

  js_threadsafe_function_s(js_env_t *env, size_t queue_limit, size_t thread_count, js_threadsafe_function_cb cb, void *context, js_finalize_cb finalize_cb, void *finalize_hint)
      : function(),
        env(env),
        async(),
        queue(),
        state(js_threadsafe_function_idle),
        thread_count(thread_count),
        context(context),
        finalize_cb(finalize_cb),
        finalize_hint(finalize_hint),
        cb(cb) {
    int err;

    err = uv_async_init(env->loop, &async, on_async);
    assert(err == 0);

    async.data = this;

    if (queue_limit) {
      queue.reset(new js_threadsafe_bounded_queue_t(std::bit_ceil(queue_limit)));
    } else {
      queue.reset(new js_threadsafe_unbounded_queue_t());
    }
  }

  bool
  push(void *data, js_threadsafe_function_call_mode_t mode) {
    if (thread_count.load(std::memory_order_relaxed) == 0) return false;

    if (queue->push(data, mode)) {
      signal();

      return true;
    }

    return false;
  }

  bool
  acquire() {
    auto thread_count = this->thread_count.load(std::memory_order_relaxed);

    while (thread_count != 0) {
      if (
        this->thread_count.compare_exchange_weak(
          thread_count,
          thread_count + 1,
          std::memory_order_acquire,
          std::memory_order_relaxed
        )
      ) {
        return true;
      }
    }

    return false;
  }

  bool
  release(js_threadsafe_function_release_mode_t mode) {
    auto thread_count = this->thread_count.load(std::memory_order_relaxed);

    auto abort = mode == js_threadsafe_function_abort;

    while (thread_count != 0) {
      if (
        this->thread_count.compare_exchange_weak(
          thread_count,
          abort ? 0 : thread_count - 1,
          std::memory_order_acquire,
          std::memory_order_relaxed
        )
      ) {
        if (abort || thread_count == 1) {
          queue->close();

          signal();
        }

        return true;
      }
    }

    return false;
  }

  void
  ref() {
    uv_ref(reinterpret_cast<uv_handle_t *>(&async));
  }

  void
  unref() {
    uv_unref(reinterpret_cast<uv_handle_t *>(&async));
  }

private:
  void
  close() {
    uv_close(reinterpret_cast<uv_handle_t *>(&async), on_close);
  }

  bool
  call() {
    int err;

    auto data = queue->pop();

    if (data.has_value()) {
      auto scope = HandleScope(env->isolate);

      auto fn = function.IsEmpty() ? nullptr : js_from_local(function.Get(env->isolate));

      if (cb) cb(env, fn, context, data.value());
      else {
        js_value_t *receiver;
        err = js_get_undefined(env, &receiver);
        assert(err == 0);

        err = js_call_function(env, receiver, fn, 0, nullptr, nullptr);
        (void) err;
      }

      return true;
    }

    if (thread_count.load(std::memory_order_relaxed) == 0) {
      close();
    }

    return false;
  }

  void
  signal() {
    int err;

    auto state = this->state.fetch_or(js_threadsafe_function_pending, std::memory_order_acq_rel);

    if (state & js_threadsafe_function_running) {
      return;
    }

    err = uv_async_send(&async);
    assert(err == 0);
  }

  void
  dispatch() {
    auto done = false;

    auto iterations = 1024;

    while (!done && --iterations >= 0) {
      state.store(js_threadsafe_function_running, std::memory_order_release);

      done = call() == false;

      if (state.exchange(js_threadsafe_function_idle, std::memory_order_acq_rel) != js_threadsafe_function_running) {
        done = false;
      }
    }

    if (!done) signal();
  }

private:
  static void
  on_async(uv_async_t *handle) {
    auto function = reinterpret_cast<js_threadsafe_function_t *>(handle->data);

    function->dispatch();
  }

  static void
  on_close(uv_handle_t *handle) {
    auto function = reinterpret_cast<js_threadsafe_function_t *>(handle->data);

    if (function->finalize_cb) function->finalize_cb(function->env, function->context, function->finalize_hint);

    delete function;
  }
};

struct js_deferred_teardown_s {
  js_env_t *env;
  js_deferred_teardown_cb cb;
  void *data;

  js_deferred_teardown_s(js_env_t *env, js_deferred_teardown_cb cb, void *data)
      : env(env),
        cb(cb),
        data(data) {
    env->ref();
  }

  ~js_deferred_teardown_s() {
    env->unref();
  }
};

struct js_inspector_channel_s : public V8Inspector::Channel {
  js_env_t *env;
  js_inspector_t *inspector;
  js_inspector_message_cb cb;
  js_inspector_message_transitional_cb cb_transitional;
  void *data;

  js_inspector_channel_s(js_env_t *env, js_inspector_t *inspector)
      : env(env),
        inspector(inspector),
        cb(),
        data() {}

  js_inspector_channel_s(const js_inspector_channel_s &) = delete;

  js_inspector_channel_s &
  operator=(const js_inspector_channel_s &) = delete;

private: // V8 embedder API
  void
  send(const StringView &string) {
    if (cb == nullptr && cb_transitional == nullptr) return;

    if (cb) {
      auto len = string.length();

      auto scope = HandleScope(env->isolate);

      auto message =
        (string.is8Bit()
           ? String::NewFromOneByte(
               env->isolate,
               reinterpret_cast<const uint8_t *>(string.characters8()),
               v8::NewStringType::kNormal,
               int(len)
             )
           : String::NewFromTwoByte(
               env->isolate,
               reinterpret_cast<const uint16_t *>(string.characters16()),
               v8::NewStringType::kNormal,
               int(len)
             ))
          .ToLocalChecked();

      cb(env, inspector, js_from_local(message), data);
    } else {
      std::vector<utf8_t> utf8;

      if (string.is8Bit()) {
        auto utf8_len = utf8_length_from_latin1(reinterpret_cast<const latin1_t *>(string.characters8()), string.length());

        utf8 = std::vector<utf8_t>(utf8_len + 1 /* NULL */);

        latin1_convert_to_utf8(reinterpret_cast<const latin1_t *>(string.characters8()), string.length(), utf8.data());
      } else {
        auto utf8_len = utf8_length_from_utf16le(reinterpret_cast<const utf16_t *>(string.characters16()), string.length());

        utf8 = std::vector<utf8_t>(utf8_len + 1 /* NULL */);

        utf16le_convert_to_utf8(reinterpret_cast<const utf16_t *>(string.characters16()), string.length(), utf8.data());
      }

      cb_transitional(env, inspector, reinterpret_cast<char *>(utf8.data()), utf8.size() - 1 /* NULL */, data);
    }
  }

  void
  sendResponse(int callId, std::unique_ptr<StringBuffer> message) override {
    send(message->string());
  }

  void
  sendNotification(std::unique_ptr<StringBuffer> message) override {
    send(message->string());
  }

  void
  flushProtocolNotifications() override {}
};

struct js_inspector_client_s : public V8InspectorClient {
  js_env_t *env;

  bool paused;

  std::list<js_inspector_t *> sessions;

  std::unique_ptr<V8Inspector> inspector;

  js_inspector_client_s(js_env_t *env)
      : env(env),
        paused(),
        sessions(),
        inspector(V8Inspector::create(env->isolate, this)) {
    inspector->contextCreated(V8ContextInfo(env->context.Get(env->isolate), 1, StringView()));
  }

private:
  static bool
  on_pause(js_inspector_t *session);

private: // V8 embedder API
  v8::Local<v8::Context>
  ensureDefaultContextInGroup(int contextGroupId) override {
    return env->context.Get(env->isolate);
  }

  void
  runMessageLoopOnPause(int contextGroupId) override {
    if (paused || sessions.empty()) return;

    paused = true;

    while (paused) {
      for (auto &session : sessions) {
        paused = on_pause(session) && paused;
      }

      env->run_macrotasks();
    }

    paused = false;
  }

  void
  quitMessageLoopOnPause() override {
    paused = false;
  }
};

struct js_inspector_s {
  js_env_t *env;
  js_inspector_channel_t channel;
  js_inspector_paused_cb cb;

  void *data;

  std::shared_ptr<js_inspector_client_t> client;

  std::unique_ptr<V8InspectorSession> session;

  bool paused;

  js_inspector_s(js_env_t *env)
      : env(env),
        channel(env, this),
        cb(),
        data(),
        session(),
        paused(false) {
    if (env->inspector == nullptr) env->inspector = std::make_shared<js_inspector_client_t>(env);

    client = env->inspector;

    client->sessions.push_back(this);
  }

  ~js_inspector_s() {
    client->sessions.remove(this);

    if (client->sessions.empty()) env->inspector = nullptr;
  }

  js_inspector_s(const js_inspector_s &) = delete;

  js_inspector_s &
  operator=(const js_inspector_s &) = delete;

  void
  connect() {
    session = client->inspector->connect(
      1,
      &channel,
      StringView(),
      V8Inspector::kFullyTrusted,
      V8Inspector::kNotWaitingForDebugger
    );
  }

  void
  attach(Local<Context> context, StringView name = StringView()) {
    client->inspector->contextCreated(V8ContextInfo(context, 1, name));
  }

  void
  detach(Local<Context> context) {
    client->inspector->contextDestroyed(context);
  }

  void
  send(Local<String> message) {
    auto utf16_len = size_t(message->Length());

    auto utf16 = std::vector<uint16_t>(utf16_len);

    message->WriteV2(env->isolate, 0, uint32_t(utf16_len), utf16.data());

    session->dispatchProtocolMessage(StringView(utf16.data(), utf16_len));
  }

  void
  send(const char *message, size_t len) {
    if (len == size_t(-1)) len = strlen(message);

    auto utf16_len = utf16_length_from_utf8(reinterpret_cast<const utf8_t *>(message), len);

    auto utf16 = std::vector<utf16_t>(utf16_len);

    utf8_convert_to_utf16le(reinterpret_cast<const utf8_t *>(message), len, utf16.data());

    session->dispatchProtocolMessage(StringView(utf16.data(), utf16_len));
  }
};

bool
js_inspector_client_s::on_pause(js_inspector_t *session) {
  if (session->cb == nullptr) return false;

  return session->cb(session->env, session, session->data);
}

struct js_garbage_collection_tracking_s {
  js_garbage_collection_tracking_options_t options;

  void *data;

  js_garbage_collection_tracking_s(js_garbage_collection_tracking_options_t options, void *data)
      : options(options),
        data(data) {}
};

namespace {

template <int N>
static inline Local<String>
js_to_string_utf8_literal(js_env_t *env, const char (&literal)[N], bool internalize = false) {
  auto type = internalize ? NewStringType::kInternalized : NewStringType::kNormal;

  return String::NewFromUtf8Literal(env->isolate, literal, type);
}

static inline MaybeLocal<String>
js_to_string_utf8(js_env_t *env, const char *data, int len = -1, bool internalize = false) {
  auto type = internalize ? NewStringType::kInternalized : NewStringType::kNormal;

  return String::NewFromUtf8(env->isolate, data, type, len);
}

static inline MaybeLocal<String>
js_to_string_utf8(js_env_t *env, const char *data, size_t len, bool internalize = false) {
  if (len == size_t(-1)) return js_to_string_utf8(env, data, -1, internalize);

  int err;

  if (len > INT_MAX) {
    err = js_throw_range_error(env, NULL, "Invalid string length");
    assert(err == 0);

    return MaybeLocal<String>();
  }

  return js_to_string_utf8(env, data, static_cast<int>(len), internalize);
}

static inline MaybeLocal<String>
js_to_string_utf8(js_env_t *env, const utf8_t *data, size_t len, bool internalize = false) {
  return js_to_string_utf8(env, reinterpret_cast<const char *>(data), len, internalize);
}

static inline MaybeLocal<String>
js_to_string_utf16le(js_env_t *env, const utf16_t *data, int len = -1, bool internalize = false) {
  auto type = internalize ? NewStringType::kInternalized : NewStringType::kNormal;

  return String::NewFromTwoByte(env->isolate, data, type, len);
}

static inline MaybeLocal<String>
js_to_string_utf16le(js_env_t *env, const utf16_t *data, size_t len, bool internalize = false) {
  if (len == size_t(-1)) return js_to_string_utf16le(env, data, -1, internalize);

  int err;

  if (len > INT_MAX) {
    err = js_throw_range_error(env, NULL, "Invalid string length");
    assert(err == 0);

    return MaybeLocal<String>();
  }

  return js_to_string_utf16le(env, data, static_cast<int>(len), internalize);
}

static inline MaybeLocal<String>
js_to_string_latin1(js_env_t *env, const latin1_t *data, int len = -1, bool internalize = false) {
  auto type = internalize ? NewStringType::kInternalized : NewStringType::kNormal;

  return String::NewFromOneByte(env->isolate, data, type, len);
}

static inline MaybeLocal<String>
js_to_string_latin1(js_env_t *env, const latin1_t *data, size_t len, bool internalize = false) {
  if (len == size_t(-1)) return js_to_string_latin1(env, data, -1, internalize);

  int err;

  if (len > INT_MAX) {
    err = js_throw_range_error(env, NULL, "Invalid string length");
    assert(err == 0);

    return MaybeLocal<String>();
  }

  return js_to_string_latin1(env, data, static_cast<int>(len), internalize);
}

} // namespace

namespace {

static const char *js_platform_identifier = "v8";

static const char *js_platform_version = V8::GetVersion();

static const js_platform_options_t js_platform_default_options = {
  .version = 1,
};

template <auto js_platform_options_t::*P, typename T>
static inline T
js_option(const js_platform_options_t *options, int min_version, T fallback = T(js_platform_default_options.*P)) {
  return T(options && options->version >= min_version ? options->*P : fallback);
}

} // namespace

extern "C" int
js_create_platform(uv_loop_t *loop, const js_platform_options_t *options, js_platform_t **result) {
  auto flags = std::string();

  // Don't freeze the flags after initialising the platform. This is both not
  // needed and also ensures that V8 doesn't attempt to call `mprotect()`, which
  // isn't allowed on iOS in unprivileged processes.
  flags += "--no-freeze-flags-after-init";

  if (js_option<&js_platform_options_t::expose_garbage_collection, bool>(options, 0)) {
    flags += " --expose-gc";
  }

  if (js_option<&js_platform_options_t::trace_garbage_collection, bool>(options, 0)) {
    flags += " --trace-gc";
  }

  if (js_option<&js_platform_options_t::optimize_for_memory, bool>(options, 1)) {
    flags += " --lite-mode";
  } else if (js_option<&js_platform_options_t::disable_optimizing_compiler, bool>(options, 0)) {
    flags += " --jitless";
  } else {
    if (js_option<&js_platform_options_t::trace_optimizations, bool>(options, 0)) {
      flags += " --trace-opt";
    }

    if (js_option<&js_platform_options_t::trace_deoptimizations, bool>(options, 0)) {
      flags += " --trace-deopt";
    }
  }

  if (js_option<&js_platform_options_t::enable_sampling_profiler, bool>(options, 0)) {
    flags += " --prof";

    auto interval = js_option<&js_platform_options_t::sampling_profiler_interval, int>(options, 0);

    if (interval > 0) {
      flags += " --prof_sampling_interval=" + std::to_string(interval);
    }
  }

  V8::SetFlagsFromString(flags.c_str());

  *result = new js_platform_t(options ? *options : js_platform_options_t(), loop);

  return 0;
}

extern "C" int
js_destroy_platform(js_platform_t *platform) {
  platform->close();

  return 0;
}

extern "C" int
js_get_platform_identifier(js_platform_t *platform, const char **result) {
  *result = js_platform_identifier;

  return 0;
}

extern "C" int
js_get_platform_version(js_platform_t *platform, const char **result) {
  *result = js_platform_version;

  return 0;
}

extern "C" int
js_get_platform_limits(js_platform_t *platform, js_platform_limits_t *result) {
  result->arraybuffer_length = ArrayBuffer::kMaxByteLength;
  result->string_length = String::kMaxLength;

  return 0;
}

extern "C" int
js_get_platform_loop(js_platform_t *platform, uv_loop_t **result) {
  *result = platform->loop;

  return 0;
}

namespace {

static const js_env_options_t js_env_default_options = {
  .version = 0,
};

template <auto js_env_options_t::*P, typename T>
static inline T
js_option(const js_env_options_t *options, int min_version, T fallback = T(js_env_default_options.*P)) {
  return T(options && options->version >= min_version ? options->*P : fallback);
}

} // namespace

extern "C" int
js_create_env(uv_loop_t *loop, js_platform_t *platform, const js_env_options_t *options, js_env_t **result) {
  Isolate::CreateParams params;

  params.array_buffer_allocator_shared = std::make_shared<js_allocator_t>();

  auto memory_limit = js_option<&js_env_options_t::memory_limit, size_t>(options, 0);

  if (memory_limit > 0) {
    params.constraints.ConfigureDefaultsFromHeapSize(0, memory_limit);
  } else {
    auto constrained_memory = uv_get_constrained_memory();
    auto total_memory = uv_get_total_memory();

    if (constrained_memory > 0 && constrained_memory < total_memory) {
      total_memory = constrained_memory;
    }

    if (total_memory > 0) {
      params.constraints.ConfigureDefaults(total_memory, 0);
    }
  }

  auto isolate = Isolate::Allocate();

  auto tasks = new js_task_runner_t(loop);

  std::unique_lock guard(platform->lock);

  platform->foreground.emplace(isolate, std::move(tasks));

  guard.unlock();

  Isolate::Initialize(isolate, params);

  isolate->Enter();

  isolate->SetMicrotasksPolicy(MicrotasksPolicy::kExplicit);

  isolate->AddMessageListener(js_env_t::on_uncaught_exception);

  isolate->SetPromiseRejectCallback(js_env_t::on_promise_reject);

  isolate->SetHostImportModuleDynamicallyCallback(js_module_t::on_dynamic_import);

  isolate->SetHostInitializeImportMetaObjectCallback(js_module_t::on_import_meta);

  auto env = new js_env_t(loop, platform, isolate);

  *result = env;

  return 0;
}

extern "C" int
js_destroy_env(js_env_t *env) {
  env->close_maybe();

  return 0;
}

extern "C" int
js_on_uncaught_exception(js_env_t *env, js_uncaught_exception_cb cb, void *data) {
  env->callbacks.uncaught_exception = cb;
  env->callbacks.uncaught_exception_data = data;

  return 0;
}

extern "C" int
js_on_unhandled_rejection(js_env_t *env, js_unhandled_rejection_cb cb, void *data) {
  env->callbacks.unhandled_rejection = cb;
  env->callbacks.unhandled_rejection_data = data;

  return 0;
}

extern "C" int
js_on_dynamic_import(js_env_t *env, js_dynamic_import_cb cb, void *data) {
  env->callbacks.dynamic_import = cb;
  env->callbacks.dynamic_import_data = data;

  return 0;
}

extern "C" int
js_on_dynamic_import_transitional(js_env_t *env, js_dynamic_import_transitional_cb cb, void *data) {
  env->callbacks.dynamic_import_transitional = cb;
  env->callbacks.dynamic_import_data = data;

  return 0;
}

extern "C" int
js_get_env_loop(js_env_t *env, uv_loop_t **result) {
  *result = env->loop;

  return 0;
}

extern "C" int
js_get_env_platform(js_env_t *env, js_platform_t **result) {
  *result = env->platform;

  return 0;
}

namespace {

static inline int
js_error(js_env_t *env) {
  return env->is_exception_pending() ? js_pending_exception : js_uncaught_exception;
}

} // namespace

extern "C" int
js_open_handle_scope(js_env_t *env, js_handle_scope_t **result) {
  // Allow continuing even with a pending exception

  *result = new js_handle_scope_t(env->isolate);

  return 0;
}

extern "C" int
js_close_handle_scope(js_env_t *env, js_handle_scope_t *scope) {
  // Allow continuing even with a pending exception

  delete scope;

  return 0;
}

extern "C" int
js_open_escapable_handle_scope(js_env_t *env, js_escapable_handle_scope_t **result) {
  // Allow continuing even with a pending exception

  *result = new js_escapable_handle_scope_t(env->isolate);

  return 0;
}

extern "C" int
js_close_escapable_handle_scope(js_env_t *env, js_escapable_handle_scope_t *scope) {
  // Allow continuing even with a pending exception

  delete scope;

  return 0;
}

extern "C" int
js_escape_handle(js_env_t *env, js_escapable_handle_scope_t *scope, js_value_t *escapee, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local(escapee);

  *result = js_from_local(scope->scope.Escape(local));

  return 0;
}

extern "C" int
js_create_context(js_env_t *env, js_context_t **result) {
  // Allow continuing even with a pending exception

  *result = new js_context_t(env);

  return 0;
}

extern "C" int
js_destroy_context(js_env_t *env, js_context_t *context) {
  // Allow continuing even with a pending exception

  delete context;

  return 0;
}

extern "C" int
js_enter_context(js_env_t *env, js_context_t *context) {
  // Allow continuing even with a pending exception

  context->context.Get(env->isolate)->Enter();

  return 0;
}

extern "C" int
js_exit_context(js_env_t *env, js_context_t *context) {
  // Allow continuing even with a pending exception

  context->context.Get(env->isolate)->Exit();

  return 0;
}

extern "C" int
js_get_bindings(js_env_t *env, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  *result = js_from_local(context->GetExtrasBindingObject());

  return 0;
}

extern "C" int
js_run_script(js_env_t *env, const char *file, size_t len, int offset, js_value_t *source, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto string = js_to_string_utf8(env, file, len, true);

  if (string.IsEmpty()) return js_error(env);

  auto origin = ScriptOrigin(
    string.ToLocalChecked(),
    offset,
    0,
    false,
    -1,
    Local<Value>(),
    false,
    false,
    false,
    Local<Data>()
  );

  auto compiler_source = ScriptCompiler::Source(js_to_local<String>(source), origin);

  auto compiled = env->try_catch<Script>(
    [&] {
      return ScriptCompiler::Compile(context, &compiler_source);
    }
  );

  if (compiled.IsEmpty()) return js_error(env);

  auto local = env->call_into_javascript<Value>(
    [&] {
      return compiled.ToLocalChecked()->Run(context);
    }
  );

  if (local.IsEmpty()) return js_error(env);

  if (result) *result = js_from_local(local.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_module(js_env_t *env, const char *name, size_t len, int offset, js_value_t *source, js_module_meta_cb cb, void *data, js_module_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto string = js_to_string_utf8(env, name, len, true);

  if (string.IsEmpty()) return js_error(env);

  auto origin = ScriptOrigin(
    string.ToLocalChecked(),
    offset,
    0,
    false,
    -1,
    Local<Value>(),
    false,
    false,
    true,
    Local<Data>()
  );

  auto compiler_source = ScriptCompiler::Source(js_to_local<String>(source), origin);

  auto compiled = env->try_catch<Module>(
    [&] {
      return ScriptCompiler::CompileModule(env->isolate, &compiler_source);
    }
  );

  if (compiled.IsEmpty()) return js_error(env);

  auto local = compiled.ToLocalChecked();

  std::string module_name;

  if (len == size_t(-1)) {
    module_name = std::string(name);
  } else {
    module_name = std::string(name, len);
  }

  auto module = new js_module_t(env->isolate, local, std::move(module_name));

  module->callbacks.meta = cb;
  module->callbacks.meta_data = data;

  env->modules.emplace(local->GetIdentityHash(), module);

  *result = module;

  return 0;
}

extern "C" int
js_create_synthetic_module(js_env_t *env, const char *name, size_t len, js_value_t *const export_names[], size_t export_names_len, js_module_evaluate_cb cb, void *data, js_module_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto string = js_to_string_utf8(env, name, len, true);

  if (string.IsEmpty()) return js_error(env);

  auto local_export_names = reinterpret_cast<Local<String> *>(const_cast<js_value_t **>(export_names));

  auto local = Module::CreateSyntheticModule(
    env->isolate,
    string.ToLocalChecked(),
    MemorySpan<const Local<String>>(
      std::vector(local_export_names, local_export_names + export_names_len).begin(),
      export_names_len
    ),
    js_module_t::on_evaluate
  );

  std::string module_name;

  if (len == size_t(-1)) {
    module_name = name;
  } else {
    module_name = std::string(name, len);
  }

  auto module = new js_module_t(env->isolate, local, std::move(module_name));

  module->callbacks.evaluate = cb;
  module->callbacks.evaluate_data = data;

  env->modules.emplace(local->GetIdentityHash(), module);

  *result = module;

  return 0;
}

extern "C" int
js_delete_module(js_env_t *env, js_module_t *module) {
  // Allow continuing even with a pending exception

  auto scope = HandleScope(env->isolate);

  auto local = module->module.Get(env->isolate);

  auto range = env->modules.equal_range(local->GetIdentityHash());

  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->module == local) {
      env->modules.erase(it);
      break;
    }
  }

  delete module;

  return 0;
}

extern "C" int
js_get_module_name(js_env_t *env, js_module_t *module, const char **result) {
  // Allow continuing even with a pending exception

  *result = module->name.data();

  return 0;
}

extern "C" int
js_get_module_namespace(js_env_t *env, js_module_t *module, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto local = module->module.Get(env->isolate);

  assert(local->GetStatus() >= Module::Status::kInstantiated);

  *result = js_from_local(local->GetModuleNamespace());

  return 0;
}

extern "C" int
js_set_module_export(js_env_t *env, js_module_t *module, js_value_t *name, js_value_t *value) {
  if (env->is_exception_pending()) return js_error(env);

  auto local = module->module.Get(env->isolate);

  auto success = env->try_catch<bool>(
    [&] {
      return local->SetSyntheticModuleExport(env->isolate, js_to_local<String>(name), js_to_local(value));
    }
  );

  if (success.IsNothing()) return js_error(env);

  return 0;
}

extern "C" int
js_instantiate_module(js_env_t *env, js_module_t *module, js_module_resolve_cb cb, void *data) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  module->callbacks.resolve = cb;
  module->callbacks.resolve_data = data;

  auto local = module->module.Get(env->isolate);

  auto success = env->call_into_javascript<bool>(
    [&] {
      return local->InstantiateModule(context, js_module_t::on_resolve);
    }
  );

  if (success.IsNothing()) return js_error(env);

  return 0;
}

extern "C" int
js_run_module(js_env_t *env, js_module_t *module, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = env->call_into_javascript<Value>(
    [&] {
      return module->module.Get(env->isolate)->Evaluate(context);
    }
  );

  if (local.IsEmpty()) return js_error(env);

  if (result) *result = js_from_local(local.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_reference(js_env_t *env, js_value_t *value, uint32_t count, js_ref_t **result) {
  // Allow continuing even with a pending exception

  auto reference = new js_ref_t(env->isolate, js_to_local(value), count);

  if (reference->count == 0) reference->set_weak();

  *result = reference;

  return 0;
}

extern "C" int
js_delete_reference(js_env_t *env, js_ref_t *reference) {
  // Allow continuing even with a pending exception

  delete reference;

  return 0;
}

extern "C" int
js_reference_ref(js_env_t *env, js_ref_t *reference, uint32_t *result) {
  // Allow continuing even with a pending exception

  reference->count++;

  if (reference->count == 1) reference->clear_weak();

  if (result) *result = reference->count;

  return 0;
}

extern "C" int
js_reference_unref(js_env_t *env, js_ref_t *reference, uint32_t *result) {
  // Allow continuing even with a pending exception

  if (reference->count > 0) {
    reference->count--;

    if (reference->count == 0) reference->set_weak();
  }

  if (result) *result = reference->count;

  return 0;
}

extern "C" int
js_get_reference_value(js_env_t *env, js_ref_t *reference, js_value_t **result) {
  // Allow continuing even with a pending exception

  if (reference->value.IsEmpty()) {
    *result = nullptr;
  } else {
    *result = js_from_local(reference->value.Get(env->isolate));
  }

  return 0;
}

extern "C" int
js_define_class(js_env_t *env, const char *name, size_t len, js_function_cb constructor, void *data, js_property_descriptor_t const properties[], size_t properties_len, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto callback = new js_callback_t(env, constructor, data);

  auto tpl = callback->to_function_template(env->isolate);

  if (name) {
    auto string = js_to_string_utf8(env, name, len, true);

    if (string.IsEmpty()) {
      delete callback;

      return js_error(env);
    }

    tpl->SetClassName(string.ToLocalChecked());
  }

  std::vector<js_property_descriptor_t> static_properties;

  for (size_t i = 0; i < properties_len; i++) {
    const js_property_descriptor_t *property = &properties[i];

    if ((property->attributes & js_static) != 0) {
      static_properties.push_back(*property);
      continue;
    }

    auto attributes = PropertyAttribute::None;

    if ((property->attributes & js_writable) == 0 && property->getter == nullptr && property->setter == nullptr) {
      attributes = static_cast<PropertyAttribute>(attributes | PropertyAttribute::ReadOnly);
    }

    if ((property->attributes & js_enumerable) == 0) {
      attributes = static_cast<PropertyAttribute>(attributes | PropertyAttribute::DontEnum);
    }

    if ((property->attributes & js_configurable) == 0) {
      attributes = static_cast<PropertyAttribute>(attributes | PropertyAttribute::DontDelete);
    }

    auto name = js_to_local<Name>(property->name);

    if (property->getter || property->setter) {
      Local<FunctionTemplate> getter;
      Local<FunctionTemplate> setter;

      if (property->getter) {
        auto callback = new js_callback_t(env, property->getter, property->data);

        getter = callback->to_function_template(env->isolate);
      }

      if (property->setter) {
        auto callback = new js_callback_t(env, property->setter, property->data);

        setter = callback->to_function_template(env->isolate);
      }

      tpl->PrototypeTemplate()->SetAccessorProperty(name, getter, setter, attributes);
    } else if (property->method) {
      auto callback = new js_callback_t(env, property->method, property->data);

      auto method = callback->to_function_template(env->isolate, Signature::New(env->isolate, tpl));

      tpl->PrototypeTemplate()->Set(name, method, attributes);
    } else {
      auto value = js_to_local(property->value);

      tpl->PrototypeTemplate()->Set(name, value, attributes);
    }
  }

  auto function = env->try_catch<Function>(
    [&] {
      return tpl->GetFunction(context);
    }
  );

  if (function.IsEmpty()) return js_error(env);

  *result = js_from_local(function.ToLocalChecked());

  return js_define_properties(env, *result, static_properties.data(), static_properties.size());
}

extern "C" int
js_define_properties(js_env_t *env, js_value_t *object, js_property_descriptor_t const properties[], size_t properties_len) {
  if (env->is_exception_pending()) return js_error(env);

  if (properties_len == 0) return 0;

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  for (size_t i = 0; i < properties_len; i++) {
    const js_property_descriptor_t *property = &properties[i];

    auto name = js_to_local<Name>(property->name);

    auto success = Nothing<bool>();

    if (property->getter || property->setter) {
      Local<Function> getter;
      Local<Function> setter;

      if (property->getter) {
        auto callback = new js_callback_t(env, property->getter, property->data);

        getter = callback->to_function(env->isolate, context).ToLocalChecked();
      }

      if (property->setter) {
        auto callback = new js_callback_t(env, property->setter, property->data);

        setter = callback->to_function(env->isolate, context).ToLocalChecked();
      }

      auto descriptor = PropertyDescriptor(getter, setter);

      descriptor.set_enumerable((property->attributes & js_enumerable) != 0);
      descriptor.set_configurable((property->attributes & js_configurable) != 0);

      success = env->try_catch<bool>(
        [&] {
          return local->DefineProperty(context, name, descriptor);
        }
      );
    } else if (property->method) {
      auto callback = new js_callback_t(env, property->method, property->data);

      auto method = callback->to_function(env->isolate, context).ToLocalChecked();

      auto descriptor = PropertyDescriptor(method, (property->attributes & js_writable) != 0);

      descriptor.set_enumerable((property->attributes & js_enumerable) != 0);
      descriptor.set_configurable((property->attributes & js_configurable) != 0);

      success = env->try_catch<bool>(
        [&] {
          return local->DefineProperty(context, name, descriptor);
        }
      );
    } else {
      auto value = js_to_local(property->value);

      if ((property->attributes & js_writable) &&
          (property->attributes & js_enumerable) &&
          (property->attributes & js_configurable)) {
        success = env->try_catch<bool>(
          [&] {
            return local->CreateDataProperty(context, name, value);
          }
        );
      } else {
        auto descriptor = PropertyDescriptor(value, (property->attributes & js_writable) != 0);

        descriptor.set_enumerable((property->attributes & js_enumerable) != 0);
        descriptor.set_configurable((property->attributes & js_configurable) != 0);

        success = env->try_catch<bool>(
          [&] {
            return local->DefineProperty(context, name, descriptor);
          }
        );
      }
    }

    if (success.IsNothing()) return js_error(env);
  }

  return 0;
}

extern "C" int
js_wrap(js_env_t *env, js_value_t *object, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_ref_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto key = env->wrapper.Get(env->isolate);

  auto local = js_to_local<Object>(object);

  auto finalizer = new js_finalizer_t(env, data, finalize_cb, finalize_hint);

  auto external = External::New(env->isolate, finalizer, js_finalizer_type_tag);

  auto success = env->try_catch<bool>(
    [&] {
      return local->SetPrivate(context, key, external);
    }
  );

  if (success.IsNothing()) {
    delete finalizer;

    return js_error(env);
  }

  finalizer->attach_to(env->isolate, local);

  if (result) return js_create_reference(env, object, 0, result);

  return 0;
}

extern "C" int
js_unwrap(js_env_t *env, js_value_t *object, void **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto key = env->wrapper.Get(env->isolate);

  auto local = js_to_local<Object>(object);

  auto external = env->try_catch<Value>(
    [&] {
      return local->GetPrivate(context, key);
    }
  );

  if (external.IsEmpty()) return js_error(env);

  auto finalizer = reinterpret_cast<js_finalizer_t *>(external.ToLocalChecked().As<External>()->Value(js_finalizer_type_tag));

  *result = finalizer->data;

  return 0;
}

extern "C" int
js_remove_wrap(js_env_t *env, js_value_t *object, void **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto key = env->wrapper.Get(env->isolate);

  auto local = js_to_local<Object>(object);

  auto external = env->try_catch<Value>(
    [&] {
      return local->GetPrivate(context, key);
    }
  );

  if (external.IsEmpty()) return js_error(env);

  local->DeletePrivate(context, key).Check();

  auto finalizer = reinterpret_cast<js_finalizer_t *>(external.ToLocalChecked().As<External>()->Value(js_finalizer_type_tag));

  finalizer->detach();

  if (result) *result = finalizer->data;

  delete finalizer;

  return 0;
}

extern "C" int
js_create_delegate(js_env_t *env, const js_delegate_callbacks_t *callbacks, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto delegate = new js_delegate_t(env, *callbacks, data, finalize_cb, finalize_hint);

  auto tpl = delegate->to_object_template(env->isolate);

  auto object = tpl->NewInstance(context);

  if (object.IsEmpty()) return js_error(env);

  delegate->attach_to(env->isolate, object.ToLocalChecked());

  *result = js_from_local(object.ToLocalChecked());

  return 0;
}

extern "C" int
js_add_finalizer(js_env_t *env, js_value_t *object, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_ref_t **result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<Object>(object);

  auto finalizer = new js_finalizer_t(env, data, finalize_cb, finalize_hint);

  finalizer->attach_to(env->isolate, local);

  if (result) return js_create_reference(env, object, 0, result);

  return 0;
}

extern "C" int
js_add_type_tag(js_env_t *env, js_value_t *object, const js_type_tag_t *tag) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  auto context = env->current_context();

  auto key = env->tag.Get(env->isolate);

  auto local = js_to_local<Object>(object);

  auto has = env->try_catch<bool>(
    [&] {
      return local->HasPrivate(context, key);
    }
  );

  if (has.IsNothing()) return js_error(env);

  if (has.ToChecked()) {
    err = js_throw_errorf(env, NULL, "Object is already type tagged");
    assert(err == 0);

    return js_error(env);
  }

  auto value = BigInt::NewFromWords(context, 0, 2, reinterpret_cast<const uint64_t *>(tag)).ToLocalChecked();

  auto success = env->try_catch<bool>(
    [&] {
      return local->SetPrivate(context, key, value);
    }
  );

  if (success.IsNothing()) return js_error(env);

  if (!success.ToChecked()) {
    err = js_throw_errorf(env, NULL, "Could not add type tag to object");
    assert(err == 0);

    return js_error(env);
  }

  return 0;
}

extern "C" int
js_check_type_tag(js_env_t *env, js_value_t *object, const js_type_tag_t *tag, bool *result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto key = env->tag.Get(env->isolate);

  auto local = js_to_local<Object>(object);

  auto value = env->try_catch<Value>(
    [&] {
      return local->GetPrivate(context, key);
    }
  );

  if (value.IsEmpty()) return js_error(env);

  *result = false;

  if (value.ToLocalChecked()->IsBigInt()) {
    js_type_tag_t existing;

    int sign, size = 2;

    value.ToLocalChecked().As<BigInt>()->ToWordsArray(&sign, &size, reinterpret_cast<uint64_t *>(&existing));

    if (sign != 0) return 0;

    if (size == 2) {
      *result = (existing.lower == tag->lower && existing.upper == tag->upper);
    } else if (size == 1) {
      *result = (existing.lower == tag->lower && 0 == tag->upper);
    } else if (size == 0) {
      *result = (0 == tag->lower && 0 == tag->upper);
    }
  }

  return 0;
}

extern "C" int
js_create_int32(js_env_t *env, int32_t value, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto integer = Integer::New(env->isolate, value);

  *result = js_from_local(integer);

  return 0;
}

extern "C" int
js_create_uint32(js_env_t *env, uint32_t value, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto integer = Integer::NewFromUnsigned(env->isolate, value);

  *result = js_from_local(integer);

  return 0;
}

extern "C" int
js_create_int64(js_env_t *env, int64_t value, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto number = Number::New(env->isolate, static_cast<double>(value));

  *result = js_from_local(number);

  return 0;
}

extern "C" int
js_create_double(js_env_t *env, double value, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto number = Number::New(env->isolate, value);

  *result = js_from_local(number);

  return 0;
}

extern "C" int
js_create_bigint_int64(js_env_t *env, int64_t value, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto bigint = BigInt::New(env->isolate, value);

  *result = js_from_local(bigint);

  return 0;
}

extern "C" int
js_create_bigint_uint64(js_env_t *env, uint64_t value, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto bigint = BigInt::NewFromUnsigned(env->isolate, value);

  *result = js_from_local(bigint);

  return 0;
}

extern "C" int
js_create_bigint_words(js_env_t *env, int sign, const uint64_t *words, size_t len, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  if (len > INT_MAX) {
    err = js_throw_range_error(env, NULL, "Invalid words length");
    assert(err == 0);

    return js_error(env);
  }

  auto context = env->context.Get(env->isolate);

  auto bigint = BigInt::NewFromWords(context, sign, static_cast<int>(len), words);

  if (bigint.IsEmpty()) return js_error(env);

  *result = js_from_local(bigint.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_string_utf8(js_env_t *env, const utf8_t *str, size_t len, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto string = js_to_string_utf8(env, str, len);

  if (string.IsEmpty()) return js_error(env);

  *result = js_from_local(string.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_string_utf16le(js_env_t *env, const utf16_t *str, size_t len, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto string = js_to_string_utf16le(env, str, len);

  if (string.IsEmpty()) return js_error(env);

  *result = js_from_local(string.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_string_latin1(js_env_t *env, const latin1_t *str, size_t len, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto string = js_to_string_latin1(env, str, len);

  if (string.IsEmpty()) return js_error(env);

  *result = js_from_local(string.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_external_string_utf8(js_env_t *env, utf8_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result, bool *copied) {
  if (env->is_exception_pending()) return js_error(env);

  if (copied) *copied = true;

  auto string = js_to_string_utf8(env, str, len);

  if (string.IsEmpty()) return js_error(env);

  *result = js_from_local(string.ToLocalChecked());

  if (finalize_cb) finalize_cb(env, str, finalize_hint);

  return 0;
}

extern "C" int
js_create_external_string_utf16le(js_env_t *env, utf16_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result, bool *copied) {
  if (env->is_exception_pending()) return js_error(env);

  auto resource = new js_external_string_utf16le_t(env, str, len, finalize_cb, finalize_hint);

  auto string = String::NewExternalTwoByte(env->isolate, resource);

  if (string.IsEmpty()) {
    delete resource;

    return js_error(env);
  }

  if (copied) *copied = false;

  *result = js_from_local(string.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_external_string_latin1(js_env_t *env, latin1_t *str, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result, bool *copied) {
  if (env->is_exception_pending()) return js_error(env);

  auto resource = new js_external_string_latin1_t(env, str, len, finalize_cb, finalize_hint);

  auto string = String::NewExternalOneByte(env->isolate, resource);

  if (string.IsEmpty()) {
    delete resource;

    return js_error(env);
  }

  if (copied) *copied = false;

  *result = js_from_local(string.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_property_key_utf8(js_env_t *env, const utf8_t *str, size_t len, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto string = js_to_string_utf8(env, str, len, true);

  if (string.IsEmpty()) return js_error(env);

  *result = js_from_local(string.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_property_key_utf16le(js_env_t *env, const utf16_t *str, size_t len, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto string = js_to_string_utf16le(env, str, len, true);

  if (string.IsEmpty()) return js_error(env);

  *result = js_from_local(string.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_property_key_latin1(js_env_t *env, const latin1_t *str, size_t len, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto string = js_to_string_latin1(env, str, len, true);

  if (string.IsEmpty()) return js_error(env);

  *result = js_from_local(string.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_symbol(js_env_t *env, js_value_t *description, js_value_t **result) {
  // Allow continuing even with a pending exception

  Local<Symbol> symbol;

  if (description == nullptr) {
    symbol = Symbol::New(env->isolate);
  } else {
    symbol = Symbol::New(env->isolate, js_to_local<String>(description));
  }

  *result = js_from_local(symbol);

  return 0;
}

extern "C" int
js_symbol_for(js_env_t *env, const char *description, size_t len, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto string = js_to_string_utf8(env, description, len, true);

  if (string.IsEmpty()) return js_error(env);

  auto symbol = Symbol::For(env->isolate, string.ToLocalChecked());

  *result = js_from_local(symbol);

  return 0;
}

extern "C" int
js_create_object(js_env_t *env, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto object = Object::New(env->isolate);

  *result = js_from_local(object);

  return 0;
}

extern "C" int
js_create_function(js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto callback = new js_callback_t(env, cb, data);

  auto function = env->try_catch<Function>(
    [&] {
      return callback->to_function(env->isolate, context);
    }
  );

  if (function.IsEmpty()) {
    delete callback;

    return js_error(env);
  }

  auto local = function.ToLocalChecked();

  if (name) {
    auto string = js_to_string_utf8(env, name, len, true);

    if (string.IsEmpty()) return js_error(env);

    local->SetName(string.ToLocalChecked());
  }

  *result = js_from_local(local);

  return 0;
}

extern "C" int
js_create_function_with_source(js_env_t *env, const char *name, size_t name_len, const char *file, size_t file_len, js_value_t *const args[], size_t args_len, int offset, js_value_t *source, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto string = js_to_string_utf8(env, file, file_len, true);

  if (string.IsEmpty()) return js_error(env);

  auto origin = ScriptOrigin(
    string.ToLocalChecked(),
    offset,
    0,
    false,
    -1,
    Local<Value>(),
    false,
    false,
    false,
    Local<Data>()
  );

  auto compiler_source = ScriptCompiler::Source(js_to_local<String>(source), origin);

  auto function = env->try_catch<Function>(
    [&] {
      return ScriptCompiler::CompileFunction(
        context,
        &compiler_source,
        args_len,
        const_cast<Local<String> *>(reinterpret_cast<const Local<String> *>(args))
      );
    }
  );

  if (function.IsEmpty()) return js_error(env);

  auto local = function.ToLocalChecked();

  if (name) {
    auto string = js_to_string_utf8(env, name, name_len, true);

    if (string.IsEmpty()) return js_error(env);

    local->SetName(string.ToLocalChecked());
  }

  *result = js_from_local(local);

  return 0;
}

namespace {

template <js_type_position_t position>
static inline std::optional<CTypeInfo>
js_to_native_type(int type, std::optional<CFunctionInfo::Int64Representation> &integer_representation) {
  using Type = CTypeInfo::Type;

  if (type == js_int64 || type == js_uint64) {
    if (integer_representation == CFunctionInfo::Int64Representation::kBigInt) {
      goto unsupported;
    }

    integer_representation = CFunctionInfo::Int64Representation::kNumber;
  } else if (type == js_bigint64 || type == js_biguint64) {
    if (integer_representation == CFunctionInfo::Int64Representation::kNumber) {
      goto unsupported;
    }

    integer_representation = CFunctionInfo::Int64Representation::kBigInt;
  }

  switch (type) {
  case js_undefined:
    return CTypeInfo(Type::kVoid);
  case js_null:
    goto handle;
  case js_boolean:
    return CTypeInfo(Type::kBool);
  case js_number:
    goto handle;
  case js_int8:
    goto unsupported;
  case js_uint8:
    goto unsupported;
  case js_int16:
    goto unsupported;
  case js_uint16:
    goto unsupported;
  case js_int32:
    return CTypeInfo(Type::kInt32);
  case js_uint32:
    return CTypeInfo(Type::kUint32);
  case js_int64:
    return CTypeInfo(Type::kInt64);
  case js_uint64:
    return CTypeInfo(Type::kUint64);
  case js_float16:
    goto unsupported;
  case js_float32:
    return CTypeInfo(Type::kFloat32);
  case js_float64:
    return CTypeInfo(Type::kFloat64);
  case js_bigint64:
    return CTypeInfo(Type::kInt64);
  case js_biguint64:
    return CTypeInfo(Type::kUint64);
  case js_string:
    goto handle;
  case js_symbol:
    goto handle;
  case js_object:
    goto handle;
  case js_function:
    goto handle;
  case js_external:
    return CTypeInfo(Type::kPointer);
  case js_bigint:
    goto handle;
  default:
  unsupported:
    return std::nullopt;
  handle:
    // Handles are not currently supported in return position
    if constexpr (position == js_return_type) goto unsupported;

    return CTypeInfo(Type::kV8Value);
  }
}

} // namespace

extern "C" int
js_create_typed_function(js_env_t *env, const char *name, size_t len, js_function_cb cb, const js_callback_signature_t *signature, const void *address, void *data, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  if (signature->version != 0) return js_create_function(env, name, len, cb, data, result);

  auto context = env->current_context();

  auto integer_representation = std::optional<CFunctionInfo::Int64Representation>();

  auto result_info = js_to_native_type<js_return_type>(signature->result, integer_representation);

  if (!result_info.has_value()) {
    return js_create_function(env, name, len, cb, data, result);
  }

  auto args_info = std::vector<CTypeInfo>();

  for (size_t i = 0, n = signature->args_len; i < n; i++) {
    auto arg_info = js_to_native_type<js_argument_type>(signature->args[i], integer_representation);

    if (!arg_info.has_value()) {
      return js_create_function(env, name, len, cb, data, result);
    }

    args_info.push_back(std::move(arg_info.value()));
  }

  args_info.emplace_back(CTypeInfo::kCallbackOptionsType);

  auto callback = new js_typed_callback_t(
    env,
    cb,
    data,
    std::move(result_info.value()),
    std::move(args_info),
    address,
    integer_representation.value_or(CFunctionInfo::Int64Representation::kNumber)
  );

  auto function = env->try_catch<Function>(
    [&] {
      return callback->to_function_template(env->isolate)->GetFunction(context);
    }
  );

  if (function.IsEmpty()) {
    delete callback;

    return js_error(env);
  }

  auto local = function.ToLocalChecked();

  if (name) {
    auto string = js_to_string_utf8(env, name, len, true);

    if (string.IsEmpty()) {
      delete callback;

      return js_error(env);
    }

    local->SetName(string.ToLocalChecked());
  }

  *result = js_from_local(local);

  return 0;
}

extern "C" int
js_create_array(js_env_t *env, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto array = Array::New(env->isolate);

  *result = js_from_local(array);

  return 0;
}

extern "C" int
js_create_array_with_length(js_env_t *env, size_t len, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  if (len > INT_MAX) {
    err = js_throw_range_error(env, NULL, "Invalid array length");
    assert(err == 0);

    return js_error(env);
  }

  auto array = Array::New(env->isolate, static_cast<int>(len));

  *result = js_from_local(array);

  return 0;
}

extern "C" int
js_create_external(js_env_t *env, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto external = External::New(env->isolate, data, js_external_type_tag);

  if (finalize_cb) {
    auto finalizer = new js_finalizer_t(env, data, finalize_cb, finalize_hint);

    finalizer->attach_to(env->isolate, external);
  }

  *result = js_from_local(external);

  return 0;
}

extern "C" int
js_create_date(js_env_t *env, double time, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto context = env->current_context();

  auto date = Date::New(context, time).ToLocalChecked();

  *result = js_from_local(date);

  return 0;
}

namespace {

template <Local<Value> Error(Local<String> message, Local<Value> options)>
static inline int
js_create_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto context = env->current_context();

  auto error = Error(js_to_local<String>(message), {}).As<Object>();

  if (code) {
    error->Set(context, js_to_string_utf8_literal(env, "code", true), js_to_local(code)).Check();
  }

  *result = js_from_local(error);

  return 0;
}

} // namespace

extern "C" int
js_create_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  return js_create_error<Exception::Error>(env, code, message, result);
}

extern "C" int
js_create_type_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  return js_create_error<Exception::TypeError>(env, code, message, result);
}

extern "C" int
js_create_range_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  return js_create_error<Exception::RangeError>(env, code, message, result);
}

extern "C" int
js_create_syntax_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  return js_create_error<Exception::SyntaxError>(env, code, message, result);
}

extern "C" int
js_create_reference_error(js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  return js_create_error<Exception::ReferenceError>(env, code, message, result);
}

extern "C" int
js_get_error_location(js_env_t *env, js_value_t *error, js_error_location_t *result) {
  auto context = env->current_context();

  auto message = Exception::CreateMessage(env->isolate, js_to_local(error));

  result->name = js_from_local(message->GetScriptResourceName());
  result->source = js_from_local(message->GetSource(context).FromMaybe(Undefined(env->isolate)));
  result->line = message->GetLineNumber(context).FromJust();
  result->column_start = message->GetStartColumn(context).FromJust();
  result->column_end = message->GetEndColumn(context).FromJust();

  return 0;
}

extern "C" int
js_create_promise(js_env_t *env, js_deferred_t **deferred, js_value_t **promise) {
  // Allow continuing even with a pending exception

  auto context = env->current_context();

  auto resolver = Promise::Resolver::New(context).ToLocalChecked();

  *deferred = new js_deferred_t(env->isolate, resolver);

  *promise = js_from_local(resolver->GetPromise());

  return 0;
}

namespace {

template <bool resolved>
static inline int
js_conclude_deferred(js_env_t *env, js_deferred_t *deferred, js_value_t *resolution) {
  // Allow continuing even with a pending exception

  auto context = env->current_context();

  auto resolver = deferred->resolver.Get(env->isolate);

  auto local = js_to_local(resolution);

  if (resolved) resolver->Resolve(context, local).Check();
  else resolver->Reject(context, local).Check();

  delete deferred;

  if (env->depth == 0) env->run_microtasks();

  return 0;
}

} // namespace

extern "C" int
js_resolve_deferred(js_env_t *env, js_deferred_t *deferred, js_value_t *resolution) {
  return js_conclude_deferred<true>(env, deferred, resolution);
}

extern "C" int
js_reject_deferred(js_env_t *env, js_deferred_t *deferred, js_value_t *resolution) {
  return js_conclude_deferred<false>(env, deferred, resolution);
}

extern "C" int
js_get_promise_state(js_env_t *env, js_value_t *promise, js_promise_state_t *result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<Promise>(promise);

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
js_get_promise_result(js_env_t *env, js_value_t *promise, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<Promise>(promise);

  assert(local->State() != Promise::PromiseState::kPending);

  *result = js_from_local(local->Result());

  return 0;
}

extern "C" int
js_create_arraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  auto local = ArrayBuffer::MaybeNew(env->isolate, len);

  if (local.IsEmpty()) {
    err = js_throw_range_error(env, NULL, "Array buffer allocation failed");
    assert(err == 0);

    return js_error(env);
  }

  auto arraybuffer = local.ToLocalChecked();

  if (data) *data = arraybuffer->Data();

  *result = js_from_local(arraybuffer);

  return 0;
}

extern "C" int
js_create_arraybuffer_with_backing_store(js_env_t *env, js_arraybuffer_backing_store_t *backing_store, void **data, size_t *len, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto arraybuffer = ArrayBuffer::New(env->isolate, backing_store->backing_store);

  if (data) *data = arraybuffer->Data();

  if (len) *len = arraybuffer->ByteLength();

  *result = js_from_local(arraybuffer);

  return 0;
}

extern "C" int
js_create_unsafe_arraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  auto local = ArrayBuffer::MaybeNew(env->isolate, len, BackingStoreInitializationMode::kUninitialized);

  if (local.IsEmpty()) {
    err = js_throw_range_error(env, NULL, "Array buffer allocation failed");
    assert(err == 0);

    return js_error(env);
  }

  auto arraybuffer = local.ToLocalChecked();

  if (data) *data = arraybuffer->Data();

  *result = js_from_local(arraybuffer);

  return 0;
}

namespace {

static void
js_finalize_external_arraybuffer(void *data, size_t len, void *deleter_data) {
  if (deleter_data == nullptr) return;

  auto finalizer = reinterpret_cast<js_finalizer_t *>(deleter_data);

  finalizer->finalize_cb(nullptr, finalizer->data, finalizer->finalize_hint);

  delete finalizer;
}

} // namespace

extern "C" int
js_create_external_arraybuffer(js_env_t *env, void *data, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  js_finalizer_t *finalizer = nullptr;

  if (finalize_cb) {
    finalizer = new js_finalizer_t(env, data, finalize_cb, finalize_hint);
  }

  auto store = ArrayBuffer::NewBackingStore(
    data,
    len,
    js_finalize_external_arraybuffer,
    finalizer
  );

  auto arraybuffer = ArrayBuffer::New(env->isolate, std::move(store));

  *result = js_from_local(arraybuffer);

  return 0;
}

extern "C" int
js_detach_arraybuffer(js_env_t *env, js_value_t *arraybuffer) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<ArrayBuffer>(arraybuffer);

  assert(local->IsDetachable());

  local->Detach(Local<Value>()).Check();

  return 0;
}

extern "C" int
js_get_arraybuffer_backing_store(js_env_t *env, js_value_t *arraybuffer, js_arraybuffer_backing_store_t **result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<ArrayBuffer>(arraybuffer);

  *result = new js_arraybuffer_backing_store_t(local->GetBackingStore());

  return 0;
}

extern "C" int
js_create_sharedarraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto sharedarraybuffer = SharedArrayBuffer::New(env->isolate, len);

  if (data) *data = sharedarraybuffer->Data();

  *result = js_from_local(sharedarraybuffer);

  return 0;
}

extern "C" int
js_create_sharedarraybuffer_with_backing_store(js_env_t *env, js_arraybuffer_backing_store_t *backing_store, void **data, size_t *len, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto sharedarraybuffer = SharedArrayBuffer::New(env->isolate, backing_store->backing_store);

  if (data) *data = sharedarraybuffer->Data();

  if (len) *len = sharedarraybuffer->ByteLength();

  *result = js_from_local(sharedarraybuffer);

  return 0;
}

extern "C" int
js_create_unsafe_sharedarraybuffer(js_env_t *env, size_t len, void **data, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto sharedarraybuffer = SharedArrayBuffer::New(env->isolate, len, BackingStoreInitializationMode::kUninitialized);

  if (data) *data = sharedarraybuffer->Data();

  *result = js_from_local(sharedarraybuffer);

  return 0;
}

namespace {

static void
js_finalize_external_sharedarraybuffer(void *data, size_t len, void *deleter_data) {
  if (deleter_data == nullptr) return;

  auto finalizer = reinterpret_cast<js_finalizer_t *>(deleter_data);

  finalizer->finalize_cb(nullptr, finalizer->data, finalizer->finalize_hint);

  delete finalizer;
}

} // namespace

extern "C" int
js_create_external_sharedarraybuffer(js_env_t *env, void *data, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  js_finalizer_t *finalizer = nullptr;

  if (finalize_cb) {
    finalizer = new js_finalizer_t(env, data, finalize_cb, finalize_hint);
  }

  auto store = ArrayBuffer::NewBackingStore(
    data,
    len,
    js_finalize_external_sharedarraybuffer,
    finalizer
  );

  auto sharedarraybuffer = SharedArrayBuffer::New(env->isolate, std::move(store));

  *result = js_from_local(sharedarraybuffer);

  return 0;
}

extern "C" int
js_get_sharedarraybuffer_backing_store(js_env_t *env, js_value_t *sharedarraybuffer, js_arraybuffer_backing_store_t **result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<SharedArrayBuffer>(sharedarraybuffer);

  *result = new js_arraybuffer_backing_store_t(local->GetBackingStore());

  return 0;
}

extern "C" int
js_release_arraybuffer_backing_store(js_env_t *env, js_arraybuffer_backing_store_t *backing_store) {
  // Allow continuing even with a pending exception

  delete backing_store;

  return 0;
}

namespace {

template <typename T>
static inline Local<TypedArray>
js_create_typedarray(js_typedarray_type_t type, T arraybuffer, size_t offset, size_t len) {
  switch (type) {
  case js_int8array:
    return Int8Array::New(arraybuffer, offset, len);
  case js_uint8array:
    return Uint8Array::New(arraybuffer, offset, len);
  case js_uint8clampedarray:
    return Uint8ClampedArray::New(arraybuffer, offset, len);
  case js_int16array:
    return Int16Array::New(arraybuffer, offset, len);
  case js_uint16array:
    return Uint16Array::New(arraybuffer, offset, len);
  case js_int32array:
    return Int32Array::New(arraybuffer, offset, len);
  case js_uint32array:
    return Uint32Array::New(arraybuffer, offset, len);
  case js_float16array:
    return Float16Array::New(arraybuffer, offset, len);
  case js_float32array:
    return Float32Array::New(arraybuffer, offset, len);
  case js_float64array:
    return Float64Array::New(arraybuffer, offset, len);
  case js_bigint64array:
    return BigInt64Array::New(arraybuffer, offset, len);
  case js_biguint64array:
    return BigUint64Array::New(arraybuffer, offset, len);
  }
}

} // namespace

extern "C" int
js_create_typedarray(js_env_t *env, js_typedarray_type_t type, size_t len, js_value_t *arraybuffer, size_t offset, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto local = js_to_local(arraybuffer);

  Local<TypedArray> typedarray;

  if (local->IsArrayBuffer()) {
    typedarray = js_create_typedarray(type, local.As<ArrayBuffer>(), offset, len);
  } else {
    typedarray = js_create_typedarray(type, local.As<SharedArrayBuffer>(), offset, len);
  }

  *result = js_from_local(typedarray);

  return 0;
}

namespace {

template <typename T>
static inline Local<DataView>
js_create_dataview(Local<T> arraybuffer, size_t offset, size_t len) {
  return DataView::New(arraybuffer, offset, len);
}

} // namespace

extern "C" int
js_create_dataview(js_env_t *env, size_t len, js_value_t *arraybuffer, size_t offset, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto local = js_to_local(arraybuffer);

  Local<DataView> dataview;

  if (local->IsArrayBuffer()) {
    dataview = js_create_dataview(local.As<ArrayBuffer>(), offset, len);
  } else {
    dataview = js_create_dataview(local.As<SharedArrayBuffer>(), offset, len);
  }

  *result = js_from_local(dataview);

  return 0;
}

extern "C" int
js_coerce_to_boolean(js_env_t *env, js_value_t *value, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local(value);

  *result = js_from_local(local->ToBoolean(env->isolate));

  return 0;
}

extern "C" int
js_coerce_to_number(js_env_t *env, js_value_t *value, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local(value);

  auto number = env->try_catch<Number>(
    [&] {
      return local->ToNumber(context);
    }
  );

  if (number.IsEmpty()) return js_error(env);

  *result = js_from_local(number.ToLocalChecked());

  return 0;
}

extern "C" int
js_coerce_to_string(js_env_t *env, js_value_t *value, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local(value);

  auto string = env->try_catch<String>(
    [&] {
      return local->ToString(context);
    }
  );

  if (string.IsEmpty()) return js_error(env);

  *result = js_from_local(string.ToLocalChecked());

  return 0;
}

extern "C" int
js_coerce_to_object(js_env_t *env, js_value_t *value, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local(value);

  auto object = env->try_catch<Object>(
    [&] {
      return local->ToObject(context);
    }
  );

  if (object.IsEmpty()) return js_error(env);

  *result = js_from_local(object.ToLocalChecked());

  return 0;
}

extern "C" int
js_typeof(js_env_t *env, js_value_t *value, js_value_type_t *result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local(value);

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
  } else {
    *result = js_undefined;
  }

  return 0;
}

extern "C" int
js_instanceof(js_env_t *env, js_value_t *object, js_value_t *constructor, bool *result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto success = env->try_catch<bool>(
    [&] {
      return js_to_local(object)->InstanceOf(context, js_to_local<Function>(constructor));
    }
  );

  if (success.IsNothing()) return js_error(env);

  *result = success.ToChecked();

  return 0;
}

extern "C" int
js_is_undefined(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsUndefined();

  return 0;
}

extern "C" int
js_is_null(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsNull();

  return 0;
}

extern "C" int
js_is_boolean(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsBoolean();

  return 0;
}

extern "C" int
js_is_number(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsNumber();

  return 0;
}

extern "C" int
js_is_int32(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsInt32();

  return 0;
}

extern "C" int
js_is_uint32(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsUint32();

  return 0;
}

extern "C" int
js_is_string(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsString();

  return 0;
}

extern "C" int
js_is_symbol(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsSymbol();

  return 0;
}

extern "C" int
js_is_object(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsObject();

  return 0;
}

extern "C" int
js_is_function(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsFunction();

  return 0;
}

extern "C" int
js_is_async_function(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsAsyncFunction();

  return 0;
}

extern "C" int
js_is_generator_function(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsGeneratorFunction();

  return 0;
}

extern "C" int
js_is_generator(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsGeneratorObject();

  return 0;
}

extern "C" int
js_is_arguments(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsArgumentsObject();

  return 0;
}

extern "C" int
js_is_array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsArray();

  return 0;
}

extern "C" int
js_is_external(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsExternal();

  return 0;
}

extern "C" int
js_is_wrapped(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  auto context = env->current_context();

  auto key = env->wrapper.Get(env->isolate);

  auto local = js_to_local(value);

  *result = local->IsObject() && local.As<Object>()->HasPrivate(context, key).FromMaybe(false);

  return 0;
}

extern "C" int
js_is_delegate(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  auto context = env->current_context();

  auto key = env->delegate.Get(env->isolate);

  auto local = js_to_local(value);

  *result = local->IsObject() && local.As<Object>()->HasPrivate(context, key).FromMaybe(false);

  return 0;
}

extern "C" int
js_is_bigint(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsBigInt();

  return 0;
}

extern "C" int
js_is_date(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsDate();

  return 0;
}

extern "C" int
js_is_regexp(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsRegExp();

  return 0;
}

extern "C" int
js_is_error(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsNativeError();

  return 0;
}

extern "C" int
js_is_promise(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsPromise();

  return 0;
}

extern "C" int
js_is_proxy(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsProxy();

  return 0;
}

extern "C" int
js_is_map(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsMap();

  return 0;
}

extern "C" int
js_is_map_iterator(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsMapIterator();

  return 0;
}

extern "C" int
js_is_set(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsSet();

  return 0;
}

extern "C" int
js_is_set_iterator(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsSetIterator();

  return 0;
}

extern "C" int
js_is_weak_map(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsWeakMap();

  return 0;
}

extern "C" int
js_is_weak_set(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsWeakSet();

  return 0;
}

extern "C" int
js_is_weak_ref(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsWeakRef();

  return 0;
}

extern "C" int
js_is_arraybuffer(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsArrayBuffer();

  return 0;
}

extern "C" int
js_is_detached_arraybuffer(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local(value);

  *result = local->IsArrayBuffer() && local.As<ArrayBuffer>()->WasDetached();

  return 0;
}

extern "C" int
js_is_sharedarraybuffer(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsSharedArrayBuffer();

  return 0;
}

extern "C" int
js_is_typedarray(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsTypedArray();

  return 0;
}

extern "C" int
js_is_int8array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsInt8Array();

  return 0;
}

extern "C" int
js_is_uint8array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsUint8Array();

  return 0;
}

extern "C" int
js_is_uint8clampedarray(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsUint8ClampedArray();

  return 0;
}

extern "C" int
js_is_int16array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsInt16Array();

  return 0;
}

extern "C" int
js_is_uint16array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsUint16Array();

  return 0;
}

extern "C" int
js_is_int32array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsInt32Array();

  return 0;
}

extern "C" int
js_is_uint32array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsUint32Array();

  return 0;
}

extern "C" int
js_is_float16array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsFloat16Array();

  return 0;
}

extern "C" int
js_is_float32array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsFloat32Array();

  return 0;
}

extern "C" int
js_is_float64array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsFloat64Array();

  return 0;
}

extern "C" int
js_is_bigint64array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsBigInt64Array();

  return 0;
}

extern "C" int
js_is_biguint64array(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsBigUint64Array();

  return 0;
}

extern "C" int
js_is_dataview(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsDataView();

  return 0;
}

extern "C" int
js_is_module_namespace(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(value)->IsModuleNamespaceObject();

  return 0;
}

extern "C" int
js_strict_equals(js_env_t *env, js_value_t *a, js_value_t *b, bool *result) {
  // Allow continuing even with a pending exception

  *result = js_to_local(a)->StrictEquals(js_to_local(b));

  return 0;
}

extern "C" int
js_get_global(js_env_t *env, js_value_t **result) {
  // Allow continuing even with a pending exception

  *result = js_from_local(env->current_context()->Global());

  return 0;
}

extern "C" int
js_get_undefined(js_env_t *env, js_value_t **result) {
  // Allow continuing even with a pending exception

  *result = js_from_local(Undefined(env->isolate));

  return 0;
}

extern "C" int
js_get_null(js_env_t *env, js_value_t **result) {
  // Allow continuing even with a pending exception

  *result = js_from_local(Null(env->isolate));

  return 0;
}

extern "C" int
js_get_boolean(js_env_t *env, bool value, js_value_t **result) {
  // Allow continuing even with a pending exception

  if (value) {
    *result = js_from_local(True(env->isolate));
  } else {
    *result = js_from_local(False(env->isolate));
  }

  return 0;
}

extern "C" int
js_get_value_bool(js_env_t *env, js_value_t *value, bool *result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<Boolean>(value);

  *result = local->Value();

  return 0;
}

extern "C" int
js_get_value_int32(js_env_t *env, js_value_t *value, int32_t *result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<Int32>(value);

  *result = local->Value();

  return 0;
}

extern "C" int
js_get_value_uint32(js_env_t *env, js_value_t *value, uint32_t *result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<Uint32>(value);

  *result = local->Value();

  return 0;
}

extern "C" int
js_get_value_int64(js_env_t *env, js_value_t *value, int64_t *result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<Number>(value);

  *result = static_cast<int64_t>(local->Value());

  return 0;
}

extern "C" int
js_get_value_double(js_env_t *env, js_value_t *value, double *result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<Number>(value);

  *result = local->Value();

  return 0;
}

extern "C" int
js_get_value_bigint_int64(js_env_t *env, js_value_t *value, int64_t *result, bool *lossless) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<BigInt>(value);

  auto n = local->Int64Value(lossless);

  *result = n;

  return 0;
}

extern "C" int
js_get_value_bigint_uint64(js_env_t *env, js_value_t *value, uint64_t *result, bool *lossless) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<BigInt>(value);

  auto n = local->Uint64Value(lossless);

  *result = n;

  return 0;
}

extern "C" int
js_get_value_bigint_words(js_env_t *env, js_value_t *value, int *sign, uint64_t *words, size_t len, size_t *result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<BigInt>(value);

  if (sign == nullptr && words == nullptr) {
    *result = size_t(local->WordCount());
  } else if (len != 0) {
    auto count = len > INT_MAX ? INT_MAX : static_cast<int>(len);

    local->ToWordsArray(sign, &count, words);

    if (result) *result = size_t(count);
  } else if (result) *result = 0;

  return 0;
}

extern "C" int
js_get_value_string_utf8(js_env_t *env, js_value_t *value, utf8_t *str, size_t len, size_t *result) {
  // Allow continuing even with a pending exception

  auto scope = HandleScope(env->isolate); // V8 might flatten the string which requires a scope

  auto local = js_to_local<String>(value);

  if (str == nullptr) {
    *result = local->Utf8LengthV2(env->isolate);
  } else if (len != 0) {
    auto written = local->WriteUtf8V2(
      env->isolate,
      reinterpret_cast<char *>(str),
      len,
      String::WriteFlags::kReplaceInvalidUtf8
    );

    if (written < len) str[written] = '\0';

    if (result) *result = written;
  } else if (result) *result = 0;

  return 0;
}

extern "C" int
js_get_value_string_utf16le(js_env_t *env, js_value_t *value, utf16_t *str, size_t len, size_t *result) {
  // Allow continuing even with a pending exception

  auto scope = HandleScope(env->isolate); // V8 might flatten the string which requires a scope

  auto local = js_to_local<String>(value);

  if (str == nullptr) {
    *result = size_t(local->Length());
  } else if (len != 0) {
    auto written = std::min(len, size_t(local->Length()));

    local->WriteV2(
      env->isolate,
      0,
      uint32_t(written),
      str,
      String::WriteFlags::kNone
    );

    if (written < len) str[written] = u'\0';

    if (result) *result = written;
  } else if (result) *result = 0;

  return 0;
}

extern "C" int
js_get_value_string_latin1(js_env_t *env, js_value_t *value, latin1_t *str, size_t len, size_t *result) {
  // Allow continuing even with a pending exception

  auto scope = HandleScope(env->isolate); // V8 might flatten the string which requires a scope

  auto local = js_to_local<String>(value);

  if (str == nullptr) {
    *result = size_t(local->Length());
  } else if (len != 0) {
    auto written = std::min(len, size_t(local->Length()));

    local->WriteOneByteV2(
      env->isolate,
      0,
      uint32_t(written),
      str,
      String::WriteFlags::kNone
    );

    if (written < len) str[written] = '\0';

    if (result) *result = written;
  } else if (result) *result = 0;

  return 0;
}

extern "C" int
js_get_value_external(js_env_t *env, js_value_t *value, void **result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<External>(value);

  *result = local->Value(js_external_type_tag);

  return 0;
}

extern "C" int
js_get_value_date(js_env_t *env, js_value_t *value, double *result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<Date>(value);

  *result = local->ValueOf();

  return 0;
}

extern "C" int
js_get_array_length(js_env_t *env, js_value_t *array, uint32_t *result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<Array>(array);

  *result = local->Length();

  return 0;
}

extern "C" int
js_get_array_elements(js_env_t *env, js_value_t *array, js_value_t **elements, size_t len, size_t offset, uint32_t *result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Array>(array);

  uint32_t written = 0;

  auto success = env->call_into_javascript<bool>(
    [&] {
      for (uint32_t i = 0, n = uint32_t(len), j = uint32_t(offset), m = local->Length(); i < n && j < m; i++, j++) {
        auto value = local->Get(context, j);

        if (value.IsEmpty()) return false;

        elements[i] = js_from_local(value.ToLocalChecked());

        written++;
      }

      return true;
    }
  );

  if (!success) return js_error(env);

  if (result) *result = written;

  return 0;
}

extern "C" int
js_set_array_elements(js_env_t *env, js_value_t *array, const js_value_t *elements[], size_t len, size_t offset) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Array>(array);

  auto success = env->call_into_javascript<bool>(
    [&] {
      for (uint32_t i = 0, n = uint32_t(len), j = uint32_t(offset); i < n; i++, j++) {
        auto value = local->Set(context, j, js_to_local(elements[i]));

        if (value.IsNothing()) return false;
      }

      return true;
    }
  );

  if (!success) return js_error(env);

  return 0;
}

extern "C" int
js_get_prototype(js_env_t *env, js_value_t *object, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<Object>(object);

  *result = js_from_local(local->GetPrototypeV2());

  return 0;
}

extern "C" int
js_get_property_names(js_env_t *env, js_value_t *object, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto mode = KeyCollectionMode::kIncludePrototypes;

  auto property_filter = static_cast<PropertyFilter>(
    PropertyFilter::ONLY_ENUMERABLE |
    PropertyFilter::SKIP_SYMBOLS
  );

  auto index_filter = IndexFilter::kIncludeIndices;

  auto key_conversion = KeyConversionMode::kConvertToString;

  auto names = env->call_into_javascript<Array>(
    [&] {
      return local->GetPropertyNames(
        context,
        mode,
        property_filter,
        index_filter,
        key_conversion
      );
    }
  );

  if (names.IsEmpty()) return js_error(env);

  if (result) *result = js_from_local(names.ToLocalChecked());

  return 0;
}

namespace {

static inline KeyCollectionMode
js_to_key_collection_mode(js_key_collection_mode_t mode) {
  switch (mode) {
  case js_key_include_prototypes:
  default:
    return KeyCollectionMode::kIncludePrototypes;
  case js_key_own_only:
    return KeyCollectionMode::kOwnOnly;
  }
}

static inline KeyConversionMode
js_to_key_conversion_mode(js_key_conversion_mode_t mode) {
  switch (mode) {
  case js_key_convert_to_string:
  default:
    return KeyConversionMode::kConvertToString;
  case js_key_keep_numbers:
    return KeyConversionMode::kKeepNumbers;
  }
}

static inline PropertyFilter
js_to_property_filter(js_property_filter_t filter) {
  PropertyFilter result = PropertyFilter::ALL_PROPERTIES;

  if (filter & js_property_only_writable) {
    result = static_cast<PropertyFilter>(result | PropertyFilter::ONLY_WRITABLE);
  }

  if (filter & js_property_only_enumerable) {
    result = static_cast<PropertyFilter>(result | PropertyFilter::ONLY_ENUMERABLE);
  }

  if (filter & js_property_only_configurable) {
    result = static_cast<PropertyFilter>(result | PropertyFilter::ONLY_CONFIGURABLE);
  }

  if (filter & js_property_skip_strings) {
    result = static_cast<PropertyFilter>(result | PropertyFilter::SKIP_STRINGS);
  }

  if (filter & js_property_skip_symbols) {
    result = static_cast<PropertyFilter>(result | PropertyFilter::SKIP_SYMBOLS);
  }

  return result;
}

static inline IndexFilter
js_to_index_filter(js_index_filter_t filter) {
  switch (filter) {
  case js_index_include_indices:
  default:
    return IndexFilter::kIncludeIndices;
  case js_index_skip_indices:
    return IndexFilter::kSkipIndices;
  }
}

} // namespace

extern "C" int
js_get_filtered_property_names(js_env_t *env, js_value_t *object, js_key_collection_mode_t mode, js_property_filter_t property_filter, js_index_filter_t index_filter, js_key_conversion_mode_t key_conversion, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto names = env->call_into_javascript<Array>(
    [&] {
      return local->GetPropertyNames(
        context,
        js_to_key_collection_mode(mode),
        js_to_property_filter(property_filter),
        js_to_index_filter(index_filter),
        js_to_key_conversion_mode(key_conversion)
      );
    }
  );

  if (names.IsEmpty()) return js_error(env);

  if (result) *result = js_from_local(names.ToLocalChecked());

  return 0;
}

extern "C" int
js_get_property(js_env_t *env, js_value_t *object, js_value_t *key, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto value = env->call_into_javascript<Value>(
    [&] {
      return local->Get(context, js_to_local(key));
    }
  );

  if (value.IsEmpty()) return js_error(env);

  if (result) *result = js_from_local(value.ToLocalChecked());

  return 0;
}

extern "C" int
js_has_property(js_env_t *env, js_value_t *object, js_value_t *key, bool *result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto success = env->call_into_javascript<bool>(
    [&] {
      return local->Has(context, js_to_local(key));
    }
  );

  if (success.IsNothing()) return js_error(env);

  if (result) *result = success.ToChecked();

  return 0;
}

extern "C" int
js_has_own_property(js_env_t *env, js_value_t *object, js_value_t *key, bool *result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto success = env->call_into_javascript<bool>(
    [&] {
      return local->HasOwnProperty(context, js_to_local<Name>(key));
    }
  );

  if (success.IsNothing()) return js_error(env);

  if (result) *result = success.ToChecked();

  return 0;
}

extern "C" int
js_set_property(js_env_t *env, js_value_t *object, js_value_t *key, js_value_t *value) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto success = env->call_into_javascript<bool>(
    [&] {
      return local->Set(context, js_to_local(key), js_to_local(value));
    }
  );

  if (success.IsNothing()) return js_error(env);

  return 0;
}

extern "C" int
js_delete_property(js_env_t *env, js_value_t *object, js_value_t *key, bool *result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto success = env->call_into_javascript<bool>(
    [&] {
      return local->Delete(context, js_to_local(key));
    }
  );

  if (success.IsNothing()) return js_error(env);

  if (result) *result = success.ToChecked();

  return 0;
}

extern "C" int
js_get_named_property(js_env_t *env, js_value_t *object, const char *name, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto key = js_to_string_utf8(env, name, -1, true);

  if (key.IsEmpty()) return js_error(env);

  auto value = env->call_into_javascript<Value>(
    [&] {
      return local->Get(context, key.ToLocalChecked());
    }
  );

  if (value.IsEmpty()) return js_error(env);

  if (result) *result = js_from_local(value.ToLocalChecked());

  return 0;
}

extern "C" int
js_has_named_property(js_env_t *env, js_value_t *object, const char *name, bool *result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto key = js_to_string_utf8(env, name, -1, true);

  if (key.IsEmpty()) return js_error(env);

  auto success = env->call_into_javascript<bool>(
    [&] {
      return local->Has(context, key.ToLocalChecked());
    }
  );

  if (success.IsNothing()) return js_error(env);

  *result = success.ToChecked();

  return 0;
}

extern "C" int
js_set_named_property(js_env_t *env, js_value_t *object, const char *name, js_value_t *value) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto key = js_to_string_utf8(env, name, -1, true);

  if (key.IsEmpty()) return js_error(env);

  auto success = env->call_into_javascript<bool>(
    [&] {
      return local->Set(context, key.ToLocalChecked(), js_to_local(value));
    }
  );

  if (success.IsNothing()) return js_error(env);

  return 0;
}

extern "C" int
js_delete_named_property(js_env_t *env, js_value_t *object, const char *name, bool *result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto key = js_to_string_utf8(env, name, -1, true);

  if (key.IsEmpty()) return js_error(env);

  auto value = env->call_into_javascript<bool>(
    [&] {
      return local->Delete(context, key.ToLocalChecked());
    }
  );

  if (value.IsNothing()) return js_error(env);

  if (result) *result = value.ToChecked();

  return 0;
}

extern "C" int
js_get_element(js_env_t *env, js_value_t *object, uint32_t index, js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto value = env->call_into_javascript<Value>(
    [&] {
      return local->Get(context, index);
    }
  );

  if (value.IsEmpty()) return js_error(env);

  if (result) *result = js_from_local(value.ToLocalChecked());

  return 0;
}

extern "C" int
js_has_element(js_env_t *env, js_value_t *object, uint32_t index, bool *result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto success = env->call_into_javascript<bool>(
    [&] {
      return local->Has(context, index);
    }
  );

  if (success.IsNothing()) return js_error(env);

  if (result) *result = success.ToChecked();

  return 0;
}

extern "C" int
js_set_element(js_env_t *env, js_value_t *object, uint32_t index, js_value_t *value) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto success = env->call_into_javascript<bool>(
    [&] {
      return local->Set(context, index, js_to_local(value));
    }
  );

  if (success.IsNothing()) return js_error(env);

  return 0;
}

extern "C" int
js_delete_element(js_env_t *env, js_value_t *object, uint32_t index, bool *result) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto local = js_to_local<Object>(object);

  auto success = env->call_into_javascript<bool>(
    [&] {
      return local->Delete(context, index);
    }
  );

  if (success.IsNothing()) return js_error(env);

  if (result) *result = success.ToChecked();

  return 0;
}

extern "C" int
js_get_string_view(js_env_t *env, js_value_t *string, js_string_encoding_t *encoding, const void **data, size_t *len, js_string_view_t **result) {
  // Allow continuing even with a pending exception

  auto scope = HandleScope(env->isolate); // V8 might flatten the string which requires a scope

  auto view = String::ValueView(env->isolate, js_to_local<String>(string));

  if (encoding) *encoding = view.is_one_byte() ? js_latin1 : js_utf16le;

  if (data) *data = view.is_one_byte() ? reinterpret_cast<const void *>(view.data8())
                                       : reinterpret_cast<const void *>(view.data16());

  if (len) *len = view.length();

  *result = nullptr;

  return 0;
}

extern "C" int
js_release_string_view(js_env_t *env, js_string_view_t *view) {
  // Allow continuing even with a pending exception

  return 0;
}

extern "C" int
js_get_callback_info(js_env_t *env, const js_callback_info_t *info, size_t *argc, js_value_t *argv[], js_value_t **receiver, void **data) {
  // Allow continuing even with a pending exception

  auto args = reinterpret_cast<const FunctionCallbackInfo<Value> *>(info);

  auto const n = args->Length();

  if (argv) {
    auto i = 0;
    auto const m = *argc;

    for (; i < n && i < m; i++) {
      argv[i] = js_from_local(args->operator[](i));
    }

    if (i < m) {
      auto undefined = js_from_local(Undefined(env->isolate));

      for (; i < m; i++) {
        argv[i] = undefined;
      }
    }
  }

  if (argc) *argc = size_t(n);

  if (receiver) *receiver = js_from_local(args->This());

  if (data) {
    *data = reinterpret_cast<js_callback_t *>(args->Data().As<External>()->Value(js_callback_info_type_tag))->data;
  }

  return 0;
}

extern "C" int
js_get_typed_callback_info(const js_typed_callback_info_t *info, js_env_t **env, void **data) {
  // Allow continuing even with a pending exception

  auto args = reinterpret_cast<const FastApiCallbackOptions *>(info);

  if (env) *env = js_env_t::from(args->isolate);

  if (data) {
    *data = reinterpret_cast<js_typed_callback_t *>(args->data.As<External>()->Value(js_callback_info_type_tag))->data;
  }

  return 0;
}

extern "C" int
js_get_new_target(js_env_t *env, const js_callback_info_t *info, js_value_t **result) {
  // Allow continuing even with a pending exception

  auto args = reinterpret_cast<const FunctionCallbackInfo<Value> *>(info);

  *result = js_from_local(args->NewTarget());

  return 0;
}

extern "C" int
js_get_arraybuffer_info(js_env_t *env, js_value_t *arraybuffer, void **data, size_t *len) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<ArrayBuffer>(arraybuffer);

  if (data) *data = local->Data();

  if (len) *len = local->ByteLength();

  return 0;
}

extern "C" int
js_get_sharedarraybuffer_info(js_env_t *env, js_value_t *arraybuffer, void **data, size_t *len) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<SharedArrayBuffer>(arraybuffer);

  if (data) *data = local->Data();

  if (len) *len = local->ByteLength();

  return 0;
}

namespace {

static inline void
js_get_arraybufferview_info(js_env_t *env, Local<ArrayBufferView> view, void **data, js_value_t **arraybuffer, size_t *offset) {
  if (arraybuffer) {
    auto buffer = view->Buffer();

    if (data) *data = static_cast<uint8_t *>(buffer->Data()) + view->ByteOffset();

    *arraybuffer = js_from_local(buffer);
  } else if (data) {
    if (view->HasBuffer()) {
      auto span = view->GetContents(MemorySpan<uint8_t>());

      *data = span.data();
    } else {
      auto scope = HandleScope(env->isolate);

      auto buffer = view->Buffer();

      *data = static_cast<uint8_t *>(buffer->Data()) + view->ByteOffset();
    }
  }

  if (offset) *offset = view->ByteOffset();
}

} // namespace

extern "C" int
js_get_typedarray_info(js_env_t *env, js_value_t *typedarray, js_typedarray_type_t *type, void **data, size_t *len, js_value_t **arraybuffer, size_t *offset) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<TypedArray>(typedarray);

  if (type) {
    if (local->IsInt8Array()) {
      *type = js_int8array;
    } else if (local->IsUint8Array()) {
      *type = js_uint8array;
    } else if (local->IsUint8ClampedArray()) {
      *type = js_uint8clampedarray;
    } else if (local->IsInt16Array()) {
      *type = js_int16array;
    } else if (local->IsUint16Array()) {
      *type = js_uint16array;
    } else if (local->IsInt32Array()) {
      *type = js_int32array;
    } else if (local->IsUint32Array()) {
      *type = js_uint32array;
    } else if (local->IsFloat16Array()) {
      *type = js_float16array;
    } else if (local->IsFloat32Array()) {
      *type = js_float32array;
    } else if (local->IsFloat64Array()) {
      *type = js_float64array;
    } else if (local->IsBigInt64Array()) {
      *type = js_bigint64array;
    } else if (local->IsBigUint64Array()) {
      *type = js_biguint64array;
    }
  }

  if (len) *len = local->Length();

  js_get_arraybufferview_info(env, local, data, arraybuffer, offset);

  return 0;
}

extern "C" int
js_get_dataview_info(js_env_t *env, js_value_t *dataview, void **data, size_t *len, js_value_t **arraybuffer, size_t *offset) {
  // Allow continuing even with a pending exception

  auto local = js_to_local<DataView>(dataview);

  if (len) *len = local->ByteLength();

  js_get_arraybufferview_info(env, local, data, arraybuffer, offset);

  return 0;
}

extern "C" int
js_call_function(js_env_t *env, js_value_t *receiver, js_value_t *function, size_t argc, js_value_t *const argv[], js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  if (argc > INT_MAX) {
    err = js_throw_range_error(env, NULL, "Invalid arguments length");
    assert(err == 0);

    return js_error(env);
  }

  auto context = env->current_context();

  auto local = env->call_into_javascript<Value>(
    [&] {
      return js_to_local<Function>(function)
        ->Call(
          context,
          js_to_local(receiver),
          static_cast<int>(argc),
          reinterpret_cast<Local<Value> *>(const_cast<js_value_t **>(argv))
        );
    }
  );

  if (local.IsEmpty()) return js_error(env);

  if (result) *result = js_from_local(local.ToLocalChecked());

  return 0;
}

extern "C" int
js_call_function_with_checkpoint(js_env_t *env, js_value_t *receiver, js_value_t *function, size_t argc, js_value_t *const argv[], js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  if (argc > INT_MAX) {
    err = js_throw_range_error(env, NULL, "Invalid arguments length");
    assert(err == 0);

    return js_error(env);
  }

  auto context = env->current_context();

  auto local = env->call_into_javascript<Value>(
    [&] {
      return js_to_local<Function>(function)
        ->Call(
          context,
          js_to_local(receiver),
          static_cast<int>(argc),
          reinterpret_cast<Local<Value> *>(const_cast<js_value_t **>(argv))
        );
    },
    true /* always_checkpoint */
  );

  if (local.IsEmpty()) return js_error(env);

  if (result) *result = js_from_local(local.ToLocalChecked());

  return 0;
}

extern "C" int
js_new_instance(js_env_t *env, js_value_t *constructor, size_t argc, js_value_t *const argv[], js_value_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  if (argc > INT_MAX) {
    err = js_throw_range_error(env, NULL, "Invalid arguments length");
    assert(err == 0);

    return js_error(env);
  }

  auto context = env->current_context();

  auto local = env->call_into_javascript<Object>(
    [&] {
      return js_to_local<Function>(constructor)
        ->NewInstance(
          context,
          static_cast<int>(argc),
          reinterpret_cast<Local<Value> *>(const_cast<js_value_t **>(argv))
        );
    }
  );

  if (local.IsEmpty()) return js_error(env);

  *result = js_from_local(local.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_threadsafe_function(js_env_t *env, js_value_t *function, size_t queue_limit, size_t initial_thread_count, js_finalize_cb finalize_cb, void *finalize_hint, void *context, js_threadsafe_function_cb cb, js_threadsafe_function_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  if (function == nullptr && cb == nullptr) {
    err = js_throw_error(env, NULL, "Either a function or a callback must be provided");
    assert(err == 0);

    return js_error(env);
  };

  if (initial_thread_count == 0) {
    err = js_throw_error(env, NULL, "Initial thread count must be greater than 0");
    assert(err == 0);

    return js_error(env);
  }

  auto threadsafe_function = new js_threadsafe_function_t(env, queue_limit, initial_thread_count, cb, context, finalize_cb, finalize_hint);

  if (function) {
    threadsafe_function->function.Reset(env->isolate, js_to_local(function));
  }

  *result = threadsafe_function;

  return 0;
}

extern "C" int
js_get_threadsafe_function_context(js_threadsafe_function_t *function, void **result) {
  // Allow continuing even with a pending exception

  *result = function->context;

  return 0;
}

extern "C" int
js_call_threadsafe_function(js_threadsafe_function_t *function, void *data, js_threadsafe_function_call_mode_t mode) {
  // Allow continuing even with a pending exception

  return function->push(data, mode) ? 0 : -1;
}

extern "C" int
js_acquire_threadsafe_function(js_threadsafe_function_t *function) {
  // Allow continuing even with a pending exception

  return function->acquire() ? 0 : -1;
}

extern "C" int
js_release_threadsafe_function(js_threadsafe_function_t *function, js_threadsafe_function_release_mode_t mode) {
  // Allow continuing even with a pending exception

  return function->release(mode) ? 0 : -1;
}

extern "C" int
js_ref_threadsafe_function(js_env_t *env, js_threadsafe_function_t *function) {
  // Allow continuing even with a pending exception

  function->ref();

  return 0;
}

extern "C" int
js_unref_threadsafe_function(js_env_t *env, js_threadsafe_function_t *function) {
  // Allow continuing even with a pending exception

  function->unref();

  return 0;
}

extern "C" int
js_add_teardown_callback(js_env_t *env, js_teardown_cb callback, void *data) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  auto status = env->add_teardown_callback(callback, data);

  switch (status) {
  case js_teardown_queue_t::status::already_registered:
    err = js_throw_error(env, NULL, "Teardown callback has already been registered");
    assert(err == 0);

    return js_error(env);

  case js_teardown_queue_t::status::drained:
    err = js_throw_error(env, NULL, "Teardown queue has already drained");
    assert(err == 0);

    return js_error(env);

  default:
    assert(status == js_teardown_queue_s::status::success);

    return 0;
  }
}

extern "C" int
js_remove_teardown_callback(js_env_t *env, js_teardown_cb callback, void *data) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  auto status = env->remove_teardown_callback(callback, data);

  switch (status) {
  case js_teardown_queue_t::status::not_registered:
    err = js_throw_error(env, NULL, "Teardown callback has not been registered");
    assert(err == 0);

    return js_error(env);

  default:
    assert(status == js_teardown_queue_s::status::success);

    return 0;
  }

  return 0;
}

namespace {

static void
js_call_deferred_teardown(void *data) {
  auto handle = reinterpret_cast<js_deferred_teardown_t *>(data);

  handle->cb(handle, handle->data);
}

} // namespace

extern "C" int
js_add_deferred_teardown_callback(js_env_t *env, js_deferred_teardown_cb callback, void *data, js_deferred_teardown_t **result) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  auto handle = new js_deferred_teardown_t(env, callback, data);

  auto status = env->add_teardown_callback(js_call_deferred_teardown, handle);

  switch (status) {
  case js_teardown_queue_t::status::already_registered:
    delete handle;

    err = js_throw_error(env, NULL, "Teardown callback has already been registered");
    assert(err == 0);

    return js_error(env);

  case js_teardown_queue_t::status::drained:
    delete handle;

    err = js_throw_error(env, NULL, "Teardown queue has already drained");
    assert(err == 0);

    return js_error(env);

  default:
    assert(status == js_teardown_queue_s::status::success);

    if (result) *result = handle;

    return 0;
  }
}

extern "C" int
js_finish_deferred_teardown_callback(js_deferred_teardown_t *handle) {
  // Allow continuing even with a pending exception

  auto status = handle->env->remove_teardown_callback(js_call_deferred_teardown, handle);

  assert(status == js_teardown_queue_s::status::success);

  delete handle;

  return 0;
}

extern "C" int
js_throw(js_env_t *env, js_value_t *error) {
  if (env->is_exception_pending()) return js_error(env);

  auto local = js_to_local(error);

  env->exception.Reset(env->isolate, local);

  env->isolate->ThrowException(local);

  return 0;
}

namespace {

template <Local<Value> Error(Local<String> message, Local<Value> options)>
static inline int
js_throw_error(js_env_t *env, const char *code, const char *message) {
  if (env->is_exception_pending()) return js_error(env);

  auto context = env->current_context();

  auto string = js_to_string_utf8(env, message, -1, true);

  if (string.IsEmpty()) return js_error(env);

  auto error = Error(string.ToLocalChecked(), {}).As<Object>();

  if (code) {
    auto string = js_to_string_utf8(env, code, -1, true);

    if (string.IsEmpty()) return js_error(env);

    error->Set(context, js_to_string_utf8_literal(env, "code", true), string.ToLocalChecked()).Check();
  }

  return js_throw(env, js_from_local(error));
}

template <Local<Value> Error(Local<String> message, Local<Value> options)>
static inline int
js_throw_verrorf(js_env_t *env, const char *code, const char *message, va_list args) {
  if (env->is_exception_pending()) return js_error(env);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"

  va_list args_copy;
  va_copy(args_copy, args);

  auto size = vsnprintf(nullptr, 0, message, args_copy);

  va_end(args_copy);

  size += 1 /* NULL */;

  auto formatted = std::vector<char>(size_t(size));

  va_copy(args_copy, args);

  vsnprintf(formatted.data(), size_t(size), message, args_copy);

  va_end(args_copy);

#pragma clang diagnostic pop

  return js_throw_error<Error>(env, code, formatted.data());
}

} // namespace

extern "C" int
js_throw_error(js_env_t *env, const char *code, const char *message) {
  return js_throw_error<Exception::Error>(env, code, message);
}

extern "C" int
js_throw_verrorf(js_env_t *env, const char *code, const char *message, va_list args) {
  return js_throw_verrorf<Exception::Error>(env, code, message, args);
}

extern "C" int
js_throw_errorf(js_env_t *env, const char *code, const char *message, ...) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  va_list args;
  va_start(args, message);

  err = js_throw_verrorf<Exception::Error>(env, code, message, args);

  va_end(args);

  return err;
}

extern "C" int
js_throw_type_error(js_env_t *env, const char *code, const char *message) {
  return js_throw_error<Exception::TypeError>(env, code, message);
}

extern "C" int
js_throw_type_verrorf(js_env_t *env, const char *code, const char *message, va_list args) {
  return js_throw_verrorf<Exception::TypeError>(env, code, message, args);
}

extern "C" int
js_throw_type_errorf(js_env_t *env, const char *code, const char *message, ...) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  va_list args;
  va_start(args, message);

  err = js_throw_verrorf<Exception::TypeError>(env, code, message, args);

  va_end(args);

  return err;
}

extern "C" int
js_throw_range_error(js_env_t *env, const char *code, const char *message) {
  return js_throw_error<Exception::RangeError>(env, code, message);
}

extern "C" int
js_throw_range_verrorf(js_env_t *env, const char *code, const char *message, va_list args) {
  return js_throw_verrorf<Exception::RangeError>(env, code, message, args);
}

extern "C" int
js_throw_range_errorf(js_env_t *env, const char *code, const char *message, ...) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  va_list args;
  va_start(args, message);

  err = js_throw_verrorf<Exception::RangeError>(env, code, message, args);

  va_end(args);

  return err;
}

extern "C" int
js_throw_syntax_error(js_env_t *env, const char *code, const char *message) {
  return js_throw_error<Exception::SyntaxError>(env, code, message);
}

extern "C" int
js_throw_syntax_verrorf(js_env_t *env, const char *code, const char *message, va_list args) {
  return js_throw_verrorf<Exception::SyntaxError>(env, code, message, args);
}

extern "C" int
js_throw_syntax_errorf(js_env_t *env, const char *code, const char *message, ...) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  va_list args;
  va_start(args, message);

  err = js_throw_verrorf<Exception::SyntaxError>(env, code, message, args);

  va_end(args);

  return err;
}

extern "C" int
js_throw_reference_error(js_env_t *env, const char *code, const char *message) {
  return js_throw_error<Exception::ReferenceError>(env, code, message);
}

extern "C" int
js_throw_reference_verrorf(js_env_t *env, const char *code, const char *message, va_list args) {
  return js_throw_verrorf<Exception::ReferenceError>(env, code, message, args);
}

extern "C" int
js_throw_reference_errorf(js_env_t *env, const char *code, const char *message, ...) {
  if (env->is_exception_pending()) return js_error(env);

  int err;

  va_list args;
  va_start(args, message);

  err = js_throw_verrorf<Exception::ReferenceError>(env, code, message, args);

  va_end(args);

  return err;
}

extern "C" int
js_is_exception_pending(js_env_t *env, bool *result) {
  // Allow continuing even with a pending exception

  *result = env->is_exception_pending();

  return 0;
}

extern "C" int
js_get_and_clear_last_exception(js_env_t *env, js_value_t **result) {
  // Allow continuing even with a pending exception

  if (env->exception.IsEmpty()) return js_get_undefined(env, result);

  *result = js_from_local(env->exception.Get(env->isolate));

  env->exception.Reset();

  return 0;
}

extern "C" int
js_fatal_exception(js_env_t *env, js_value_t *error) {
  // Allow continuing even with a pending exception

  env->uncaught_exception(js_to_local(error));

  return 0;
}

extern "C" int
js_terminate_execution(js_env_t *env) {
  // Allow continuing even with a pending exception

  env->isolate->TerminateExecution();

  return 0;
}

extern "C" int
js_adjust_external_memory(js_env_t *env, int64_t change_in_bytes, int64_t *result) {
  // Allow continuing even with a pending exception

  env->memory.Update(env->isolate, change_in_bytes);

  if (result) *result = ExternalMemoryAccounter::GetTotalAmountOfExternalAllocatedMemoryForTesting(env->isolate);

  return 0;
}

extern "C" int
js_request_garbage_collection(js_env_t *env) {
  // Allow continuing even with a pending exception

  if (env->platform->options.expose_garbage_collection) {
    env->isolate->RequestGarbageCollectionForTesting(Isolate::GarbageCollectionType::kFullGarbageCollection);
  }

  return 0;
}

namespace {

static inline std::optional<js_garbage_collection_type_t>
js_to_garbage_collection_type(GCType type) {
  switch (type) {
  case GCType::kGCTypeScavenge:
    return js_garbage_collection_type_generational;
  case GCType::kGCTypeMarkSweepCompact:
    return js_garbage_collection_type_mark_compact;
  default:
    return std::nullopt;
  }
}

static void
js_garbage_collection_tracking_prologue(Isolate *isolate, GCType type, GCCallbackFlags flags, void *data) {
  auto t = js_to_garbage_collection_type(type);

  auto tracking = static_cast<js_garbage_collection_tracking_t *>(data);

  tracking->options.start(t.value(), tracking->data);
}

static void
js_garbage_collection_tracking_epilogue(Isolate *isolate, GCType type, GCCallbackFlags flags, void *data) {
  auto t = js_to_garbage_collection_type(type);

  auto tracking = static_cast<js_garbage_collection_tracking_t *>(data);

  tracking->options.end(t.value(), tracking->data);
}

} // namespace

extern "C" int
js_enable_garbage_collection_tracking(js_env_t *env, const js_garbage_collection_tracking_options_t *options, void *data, js_garbage_collection_tracking_t **result) {
  // Allow continuing even with a pending exception

  auto tracking = new js_garbage_collection_tracking_t(*options, data);

  auto filter = static_cast<GCType>(GCType::kGCTypeScavenge | GCType::kGCTypeMarkSweepCompact);

  env->isolate->AddGCPrologueCallback(js_garbage_collection_tracking_prologue, tracking, filter);
  env->isolate->AddGCEpilogueCallback(js_garbage_collection_tracking_epilogue, tracking, filter);

  *result = tracking;

  return 0;
}

extern "C" int
js_disable_garbage_collection_tracking(js_env_t *env, js_garbage_collection_tracking_t *tracking) {
  // Allow continuing even with a pending exception

  env->isolate->RemoveGCPrologueCallback(js_garbage_collection_tracking_prologue, tracking);
  env->isolate->RemoveGCEpilogueCallback(js_garbage_collection_tracking_epilogue, tracking);

  delete tracking;

  return 0;
}

extern "C" int
js_get_heap_statistics(js_env_t *env, js_heap_statistics_t *result) {
  // Allow continuing even with a pending exception

  HeapStatistics heap_statistics;

  env->isolate->GetHeapStatistics(&heap_statistics);

  result->total_heap_size = heap_statistics.total_heap_size();
  result->used_heap_size = heap_statistics.used_heap_size();

  if (result->version >= 1) {
    result->external_memory = heap_statistics.external_memory();
  }

  return 0;
}

extern "C" int
js_get_heap_space_statistics(js_env_t *env, js_heap_space_statistics_t statistics[], size_t len, size_t offset, size_t *result) {
  // Allow continuing even with a pending exception

  if (statistics == nullptr) {
    *result = env->isolate->NumberOfHeapSpaces();
  } else if (len != 0) {
    HeapSpaceStatistics heap_space_statistics;

    size_t written = 0;

    for (size_t i = 0, n = env->isolate->NumberOfHeapSpaces(), j = offset; i < len && j < n; i++, j++) {
      env->isolate->GetHeapSpaceStatistics(&heap_space_statistics, j);

      statistics[i] = {
        .version = 0,

        .space_name = heap_space_statistics.space_name(),
        .space_size = heap_space_statistics.space_used_size(),
        .space_used_size = heap_space_statistics.space_used_size(),
        .space_available_size = heap_space_statistics.space_available_size()
      };

      written++;
    }

    if (result) *result = written;
  } else if (result) *result = 0;

  return 0;
}

extern "C" int
js_create_inspector(js_env_t *env, js_inspector_t **result) {
  *result = new js_inspector_t(env);

  return 0;
}

extern "C" int
js_destroy_inspector(js_env_t *env, js_inspector_t *inspector) {
  delete inspector;

  return 0;
}

extern "C" int
js_on_inspector_response(js_env_t *env, js_inspector_t *inspector, js_inspector_message_cb cb, void *data) {
  inspector->channel.cb = cb;
  inspector->channel.data = data;

  return 0;
}

extern "C" int
js_on_inspector_response_transitional(js_env_t *env, js_inspector_t *inspector, js_inspector_message_transitional_cb cb, void *data) {
  inspector->channel.cb_transitional = cb;
  inspector->channel.data = data;

  return 0;
}

extern "C" int
js_on_inspector_paused(js_env_t *env, js_inspector_t *inspector, js_inspector_paused_cb cb, void *data) {
  inspector->cb = cb;
  inspector->data = data;

  return 0;
}

extern "C" int
js_connect_inspector(js_env_t *env, js_inspector_t *inspector) {
  inspector->connect();

  return 0;
}

extern "C" int
js_send_inspector_request(js_env_t *env, js_inspector_t *inspector, js_value_t *message) {
  inspector->send(js_to_local<String>(message));

  return 0;
}

extern "C" int
js_send_inspector_request_transitional(js_env_t *env, js_inspector_t *inspector, const char *message, size_t len) {
  inspector->send(message, len);

  return 0;
}

extern "C" int
js_attach_context_to_inspector(js_env_t *env, js_inspector_t *inspector, js_context_t *context, const char *name, size_t len) {
  inspector->attach(context->context.Get(env->isolate), name ? StringView(reinterpret_cast<const uint8_t *>(name), len) : StringView());

  return 0;
}

extern "C" int
js_detach_context_from_inspector(js_env_t *env, js_inspector_t *inspector, js_context_t *context) {
  inspector->detach(context->context.Get(env->isolate));

  return 0;
}
