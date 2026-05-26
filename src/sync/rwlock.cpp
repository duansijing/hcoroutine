// src/sync/rwlock.cpp
// co_rwlock 实现: 协程读写锁, 多读单写, 写优先防饥饿
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include <hcoroutine/rwlock.h>
#include "src/coroutine/coroutine.h"
#include "src/scheduler/worker.h"

namespace hco {

void co_rwlock::read_lock() {
    Coroutine* self = coroutine_self();
    if (!self) return;

    {
        std::lock_guard<std::mutex> lk(mtx_);
        // 如果有写者持有锁 或 有写者在等待 (避免写饥饿), 则等待
        if (state_ >= 0 && wait_queue_.empty()) {
            state_++;
            return;
        }
        wait_queue_.push({self, false});
    }

    self->reason = SuspendReason::MUTEX;
    coroutine_suspend();
    // 被唤醒后已持有读锁
}

void co_rwlock::read_unlock() {
    std::lock_guard<std::mutex> lk(mtx_);
    state_--;
    if (state_ == 0) {
        wake_next();
    }
}

void co_rwlock::write_lock() {
    Coroutine* self = coroutine_self();
    if (!self) return;

    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (state_ == 0) {
            state_ = -1;
            return;
        }
        wait_queue_.push({self, true});
    }

    self->reason = SuspendReason::MUTEX;
    coroutine_suspend();
    // 被唤醒后已持有写锁
}

void co_rwlock::write_unlock() {
    std::lock_guard<std::mutex> lk(mtx_);
    state_ = 0;
    wake_next();
}

void co_rwlock::wake_next() {
    if (wait_queue_.empty()) return;

    RwWaitEntry e = wait_queue_.front();
    wait_queue_.pop();

    if (e.is_writer) {
        state_ = -1;
        e.co->state = CoroutineState::READY;
        t_current_worker->enqueue_priority(e.co);
    } else {
        // 唤醒连续的所有读者
        state_ = 1;
        e.co->state = CoroutineState::READY;
        t_current_worker->enqueue_priority(e.co);

        while (!wait_queue_.empty() && !wait_queue_.front().is_writer) {
            RwWaitEntry re = wait_queue_.front();
            wait_queue_.pop();
            state_++;
            re.co->state = CoroutineState::READY;
            t_current_worker->enqueue_priority(re.co);
        }
    }
}

} // namespace hco
