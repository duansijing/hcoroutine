// include/hcoroutine/types.h
// 基础类型定义: 协程句柄, 状态枚举, 配置结构体
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace hco {

// coroutine handle
using co_handle = uint64_t;
constexpr co_handle INVALID_HANDLE = 0;

// coroutine state
enum class CoroutineState {
    READY,
    RUNNING,
    SUSPENDED,
    DEAD
};

// suspend reason
enum class SuspendReason {
    NONE,
    IO_WAIT,
    SLEEP,
    MUTEX,
    COND,
    CHANNEL,
    YIELD
};

// startup options
struct co_options {
    int    priority   = 0;
    size_t stack_size = 65536;   // 64KB, 确保足够的栈空间给 C++ 函数调用
    size_t max_stack  = 1048576;
};

// scheduler configuration
struct co_config {
    int              worker_count     = 0;
    std::vector<int> cpu_ids          = {};
    int              event_timeout_ms = 10;
};

} // namespace hco
