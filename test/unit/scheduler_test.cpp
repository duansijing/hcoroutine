// test/unit/scheduler_test.cpp
// 调度器集成测试: 多协程并发执行
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include <hcoroutine/scheduler.h>
#include <hcoroutine/coroutine.h>
#include <cassert>
#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>

int main() {
    // 测试 1: co_init + co_run 启动单协程
    {
        std::atomic<int> counter{0};

        hco::co_config cfg;
        cfg.worker_count = 2;
        hco::co_init(cfg);

        std::thread runner([]() {
            hco::co_run();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        hco::co_go([&]() {
            counter.fetch_add(1);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        hco::co_shutdown();
        runner.join();

        assert(counter.load() == 1);
        std::cout << "PASS: co_init + co_run + single coroutine\n";
    }

    // 测试 2: 多协程并发执行
    {
        std::atomic<int> counter{0};
        constexpr int N = 100;

        hco::co_config cfg;
        cfg.worker_count = 4;
        hco::co_init(cfg);

        std::thread runner([]() { hco::co_run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        for (int i = 0; i < N; ++i) {
            hco::co_go([&]() {
                counter.fetch_add(1);
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        hco::co_shutdown();
        runner.join();

        assert(counter.load() == N);
        std::cout << "PASS: " << N << " coroutines executed\n";
    }

    std::cout << "All scheduler tests passed.\n";
    return 0;
}
