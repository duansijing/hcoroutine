// include/hcoroutine/cond.h
// 协程条件变量 co_cond: wait / signal / broadcast
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <queue>
#include <mutex>

namespace hco {

class co_mutex;
struct Coroutine;

class co_cond {
public:
    co_cond() = default;
    ~co_cond() = default;

    co_cond(const co_cond&) = delete;
    co_cond& operator=(const co_cond&) = delete;

    // 原子地释放 mutex 并挂起, 被唤醒后重新获取 mutex
    void wait(co_mutex& m);

    // 唤醒一个等待者
    void signal();

    // 唤醒所有等待者
    void broadcast();

private:
    std::mutex mtx_;
    std::queue<Coroutine*> wait_queue_;
};

} // namespace hco
