// src/sync/waitgroup.cpp
// co_waitgroup 实现: 计数器归零时唤醒所有等待者
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include <hcoroutine/waitgroup.h>
#include "src/coroutine/coroutine.h"
#include "src/scheduler/worker.h"

namespace hco {

void co_waitgroup::add(int n) {
    counter_.fetch_add(n, std::memory_order_release);
}

void co_waitgroup::done() {
    int prev = counter_.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1) {
        // 减到 0, 唤醒所有等待者
        std::lock_guard<std::mutex> lk(mtx_);
        while (!wait_queue_.empty()) {
            Coroutine* co = wait_queue_.front();
            wait_queue_.pop();
            worker_wake_coroutine(co);
        }
    }
}

void co_waitgroup::wait() {
    if (counter_.load(std::memory_order_acquire) == 0) return;

    Coroutine* self = coroutine_self();
    if (!self) return;

    {
        std::lock_guard<std::mutex> lk(mtx_);
        // 双重检查: 可能在获取锁期间 counter 变为 0
        if (counter_.load(std::memory_order_acquire) == 0) return;
        wait_queue_.push(self);
    }

    self->reason = SuspendReason::COND;
    coroutine_suspend();
}

} // namespace hco
