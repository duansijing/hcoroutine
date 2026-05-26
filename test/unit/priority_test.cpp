// test/unit/priority_test.cpp
// 多级优先级队列单元测试
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include "src/common/prio_queue.h"
#include <cassert>
#include <iostream>

int main() {
    // 测试 1: 高优先级先出队
    {
        hco::PrioQueue<> q;
        hco::PrioEntry e1 {nullptr, -10, 0};
        hco::PrioEntry e2 {nullptr, 0,   0};
        hco::PrioEntry e3 {nullptr, 10,  0};
        q.push(e1);
        q.push(e2);
        q.push(e3);

        hco::PrioEntry out;
        assert(q.pop(out));
        assert(out.priority == 10);  // 最高优先级的先出
        std::cout << "PASS: high priority first\n";
    }

    // 测试 2: 同优先级 FIFO
    {
        hco::PrioQueue<> q;
        hco::PrioEntry e1 {nullptr, 0, 100};
        hco::PrioEntry e2 {nullptr, 0, 200};
        q.push(e1);
        q.push(e2);

        hco::PrioEntry out;
        assert(q.pop(out));
        assert(out.enqueue_time_ms == 100);  // 先入先出
        assert(q.pop(out));
        assert(out.enqueue_time_ms == 200);
        std::cout << "PASS: FIFO within same priority\n";
    }

    // 测试 3: 窃取最高优先级
    {
        hco::PrioQueue<> q;
        hco::PrioEntry e1 {nullptr, -5, 0};
        hco::PrioEntry e2 {nullptr, 15, 0};
        q.push(e1);
        q.push(e2);

        hco::PrioEntry out;
        assert(q.steal(out));
        assert(out.priority == 15);  // 窃取返回最高优先级
        std::cout << "PASS: steal highest priority\n";
    }

    // 测试 4: 空队列操作
    {
        hco::PrioQueue<> q;
        assert(q.empty());

        hco::PrioEntry out;
        assert(!q.pop(out));
        assert(!q.steal(out));
        std::cout << "PASS: empty queue operations\n";
    }

    // 测试 5: 边界优先级 (-20 和 +20)
    {
        hco::PrioQueue<> q;
        hco::PrioEntry e_low  {nullptr, -20, 0};
        hco::PrioEntry e_high {nullptr, 20,  0};
        q.push(e_low);
        q.push(e_high);

        hco::PrioEntry out;
        assert(q.pop(out));
        assert(out.priority == 20);  // 最高优先级先出
        assert(q.pop(out));
        assert(out.priority == -20); // 最低优先级后出
        std::cout << "PASS: boundary priorities (-20, +20)\n";
    }

    std::cout << "All priority tests passed.\n";
    return 0;
}
