// src/coroutine/stack.h
// Copy-on-Grow 协程栈: 4KB 起始, 按需扩展, 跨平台 (mmap/VirtualAlloc)
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <cstddef>
#include <sys/types.h>

namespace hco {

class Stack {
public:
    Stack(size_t initial_size = 4096, size_t max_size = 1048576);
    ~Stack();

    // 禁止拷贝
    Stack(const Stack&) = delete;
    Stack& operator=(const Stack&) = delete;

    // 获取栈底指针 (低地址)
    void* stack_ptr() const { return stack_ptr_; }

    // 获取栈顶指针 (高地址, fcontext 使用)
    void* stack_top() const;

    // 当前栈大小
    size_t size() const { return size_; }

private:
    void*  stack_ptr_;    // mmap 基地址
    size_t size_;         // 当前栈大小 (不含 guard page)
    size_t max_size_;     // 栈大小上限
    void*  guard_page_;   // guard page 地址

    bool try_grow();      // 检测到溢出后尝试扩容, 返回是否成功
    friend class Coroutine;
};

} // namespace hco
