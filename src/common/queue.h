// src/common/queue.h
// SPMC 无锁环形队列: 单生产者入队, CAS 多消费者出队/窃取
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <atomic>
#include <vector>
#include <cstddef>

namespace hco {

// SPMC: 单生产者多消费者 有界无锁队列
// owner 线程 push, 其他线程 steal
template<typename T>
class SPMCQueue {
public:
    explicit SPMCQueue(size_t capacity) {
        capacity_ = 1;
        while (capacity_ < capacity) capacity_ <<= 1;
        mask_ = capacity_ - 1;
        buffer_.resize(capacity_);
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    bool push(T item) {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_relaxed);
        if (t - h >= capacity_) return false;
        buffer_[t & mask_] = item;
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t;
        for (;;) {
            t = tail_.load(std::memory_order_acquire);
            if (h >= t) return false;
            T val = buffer_[h & mask_];
            if (head_.compare_exchange_weak(h, h + 1,
                     std::memory_order_release,
                     std::memory_order_relaxed)) {
                item = val;
                return true;
            }
            // CAS 失败: h 已更新为当前 head, 重试
        }
    }

    bool steal(T& item) {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t;
        for (;;) {
            t = tail_.load(std::memory_order_acquire);
            if (h >= t) return false;
            T val = buffer_[h & mask_];
            if (head_.compare_exchange_weak(h, h + 1,
                    std::memory_order_release,
                    std::memory_order_relaxed)) {
                item = val;
                return true;
            }
            // CAS 失败: h 已更新为当前 head, 重试
        }
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) >=
               tail_.load(std::memory_order_acquire);
    }

    size_t size() const {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_acquire);
        return (t >= h) ? (t - h) : 0;
    }

private:
    size_t capacity_;
    size_t mask_;
    std::vector<T> buffer_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
};

} // namespace hco
