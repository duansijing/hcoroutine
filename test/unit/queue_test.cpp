// test/unit/queue_test.cpp
// SPMC 无锁队列单元测试
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include "src/common/queue.h"
#include <cassert>
#include <thread>
#include <iostream>

int main() {
    // 测试 1: push/pop 基本操作
    {
        hco::SPMCQueue<int> q(4);
        assert(q.empty());
        assert(q.push(42));
        assert(!q.empty());
        assert(q.size() == 1);

        int v = 0;
        assert(q.pop(v));
        assert(v == 42);
        assert(q.empty());
        std::cout << "PASS: push/pop\n";
    }

    // 测试 2: 满队列 push 返回 false
    {
        hco::SPMCQueue<int> q(2);
        assert(q.push(1));
        assert(q.push(2));
        assert(!q.push(3));
        std::cout << "PASS: full queue\n";
    }

    // 测试 3: 空队列 pop 返回 false
    {
        hco::SPMCQueue<int> q(4);
        int v;
        assert(!q.pop(v));
        std::cout << "PASS: empty pop\n";
    }

    // 测试 4: steal 基本操作
    {
        hco::SPMCQueue<int> q(4);
        q.push(10);
        q.push(20);

        int v = 0;
        assert(q.steal(v));
        assert(v == 10);
        assert(q.size() == 1);

        assert(q.pop(v));
        assert(v == 20);
        std::cout << "PASS: steal\n";
    }

    // 测试 5: 多线程 steal
    {
        hco::SPMCQueue<int> q(256);
        std::atomic<int> stolen_sum{0};

        for (int i = 0; i < 100; ++i) {
            q.push(i);
        }

        std::thread thiefs[3];
        for (int t = 0; t < 3; ++t) {
            thiefs[t] = std::thread([&]() {
                int v;
                while (q.steal(v)) {
                    stolen_sum.fetch_add(v, std::memory_order_relaxed);
                }
            });
        }
        for (auto& th : thiefs) th.join();

        int expected = (99 * 100) / 2;
        assert(stolen_sum.load() == expected);
        assert(q.empty());
        std::cout << "PASS: multi-thread steal (sum=" << stolen_sum.load() << ")\n";
    }

    std::cout << "All queue tests passed.\n";
    return 0;
}
