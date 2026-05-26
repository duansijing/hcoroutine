// src/scheduler/worker.cpp
// Worker 实现: 主循环 (出队/窃取/执行/挂起处理), 定时器轮询, Reactor 集成
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include "worker.h"
#include <hcoroutine/types.h>
#include "src/common/fcontext/fcontext.h"
#include "src/coroutine/coroutine.h"
#include <functional>
#include <cstdlib>
#include <thread>
#include <algorithm>

namespace hco {

// ---- 外部全局 (在 scheduler.cpp 中定义, 测试时由测试文件提供) ----
extern std::atomic<bool> g_shutting_down;
extern std::vector<Worker*> g_workers;
void worker_steal_from_others(Worker* self, Coroutine*& out);

thread_local Worker* t_current_worker = nullptr;

Worker::Worker(int id, int cpu_id)
    : id_(id), cpu_id_(cpu_id)
{
}

Worker::~Worker() {
    stop();
}

void Worker::start() {
    running_.store(true);
    pthread_create(&thread_, nullptr, thread_func, this);
}

void Worker::stop() {
    running_.store(false);
}

void Worker::join() {
    pthread_join(thread_, nullptr);
}

void Worker::enqueue(Coroutine* co) {
    PrioEntry entry;
    entry.co = co;
    entry.priority = co->priority;
    entry.enqueue_time_ms = now_ms();
    queue_.push(entry);
}

Coroutine* Worker::dequeue() {
    PrioEntry entry;
    if (queue_.pop(entry)) {
        return entry.co;
    }
    return nullptr;
}

bool Worker::steal(Coroutine*& co) {
    PrioEntry entry;
    if (queue_.steal(entry)) {
        co = entry.co;
        return true;
    }
    return false;
}

bool Worker::has_work() const {
    return !queue_.empty();
}

void Worker::enqueue_priority(Coroutine* co) {
    uint64_t now = now_ms();
    if (co->born_time_ms == 0) {
        co->born_time_ms = now;
    }

    // Aging: 低优先级超过 100ms 未执行, 临时提升优先级
    int effective_prio = co->priority;
    if (co->born_time_ms > 0 && now - co->born_time_ms > 100) {
        effective_prio = std::min(20, co->priority + 10);
    }

    PrioEntry entry;
    entry.co = co;
    entry.priority = effective_prio;
    entry.enqueue_time_ms = now;
    queue_.push(entry);
}

void* Worker::thread_func(void* arg) {
    Worker* self = static_cast<Worker*>(arg);
    self->run();
    return nullptr;
}

void Worker::run() {
    t_current_worker = this;

    while (running_.load(std::memory_order_acquire) || has_work() || !timers_.empty()) {
        // 1. 检查定时器, 唤醒到期的 SLEEP 协程
        uint64_t now = now_ms();
        auto expired = timers_.poll(now);
        for (auto* co : expired) {
            co->state = CoroutineState::READY;
            enqueue_priority(co);
        }

        // 2. 从队列获取协程
        PrioEntry entry;
        Coroutine* co = nullptr;

        if (queue_.pop(entry)) {
            co = entry.co;
        } else {
            worker_steal_from_others(this, co);
        }

        if (co) {
            if (co->state == CoroutineState::DEAD) {
                coroutine_destroy(co);
                continue;
            }

            coroutine_resume(co);

            if (co->state == CoroutineState::SUSPENDED) {
                if (co->reason == SuspendReason::YIELD) {
                    enqueue_priority(co);
                } else if (co->reason == SuspendReason::SLEEP) {
                    timers_.add_timer(co, co->wake_time_ms);
                } else if (co->reason == SuspendReason::IO_WAIT) {
                    reactor_.add(co->wait_fd, EPOLLIN | EPOLLOUT, co);
                }
                // MUTEX/COND/CHANNEL 由 waker 负责唤醒
            } else if (co->state == CoroutineState::DEAD) {
                coroutine_destroy(co);
            }
        } else {
            // 3. 轮询 Reactor, 唤醒 I/O 就绪的协程
            auto io_ready = reactor_.poll(0);  // 非阻塞轮询
            for (auto* c : io_ready) {
                c->state = CoroutineState::READY;
                enqueue_priority(c);
            }

            if (queue_.empty()) {
                // 无就绪任务, 根据最近定时器决定等待时长
                uint64_t timeout = timers_.next_timeout(now_ms());
                if (timeout == UINT64_MAX) {
                    // 有 reactor 监听时短暂等待, 否则稍微休眠
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(
                        std::min(timeout, static_cast<uint64_t>(10))));
                }
            }
        }
    }

    t_current_worker = nullptr;
}

// 工作窃取: 从其他 Worker 随机窃取任务
void worker_steal_from_others(Worker* self, Coroutine*& out) {
    if (g_workers.empty()) return;
    size_t victim_idx = rand() % g_workers.size();
    Worker* victim = g_workers[victim_idx];
    if (victim != self) {
        victim->steal(out);
    }
}

} // namespace hco
