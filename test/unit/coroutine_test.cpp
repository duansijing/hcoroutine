// test/unit/coroutine_test.cpp
// 协程创建/挂起/恢复/销毁单元测试
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include <hcoroutine/types.h>
#include "src/coroutine/coroutine.h"
#include <cassert>
#include <iostream>
#include <functional>

int main() {
    // 测试 1: 创建协程
    bool called = false;
    hco::Coroutine* co = hco::coroutine_create([&]() { called = true; }, {});
    assert(co != nullptr);
    assert(co->id > 0);
    std::cout << "PASS: coroutine_create\n";

    // 测试 2: 查找协程
    hco::Coroutine* found = hco::coroutine_get(co->id);
    assert(found == co);
    std::cout << "PASS: coroutine_get\n";

    // 测试 3: 查找不存在的协程
    hco::Coroutine* not_found = hco::coroutine_get(99999);
    assert(not_found == nullptr);
    std::cout << "PASS: coroutine_get (not found)\n";

    // 测试 4: 销毁协程
    hco::coroutine_destroy(co);
    assert(hco::coroutine_get(co->id) == nullptr);
    std::cout << "PASS: coroutine_destroy\n";

    std::cout << "All coroutine tests passed.\n";
    return 0;
}
