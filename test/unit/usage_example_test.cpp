// test/unit/usage_example_test.cpp
// 业务使用样例: 模拟后端服务中协程调度、任务分发、结果收集的完整流程
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine

#include <hcoroutine/scheduler.h>
#include <hcoroutine/coroutine.h>
#include <hcoroutine/mutex.h>
#include <hcoroutine/channel.h>
#include <cassert>
#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <memory>

using std::cerr;
using std::endl;

struct Request  { int id; int payload; };
struct Response { int request_id; int result; bool success; };

static int compute(int input) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    return input * input + input;
}

int main() {
    cerr << "=== HCoroutine 业务使用样例 ===" << endl << endl;

    // ---- Step 1: 初始化调度器 ----
    cerr << "[Step 1] 初始化调度器" << endl;
    hco::co_config cfg;
    cfg.worker_count = 4;
    hco::co_init(cfg);
    cerr << "  co_init done" << endl;

    std::thread scheduler_thread([]() { hco::co_run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    cerr << "  调度器已启动, " << cfg.worker_count << " 个 Worker 就绪" << endl << endl;

    // ---- Step 2: 创建通道 ----
    cerr << "[Step 2] 创建任务通道" << endl;
    constexpr int N = 30;
    auto req_ch = std::make_unique<hco::co_channel<Request>>(N);
    auto rsp_ch = std::make_unique<hco::co_channel<Response>>(N);
    cerr << "  通道创建完毕" << endl << endl;

    // ---- Step 3: 共享状态 ----
    hco::co_mutex mtx;
    int processed = 0;
    int succeeded = 0;

    // ---- Step 4: 生产者协程 ----
    cerr << "[Step 3] 创建协程" << endl;

    hco::co_handle producer = hco::co_go([&]() {
        for (int i = 0; i < N; ++i) {
            Request req{i + 1, (i * 7) % 100};
            req_ch->send(req);
        }
        req_ch->close();
        cerr << "  [生产者] 已发送 " << N << " 个请求" << endl;
    });
    cerr << "  生产者已创建, handle=" << producer << endl;

    // ---- Step 5: 消费者协程组 ----
    constexpr int WORKERS = 4;
    for (int w = 0; w < WORKERS; ++w) {
        hco::co_go([&, w]() {
            int cnt = 0;
            while (true) {
                Request req;
                if (!req_ch->recv(req)) break;
                int res = compute(req.payload);
                Response rsp{req.id, res, true};
                rsp_ch->send(rsp);
                cnt++;
            }
            cerr << "  [消费者-" << w << "] 完成, 共处理 " << cnt << " 个" << endl;
        });
    }
    cerr << "  已创建 " << WORKERS << " 个消费者" << endl;

    // ---- Step 6: 监控协程 (高优先级) ----
    hco::co_options hi_opts;
    hi_opts.priority = 10;
    hco::co_go([&]() {
        for (int i = 0; i < 3; ++i) {
            hco::co_sleep(50);
            hco::co_lock_guard<hco::co_mutex> lk(mtx);
            cerr << "  [监控] " << processed << "/" << N << endl;
        }
    }, hi_opts);

    // ---- Step 7: 收集协程 ----
    std::atomic<bool> all_done{false};
    hco::co_go([&]() {
        for (int i = 0; i < N; ++i) {
            Response rsp;
            rsp_ch->recv(rsp);
            hco::co_lock_guard<hco::co_mutex> lk(mtx);
            processed++;
            if (rsp.success) succeeded++;
        }
        all_done.store(true);
        cerr << "  [收集器] 全部收集完毕" << endl;
    });

    // ---- Step 8: 等待完成 ----
    cerr << endl << "[Step 4] 等待任务完成..." << endl;
    auto start = std::chrono::steady_clock::now();
    while (!all_done.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (ms > 10000) {
            cerr << "TIMEOUT after " << ms << "ms" << endl;
            break;
        }
    }

    // ---- Step 9: 验证 ----
    cerr << endl << "[Step 5] 验证结果:" << endl;
    {
        hco::co_lock_guard<hco::co_mutex> lk(mtx);
        cerr << "  已处理: " << processed << "/" << N << endl;
        cerr << "  成功:   " << succeeded << "/" << N << endl;
        assert(processed == N);
        assert(succeeded == N);
    }
    assert(producer != hco::INVALID_HANDLE);
    cerr << "  所有断言通过" << endl;

    // ---- Step 10: 关闭 ----
    cerr << endl << "[Step 6] 优雅关闭" << endl;
    hco::co_shutdown();
    scheduler_thread.join();
    cerr << "  调度器已安全退出" << endl;
    cerr << endl << "=== 业务使用样例执行完毕 ===" << endl;

    return 0;
}
