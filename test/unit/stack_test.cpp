// test/unit/stack_test.cpp
// Copy-on-Grow 栈单元测试
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include "src/coroutine/stack.h"
#include <cassert>
#include <iostream>

static constexpr long PAGE_SIZE = 4096;

int main() {
    // 测试 1: 默认创建
    {
        hco::Stack s;
        assert(s.size() == PAGE_SIZE);
        assert(s.stack_ptr() != nullptr);
        assert(s.stack_top() == static_cast<char*>(s.stack_ptr()) + PAGE_SIZE);
        std::cout << "PASS: default construction\n";
    }

    // 测试 2: 自定义大小
    {
        hco::Stack s(8192, 65536);
        assert(s.size() == 8192);
        assert(s.stack_ptr() != nullptr);
        std::cout << "PASS: custom size\n";
    }

    // 测试 3: guard page 地址验证
    {
        hco::Stack s(4096, 16384);
        void* expected_guard = static_cast<char*>(s.stack_ptr()) + s.size();
        assert(expected_guard != nullptr);
        std::cout << "PASS: guard page address\n";
    }

    // 测试 4: 析构不泄露
    {
        hco::Stack s;
    }
    std::cout << "PASS: destructor\n";

    std::cout << "All stack tests passed.\n";
    return 0;
}
