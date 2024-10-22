// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/test/create_task_queue_manager_for_test.h"

#include "platform/scheduler/base/task_queue_manager.h"
#include "platform/scheduler/base/thread_controller_impl.h"

namespace blink {
namespace scheduler {

namespace {

class TaskQueueManagerForTest : public TaskQueueManager {
 public:
  explicit TaskQueueManagerForTest(
      std::unique_ptr<internal::ThreadController> thread_controller)
      : TaskQueueManager(std::move(thread_controller)) {}
};

class ThreadControllerForTest : public internal::ThreadControllerImpl {
 public:
  ThreadControllerForTest(
      base::MessageLoop* message_loop,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::TickClock* time_source)
      : ThreadControllerImpl(message_loop,
                             std::move(task_runner),
                             time_source) {}

  void AddNestingObserver(base::RunLoop::NestingObserver* observer) override {
    if (!message_loop_)
      return;
    ThreadControllerImpl::AddNestingObserver(observer);
  }

  void RemoveNestingObserver(
      base::RunLoop::NestingObserver* observer) override {
    if (!message_loop_)
      return;
    ThreadControllerImpl::RemoveNestingObserver(observer);
  }

  ~ThreadControllerForTest() override = default;
};

}  // namespace

std::unique_ptr<TaskQueueManager> CreateTaskQueueManagerWithUnownedClockForTest(
    base::MessageLoop* message_loop,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::SimpleTestTickClock* clock) {
  return CreateTaskQueueManagerForTest(message_loop, std::move(task_runner),
                                       clock);
}

std::unique_ptr<TaskQueueManager> CreateTaskQueueManagerForTest(
    base::MessageLoop* message_loop,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::TickClock* clock) {
  return std::make_unique<TaskQueueManagerForTest>(
      std::make_unique<ThreadControllerForTest>(message_loop,
                                                std::move(task_runner), clock));
}

}  // namespace scheduler
}  // namespace blink
