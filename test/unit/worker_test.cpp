// test/unit/worker_test.cpp
// Worker 线程调度单元测试
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include "src/scheduler/worker.h"
#include "src/coroutine/coroutine.h"
#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>

// 提供 worker.cpp 中 extern 引用的全局变量定义
namespace hco {
std::atomic<bool> g_shutting_down{false};
std::vector<Worker*> g_workers;
}

int main() {
    // 测试 1: Worker 创建和启动
    {
        hco::Worker w(0, -1);
        assert(!w.has_work());
        w.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        w.stop();
        w.join();
        std::cout << "PASS: worker start/stop\n";
    }

    // 测试 2: 入队协程后被 Worker 执行
    {
        std::atomic<bool> executed{false};
        hco::Coroutine* co = hco::coroutine_create([&]() {
            executed.store(true);
        }, {});

        hco::g_workers.clear();
        hco::Worker w(0);
        hco::g_workers.push_back(&w);
        hco::g_shutting_down.store(false);

        w.enqueue(co);
        w.start();

        for (int i = 0; i < 100 && !executed.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        w.stop();
        w.join();

        assert(executed.load());
        std::cout << "PASS: worker executes coroutine\n";
    }

    std::cout << "All worker tests passed.\n";
    return 0;
}
