// src/scheduler/scheduler.cpp
// 调度器入口: co_init / co_run / co_shutdown / co_go / co_yield / co_sleep
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include <hcoroutine/scheduler.h>
#include <hcoroutine/coroutine.h>
#include <hcoroutine/types.h>
#include "worker.h"
#include "src/coroutine/coroutine.h"
#include <vector>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <thread>

namespace hco {

// ---- 全局状态 ----
std::atomic<bool> g_shutting_down{false};
std::vector<Worker*> g_workers;

static int g_worker_count = 0;
static co_config g_config;

void co_init(const co_config& cfg) {
    g_config = cfg;
    g_shutting_down.store(false);

    if (!cfg.cpu_ids.empty()) {
        g_worker_count = static_cast<int>(cfg.cpu_ids.size());
    } else if (cfg.worker_count > 0) {
        g_worker_count = cfg.worker_count;
    } else {
        g_worker_count = std::thread::hardware_concurrency();
    }
    if (g_worker_count <= 0) g_worker_count = 1;

    for (int i = 0; i < g_worker_count; ++i) {
        int cpu = cfg.cpu_ids.empty() ? i : cfg.cpu_ids[i];
        auto* w = new Worker(i, cpu);
        g_workers.push_back(w);
    }
}

void co_run() {
    for (auto* w : g_workers) {
        w->start();
    }

    while (!g_shutting_down.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    for (auto* w : g_workers) {
        w->join();
    }

    for (auto* w : g_workers) {
        delete w;
    }
    g_workers.clear();
}

void co_shutdown() {
    g_shutting_down.store(true);
    for (auto* w : g_workers) {
        w->stop();
    }
}

// ---- coroutine.h API 实现 ----

co_handle co_go(std::function<void()> task, co_options opts) {
    if (g_shutting_down.load(std::memory_order_acquire)) {
        return INVALID_HANDLE;
    }
    if (g_workers.empty()) return INVALID_HANDLE;

    Coroutine* co = coroutine_create(std::move(task), opts);

    static std::atomic<int> rr{0};
    int idx = rr.fetch_add(1, std::memory_order_relaxed) % g_worker_count;
    g_workers[idx]->enqueue_priority(co);

    return co->id;
}

void co_join(co_handle h) {
    while (true) {
        Coroutine* co = coroutine_get(h);
        if (!co || co->state.load() == CoroutineState::DEAD) break;
        co_yield();
    }
}

void co_yield() {
    Coroutine* self = coroutine_self();
    if (!self) return;
    self->reason = SuspendReason::YIELD;
    coroutine_suspend();
}

void co_sleep(uint64_t ms) {
    Coroutine* self = coroutine_self();
    if (!self) return;
    self->reason = SuspendReason::SLEEP;
    self->wake_time_ms = now_ms() + ms;
    coroutine_suspend();
}

co_handle co_self() {
    Coroutine* self = coroutine_self();
    return self ? self->id : INVALID_HANDLE;
}

} // namespace hco
