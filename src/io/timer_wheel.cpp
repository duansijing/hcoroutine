// src/io/timer_wheel.cpp
// TimerWheel 实现: add_timer / poll / next_timeout
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include "timer_wheel.h"
#include "src/coroutine/coroutine.h"
#include "src/scheduler/worker.h"
#include <algorithm>

namespace hco {

void TimerWheel::add_timer(Coroutine* co, uint64_t wake_time_ms) {
    std::lock_guard<std::mutex> lk(mtx_);
    timers_.emplace(wake_time_ms, co);
}

std::vector<Coroutine*> TimerWheel::poll(uint64_t now_ms) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<Coroutine*> ready;

    auto it = timers_.begin();
    while (it != timers_.end() && it->first <= now_ms) {
        ready.push_back(it->second);
        it = timers_.erase(it);
    }
    return ready;
}

uint64_t TimerWheel::next_timeout(uint64_t now_ms) const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (timers_.empty()) return UINT64_MAX;
    auto it = timers_.begin();
    if (it->first <= now_ms) return 0;
    return it->first - now_ms;
}

bool TimerWheel::empty() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return timers_.empty();
}

} // namespace hco
