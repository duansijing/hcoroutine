// src/sync/cond.cpp
// co_cond 实现: 协程条件变量, wait/signal/broadcast
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include <hcoroutine/cond.h>
#include <hcoroutine/mutex.h>
#include "src/coroutine/coroutine.h"
#include "src/scheduler/worker.h"

namespace hco {

void co_cond::wait(co_mutex& m) {
    Coroutine* self = coroutine_self();
    if (!self) return;

    {
        std::lock_guard<std::mutex> lk(mtx_);
        wait_queue_.push(self);
    }

    // 先释放 mutex, 再挂起
    m.unlock();

    self->reason = SuspendReason::COND;
    coroutine_suspend();

    // 被唤醒后重新获取 mutex
    m.lock();
}

void co_cond::signal() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (wait_queue_.empty()) return;

    Coroutine* co = wait_queue_.front();
    wait_queue_.pop();
    co->state = CoroutineState::READY;
    t_current_worker->enqueue_priority(co);
}

void co_cond::broadcast() {
    std::lock_guard<std::mutex> lk(mtx_);
    while (!wait_queue_.empty()) {
        Coroutine* co = wait_queue_.front();
        wait_queue_.pop();
        co->state = CoroutineState::READY;
        t_current_worker->enqueue_priority(co);
    }
}

} // namespace hco
