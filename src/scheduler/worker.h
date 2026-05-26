// src/scheduler/worker.h
// Worker 线程: 拥有本地优先级队列, 执行协程, 支持工作窃取与定时器/Reactor 集成
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <pthread.h>
#include <atomic>
#include <vector>
#include <functional>
#include "src/common/prio_queue.h"
#include "src/io/timer_wheel.h"
#include "src/io/reactor.h"

namespace hco {

struct Coroutine;

class Worker {
public:
    Worker(int id, int cpu_id = -1);
    ~Worker();

    void start();
    void stop();
    void join();

    // 入队协程
    void enqueue(Coroutine* co);

    // 从本 Worker pop
    Coroutine* dequeue();

    // 从本 Worker 窃取
    bool steal(Coroutine*& co);

    bool has_work() const;

    void enqueue_priority(Coroutine* co);

    int id() const { return id_; }

private:
    void run();

    int id_;
    int cpu_id_;
    pthread_t thread_;

    PrioQueue<> queue_;
    TimerWheel timers_;
    Reactor reactor_;

    std::atomic<bool> running_{false};

    static void* thread_func(void* arg);

    friend class Scheduler;
};

// 当前线程正在运行的 Worker (由 Worker::run 设置, 供同步原语使用)
extern thread_local Worker* t_current_worker;

} // namespace hco
