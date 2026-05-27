// src/coroutine/coroutine.h
// 协程内部结构定义与内部 API: coroutine_create / resume / suspend / destroy
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <atomic>
#include <hcoroutine/types.h>
#include "stack.h"
#include "src/common/fcontext/fcontext.h"

namespace hco {

struct Coroutine {
    co_handle      id;
    fcontext_t     ctx;
    std::atomic<CoroutineState> state;
    int            priority;
    uint64_t       born_time_ms;

    Stack          stack;
    SuspendReason  reason;
    int            wait_fd;
    uint64_t       wake_time_ms;

    std::function<void()> task;  // 协程要执行的任务

    Coroutine(co_handle id_, fcontext_t ctx_, int pri, size_t stack_sz, size_t max_sz)
        : id(id_), ctx(ctx_), state(CoroutineState::READY)
        , priority(pri), born_time_ms(0)
        , stack(stack_sz, max_sz)
        , reason(SuspendReason::NONE)
        , wait_fd(-1), wake_time_ms(0)
    {}
};

// ---- 内部 API (调度器使用) ----

Coroutine* coroutine_create(std::function<void()> task, const co_options& opts);
void       coroutine_resume(Coroutine* co);
void       coroutine_suspend();
void       coroutine_destroy(Coroutine* co);
Coroutine* coroutine_get(co_handle id);
Coroutine* coroutine_self();

} // namespace hco
