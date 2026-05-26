// src/common/prio_queue.h
// 多级优先级队列: 41 级 (-20~+20), 每级一个 SPMCQueue
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <vector>
#include <atomic>
#include <chrono>
#include "queue.h"

namespace hco {

struct PrioEntry {
    struct Coroutine* co = nullptr;
    int               priority = 0;
    uint64_t          enqueue_time_ms = 0;
};

inline uint64_t now_ms() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

// 多级优先级队列: 每个优先级级别一个 SPMCQueue 实例
// LEVELS = 41: 优先级范围 -20 ~ +20 (PRIO_OFFSET = 20)
// push/pop/steal 按优先级从高到低扫描
template<size_t LEVELS = 41, size_t CAPACITY_PER_LEVEL = 64>
class PrioQueue {
public:
    static constexpr int PRIO_OFFSET = 20;  // priority + 20 = index

    PrioQueue() {
        for (size_t i = 0; i < LEVELS; ++i) {
            slots_[i] = new SPMCQueue<PrioEntry>(CAPACITY_PER_LEVEL);
        }
    }

    ~PrioQueue() {
        for (size_t i = 0; i < LEVELS; ++i) {
            delete slots_[i];
        }
    }

    // 禁止拷贝 (每个 level 的 SPMCQueue 不可拷贝)
    PrioQueue(const PrioQueue&) = delete;
    PrioQueue& operator=(const PrioQueue&) = delete;

    // 按优先级入队
    bool push(PrioEntry entry) {
        size_t slot = entry.priority + PRIO_OFFSET;
        if (slot >= LEVELS) slot = LEVELS - 1;
        return slots_[slot]->push(entry);
    }

    // 从高到低扫描出队
    bool pop(PrioEntry& entry) {
        for (int i = static_cast<int>(LEVELS) - 1; i >= 0; --i) {
            if (slots_[i]->pop(entry)) return true;
        }
        return false;
    }

    // 从高到低扫描窃取 (多消费者安全)
    bool steal(PrioEntry& entry) {
        for (int i = static_cast<int>(LEVELS) - 1; i >= 0; --i) {
            if (slots_[i]->steal(entry)) return true;
        }
        return false;
    }

    // 检查所有级别是否都为空
    bool empty() const {
        for (size_t i = 0; i < LEVELS; ++i) {
            if (!slots_[i]->empty()) return false;
        }
        return true;
    }

private:
    // 使用指针数组, 因为 SPMCQueue 不可拷贝
    SPMCQueue<PrioEntry>* slots_[LEVELS];
};

} // namespace hco
