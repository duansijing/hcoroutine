// include/hcoroutine/waitgroup.h
// 协程等待组 co_waitgroup: add / done / wait
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <atomic>
#include <queue>
#include <mutex>

namespace hco {

struct Coroutine;

class co_waitgroup {
public:
    co_waitgroup() = default;
    ~co_waitgroup() = default;

    void add(int n = 1);
    void done();
    void wait();

private:
    std::atomic<int> counter_{0};
    std::mutex mtx_;
    std::queue<Coroutine*> wait_queue_;
};

} // namespace hco
