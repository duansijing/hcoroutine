// include/hcoroutine/mutex.h
// 协程互斥锁 co_mutex 与 RAII 守卫 co_lock_guard
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <queue>
#include <mutex>

namespace hco {

struct Coroutine;

class co_mutex {
public:
    co_mutex() = default;
    ~co_mutex() = default;

    co_mutex(const co_mutex&) = delete;
    co_mutex& operator=(const co_mutex&) = delete;

    void lock();
    void unlock();
    bool try_lock();

private:
    std::mutex mtx_;
    Coroutine* owner_ = nullptr;
    std::queue<Coroutine*> wait_queue_;
};

// RAII 锁守卫
template<typename M>
class co_lock_guard {
public:
    explicit co_lock_guard(M& m) : m_(m) { m_.lock(); }
    ~co_lock_guard() { m_.unlock(); }

    co_lock_guard(const co_lock_guard&) = delete;
    co_lock_guard& operator=(const co_lock_guard&) = delete;

private:
    M& m_;
};

} // namespace hco
