// src/io/timer_wheel.h
// 时间轮定时器: 基于 multimap 的毫秒级定时, 用于 co_sleep 唤醒
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include <mutex>

namespace hco {

struct Coroutine;

class TimerWheel {
public:
    TimerWheel() = default;

    // 添加定时器: wake_time_ms 为绝对唤醒时间 (毫秒)
    void add_timer(Coroutine* co, uint64_t wake_time_ms);

    // 轮询: 返回所有已到期的协程
    std::vector<Coroutine*> poll(uint64_t now_ms);

    // 下一个定时器的到期时间, 无定时器返回 UINT64_MAX
    uint64_t next_timeout(uint64_t now_ms) const;

    bool empty() const;

private:
    mutable std::mutex mtx_;
    std::multimap<uint64_t, Coroutine*> timers_;
};

} // namespace hco
