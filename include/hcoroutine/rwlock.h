// include/hcoroutine/rwlock.h
// 协程读写锁 co_rwlock: 多读单写, 写优先防饥饿
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <queue>
#include <mutex>

namespace hco {

struct Coroutine;

struct RwWaitEntry {
    Coroutine* co;
    bool is_writer;
};

class co_rwlock {
public:
    co_rwlock() = default;
    ~co_rwlock() = default;

    co_rwlock(const co_rwlock&) = delete;
    co_rwlock& operator=(const co_rwlock&) = delete;

    void read_lock();
    void read_unlock();
    void write_lock();
    void write_unlock();

private:
    void wake_next();

    std::mutex mtx_;
    // 0 = 空闲, -1 = 写者持有, >0 = 读者数量
    int state_ = 0;
    std::queue<RwWaitEntry> wait_queue_;
};

} // namespace hco
