// test/unit/sync_test.cpp
// 同步原语集成测试: mutex / cond / rwlock / channel
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include <hcoroutine/scheduler.h>
#include <hcoroutine/coroutine.h>
#include <hcoroutine/mutex.h>
#include <hcoroutine/cond.h>
#include <hcoroutine/rwlock.h>
#include <hcoroutine/waitgroup.h>
#include <hcoroutine/channel.h>
#include <cassert>
#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>

// 辅助: 轮询等待条件
static bool wait_for(std::function<bool()> cond, int timeout_ms = 5000) {
    auto start = std::chrono::steady_clock::now();
    while (!cond()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() > timeout_ms) {
            return false;
        }
    }
    return true;
}

int main() {
    // ========== co_mutex 测试 ==========

    // 测试 1: 互斥 — N 个协程递增共享计数器
    {
        std::atomic<int> counter{0};
        hco::co_mutex mtx;

        hco::co_config cfg;
        cfg.worker_count = 4;
        hco::co_init(cfg);

        std::thread runner([]() { hco::co_run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        constexpr int N = 200;
        std::atomic<int> finished{0};
        for (int i = 0; i < N; ++i) {
            hco::co_go([&]() {
                hco::co_lock_guard<hco::co_mutex> lk(mtx);
                int v = counter.load();
                counter.store(v + 1);
                finished.fetch_add(1);
            });
        }

        bool ok = wait_for([&]() { return counter.load() == N; });
        if (!ok) {
            std::cerr << "DEBUG: counter=" << counter.load()
                      << " finished=" << finished.load()
                      << " expected=" << N << "\n";
        }
        assert(ok);
        hco::co_shutdown();
        runner.join();

        assert(counter.load() == N);
        std::cout << "PASS: co_mutex mutual exclusion (" << N << " increments)\n";
    }

    // 测试 2: 协程 A 持有锁时, 协程 B 的 try_lock 应失败
    {
        hco::co_mutex mtx;
        std::atomic<bool> a_locked{false};
        std::atomic<bool> b_try_result{true};

        hco::co_config cfg;
        cfg.worker_count = 1;
        hco::co_init(cfg);

        std::thread runner([]() { hco::co_run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 协程 A: 获取锁然后挂起 (保持持有)
        hco::co_go([&]() {
            mtx.lock();
            a_locked.store(true);
            // 等待 B 完成尝试
            while (!b_try_result.load()) { hco::co_yield(); } // B 设为 false 后跳出
            hco::co_yield(); // 让 B 执行一次
            mtx.unlock();
        });

        // 协程 B: 等待 A 获取锁后尝试 try_lock
        hco::co_go([&]() {
            while (!a_locked.load()) { hco::co_yield(); }
            b_try_result.store(mtx.try_lock());
        });

        assert(wait_for([&]() { return !b_try_result.load(); }));
        hco::co_shutdown();
        runner.join();

        assert(!b_try_result.load());
        std::cout << "PASS: co_mutex try_lock\n";
    }

    // ========== co_cond 测试 ==========

    // 测试 3: signal 唤醒等待者
    {
        std::atomic<bool> woken{false};
        std::atomic<bool> signaled{false};
        hco::co_mutex mtx;
        hco::co_cond cv;

        hco::co_config cfg;
        cfg.worker_count = 2;
        hco::co_init(cfg);

        std::thread runner([]() { hco::co_run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 协程 A: 等待信号
        hco::co_go([&]() {
            hco::co_lock_guard<hco::co_mutex> lk(mtx);
            while (!signaled.load()) {
                cv.wait(mtx);
            }
            woken.store(true);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // 协程 B: 发出信号
        hco::co_go([&]() {
            {
                hco::co_lock_guard<hco::co_mutex> lk(mtx);
                signaled.store(true);
                cv.signal();
            }
        });

        assert(wait_for([&]() { return woken.load(); }));
        hco::co_shutdown();
        runner.join();

        assert(woken.load());
        std::cout << "PASS: co_cond signal\n";
    }

    // ========== co_rwlock 测试 ==========

    // 测试 4: 写者独占访问
    {
        std::atomic<int> concurrent{0};
        std::atomic<int> violations{0};
        hco::co_rwlock rwl;

        hco::co_config cfg;
        cfg.worker_count = 2;
        hco::co_init(cfg);

        std::thread runner([]() { hco::co_run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        constexpr int N = 20;
        std::atomic<int> done{0};
        for (int i = 0; i < N; ++i) {
            hco::co_go([&]() {
                rwl.write_lock();
                int c = concurrent.fetch_add(1) + 1;
                if (c > 1) violations.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                concurrent.fetch_sub(1);
                rwl.write_unlock();
                done.fetch_add(1);
            });
        }

        assert(wait_for([&]() { return done.load() == N; }, 10000));
        hco::co_shutdown();
        runner.join();

        assert(violations.load() == 0);
        std::cout << "PASS: co_rwlock writer exclusivity (" << N << " writers)\n";
    }

    // ========== co_channel 测试 ==========

    // 测试 5: Channel send/recv
    {
        hco::co_channel<int> ch(5);
        std::atomic<int> result{-1};

        hco::co_config cfg;
        cfg.worker_count = 2;
        hco::co_init(cfg);

        std::thread runner([]() { hco::co_run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        hco::co_go([&]() { ch.send(42); });
        hco::co_go([&]() { int v; ch.recv(v); result.store(v); });

        assert(wait_for([&]() { return result.load() == 42; }));
        hco::co_shutdown();
        runner.join();

        assert(result.load() == 42);
        std::cout << "PASS: co_channel send/recv\n";
    }

    // 测试 6: Channel 缓冲 3 个值
    {
        hco::co_channel<int> ch(3);
        std::atomic<int> sum{0};

        hco::co_config cfg;
        cfg.worker_count = 2;
        hco::co_init(cfg);

        std::thread runner([]() { hco::co_run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        hco::co_go([&]() { ch.send(1); ch.send(2); ch.send(3); });
        hco::co_go([&]() { int a,b,c; ch.recv(a); ch.recv(b); ch.recv(c); sum.store(a+b+c); });

        assert(wait_for([&]() { return sum.load() == 6; }));
        hco::co_shutdown();
        runner.join();

        assert(sum.load() == 6);
        std::cout << "PASS: co_channel buffered (sum=6)\n";
    }

    std::cout << "All sync tests passed.\n";
    return 0;
}
