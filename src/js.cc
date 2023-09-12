#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

#include <assert.h>
#include <mem.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <utf.h>
#include <uv.h>

#include <v8-fast-api-calls.h>
#include <v8.h>

#include <libplatform/libplatform.h>

#include "../include/js.h"
#include "../include/js/ffi.h"

using namespace v8;

typedef struct js_env_scope_s js_env_scope_t;
typedef struct js_callback_s js_callback_t;
typedef struct js_finalizer_s js_finalizer_t;
typedef struct js_delegate_s js_delegate_t;
typedef struct js_tracing_controller_s js_tracing_controller_t;
typedef struct js_task_handle_s js_task_handle_t;
typedef struct js_delayed_task_handle_s js_delayed_task_handle_t;
typedef struct js_idle_task_handle_s js_idle_task_handle_t;
typedef struct js_task_runner_s js_task_runner_t;
typedef struct js_task_scope_s js_task_scope_t;
typedef struct js_job_state_s js_job_state_t;
typedef struct js_job_delegate_s js_job_delegate_t;
typedef struct js_job_handle_s js_job_handle_t;
typedef struct js_worker_s js_worker_t;
typedef struct js_heap_s js_heap_t;
typedef struct js_allocator_s js_allocator_t;

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
  uv_timer_t *timer;
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
        timer(new uv_timer_t()),
        tasks(),
        delayed_tasks(),
        idle_tasks(),
        lock(),
        depth(0),
        outstanding(0),
        disposable(0),
        available(),
        drained() {
    uv_timer_init(loop, timer);
    timer->data = this;
  }

  ~js_task_runner_s() {
    terminate();

    uv_close(reinterpret_cast<uv_handle_t *>(timer), on_handle_close);
  }

  inline uint64_t
  now () {
    return uv_hrtime();
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

    return std::move(value);
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

    while (outstanding > disposable) {
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

    if (--outstanding <= disposable) {
      drained.notify_all();
    }
  }

  static void
  on_timer (uv_timer_t *handle) {
    auto tasks = reinterpret_cast<js_task_runner_t *>(handle->data);

    tasks->move_expired_tasks();
  }

  static void
  on_handle_close (uv_handle_t *handle) {
    delete handle;
  }

  void
  adjust_timer () {
    std::scoped_lock guard(lock);

    if (delayed_tasks.empty()) {
      uv_timer_stop(timer);
    } else {
      js_delayed_task_handle_t const &task = delayed_tasks.top();

      uint64_t timeout = task.expiry - now();

      uv_timer_start(timer, on_timer, timeout, 0);

      // Don't let the timer keep the loop alive if all outstanding tasks are
      // disposable.
      if (outstanding == disposable) {
        uv_unref(reinterpret_cast<uv_handle_t *>(timer));
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

struct js_heap_s {
  mem_heap_t *heap;
  bool zero_fill;

private:
  js_heap_s()
      : heap(nullptr),
        zero_fill(true) {
    mem_heap_init(nullptr, &heap);
  }

public:
  ~js_heap_s() {
    mem_heap_destroy(heap);
  }

  static std::shared_ptr<js_heap_t>
  local () {
    thread_local static auto heap = std::shared_ptr<js_heap_t>(new js_heap_t());

    return heap;
  }

  inline void *
  alloc (size_t size) {
    if (zero_fill) return mem_zalloc(heap, size);
    return mem_alloc(heap, size);
  }

  inline void *
  alloc_unsafe (size_t size) {
    return mem_alloc(heap, size);
  }

  inline void *
  realloc (void *ptr, size_t size) {
    if (zero_fill) return mem_rezalloc(heap, ptr, size);
    return mem_realloc(heap, ptr, size);
  }

  inline void *
  realloc_unsafe (void *ptr, size_t size) {
    return mem_realloc(heap, ptr, size);
  }

  inline void
  free (void *ptr) {
    mem_free(ptr);
  }
};

struct js_allocator_s : public ArrayBuffer::Allocator {
private: // V8 embedder API
  void *
  Allocate (size_t length) override {
    return js_heap_t::local()->alloc(length);
  }

  void *
  AllocateUninitialized (size_t length) override {
    return js_heap_t::local()->alloc_unsafe(length);
  }

  void
  Free (void *data, size_t length) override {
    js_heap_t::local()->free(data);
  }

  void *
  Reallocate (void *data, size_t old_length, size_t new_length) override {
    return js_heap_t::local()->realloc(data, new_length);
  }
};

struct js_platform_s : public Platform {
  js_platform_options_t options;
  uv_loop_t *loop;
  uv_prepare_t *prepare;
  uv_check_t *check;
  std::map<Isolate *, std::shared_ptr<js_task_runner_t>> foreground;
  std::shared_ptr<js_task_runner_t> background;
  std::vector<std::shared_ptr<js_worker_t>> workers;
  std::unique_ptr<js_tracing_controller_t> trace;
  std::recursive_mutex lock;

  js_platform_s(js_platform_options_t options, uv_loop_t *loop)
      : options(options),
        loop(loop),
        prepare(new uv_prepare_t()),
        check(new uv_check_t()),
        foreground(),
        background(new js_task_runner_t(loop)),
        workers(),
        trace(new js_tracing_controller_t()),
        lock() {
    uv_prepare_init(loop, prepare);
    uv_prepare_start(prepare, on_prepare);
    prepare->data = this;

    uv_check_init(loop, check);
    uv_check_start(check, on_check);
    check->data = this;

    // The check handle should not on its own keep the loop alive; it's simply
    // used for running any outstanding tasks that might cause additional work
    // to be queued.
    uv_unref(reinterpret_cast<uv_handle_t *>(check));

    start_workers();
  }

  ~js_platform_s() {
    uv_close(reinterpret_cast<uv_handle_t *>(prepare), on_handle_close);
    uv_close(reinterpret_cast<uv_handle_t *>(check), on_handle_close);
  }

  inline uint64_t
  now () {
    return uv_hrtime();
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
    if (background->empty() || background->outstanding == background->disposable) {
      uv_prepare_stop(prepare);
    } else {
      uv_prepare_start(prepare, on_prepare);
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

  static void
  on_handle_close (uv_handle_t *handle) {
    delete handle;
  }

  inline void
  start_workers () {
    workers.reserve(uv_available_parallelism() - 1 /* main thread */);

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
    background->push_task(js_delayed_task_handle_t(std::move(task), js_task_nestable, background->now() + (delay * 1000)));
  }

  std::unique_ptr<JobHandle>
  CreateJob (TaskPriority priority, std::unique_ptr<JobTask> task) override {
    return platform::NewDefaultJobHandle(this, priority, std::move(task), workers.size());
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
  uv_prepare_t *prepare;
  uv_check_t *check;
  js_platform_t *platform;
  std::shared_ptr<js_task_runner_t> tasks;
  Isolate *isolate;
  HandleScope scope;
  uint32_t depth;
  Persistent<Context> context;
  Persistent<Private> wrapper;
  Persistent<Private> delegate;
  Persistent<Value> exception;
  std::multimap<size_t, js_module_t *> modules;
  std::vector<Global<Promise>> unhandled_promises;
  js_uncaught_exception_cb on_uncaught_exception;
  void *uncaught_exception_data;
  js_unhandled_rejection_cb on_unhandled_rejection;
  void *unhandled_rejection_data;
  js_dynamic_import_cb on_dynamic_import;
  void *dynamic_import_data;

  js_env_s(uv_loop_t *loop, js_platform_t *platform, Isolate *isolate)
      : loop(loop),
        prepare(new uv_prepare_t()),
        check(new uv_check_t()),
        platform(platform),
        tasks(platform->foreground[isolate]),
        isolate(isolate),
        scope(isolate),
        depth(0),
        context(isolate, Context::New(isolate)),
        wrapper(isolate, Private::New(isolate)),
        delegate(isolate, Private::New(isolate)),
        exception(),
        modules(),
        unhandled_promises(),
        on_uncaught_exception(nullptr),
        uncaught_exception_data(nullptr),
        on_unhandled_rejection(nullptr),
        unhandled_rejection_data(nullptr),
        on_dynamic_import(nullptr),
        dynamic_import_data(nullptr) {
    uv_prepare_init(loop, prepare);
    uv_prepare_start(prepare, on_prepare);
    prepare->data = this;

    uv_check_init(loop, check);
    uv_check_start(check, on_check);
    check->data = this;

    // The check handle should not on its own keep the loop alive; it's simply
    // used for running any outstanding tasks that might cause additional work
    // to be queued.
    uv_unref(reinterpret_cast<uv_handle_t *>(check));

    to_local(this->context)->Enter();
  }

  ~js_env_s() {
    uv_close(reinterpret_cast<uv_handle_t *>(prepare), on_handle_close);
    uv_close(reinterpret_cast<uv_handle_t *>(check), on_handle_close);
  }

  inline uint64_t
  now () {
    return uv_hrtime();
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

    if (on_unhandled_rejection) {
      for (auto &promise : unhandled_promises) {
        auto local = promise.Get(isolate);

        on_unhandled_rejection(
          this,
          from_local(local->Result()),
          from_local(local),
          unhandled_rejection_data
        );
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

private:
  inline void
  check_liveness () {
    tasks->move_expired_tasks();

    if (tasks->empty() || tasks->outstanding == tasks->disposable) {
      uv_prepare_stop(prepare);
    } else {
      uv_prepare_start(prepare, on_prepare);
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

    if (uv_loop_alive(env->loop)) return;

    env->idle();

    env->check_liveness();
  }

  static void
  on_handle_close (uv_handle_t *handle) {
    delete handle;
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
  Global<Module> module;
  js_module_resolve_cb resolve;
  void *resolve_data;
  js_module_meta_cb meta;
  void *meta_data;
  js_module_evaluate_cb evaluate;
  void *evaluate_data;
  char *name;

  js_module_s(Isolate *isolate, Local<Module> module, char *name)
      : module(isolate, module),
        resolve(nullptr),
        resolve_data(nullptr),
        meta(nullptr),
        meta_data(nullptr),
        evaluate(nullptr),
        evaluate_data(nullptr),
        name(name) {}

  ~js_module_s() {
    delete name;
  }
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
  Persistent<External> external;
  js_env_t *env;
  js_function_cb cb;
  void *data;

  js_callback_s(js_env_t *env, js_function_cb cb, void *data)
      : env(env),
        cb(cb),
        data(data) {}

  static void
  on_finalize (const WeakCallbackInfo<js_callback_t> &info) {
    auto callback = info.GetParameter();

    callback->external.Reset();

    delete callback;
  }

  static void
  on_call (const FunctionCallbackInfo<Value> &info) {
    auto callback = reinterpret_cast<js_callback_t *>(info.Data().As<External>()->Value());

    auto env = callback->env;

    auto result = callback->cb(env, reinterpret_cast<js_callback_info_t *>(const_cast<FunctionCallbackInfo<Value> *>(&info)));

    if (env->exception.IsEmpty()) {
      if (result) {
        info.GetReturnValue().Set(to_local(result));
      }
    } else {
      env->isolate->ThrowException(Local<Value>::New(env->isolate, env->exception));

      env->exception.Reset();
    }
  }
};

struct js_finalizer_s {
  Persistent<Value> value;
  js_env_t *env;
  void *data;
  js_finalize_cb finalize_cb;
  void *finalize_hint;

  js_finalizer_s(js_env_t *env, Local<Value> value, void *data, js_finalize_cb finalize_cb, void *finalize_hint)
      : value(env->isolate, value),
        env(env),
        data(data),
        finalize_cb(finalize_cb),
        finalize_hint(finalize_hint) {}
};

struct js_delegate_s {
  Persistent<Value> value;
  js_env_t *env;
  js_delegate_callbacks_t callbacks;
  void *data;
  js_finalize_cb finalize_cb;
  void *finalize_hint;

  js_delegate_s(js_env_t *env, Local<Value> value, const js_delegate_callbacks_t &callbacks, void *data, js_finalize_cb finalize_cb, void *finalize_hint)
      : env(env),
        value(env->isolate, value),
        callbacks(callbacks),
        data(data),
        finalize_cb(finalize_cb),
        finalize_hint(finalize_hint) {}
};

struct js_arraybuffer_backing_store_s {
  std::shared_ptr<BackingStore> backing_store;

  js_arraybuffer_backing_store_s(std::shared_ptr<BackingStore> backing_store)
      : backing_store(backing_store) {}
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

  js_ffi_function_s(const void *function, const CFunctionInfo *function_info)
      : function(function, function_info) {}
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

static const char *js_platform_identifier = "v8";

static const char *js_platform_version = V8::GetVersion();

extern "C" int
js_create_platform (uv_loop_t *loop, const js_platform_options_t *options, js_platform_t **result) {
  auto flags = std::string();

  // Don't freeze the flags after initialising the platform. This is both not
  // needed and also ensures that V8 doesn't attempt to call `mprotect()`, which
  // isn't allowed on iOS in unprivileged processes.
  flags += "--no-freeze-flags-after-init";

  if (options) {
    if (options->expose_garbage_collection) {
      flags += " --expose-gc";
    }

    if (options->trace_garbage_collection) {
      flags += " --trace-gc";
    }

    if (options->disable_optimizing_compiler) {
      flags += " --jitless --no-expose-wasm";
    } else {
      if (options->trace_optimizations) {
        flags += " --trace-opt";
      }

      if (options->trace_deoptimizations) {
        flags += " --trace-deopt";
      }
    }
  }

  V8::SetFlagsFromString(flags.c_str());

  auto platform = new js_platform_t(options ? *options : js_platform_options_t(), loop);

  V8::InitializePlatform(platform);
  V8::Initialize();

  *result = platform;

  return 0;
}

extern "C" int
js_destroy_platform (js_platform_t *platform) {
  platform->background->terminate();

  for (auto &worker : platform->workers) {
    worker->join();
  }

  V8::Dispose();
  V8::DisposePlatform();

  delete platform;

  return 0;
}

extern "C" int
js_get_platform_identifier (js_platform_t *platform, const char **result) {
  *result = js_platform_identifier;

  return 0;
}

extern "C" int
js_get_platform_version (js_platform_t *platform, const char **result) {
  *result = js_platform_version;

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
  } else {
    env->exception.Reset(env->isolate, error);
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

static MaybeLocal<Promise>
on_dynamic_import (Local<Context> context, Local<Data> data, Local<Value> referrer, Local<String> specifier, Local<FixedArray> raw_assertions) {
  auto env = get_env(context);

  if (env->on_dynamic_import == nullptr) {
    js_throw_error(env, nullptr, "Dynamic import() is not supported");

    return MaybeLocal<Promise>();
  }

  auto assertions = Object::New(env->isolate, Null(env->isolate), nullptr, nullptr, 0);

  for (int i = 0; i < raw_assertions->Length(); i += 3) {
    assertions
      ->Set(
        context,
        raw_assertions->Get(context, i).As<String>(),
        raw_assertions->Get(context, i + 1).As<Value>()
      )
      .Check();
  }

  auto result = env->on_dynamic_import(
    env,
    from_local(specifier),
    from_local(assertions),
    from_local(referrer),
    env->dynamic_import_data
  );

  if (env->exception.IsEmpty()) {
    auto module = result->module.Get(env->isolate);

    auto resolver = Promise::Resolver::New(context).ToLocalChecked();

    auto success = resolver->Resolve(context, module->GetModuleNamespace());

    success.Check();

    return resolver->GetPromise();
  }

  auto exception = env->exception.Get(env->isolate);

  env->exception.Reset();

  env->isolate->ThrowException(exception);

  return MaybeLocal<Promise>();
}

static void
on_import_meta (Local<Context> context, Local<Module> local, Local<Object> meta) {
  auto env = get_env(context);

  auto module = get_module(context, local);

  if (module->meta == nullptr) return;

  module->meta(
    env,
    module,
    from_local(meta),
    module->meta_data
  );

  if (env->exception.IsEmpty()) return;

  auto exception = env->exception.Get(env->isolate);

  env->exception.Reset();

  env->isolate->ThrowException(exception);
}

extern "C" int
js_create_env (uv_loop_t *loop, js_platform_t *platform, const js_env_options_t *options, js_env_t **result) {
  std::scoped_lock guard(platform->lock);

  Isolate::CreateParams params;
  params.array_buffer_allocator_shared = std::make_shared<js_allocator_t>();
  params.allow_atomics_wait = false;

  if (options && options->memory_limit > 0) {
    params.constraints.ConfigureDefaultsFromHeapSize(0, options->memory_limit);
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

  platform->foreground.emplace(isolate, std::move(tasks));

  Isolate::Initialize(isolate, params);

  isolate->SetMicrotasksPolicy(MicrotasksPolicy::kExplicit);

  isolate->AddMessageListener(on_uncaught_exception);

  isolate->SetPromiseRejectCallback(on_promise_reject);

  isolate->SetHostImportModuleDynamicallyCallback(on_dynamic_import);

  isolate->SetHostInitializeImportMetaObjectCallback(on_import_meta);

  auto env = new js_env_t(loop, platform, isolate);

  env->enter();

  auto context = to_local(env->context);

  context->SetAlignedPointerInEmbedderData(js_context_environment, env);

  *result = env;

  return 0;
}

extern "C" int
js_destroy_env (js_env_t *env) {
  std::scoped_lock guard(env->platform->lock);

  auto isolate = env->isolate;

  env->exit();

  env->platform->foreground.erase(isolate);

  delete env;

  isolate->Dispose();

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
js_on_dynamic_import (js_env_t *env, js_dynamic_import_cb cb, void *data) {
  env->on_dynamic_import = cb;
  env->dynamic_import_data = data;

  return 0;
}

extern "C" int
js_get_env_loop (js_env_t *env, uv_loop_t **result) {
  *result = env->loop;

  return 0;
}

extern "C" int
js_get_env_platform (js_env_t *env, js_platform_t **result) {
  *result = env->platform;

  return 0;
}

extern "C" int
js_open_handle_scope (js_env_t *env, js_handle_scope_t **result) {
  *result = new js_handle_scope_t(env->isolate);

  return 0;
}

extern "C" int
js_close_handle_scope (js_env_t *env, js_handle_scope_t *scope) {
  delete scope;

  return 0;
}

extern "C" int
js_open_escapable_handle_scope (js_env_t *env, js_escapable_handle_scope_t **result) {
  *result = new js_escapable_handle_scope_t(env->isolate);

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
    js_throw_error(env, nullptr, "Scope has already been escaped");

    return -1;
  }

  scope->escaped = true;

  auto local = to_local(escapee);

  *result = from_local(scope->scope.Escape(local));

  return 0;
}

extern "C" int
js_run_script (js_env_t *env, const char *file, size_t len, int offset, js_value_t *source, js_value_t **result) {
  auto context = to_local(env->context);

  auto local_source = to_local<String>(source);

  MaybeLocal<String> local_file;

  if (len == size_t(-1)) {
    local_file = String::NewFromUtf8(env->isolate, file);
  } else {
    local_file = String::NewFromUtf8(env->isolate, file, NewStringType::kNormal, len);
  }

  if (local_file.IsEmpty()) {
    js_throw_error(env, nullptr, "Invalid string length");

    return -1;
  }

  auto origin = ScriptOrigin(
    env->isolate,
    local_file.ToLocalChecked(),
    offset,
    0,
    false,
    -1,
    Local<Value>(),
    false,
    false,
    false
  );

  auto v8_source = ScriptCompiler::Source(local_source, origin);

  auto try_catch = TryCatch(env->isolate);

  auto compiled = ScriptCompiler::Compile(context, &v8_source);

  if (try_catch.HasCaught()) {
    auto error = try_catch.Exception();

    if (env->depth == 0) {
      on_uncaught_exception(Exception::CreateMessage(env->isolate, error), error);
    } else {
      env->exception.Reset(env->isolate, error);
    }

    return -1;
  }

  env->depth++;

  auto local = compiled.ToLocalChecked()->Run(context);

  if (env->depth == 1) env->run_microtasks();

  env->depth--;

  if (try_catch.HasCaught()) {
    auto error = try_catch.Exception();

    if (env->depth == 0) {
      on_uncaught_exception(Exception::CreateMessage(env->isolate, error), error);
    } else {
      env->exception.Reset(env->isolate, error);
    }

    return -1;
  }

  if (result) {
    *result = from_local(local.ToLocalChecked());
  }

  return 0;
}

static MaybeLocal<Module>
on_resolve_module (Local<Context> context, Local<String> specifier, Local<FixedArray> raw_assertions, Local<Module> referrer) {
  auto env = get_env(context);

  auto module = get_module(context, referrer);

  auto assertions = Object::New(env->isolate, Null(env->isolate), nullptr, nullptr, 0);

  for (int i = 0; i < raw_assertions->Length(); i += 3) {
    assertions
      ->Set(
        context,
        raw_assertions->Get(context, i).As<String>(),
        raw_assertions->Get(context, i + 1).As<Value>()
      )
      .Check();
  }

  auto result = module->resolve(
    env,
    from_local(specifier),
    from_local(assertions),
    module,
    module->resolve_data
  );

  if (env->exception.IsEmpty()) {
    if (result->resolve == nullptr) {
      result->resolve = module->resolve;
      result->resolve_data = module->resolve_data;
    }

    return result->module.Get(env->isolate);
  }

  auto exception = env->exception.Get(env->isolate);

  env->exception.Reset();

  env->isolate->ThrowException(exception);

  return MaybeLocal<Module>();
}

extern "C" int
js_create_module (js_env_t *env, const char *name, size_t len, int offset, js_value_t *source, js_module_meta_cb cb, void *data, js_module_t **result) {
  auto context = to_local(env->context);

  auto local_source = to_local<String>(source);

  MaybeLocal<String> local_name;

  if (len == size_t(-1)) {
    local_name = String::NewFromUtf8(env->isolate, name);
  } else {
    local_name = String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, len);
  }

  if (local_name.IsEmpty()) {
    js_throw_error(env, nullptr, "Invalid string length");

    return -1;
  }

  auto origin = ScriptOrigin(
    env->isolate,
    local_name.ToLocalChecked(),
    offset,
    0,
    false,
    -1,
    Local<Value>(),
    false,
    false,
    true
  );

  auto v8_source = ScriptCompiler::Source(local_source, origin);

  auto try_catch = TryCatch(env->isolate);

  auto compiled = ScriptCompiler::CompileModule(env->isolate, &v8_source);

  if (try_catch.HasCaught()) {
    auto error = try_catch.Exception();

    if (env->depth == 0) {
      on_uncaught_exception(Exception::CreateMessage(env->isolate, error), error);
    } else {
      env->exception.Reset(env->isolate, error);
    }

    return -1;
  }

  auto local = compiled.ToLocalChecked();

  char *module_name;

  if (len == size_t(-1)) {
    module_name = strdup(name);
  } else {
    module_name = new char[len + 1];
    module_name[len] = '\0';

    memcpy(module_name, name, len);
  }

  auto module = new js_module_t(env->isolate, local, module_name);

  module->meta = cb;
  module->meta_data = data;

  env->modules.emplace(local->GetIdentityHash(), module);

  *result = module;

  return 0;
}

static MaybeLocal<Value>
on_evaluate_module (Local<Context> context, Local<Module> referrer) {
  auto env = get_env(context);

  auto module = get_module(context, referrer);

  module->evaluate(env, module, module->evaluate_data);

  return Undefined(env->isolate);
}

extern "C" int
js_create_synthetic_module (js_env_t *env, const char *name, size_t len, js_value_t *const export_names[], size_t export_names_len, js_module_evaluate_cb cb, void *data, js_module_t **result) {
  auto context = to_local(env->context);

  auto local = reinterpret_cast<Local<String> *>(const_cast<js_value_t **>(export_names));

  auto names = std::vector<Local<String>>(local, local + export_names_len);

  MaybeLocal<String> local_name;

  if (len == size_t(-1)) {
    local_name = String::NewFromUtf8(env->isolate, name);
  } else {
    local_name = String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, len);
  }

  if (local_name.IsEmpty()) {
    js_throw_error(env, nullptr, "Invalid string length");

    return -1;
  }

  auto compiled = Module::CreateSyntheticModule(
    env->isolate,
    local_name.ToLocalChecked(),
    names,
    on_evaluate_module
  );

  char *module_name;

  if (len == size_t(-1)) {
    module_name = strdup(name);
  } else {
    module_name = new char[len + 1];
    module_name[len] = '\0';

    memcpy(module_name, name, len);
  }

  auto module = new js_module_t(env->isolate, compiled, module_name);

  module->evaluate = cb;
  module->evaluate_data = data;

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
js_get_module_name (js_env_t *env, js_module_t *module, const char **result) {
  *result = module->name;

  return 0;
}

extern "C" int
js_get_module_namespace (js_env_t *env, js_module_t *module, js_value_t **result) {
  auto local = module->module.Get(env->isolate);

  if (local->GetStatus() < Module::Status::kInstantiated) {
    js_throw_error(env, nullptr, "Module must be instantiaed");

    return -1;
  }

  *result = from_local(local->GetModuleNamespace());

  return 0;
}

extern "C" int
js_set_module_export (js_env_t *env, js_module_t *module, js_value_t *name, js_value_t *value) {
  auto local = module->module.Get(env->isolate);

  local->SetSyntheticModuleExport(env->isolate, to_local<String>(name), to_local(value)).Check();

  return 0;
}

extern "C" int
js_instantiate_module (js_env_t *env, js_module_t *module, js_module_resolve_cb cb, void *data) {
  auto context = to_local(env->context);

  module->resolve = cb;
  module->resolve_data = data;

  env->depth++;

  auto try_catch = TryCatch(env->isolate);

  auto local = module->module.Get(env->isolate);

  auto success = local->InstantiateModule(context, on_resolve_module);

  env->depth--;

  if (try_catch.HasCaught()) {
    auto error = try_catch.Exception();

    if (env->depth == 0) {
      on_uncaught_exception(Exception::CreateMessage(env->isolate, error), error);
    } else {
      env->exception.Reset(env->isolate, error);
    }

    return -1;
  }

  success.Check();

  return 0;
}

extern "C" int
js_run_module (js_env_t *env, js_module_t *module, js_value_t **result) {
  auto context = to_local(env->context);

  env->depth++;

  auto try_catch = TryCatch(env->isolate);

  auto local = module->module.Get(env->isolate)->Evaluate(context);

  if (env->depth == 1) env->run_microtasks();

  env->depth--;

  if (try_catch.HasCaught()) {
    auto error = try_catch.Exception();

    if (env->depth == 0) {
      on_uncaught_exception(Exception::CreateMessage(env->isolate, error), error);
    } else {
      env->exception.Reset(env->isolate, error);
    }

    return -1;
  }

  if (result) {
    *result = from_local(local.ToLocalChecked());
  }

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
  reference->value.ClearWeak<js_ref_t>();
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
  reference->value.Reset();

  delete reference;

  return 0;
}

extern "C" int
js_reference_ref (js_env_t *env, js_ref_t *reference, uint32_t *result) {
  reference->count++;

  if (reference->count == 1) js_clear_weak_reference(env, reference);

  if (result) {
    *result = reference->count;
  }

  return 0;
}

extern "C" int
js_reference_unref (js_env_t *env, js_ref_t *reference, uint32_t *result) {
  if (reference->count == 0) {
    js_throw_error(env, nullptr, "Cannot decrease reference count");

    return -1;
  }

  reference->count--;

  if (reference->count == 0) js_set_weak_reference(env, reference);

  if (result) {
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
js_define_class (js_env_t *env, const char *name, size_t len, js_function_cb constructor, void *data, js_property_descriptor_t const properties[], size_t properties_len, js_value_t **result) {
  auto context = to_local(env->context);

  auto callback = new js_callback_t(env, constructor, data);

  auto external = External::New(env->isolate, callback);

  callback->external.Reset(env->isolate, external);

  callback->external.SetWeak(callback, js_callback_t::on_finalize, WeakCallbackType::kParameter);

  auto tpl = FunctionTemplate::New(env->isolate, js_callback_t::on_call, external);

  if (name) {
    MaybeLocal<String> string;

    if (len == size_t(-1)) {
      string = String::NewFromUtf8(env->isolate, name);
    } else {
      string = String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, len);
    }

    if (string.IsEmpty()) {
      js_throw_error(env, nullptr, "Invalid string length");

      return -1;
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

    int attributes = PropertyAttribute::None;

    if ((property->attributes & js_writable) == 0 && property->getter == nullptr && property->setter == nullptr) {
      attributes |= PropertyAttribute::ReadOnly;
    }

    if ((property->attributes & js_enumerable) == 0) {
      attributes |= PropertyAttribute::DontEnum;
    }

    if ((property->attributes & js_configurable) == 0) {
      attributes |= PropertyAttribute::DontDelete;
    }

    auto name = String::NewFromUtf8(env->isolate, property->name).ToLocalChecked();

    if (property->getter || property->setter) {
      Local<FunctionTemplate> getter;
      Local<FunctionTemplate> setter;

      if (property->getter) {
        auto callback = new js_callback_t(env, property->getter, property->data);

        auto external = External::New(env->isolate, callback);

        callback->external.Reset(env->isolate, external);

        callback->external.SetWeak(callback, js_callback_t::on_finalize, WeakCallbackType::kParameter);

        getter = FunctionTemplate::New(env->isolate, js_callback_t::on_call, external);
      }

      if (property->setter) {
        auto callback = new js_callback_t(env, property->setter, property->data);

        auto external = External::New(env->isolate, callback);

        callback->external.Reset(env->isolate, external);

        callback->external.SetWeak(callback, js_callback_t::on_finalize, WeakCallbackType::kParameter);

        setter = FunctionTemplate::New(env->isolate, js_callback_t::on_call, external);
      }

      tpl->PrototypeTemplate()->SetAccessorProperty(
        name,
        getter,
        setter,
        static_cast<PropertyAttribute>(attributes)
      );
    } else if (property->method) {
      auto callback = new js_callback_t(env, property->method, property->data);

      auto external = External::New(env->isolate, callback);

      callback->external.Reset(env->isolate, external);

      callback->external.SetWeak(callback, js_callback_t::on_finalize, WeakCallbackType::kParameter);

      auto method = FunctionTemplate::New(
        env->isolate,
        js_callback_t::on_call,
        external,
        Signature::New(env->isolate, tpl)
      );

      tpl->PrototypeTemplate()->Set(
        name,
        method,
        static_cast<PropertyAttribute>(attributes)
      );
    } else {
      auto value = to_local(property->value);

      tpl->PrototypeTemplate()->Set(
        name,
        value,
        static_cast<PropertyAttribute>(attributes)
      );
    }
  }

  auto function = tpl->GetFunction(context).ToLocalChecked();

  *result = from_local(function);

  return js_define_properties(env, *result, static_properties.data(), static_properties.size());
}

extern "C" int
js_define_properties (js_env_t *env, js_value_t *object, js_property_descriptor_t const properties[], size_t properties_len) {
  if (properties_len == 0) return 0;

  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  for (size_t i = 0; i < properties_len; i++) {
    const js_property_descriptor_t *property = &properties[i];

    auto name = String::NewFromUtf8(env->isolate, property->name).ToLocalChecked();

    if (property->getter || property->setter) {
      Local<Function> getter;
      Local<Function> setter;

      if (property->getter) {
        auto callback = new js_callback_t(env, property->getter, property->data);

        auto external = External::New(env->isolate, callback);

        callback->external.Reset(env->isolate, external);

        callback->external.SetWeak(callback, js_callback_t::on_finalize, WeakCallbackType::kParameter);

        getter = Function::New(context, js_callback_t::on_call, external).ToLocalChecked();
      }

      if (property->setter) {
        auto callback = new js_callback_t(env, property->setter, property->data);

        auto external = External::New(env->isolate, callback);

        callback->external.Reset(env->isolate, external);

        callback->external.SetWeak(callback, js_callback_t::on_finalize, WeakCallbackType::kParameter);

        setter = Function::New(context, js_callback_t::on_call, external).ToLocalChecked();
      }

      auto descriptor = PropertyDescriptor(getter, setter);

      descriptor.set_enumerable((property->attributes & js_enumerable) != 0);
      descriptor.set_configurable((property->attributes & js_configurable) != 0);

      local->DefineProperty(context, name, descriptor).Check();
    } else if (property->method) {
      auto callback = new js_callback_t(env, property->method, property->data);

      auto external = External::New(env->isolate, callback);

      callback->external.Reset(env->isolate, external);

      callback->external.SetWeak(callback, js_callback_t::on_finalize, WeakCallbackType::kParameter);

      auto method = Function::New(context, js_callback_t::on_call, external).ToLocalChecked();

      auto descriptor = PropertyDescriptor(method, (property->attributes & js_writable) != 0);

      descriptor.set_enumerable((property->attributes & js_enumerable) != 0);
      descriptor.set_configurable((property->attributes & js_configurable) != 0);

      local->DefineProperty(context, name, descriptor).Check();
    } else {
      auto value = to_local(property->value);

      auto descriptor = PropertyDescriptor(value, (property->attributes & js_writable) != 0);

      descriptor.set_enumerable((property->attributes & js_enumerable) != 0);
      descriptor.set_configurable((property->attributes & js_configurable) != 0);

      local->DefineProperty(context, name, descriptor).Check();
    }
  }

  return 0;
}

static void
on_wrap_finalize (const WeakCallbackInfo<js_finalizer_t> &info) {
  auto finalizer = info.GetParameter();

  if (finalizer->finalize_cb) {
    finalizer->finalize_cb(finalizer->env, finalizer->data, finalizer->finalize_hint);
  }

  finalizer->value.Reset();

  delete finalizer;
}

extern "C" int
js_wrap (js_env_t *env, js_value_t *object, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_ref_t **result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  auto finalizer = new js_finalizer_t(env, local, data, finalize_cb, finalize_hint);

  auto external = External::New(env->isolate, finalizer);

  local->SetPrivate(context, to_local(env->wrapper), external);

  finalizer->value.SetWeak(finalizer, on_wrap_finalize, WeakCallbackType::kParameter);

  if (result) js_create_reference(env, object, 0, result);

  return 0;
}

extern "C" int
js_unwrap (js_env_t *env, js_value_t *object, void **result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  auto external = local->GetPrivate(context, to_local(env->wrapper)).ToLocalChecked();

  auto finalizer = reinterpret_cast<js_finalizer_t *>(external.As<External>()->Value());

  *result = finalizer->data;

  return 0;
}

extern "C" int
js_remove_wrap (js_env_t *env, js_value_t *object, void **result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  auto external = local->GetPrivate(context, to_local(env->wrapper)).ToLocalChecked();

  auto finalizer = reinterpret_cast<js_finalizer_t *>(external.As<External>()->Value());

  local->DeletePrivate(context, to_local(env->wrapper)).Check();

  finalizer->value.SetWeak();

  if (result) {
    *result = finalizer->data;
  }

  delete finalizer;

  return 0;
}

static void
on_delegate_get (Local<Name> property, const PropertyCallbackInfo<Value> &info) {
  auto delegate = static_cast<js_delegate_t *>(info.Data().As<External>()->Value());

  if (delegate->callbacks.get) {
    auto result = delegate->callbacks.get(delegate->env, from_local(property), delegate->data);

    if (result) {
      info.GetReturnValue().Set(to_local(result));
    }
  }
}

static void
on_delegate_set (Local<Name> property, Local<Value> value, const PropertyCallbackInfo<Value> &info) {
  auto delegate = static_cast<js_delegate_t *>(info.Data().As<External>()->Value());

  if (delegate->callbacks.set) {
    auto result = delegate->callbacks.set(delegate->env, from_local(property), from_local(value), delegate->data);

    info.GetReturnValue().Set(result);
  }
}

static void
on_delegate_query (Local<Name> property, const PropertyCallbackInfo<Integer> &info) {
  auto delegate = static_cast<js_delegate_t *>(info.Data().As<External>()->Value());

  if (delegate->callbacks.has) {
    auto result = delegate->callbacks.has(delegate->env, from_local(property), delegate->data);

    if (result) {
      info.GetReturnValue().Set(PropertyAttribute::None);
    }
  }
}

static void
on_delegate_delete (Local<Name> property, const PropertyCallbackInfo<Boolean> &info) {
  auto delegate = static_cast<js_delegate_t *>(info.Data().As<External>()->Value());

  if (delegate->callbacks.delete_property) {
    auto result = delegate->callbacks.delete_property(delegate->env, from_local(property), delegate->data);

    info.GetReturnValue().Set(result);
  }
}

static void
on_delegate_enumerate (const PropertyCallbackInfo<Array> &info) {
  auto delegate = static_cast<js_delegate_t *>(info.Data().As<External>()->Value());

  if (delegate->callbacks.own_keys) {
    auto result = delegate->callbacks.own_keys(delegate->env, delegate->data);

    if (result) {
      auto local = to_local(result).As<Array>();

      info.GetReturnValue().Set(local);
    }
  }
}

static void
on_delegate_finalize (const WeakCallbackInfo<js_delegate_t> &info) {
  auto delegate = info.GetParameter();

  if (delegate->finalize_cb) {
    delegate->finalize_cb(delegate->env, delegate->data, delegate->finalize_hint);
  }

  delegate->value.Reset();

  delete delegate;
}

extern "C" int
js_create_delegate (js_env_t *env, const js_delegate_callbacks_t *callbacks, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result) {
  auto context = to_local(env->context);

  auto delegate = new js_delegate_t(env, Local<Value>(), *callbacks, data, finalize_cb, finalize_hint);

  auto external = External::New(env->isolate, delegate);

  auto tpl = ObjectTemplate::New(env->isolate);

  tpl->SetHandler(NamedPropertyHandlerConfiguration(
    on_delegate_get,
    on_delegate_set,
    on_delegate_query,
    on_delegate_delete,
    on_delegate_enumerate,
    external
  ));

  auto object = tpl->NewInstance(context).ToLocalChecked();

  object->SetPrivate(context, to_local(env->delegate), external);

  delegate->value.Reset(env->isolate, object);

  delegate->value.SetWeak(delegate, on_delegate_finalize, WeakCallbackType::kParameter);

  *result = from_local(object);

  return 0;
}

static void
on_finalizer_finalize (const WeakCallbackInfo<js_finalizer_t> &info) {
  auto finalizer = info.GetParameter();

  if (finalizer->finalize_cb) {
    finalizer->finalize_cb(finalizer->env, finalizer->data, finalizer->finalize_hint);
  }

  finalizer->value.Reset();

  delete finalizer;
}

extern "C" int
js_add_finalizer (js_env_t *env, js_value_t *object, void *data, js_finalize_cb finalize_cb, void *finalize_hint, js_ref_t **result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  auto finalizer = new js_finalizer_t(env, local, data, finalize_cb, finalize_hint);

  finalizer->value.SetWeak(finalizer, on_finalizer_finalize, WeakCallbackType::kParameter);

  if (result) js_create_reference(env, object, 0, result);

  return 0;
}

extern "C" int
js_create_int32 (js_env_t *env, int32_t value, js_value_t **result) {
  auto integer = Integer::New(env->isolate, value);

  *result = from_local(integer);

  return 0;
}

extern "C" int
js_create_uint32 (js_env_t *env, uint32_t value, js_value_t **result) {
  auto integer = Integer::NewFromUnsigned(env->isolate, value);

  *result = from_local(integer);

  return 0;
}

extern "C" int
js_create_int64 (js_env_t *env, int64_t value, js_value_t **result) {
  auto number = Number::New(env->isolate, static_cast<double>(value));

  *result = from_local(number);

  return 0;
}

extern "C" int
js_create_double (js_env_t *env, double value, js_value_t **result) {
  auto number = Number::New(env->isolate, value);

  *result = from_local(number);

  return 0;
}

extern "C" int
js_create_bigint_int64 (js_env_t *env, int64_t value, js_value_t **result) {
  auto bigint = BigInt::New(env->isolate, value);

  *result = from_local(bigint);

  return 0;
}

extern "C" int
js_create_bigint_uint64 (js_env_t *env, uint64_t value, js_value_t **result) {
  auto bigint = BigInt::NewFromUnsigned(env->isolate, value);

  *result = from_local(bigint);

  return 0;
}

extern "C" int
js_create_string_utf8 (js_env_t *env, const utf8_t *value, size_t len, js_value_t **result) {
  MaybeLocal<String> string;

  if (len == size_t(-1)) {
    string = String::NewFromUtf8(env->isolate, reinterpret_cast<const char *>(value));
  } else {
    string = String::NewFromUtf8(env->isolate, reinterpret_cast<const char *>(value), NewStringType::kNormal, len);
  }

  if (string.IsEmpty()) {
    js_throw_error(env, nullptr, "Invalid string length");

    return -1;
  }

  *result = from_local(string.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_string_utf16le (js_env_t *env, const utf16_t *value, size_t len, js_value_t **result) {
  MaybeLocal<String> string;

  if (len == size_t(-1)) {
    string = String::NewFromTwoByte(env->isolate, value);
  } else {
    string = String::NewFromTwoByte(env->isolate, value, NewStringType::kNormal, len);
  }

  if (string.IsEmpty()) {
    js_throw_error(env, nullptr, "Invalid string length");

    return -1;
  }

  *result = from_local(string.ToLocalChecked());

  return 0;
}

extern "C" int
js_create_symbol (js_env_t *env, js_value_t *description, js_value_t **result) {
  Local<Symbol> symbol;

  if (description == nullptr) {
    symbol = Symbol::New(env->isolate);
  } else {
    symbol = Symbol::New(env->isolate, to_local<String>(description));
  }

  *result = from_local(symbol);

  return 0;
}

extern "C" int
js_create_object (js_env_t *env, js_value_t **result) {
  auto context = to_local(env->context);

  auto object = Object::New(env->isolate);

  *result = from_local(object);

  return 0;
}

extern "C" int
js_create_function (js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data, js_value_t **result) {
  auto context = to_local(env->context);

  auto callback = new js_callback_t(env, cb, data);

  auto external = External::New(env->isolate, callback);

  callback->external.Reset(env->isolate, external);

  callback->external.SetWeak(callback, js_callback_t::on_finalize, WeakCallbackType::kParameter);

  auto tpl = FunctionTemplate::New(env->isolate, js_callback_t::on_call, external);

  auto function = tpl->GetFunction(context).ToLocalChecked();

  if (name) {
    MaybeLocal<String> string;

    if (len == size_t(-1)) {
      string = String::NewFromUtf8(env->isolate, name);
    } else {
      string = String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, len);
    }

    if (string.IsEmpty()) {
      js_throw_error(env, nullptr, "Invalid string length");

      return -1;
    }

    function->SetName(string.ToLocalChecked());
  }

  *result = from_local(function);

  return 0;
}

extern "C" int
js_create_function_with_source (js_env_t *env, const char *name, size_t name_len, const char *file, size_t file_len, js_value_t *const args[], size_t args_len, int offset, js_value_t *source, js_value_t **result) {
  auto context = to_local(env->context);

  auto local_source = to_local<String>(source);

  MaybeLocal<String> local_file;

  if (file_len == size_t(-1)) {
    local_file = String::NewFromUtf8(env->isolate, file);
  } else {
    local_file = String::NewFromUtf8(env->isolate, file, NewStringType::kNormal, file_len);
  }

  if (local_file.IsEmpty()) {
    js_throw_error(env, nullptr, "Invalid string length");

    return -1;
  }

  auto origin = ScriptOrigin(
    env->isolate,
    local_file.ToLocalChecked(),
    offset,
    0,
    false,
    -1,
    Local<Value>(),
    false,
    false,
    false
  );

  auto v8_source = ScriptCompiler::Source(local_source, origin);

  auto try_catch = TryCatch(env->isolate);

  auto compiled = ScriptCompiler::CompileFunction(
    context,
    &v8_source,
    args_len,
    const_cast<Local<String> *>(reinterpret_cast<const Local<String> *>(args))
  );

  if (try_catch.HasCaught()) {
    auto error = try_catch.Exception();

    if (env->depth == 0) {
      on_uncaught_exception(Exception::CreateMessage(env->isolate, error), error);
    } else {
      env->exception.Reset(env->isolate, error);
    }

    return -1;
  }

  auto function = compiled.ToLocalChecked();

  if (name) {
    MaybeLocal<String> string;

    if (name_len == size_t(-1)) {
      string = String::NewFromUtf8(env->isolate, name);
    } else {
      string = String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, name_len);
    }

    if (string.IsEmpty()) {
      js_throw_error(env, nullptr, "Invalid string length");

      return -1;
    }

    function->SetName(string.ToLocalChecked());
  }

  *result = from_local(function);

  return 0;
}

extern "C" int
js_create_function_with_ffi (js_env_t *env, const char *name, size_t len, js_function_cb cb, void *data, js_ffi_function_t *ffi, js_value_t **result) {
  auto context = to_local(env->context);

  auto callback = new js_callback_t(env, cb, data);

  auto external = External::New(env->isolate, callback);

  callback->external.Reset(env->isolate, external);

  callback->external.SetWeak(callback, js_callback_t::on_finalize, WeakCallbackType::kParameter);

  auto tpl = FunctionTemplate::New(
    env->isolate,
    js_callback_t::on_call,
    external,
    Local<Signature>(),
    0,
    ConstructorBehavior::kThrow,
    SideEffectType::kHasSideEffect,
    &ffi->function
  );

  auto function = tpl->GetFunction(context).ToLocalChecked();

  if (name) {
    MaybeLocal<String> string;

    if (len == size_t(-1)) {
      string = String::NewFromUtf8(env->isolate, name);
    } else {
      string = String::NewFromUtf8(env->isolate, name, NewStringType::kNormal, len);
    }

    if (string.IsEmpty()) {
      js_throw_error(env, nullptr, "Invalid string length");

      return -1;
    }

    function->SetName(string.ToLocalChecked());
  }

  *result = from_local(function);

  return 0;
}

extern "C" int
js_create_array (js_env_t *env, js_value_t **result) {
  auto array = Array::New(env->isolate);

  *result = from_local(array);

  return 0;
}

extern "C" int
js_create_array_with_length (js_env_t *env, size_t len, js_value_t **result) {
  auto array = Array::New(env->isolate, len);

  *result = from_local(array);

  return 0;
}

static void
on_external_finalize (const WeakCallbackInfo<js_finalizer_t> &info) {
  auto finalizer = info.GetParameter();

  auto external = to_local(finalizer->value).As<External>();

  finalizer->finalize_cb(finalizer->env, finalizer->data, finalizer->finalize_hint);

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
js_create_date (js_env_t *env, double time, js_value_t **result) {
  auto context = to_local(env->context);

  auto date = Date::New(context, time);

  if (date.IsEmpty()) {
    js_throw_error(env, nullptr, "Invalid Date");

    return -1;
  }

  *result = from_local(date.ToLocalChecked());

  return 0;
}

template <Local<Value> Error(Local<String> message)>
static inline int
js_create_error (js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  auto context = to_local(env->context);

  auto error = Error(to_local<String>(message)).As<Object>();

  if (code) {
    error->Set(context, String::NewFromUtf8Literal(env->isolate, "code"), to_local(code)).Check();
  }

  *result = from_local(error);

  return 0;
}

extern "C" int
js_create_error (js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  return js_create_error<Exception::Error>(env, code, message, result);
}

extern "C" int
js_create_type_error (js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  return js_create_error<Exception::TypeError>(env, code, message, result);
}

extern "C" int
js_create_range_error (js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  return js_create_error<Exception::RangeError>(env, code, message, result);
}

extern "C" int
js_create_syntax_error (js_env_t *env, js_value_t *code, js_value_t *message, js_value_t **result) {
  return js_create_error<Exception::SyntaxError>(env, code, message, result);
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

  if (env->depth == 0) env->run_microtasks();

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
    js_throw_error(env, nullptr, "Promise is pending");

    return -1;
  }

  *result = from_local(local->Result());

  return 0;
}

extern "C" int
js_create_arraybuffer (js_env_t *env, size_t len, void **data, js_value_t **result) {
  auto arraybuffer = ArrayBuffer::New(env->isolate, len);

  if (data) {
    *data = arraybuffer->Data();
  }

  *result = from_local(arraybuffer);

  return 0;
}

extern "C" int
js_create_arraybuffer_with_backing_store (js_env_t *env, js_arraybuffer_backing_store_t *backing_store, void **data, size_t *len, js_value_t **result) {
  auto arraybuffer = ArrayBuffer::New(env->isolate, backing_store->backing_store);

  if (data) {
    *data = arraybuffer->Data();
  }

  if (len) {
    *len = arraybuffer->ByteLength();
  }

  *result = from_local(arraybuffer);

  return 0;
}

static void
on_unsafe_arraybuffer_finalize (void *data, size_t len, void *deleter_data) {
  js_heap_t::local()->free(data);
}

extern "C" int
js_create_unsafe_arraybuffer (js_env_t *env, size_t len, void **pdata, js_value_t **result) {
  auto data = js_heap_t::local()->alloc_unsafe(len);

  auto store = ArrayBuffer::NewBackingStore(
    data,
    len,
    on_unsafe_arraybuffer_finalize,
    nullptr
  );

  auto arraybuffer = ArrayBuffer::New(env->isolate, std::move(store));

  if (pdata) {
    *pdata = data;
  }

  *result = from_local(arraybuffer);

  return 0;
}

static void
on_external_arraybuffer_finalize (void *data, size_t len, void *deleter_data) {
  auto finalizer = reinterpret_cast<js_finalizer_t *>(deleter_data);

  if (finalizer) {
    finalizer->finalize_cb(finalizer->env, finalizer->data, finalizer->finalize_hint);

    delete finalizer;
  }
}

extern "C" int
js_create_external_arraybuffer (js_env_t *env, void *data, size_t len, js_finalize_cb finalize_cb, void *finalize_hint, js_value_t **result) {
#if defined(V8_ENABLE_SANDBOX)
  js_throw_error(env, nullptr, "External array buffers are not allowed");

  return -1;
#else
  js_finalizer_t *finalizer = nullptr;

  if (finalize_cb) {
    finalizer = new js_finalizer_t(env, Local<Value>(), data, finalize_cb, finalize_hint);
  }

  auto store = ArrayBuffer::NewBackingStore(
    data,
    len,
    on_external_arraybuffer_finalize,
    finalizer
  );

  auto arraybuffer = ArrayBuffer::New(env->isolate, std::move(store));

  *result = from_local(arraybuffer);

  return 0;
#endif
}

extern "C" int
js_detach_arraybuffer (js_env_t *env, js_value_t *arraybuffer) {
  auto local = to_local<ArrayBuffer>(arraybuffer);

  if (!local->IsDetachable()) {
    js_throw_error(env, nullptr, "Array buffer cannot be detached");

    return -1;
  }

  local->Detach();

  return 0;
}

extern "C" int
js_get_arraybuffer_backing_store (js_env_t *env, js_value_t *arraybuffer, js_arraybuffer_backing_store_t **result) {
  auto local = to_local<ArrayBuffer>(arraybuffer);

  *result = new js_arraybuffer_backing_store_t(local->GetBackingStore());

  return 0;
}

extern "C" int
js_create_sharedarraybuffer (js_env_t *env, size_t len, void **data, js_value_t **result) {
  auto sharedarraybuffer = SharedArrayBuffer::New(env->isolate, len);

  if (data) {
    *data = sharedarraybuffer->Data();
  }

  *result = from_local(sharedarraybuffer);

  return 0;
}

extern "C" int
js_create_sharedarraybuffer_with_backing_store (js_env_t *env, js_arraybuffer_backing_store_t *backing_store, void **data, size_t *len, js_value_t **result) {
  auto sharedarraybuffer = SharedArrayBuffer::New(env->isolate, backing_store->backing_store);

  if (data) {
    *data = sharedarraybuffer->Data();
  }

  if (len) {
    *len = sharedarraybuffer->ByteLength();
  }

  *result = from_local(sharedarraybuffer);

  return 0;
}

static void
on_unsafe_sharedarraybuffer_finalize (void *data, size_t len, void *deleter_data) {
  js_heap_t::local()->free(data);
}

extern "C" int
js_create_unsafe_sharedarraybuffer (js_env_t *env, size_t len, void **pdata, js_value_t **result) {
  auto data = js_heap_t::local()->alloc_unsafe(len);

  auto store = SharedArrayBuffer::NewBackingStore(
    data,
    len,
    on_unsafe_sharedarraybuffer_finalize,
    nullptr
  );

  auto sharedarraybuffer = SharedArrayBuffer::New(env->isolate, std::move(store));

  if (pdata) {
    *pdata = data;
  }

  *result = from_local(sharedarraybuffer);

  return 0;
}

extern "C" int
js_get_sharedarraybuffer_backing_store (js_env_t *env, js_value_t *sharedarraybuffer, js_arraybuffer_backing_store_t **result) {
  auto local = to_local<SharedArrayBuffer>(sharedarraybuffer);

  *result = new js_arraybuffer_backing_store_t(local->GetBackingStore());

  return 0;
}

extern "C" int
js_release_arraybuffer_backing_store (js_env_t *env, js_arraybuffer_backing_store_t *backing_store) {
  delete backing_store;

  return 0;
}

extern "C" int
js_set_arraybuffer_zero_fill_enabled (bool enabled) {
  js_heap_t::local()->zero_fill = enabled;

  return 0;
}

extern "C" int
js_create_typedarray (js_env_t *env, js_typedarray_type_t type, size_t len, js_value_t *arraybuffer, size_t offset, js_value_t **result) {
  auto local = to_local<ArrayBuffer>(arraybuffer);

  Local<TypedArray> typedarray;

  switch (type) {
  case js_int8_array:
    typedarray = Int8Array::New(local, offset, len);
    break;
  case js_uint8_array:
    typedarray = Uint8Array::New(local, offset, len);
    break;
  case js_uint8_clamped_array:
    typedarray = Uint8ClampedArray::New(local, offset, len);
    break;
  case js_int16_array:
    typedarray = Int16Array::New(local, offset, len);
    break;
  case js_uint16_array:
    typedarray = Uint16Array::New(local, offset, len);
    break;
  case js_int32_array:
    typedarray = Int32Array::New(local, offset, len);
    break;
  case js_uint32_array:
    typedarray = Uint32Array::New(local, offset, len);
    break;
  case js_float32_array:
    typedarray = Float32Array::New(local, offset, len);
    break;
  case js_float64_array:
    typedarray = Float64Array::New(local, offset, len);
    break;
  case js_bigint64_array:
    typedarray = BigInt64Array::New(local, offset, len);
    break;
  case js_biguint64_array:
    typedarray = BigUint64Array::New(local, offset, len);
    break;
  }

  *result = from_local(typedarray);

  return 0;
}

extern "C" int
js_create_dataview (js_env_t *env, size_t len, js_value_t *arraybuffer, size_t offset, js_value_t **result) {
  auto local = to_local<ArrayBuffer>(arraybuffer);

  auto dataview = DataView::New(local, offset, len);

  *result = from_local(dataview);

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
js_instanceof (js_env_t *env, js_value_t *object, js_value_t *constructor, bool *result) {
  auto context = to_local(env->context);

  *result = to_local(object)->InstanceOf(context, to_local<Function>(constructor)).FromJust();

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
js_is_native_function (js_env_t *env, js_value_t *value, bool *result) {
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
js_is_wrapped (js_env_t *env, js_value_t *value, bool *result) {
  auto context = to_local(env->context);

  auto local = to_local(value);

  *result = local->IsObject() && local.As<Object>()->HasPrivate(context, to_local(env->wrapper)).FromMaybe(false);

  return 0;
}

extern "C" int
js_is_delegate (js_env_t *env, js_value_t *value, bool *result) {
  auto context = to_local(env->context);

  auto local = to_local(value);

  *result = local->IsObject() && local.As<Object>()->HasPrivate(context, to_local(env->delegate)).FromMaybe(false);

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
js_is_detached_arraybuffer (js_env_t *env, js_value_t *value, bool *result) {
  auto local = to_local(value);

  *result = local->IsArrayBuffer() && local.As<ArrayBuffer>()->WasDetached();

  return 0;
}

extern "C" int
js_is_sharedarraybuffer (js_env_t *env, js_value_t *value, bool *result) {
  *result = to_local(value)->IsSharedArrayBuffer();

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
js_get_value_bool (js_env_t *env, js_value_t *value, bool *result) {
  auto local = to_local<Boolean>(value);

  *result = local->Value();

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
js_get_value_int64 (js_env_t *env, js_value_t *value, int64_t *result) {
  auto local = to_local<Number>(value);

  *result = static_cast<int64_t>(local->Value());

  return 0;
}

extern "C" int
js_get_value_double (js_env_t *env, js_value_t *value, double *result) {
  auto local = to_local<Number>(value);

  *result = local->Value();

  return 0;
}

extern "C" int
js_get_value_bigint_int64 (js_env_t *env, js_value_t *value, int64_t *result, bool *lossless) {
  auto local = to_local<BigInt>(value);

  auto n = local->Int64Value(lossless);

  if (result) *result = n;

  return 0;
}

extern "C" int
js_get_value_bigint_uint64 (js_env_t *env, js_value_t *value, uint64_t *result, bool *lossless) {
  auto local = to_local<BigInt>(value);

  auto n = local->Uint64Value(lossless);

  if (result) *result = n;

  return 0;
}

extern "C" int
js_get_value_string_utf8 (js_env_t *env, js_value_t *value, utf8_t *str, size_t len, size_t *result) {
  auto local = to_local<String>(value);

  if (str == nullptr) {
    *result = local->Utf8Length(env->isolate);
  } else if (len != 0) {
    int written = local->WriteUtf8(
      env->isolate,
      reinterpret_cast<char *>(str),
      len,
      nullptr,
      String::NO_NULL_TERMINATION | String::REPLACE_INVALID_UTF8
    );

    if (written < len) {
      str[written] = '\0';
    }

    if (result) {
      *result = written;
    }
  } else if (result) {
    *result = 0;
  }

  return 0;
}

extern "C" int
js_get_value_string_utf16le (js_env_t *env, js_value_t *value, utf16_t *str, size_t len, size_t *result) {
  auto local = to_local<String>(value);

  if (str == nullptr) {
    *result = local->Length();
  } else if (len != 0) {
    int written = local->Write(
      env->isolate,
      str,
      0,
      len,
      String::NO_NULL_TERMINATION
    );

    if (written < len) {
      str[written] = u'\0';
    }

    if (result) {
      *result = written;
    }
  } else if (result) {
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
js_get_value_date (js_env_t *env, js_value_t *value, double *result) {
  auto local = to_local<Date>(value);

  *result = local->ValueOf();

  return 0;
}

extern "C" int
js_get_array_length (js_env_t *env, js_value_t *value, uint32_t *result) {
  auto local = to_local<Array>(value);

  *result = local->Length();

  return 0;
}

extern "C" int
js_get_prototype (js_env_t *env, js_value_t *object, js_value_t **result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  *result = from_local(local->GetPrototype());

  return 0;
}

extern "C" int
js_get_property_names (js_env_t *env, js_value_t *object, js_value_t **result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  auto mode = KeyCollectionMode::kIncludePrototypes;

  auto property_filter = static_cast<PropertyFilter>(
    PropertyFilter::ONLY_ENUMERABLE |
    PropertyFilter::SKIP_SYMBOLS
  );

  auto index_filter = IndexFilter::kIncludeIndices;

  auto key_conversion = KeyConversionMode::kConvertToString;

  auto names = local->GetPropertyNames(
    context,
    mode,
    property_filter,
    index_filter,
    key_conversion
  );

  *result = from_local(names.ToLocalChecked());

  return 0;
}

extern "C" int
js_get_property (js_env_t *env, js_value_t *object, js_value_t *key, js_value_t **result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  auto value = local->Get(context, to_local<String>(key));

  *result = from_local(value.ToLocalChecked());

  return 0;
}

extern "C" int
js_has_property (js_env_t *env, js_value_t *object, js_value_t *key, bool *result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  *result = local->Has(context, to_local<String>(key)).ToChecked();

  return 0;
}

extern "C" int
js_set_property (js_env_t *env, js_value_t *object, js_value_t *key, js_value_t *value) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  local->Set(context, to_local<String>(key), to_local(value)).Check();

  return 0;
}

extern "C" int
js_delete_property (js_env_t *env, js_value_t *object, js_value_t *key, bool *result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  auto deleted = local->Delete(context, to_local<String>(key)).ToChecked();

  if (result) {
    *result = deleted;
  }

  return 0;
}

extern "C" int
js_get_named_property (js_env_t *env, js_value_t *object, const char *name, js_value_t **result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  auto key = String::NewFromUtf8(env->isolate, name);

  if (key.IsEmpty()) {
    js_throw_error(env, nullptr, "Invalid string length");

    return -1;
  }

  auto value = local->Get(context, key.ToLocalChecked());

  *result = from_local(value.ToLocalChecked());

  return 0;
}

extern "C" int
js_has_named_property (js_env_t *env, js_value_t *object, const char *name, bool *result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  auto key = String::NewFromUtf8(env->isolate, name);

  if (key.IsEmpty()) {
    js_throw_error(env, nullptr, "Invalid string length");

    return -1;
  }

  *result = local->Has(context, key.ToLocalChecked()).ToChecked();

  return 0;
}

extern "C" int
js_set_named_property (js_env_t *env, js_value_t *object, const char *name, js_value_t *value) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  auto key = String::NewFromUtf8(env->isolate, name);

  if (key.IsEmpty()) {
    js_throw_error(env, nullptr, "Invalid string length");

    return -1;
  }

  local->Set(context, key.ToLocalChecked(), to_local(value)).Check();

  return 0;
}

extern "C" int
js_delete_named_property (js_env_t *env, js_value_t *object, const char *name, bool *result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  auto key = String::NewFromUtf8(env->isolate, name);

  if (key.IsEmpty()) {
    js_throw_error(env, nullptr, "Invalid string length");

    return -1;
  }

  auto value = local->Delete(context, key.ToLocalChecked()).ToChecked();

  if (result) {
    *result = value;
  }

  return 0;
}

extern "C" int
js_get_element (js_env_t *env, js_value_t *object, uint32_t index, js_value_t **result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  auto value = local->Get(context, index);

  *result = from_local(value.ToLocalChecked());

  return 0;
}

extern "C" int
js_has_element (js_env_t *env, js_value_t *object, uint32_t index, bool *result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  *result = local->Has(context, index).ToChecked();

  return 0;
}

extern "C" int
js_set_element (js_env_t *env, js_value_t *object, uint32_t index, js_value_t *value) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  local->Set(context, index, to_local(value)).Check();

  return 0;
}

extern "C" int
js_delete_element (js_env_t *env, js_value_t *object, uint32_t index, bool *result) {
  auto context = to_local(env->context);

  auto local = to_local<Object>(object);

  auto deleted = local->Delete(context, index).ToChecked();

  if (result) {
    *result = deleted;
  }

  return 0;
}

extern "C" int
js_get_callback_info (js_env_t *env, const js_callback_info_t *info, size_t *argc, js_value_t *argv[], js_value_t **receiver, void **data) {
  auto v8_info = reinterpret_cast<const FunctionCallbackInfo<Value> &>(*info);

  if (argv) {
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

  if (argc) {
    *argc = v8_info.Length();
  }

  if (receiver) {
    *receiver = from_local(v8_info.This());
  }

  if (data) {
    *data = reinterpret_cast<js_callback_t *>(v8_info.Data().As<External>()->Value())->data;
  }

  return 0;
}

extern "C" int
js_get_new_target (js_env_t *env, const js_callback_info_t *info, js_value_t **result) {
  auto v8_info = reinterpret_cast<const FunctionCallbackInfo<Value> &>(*info);

  *result = from_local(v8_info.NewTarget());

  return 0;
}

extern "C" int
js_get_arraybuffer_info (js_env_t *env, js_value_t *arraybuffer, void **data, size_t *len) {
  auto local = to_local(arraybuffer).As<ArrayBuffer>();

  if (data) {
    *data = local->Data();
  }

  if (len) {
    *len = local->ByteLength();
  }

  return 0;
}

extern "C" int
js_get_typedarray_info (js_env_t *env, js_value_t *typedarray, js_typedarray_type_t *type, void **data, size_t *len, js_value_t **arraybuffer, size_t *offset) {
  auto local = to_local(typedarray).As<TypedArray>();

  if (type) {
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

  if (len) {
    *len = local->Length();
  }

  Local<ArrayBuffer> buffer;

  if (data || arraybuffer) {
    buffer = local->Buffer();
  }

  if (data) {
    *data = static_cast<uint8_t *>(buffer->Data()) + local->ByteOffset();
  }

  if (arraybuffer) {
    *arraybuffer = from_local(buffer);
  }

  if (offset) {
    *offset = local->ByteOffset();
  }

  return 0;
}

extern "C" int
js_get_dataview_info (js_env_t *env, js_value_t *dataview, void **data, size_t *len, js_value_t **arraybuffer, size_t *offset) {
  auto local = to_local(dataview).As<DataView>();

  if (len) {
    *len = local->ByteLength();
  }

  Local<ArrayBuffer> buffer;

  if (data || arraybuffer) {
    buffer = local->Buffer();
  }

  if (data) {
    *data = static_cast<uint8_t *>(buffer->Data()) + local->ByteOffset();
  }

  if (arraybuffer) {
    *arraybuffer = from_local(buffer);
  }

  if (offset) {
    *offset = local->ByteOffset();
  }

  return 0;
}

extern "C" int
js_call_function (js_env_t *env, js_value_t *receiver, js_value_t *function, size_t argc, js_value_t *const argv[], js_value_t **result) {
  auto context = to_local(env->context);

  auto local_receiver = to_local(receiver);

  auto local_function = to_local<Function>(function);

  auto try_catch = TryCatch(env->isolate);

  env->depth++;

  auto local = local_function->Call(
    context,
    local_receiver,
    argc,
    reinterpret_cast<Local<Value> *>(const_cast<js_value_t **>(argv))
  );

  if (env->depth == 1) env->run_microtasks();

  env->depth--;

  if (try_catch.HasCaught()) {
    auto error = try_catch.Exception();

    if (env->depth == 0) {
      on_uncaught_exception(Exception::CreateMessage(env->isolate, error), error);
    } else {
      env->exception.Reset(env->isolate, error);
    }

    return -1;
  }

  if (result) {
    *result = from_local(local.ToLocalChecked());
  }

  return 0;
}

extern "C" int
js_call_function_with_checkpoint (js_env_t *env, js_value_t *receiver, js_value_t *function, size_t argc, js_value_t *const argv[], js_value_t **result) {
  auto context = to_local(env->context);

  auto local_receiver = to_local(receiver);

  auto local_function = to_local<Function>(function);

  auto try_catch = TryCatch(env->isolate);

  env->depth++;

  auto local = local_function->Call(
    context,
    local_receiver,
    argc,
    reinterpret_cast<Local<Value> *>(const_cast<js_value_t **>(argv))
  );

  env->run_microtasks();

  env->depth--;

  if (try_catch.HasCaught()) {
    auto error = try_catch.Exception();

    on_uncaught_exception(Exception::CreateMessage(env->isolate, error), error);

    return -1;
  }

  if (result) {
    *result = from_local(local.ToLocalChecked());
  }

  return 0;
}

extern "C" int
js_new_instance (js_env_t *env, js_value_t *constructor, size_t argc, js_value_t *const argv[], js_value_t **result) {
  auto context = to_local(env->context);

  auto local_constructor = to_local<Function>(constructor);

  auto try_catch = TryCatch(env->isolate);

  env->depth++;

  auto local = local_constructor->NewInstance(
    context,
    argc,
    reinterpret_cast<Local<Value> *>(const_cast<js_value_t **>(argv))
  );

  env->depth--;

  if (try_catch.HasCaught()) {
    auto error = try_catch.Exception();

    if (env->depth == 0) {
      on_uncaught_exception(Exception::CreateMessage(env->isolate, error), error);
    } else {
      env->exception.Reset(env->isolate, error);
    }

    return -1;
  }

  *result = from_local(local.ToLocalChecked());

  return 0;
}

extern "C" int
js_throw (js_env_t *env, js_value_t *error) {
  auto local = to_local(error);

  env->isolate->ThrowException(local);

  env->exception.Reset(env->isolate, local);

  return 0;
}

template <Local<Value> Error(Local<String> message)>
static inline int
js_throw_error (js_env_t *env, const char *code, const char *message) {
  auto context = to_local(env->context);

  auto local = String::NewFromUtf8(env->isolate, message);

  if (local.IsEmpty()) {
    js_throw_error(env, nullptr, "Invalid string length");

    return -1;
  }

  auto error = Error(local.ToLocalChecked()).As<Object>();

  if (code) {
    auto local = String::NewFromUtf8(env->isolate, code);

    if (local.IsEmpty()) {
      js_throw_error(env, nullptr, "Invalid string length");

      return -1;
    }

    error->Set(context, String::NewFromUtf8Literal(env->isolate, "code"), local.ToLocalChecked()).Check();
  }

  return js_throw(env, from_local(error));
}

template <Local<Value> Error(Local<String> message)>
static inline int
js_throw_verrorf (js_env_t *env, const char *code, const char *message, va_list args) {
  va_list args_copy;
  va_copy(args_copy, args);

  auto size = vsnprintf(nullptr, 0, message, args_copy);

  va_end(args_copy);

  size += 1 /* NULL */;

  auto formatted = std::vector<char>(size);

  va_copy(args_copy, args);

  vsnprintf(formatted.data(), size, message, args_copy);

  va_end(args_copy);

  return js_throw_error(env, code, formatted.data());
}

extern "C" int
js_throw_error (js_env_t *env, const char *code, const char *message) {
  return js_throw_error<Exception::Error>(env, code, message);
}

extern "C" int
js_throw_verrorf (js_env_t *env, const char *code, const char *message, va_list args) {
  return js_throw_verrorf<Exception::Error>(env, code, message, args);
}

extern "C" int
js_throw_errorf (js_env_t *env, const char *code, const char *message, ...);

extern "C" int
js_throw_type_error (js_env_t *env, const char *code, const char *message) {
  return js_throw_error<Exception::TypeError>(env, code, message);
}

extern "C" int
js_throw_type_verrorf (js_env_t *env, const char *code, const char *message, va_list args) {
  return js_throw_verrorf<Exception::TypeError>(env, code, message, args);
}

extern "C" int
js_throw_type_errorf (js_env_t *env, const char *code, const char *message, ...);

extern "C" int
js_throw_range_error (js_env_t *env, const char *code, const char *message) {
  return js_throw_error<Exception::RangeError>(env, code, message);
}

extern "C" int
js_throw_range_verrorf (js_env_t *env, const char *code, const char *message, va_list args) {
  return js_throw_verrorf<Exception::RangeError>(env, code, message, args);
}

extern "C" int
js_throw_range_errorf (js_env_t *env, const char *code, const char *message, ...);

extern "C" int
js_throw_syntax_error (js_env_t *env, const char *code, const char *message) {
  return js_throw_error<Exception::SyntaxError>(env, code, message);
}

extern "C" int
js_throw_syntax_verrorf (js_env_t *env, const char *code, const char *message, va_list args) {
  return js_throw_verrorf<Exception::SyntaxError>(env, code, message, args);
}

extern "C" int
js_throw_syntax_errorf (js_env_t *env, const char *code, const char *message, ...);

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

extern "C" int
js_adjust_external_memory (js_env_t *env, int64_t change_in_bytes, int64_t *result) {
  int64_t bytes = env->isolate->AdjustAmountOfExternalAllocatedMemory(change_in_bytes);

  if (result) {
    *result = bytes;
  }

  return 0;
}

extern "C" int
js_request_garbage_collection (js_env_t *env) {
  if (!env->platform->options.expose_garbage_collection) {
    js_throw_error(env, nullptr, "Garbage collection is unavailable");

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
  case js_ffi_pointer:
    v8_type = CTypeInfo::Type::kPointer;
    break;
  case js_ffi_string:
    v8_type = CTypeInfo::Type::kSeqOneByteString;
    break;
  case js_ffi_arraybuffer:
    v8_type = CTypeInfo::Type::kUint8;
    v8_sequence_type = CTypeInfo::SequenceType::kIsArrayBuffer;
    break;
  case js_ffi_uint8array:
    v8_type = CTypeInfo::Type::kUint8;
    v8_sequence_type = CTypeInfo::SequenceType::kIsTypedArray;
    break;
  }

  *result = new js_ffi_type_info_t(v8_type, v8_sequence_type, v8_flags);

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

  *result = new js_ffi_function_info_t(v8_return_info, std::move(v8_arg_info));

  return 0;
}

extern "C" int
js_ffi_create_function (const void *function, const js_ffi_function_info_t *type_info, js_ffi_function_t **result) {
  auto v8_type_info = &type_info->function_info;

  *result = new js_ffi_function_t(function, v8_type_info);

  return 0;
}
