// src/coroutine/coroutine.cpp
// 协程核心实现: 创建/挂起/恢复/销毁, 全局注册表, fcontext 入口
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include <hcoroutine/types.h>
#include "coroutine.h"
#include <unordered_map>
#include <mutex>
#include <stdexcept>
#include <cassert>
#include <functional>

namespace hco {

// ---- 全局协程注册表 ----
static std::unordered_map<co_handle, Coroutine*> g_coroutines;
static std::mutex g_coro_mutex;
static co_handle g_next_id = 1;

// ---- 当前线程正在运行的协程 ----
static thread_local Coroutine* t_current = nullptr;

// ---- fcontext 入口回调 ----
HCO_SYSV_ABI
static void coroutine_entry(transfer_t t) {
    Coroutine* self = static_cast<Coroutine*>(t.data);

    // 保存调度器上下文, 用于返回
    self->ctx = t.fctx;

    // 执行协程存储的任务
    if (self->task) {
        self->task();
    }
    self->state.store(CoroutineState::DEAD);

    // 跳回调度器
    jump_fcontext(self->ctx, nullptr);
}

// ---- API ----

Coroutine* coroutine_create(std::function<void()> task, const co_options& opts) {
    std::lock_guard<std::mutex> lock(g_coro_mutex);

    co_handle id = g_next_id++;
    Coroutine* co = new Coroutine(id, nullptr, opts.priority,
                                   opts.stack_size, opts.max_stack);
    co->task = std::move(task);
    g_coroutines[id] = co;

    // 创建 fcontext, 入口为 coroutine_entry
    co->ctx = make_fcontext(co->stack.stack_top(), co->stack.size(),
                            coroutine_entry);

    return co;
}

void coroutine_resume(Coroutine* co) {
    co->state.store(CoroutineState::RUNNING);
    t_current = co;
    // 通过 data 参数传递 Coroutine* 指针, 供 coroutine_entry 使用
    transfer_t t = jump_fcontext(co->ctx, co);
    // 协程挂起或结束后回到这里, 保存协程的返回上下文
    co->ctx = t.fctx;
    t_current = nullptr;
}

void coroutine_suspend() {
    Coroutine* self = t_current;
    CoroutineState expected = CoroutineState::RUNNING;
    self->state.compare_exchange_strong(expected, CoroutineState::SUSPENDED);
    // 如果 CAS 失败, 说明 waker 已将 state 设为 READY — 仍切换到调度器, 但保持 READY 状态
    transfer_t t = jump_fcontext(self->ctx, nullptr);
    self->ctx = t.fctx;
    self->state.store(CoroutineState::RUNNING);
}

void coroutine_destroy(Coroutine* co) {
    std::lock_guard<std::mutex> lock(g_coro_mutex);
    g_coroutines.erase(co->id);
    delete co;
}

Coroutine* coroutine_get(co_handle id) {
    std::lock_guard<std::mutex> lock(g_coro_mutex);
    auto it = g_coroutines.find(id);
    return (it != g_coroutines.end()) ? it->second : nullptr;
}

Coroutine* coroutine_self() {
    return t_current;
}

} // namespace hco
