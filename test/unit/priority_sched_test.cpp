// test/unit/priority_sched_test.cpp
// 优先级调度 + Aging 集成测试: 高优先先执行, 低优先级防饥饿
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include <hcoroutine/scheduler.h>
#include <hcoroutine/coroutine.h>
#include <cassert>
#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>
#include <vector>

int main() {
    // 测试 1: 高优先级先于低优先级执行
    // 关键: 两个协程必须在同一时刻都在队列中, 才能观测优先级效果
    {
        std::atomic<int> execution_order{0};
        std::atomic<int> high_done_at{9999};
        std::atomic<int> low_start_at{0};
        std::atomic<int> step{0};

        hco::co_config cfg;
        cfg.worker_count = 1;
        hco::co_init(cfg);

        std::thread runner([]() { hco::co_run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 在一个 setup 协程中同时派发低/高优先级协程, 确保它们同时入队
        hco::co_go([&]() {
            hco::co_options low_opts;
            low_opts.priority = -10;
            hco::co_go([&]() {
                low_start_at.store(step.fetch_add(1));
            }, low_opts);

            hco::co_options high_opts;
            high_opts.priority = 10;
            hco::co_go([&]() {
                high_done_at.store(step.fetch_add(1));
            }, high_opts);
        });

        // 等待两个协程都执行完毕
        auto start = std::chrono::steady_clock::now();
        while (step.load() < 2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count() > 5000) {
                std::cerr << "TIMEOUT: step=" << step.load() << "\n";
                break;
            }
        }

        hco::co_shutdown();
        runner.join();

        // 高优先级 (-10 < 10) 应先出队, step=0=高, step=1=低
        assert(high_done_at.load() == 0);
        assert(low_start_at.load() == 1);
        std::cout << "PASS: high priority executed first (high=" << high_done_at.load()
                  << " low=" << low_start_at.load() << ")\n";
    }

    // 测试 2: Aging — 低优先级等待足够久后获得临时提升, 避免饥饿
    {
        std::atomic<int> low_executed{0};
        std::atomic<int> high_executed{0};
        std::atomic<int> order{0};
        std::atomic<int> low_first_order{9999};
        std::atomic<int> high_first_order{9999};

        hco::co_config cfg;
        cfg.worker_count = 1;
        hco::co_init(cfg);

        std::thread runner([]() { hco::co_run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 在一个 setup 协程中同时派发, 保证都在队列中
        hco::co_go([&]() {
            constexpr int N_LOW = 5;
            constexpr int N_HIGH = 5;
            hco::co_options low_opts;
            low_opts.priority = -10;
            hco::co_options high_opts;
            high_opts.priority = 10;

            // 先派发低优先级, 再派发高优先级
            for (int i = 0; i < N_LOW; ++i) {
                hco::co_go([&]() {
                    int o = order.fetch_add(1);
                    if (low_first_order.load() == 9999) low_first_order.store(o);
                    low_executed.fetch_add(1);
                }, low_opts);
            }

            for (int i = 0; i < N_HIGH; ++i) {
                hco::co_go([&]() {
                    int o = order.fetch_add(1);
                    if (high_first_order.load() == 9999) high_first_order.store(o);
                    high_executed.fetch_add(1);
                }, high_opts);
            }
        });

        auto start = std::chrono::steady_clock::now();
        while (low_executed.load() + high_executed.load() < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count() > 10000) {
                std::cerr << "TIMEOUT: low=" << low_executed.load()
                          << " high=" << high_executed.load() << "\n";
                break;
            }
        }

        hco::co_shutdown();
        runner.join();

        // 高优先级的第一个协程应在低优先级的第一个协程之前执行
        assert(high_first_order.load() < low_first_order.load());
        assert(low_executed.load() == 5);
        assert(high_executed.load() == 5);
        std::cout << "PASS: high priority before low, no starvation (low="
                  << low_executed.load() << " high=" << high_executed.load() << ")\n";
    }

    std::cout << "All priority scheduling tests passed.\n";
    return 0;
}
