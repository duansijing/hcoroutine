// include/hcoroutine/channel.h
// 协程通道 co_channel<T>: 带缓冲的协程间通信
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <queue>
#include <mutex>
#include <vector>

#include "src/coroutine/coroutine.h"
#include "src/scheduler/worker.h"

namespace hco {

template<typename T>
class co_channel {
public:
    explicit co_channel(size_t capacity = 1)
        : buffer_(capacity)
        , capacity_(capacity)
        , closed_(false)
    {}

    ~co_channel() = default;

    co_channel(const co_channel&) = delete;
    co_channel& operator=(const co_channel&) = delete;

    // 发送: 缓冲区满则挂起当前协程; 通道关闭则静默返回
    void send(const T& val) {
        Coroutine* self = coroutine_self();

        for (;;) {
            bool should_suspend = false;
            {
                std::lock_guard<std::mutex> lk(mtx_);
                if (closed_) return;

                if (count_ < capacity_) {
                    buffer_[write_pos_] = val;
                    write_pos_ = (write_pos_ + 1) % capacity_;
                    count_++;

                    // 唤醒一个等待的接收者
                    if (!recv_wait_.empty()) {
                        Coroutine* recver = recv_wait_.front();
                        recv_wait_.pop();
                        recver->state = CoroutineState::READY;
                        t_current_worker->enqueue_priority(recver);
                    }
                    return;
                }

                // 缓冲区满, 加入等待队列
                if (self) {
                    send_wait_.push(self);
                    should_suspend = true;
                }
            }

            if (should_suspend) {
                self->reason = SuspendReason::CHANNEL;
                coroutine_suspend();
                // 恢复后回到循环开头, 重新检查状态
            }
        }
    }

    // 接收: 将值写入 out, 返回 true
    // 通道关闭且缓冲区为空时返回 false (不会抛异常)
    bool recv(T& out) {
        Coroutine* self = coroutine_self();

        for (;;) {
            bool should_suspend = false;
            {
                std::lock_guard<std::mutex> lk(mtx_);

                if (count_ > 0) {
                    out = buffer_[read_pos_];
                    read_pos_ = (read_pos_ + 1) % capacity_;
                    count_--;

                    // 唤醒一个等待的发送者
                    if (!send_wait_.empty()) {
                        Coroutine* sender = send_wait_.front();
                        send_wait_.pop();
                        sender->state = CoroutineState::READY;
                        t_current_worker->enqueue_priority(sender);
                    }
                    return true;
                }

                // 通道已关闭且无数据可读
                if (closed_ && send_wait_.empty()) {
                    return false;
                }

                // 缓冲区空, 加入等待队列
                if (self) {
                    recv_wait_.push(self);
                    should_suspend = true;
                }
            }

            if (should_suspend) {
                self->reason = SuspendReason::CHANNEL;
                coroutine_suspend();
                // 恢复后回到循环开头, 重新检查状态
            }
        }
    }

    void close() {
        std::lock_guard<std::mutex> lk(mtx_);
        closed_ = true;
        // 唤醒所有等待的发送者和接收者
        while (!send_wait_.empty()) {
            Coroutine* co = send_wait_.front();
            send_wait_.pop();
            co->state = CoroutineState::READY;
            t_current_worker->enqueue_priority(co);
        }
        while (!recv_wait_.empty()) {
            Coroutine* co = recv_wait_.front();
            recv_wait_.pop();
            co->state = CoroutineState::READY;
            t_current_worker->enqueue_priority(co);
        }
    }

    bool is_closed() const { return closed_; }

private:
    std::vector<T> buffer_;
    size_t capacity_;
    size_t count_ = 0;
    size_t read_pos_ = 0;
    size_t write_pos_ = 0;
    bool closed_;

    std::mutex mtx_;
    std::queue<Coroutine*> send_wait_;
    std::queue<Coroutine*> recv_wait_;
};

} // namespace hco
