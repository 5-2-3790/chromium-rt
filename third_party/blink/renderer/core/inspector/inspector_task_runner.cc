// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"

#include "third_party/blink/renderer/core/inspector/thread_debugger.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

InspectorTaskRunner::IgnoreInterruptsScope::IgnoreInterruptsScope(
    scoped_refptr<InspectorTaskRunner> task_runner)
    : was_ignoring_(task_runner->ignore_interrupts_),
      task_runner_(task_runner) {
  // There may be nested scopes e.g. when tasks are being executed on XHR
  // breakpoint.
  task_runner_->ignore_interrupts_ = true;
}

InspectorTaskRunner::IgnoreInterruptsScope::~IgnoreInterruptsScope() {
  task_runner_->ignore_interrupts_ = was_ignoring_;
}

InspectorTaskRunner::InspectorTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> isolate_task_runner)
    : isolate_task_runner_(isolate_task_runner) {}

InspectorTaskRunner::~InspectorTaskRunner() = default;

void InspectorTaskRunner::InitIsolate(v8::Isolate* isolate) {
  MutexLocker lock(mutex_);
  isolate_ = isolate;
}

void InspectorTaskRunner::Dispose() {
  MutexLocker lock(mutex_);
  disposed_ = true;
  isolate_ = nullptr;
  isolate_task_runner_ = nullptr;
  condition_.Broadcast();
}

void InspectorTaskRunner::AppendTask(Task task) {
  MutexLocker lock(mutex_);
  if (disposed_)
    return;
  queue_.push_back(std::move(task));
  condition_.Signal();
  PostCrossThreadTask(
      *isolate_task_runner_, FROM_HERE,
      CrossThreadBind(&InspectorTaskRunner::PerformSingleTaskDontWait,
                      WrapRefCounted(this)));
  if (isolate_)
    isolate_->RequestInterrupt(&V8InterruptCallback, this);
}

bool InspectorTaskRunner::WaitForAndRunSingleTask() {
  // |isolate_task_runner_| might be null in unit tests.
  DCHECK(!isolate_task_runner_ ||
         isolate_task_runner_->BelongsToCurrentThread());
  {
    MutexLocker lock(mutex_);
    if (isolate_)
      ThreadDebugger::IdleStarted(isolate_);
  }
  Task task = TakeNextTask(kWaitForTask);
  {
    MutexLocker lock(mutex_);
    if (isolate_)
      ThreadDebugger::IdleFinished(isolate_);
  }
  if (!task)
    return false;
  PerformSingleTask(std::move(task));
  return true;
}

bool InspectorTaskRunner::IsRunningTask() {
  MutexLocker lock(mutex_);
  return running_task_;
}

InspectorTaskRunner::Task InspectorTaskRunner::TakeNextTask(
    InspectorTaskRunner::WaitMode wait_mode) {
  MutexLocker lock(mutex_);
  bool timed_out = false;

  static double infinite_time = std::numeric_limits<double>::max();
  double absolute_time = wait_mode == kWaitForTask ? infinite_time : 0.0;
  while (!disposed_ && !timed_out && queue_.IsEmpty())
    timed_out = !condition_.TimedWait(mutex_, absolute_time);
  DCHECK(!timed_out || absolute_time != infinite_time);

  if (disposed_ || timed_out)
    return Task();

  SECURITY_DCHECK(!queue_.IsEmpty());
  return queue_.TakeFirst();
}

void InspectorTaskRunner::PerformSingleTask(Task task) {
  DCHECK(isolate_task_runner_->BelongsToCurrentThread());
  IgnoreInterruptsScope scope(this);
  {
    MutexLocker lock(mutex_);
    DCHECK(!running_task_);
    running_task_ = true;
  }
  std::move(task).Run();
  {
    MutexLocker lock(mutex_);
    running_task_ = false;
  }
}

void InspectorTaskRunner::PerformSingleTaskDontWait() {
  Task task = TakeNextTask(kDontWaitForTask);
  if (task) {
    DCHECK(isolate_task_runner_->BelongsToCurrentThread());
    PerformSingleTask(std::move(task));
  }
}

void InspectorTaskRunner::V8InterruptCallback(v8::Isolate*, void* data) {
  InspectorTaskRunner* runner = static_cast<InspectorTaskRunner*>(data);
  DCHECK(runner->isolate_task_runner_->BelongsToCurrentThread());
  if (runner->ignore_interrupts_)
    return;
  while (true) {
    Task task = runner->TakeNextTask(kDontWaitForTask);
    if (!task)
      return;
    runner->PerformSingleTask(std::move(task));
  }
}

}  // namespace blink
