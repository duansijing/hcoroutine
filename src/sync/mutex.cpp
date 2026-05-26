// src/sync/mutex.cpp
// co_mutex 实现: 协程互斥锁, 挂起等待 + 唤醒队列
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include <hcoroutine/mutex.h>
#include "src/coroutine/coroutine.h"
#include "src/scheduler/worker.h"

namespace hco {

void co_mutex::lock() {
    Coroutine* self = coroutine_self();
    if (!self) return;  // 非协程上下文无操作

    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!owner_) {
            owner_ = self;
            return;
        }
        wait_queue_.push(self);
    }

    self->reason = SuspendReason::MUTEX;
    coroutine_suspend();
}

void co_mutex::unlock() {
    std::lock_guard<std::mutex> lk(mtx_);

    if (!wait_queue_.empty()) {
        Coroutine* next = wait_queue_.front();
        wait_queue_.pop();
        owner_ = next;
        next->state = CoroutineState::READY;
        t_current_worker->enqueue_priority(next);
        return;
    }

    owner_ = nullptr;
}

bool co_mutex::try_lock() {
    Coroutine* self = coroutine_self();
    if (!self) return false;

    std::lock_guard<std::mutex> lk(mtx_);
    if (!owner_) {
        owner_ = self;
        return true;
    }
    return false;
}

} // namespace hco
