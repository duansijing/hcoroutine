// test/unit/echo_test.cpp
// Echo Server 模式集成测试: 用 Channel 模拟网络 I/O 的请求-响应流程
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include <hcoroutine/scheduler.h>
#include <hcoroutine/coroutine.h>
#include <hcoroutine/channel.h>
#include <cassert>
#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <memory>

// 模拟一个连接: (request_channel, response_channel) 对
struct Connection {
    hco::co_channel<std::string>* req;
    hco::co_channel<std::string>* rsp;
};

int main() {
    // 测试: 多个客户端向 echo server 发送消息并验证回显
    {
        constexpr int N_CLIENTS = 20;
        std::atomic<int> responses{0};
        std::atomic<bool> all_ok{true};

        hco::co_config cfg;
        cfg.worker_count = 4;
        hco::co_init(cfg);

        std::thread runner([]() { hco::co_run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 为每个客户端创建通道对 (co_channel 不可拷贝, 用 unique_ptr)
        using Ch = hco::co_channel<std::string>;
        std::vector<std::unique_ptr<Ch>> req_channels;
        std::vector<std::unique_ptr<Ch>> rsp_channels;
        for (int i = 0; i < N_CLIENTS; ++i) {
            req_channels.push_back(std::make_unique<Ch>(1));
            rsp_channels.push_back(std::make_unique<Ch>(1));
        }

        // 启动 Echo Server 协程: 串行处理所有请求
        hco::co_go([&]() {
            for (int i = 0; i < N_CLIENTS; ++i) {
                std::string msg;
                req_channels[i]->recv(msg);
                rsp_channels[i]->send(msg);
            }
        });

        // 启动客户端协程: 发送消息并等待回显
        for (int i = 0; i < N_CLIENTS; ++i) {
            hco::co_go([&, i]() {
                std::string msg = "hello from client " + std::to_string(i);
                req_channels[i]->send(msg);
                std::string echo;
                rsp_channels[i]->recv(echo);
                if (echo != msg) {
                    all_ok.store(false);
                }
                responses.fetch_add(1);
            });
        }

        // 等待所有响应
        auto start = std::chrono::steady_clock::now();
        while (responses.load() < N_CLIENTS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count() > 5000) {
                std::cerr << "TIMEOUT: " << responses.load() << "/" << N_CLIENTS << "\n";
                break;
            }
        }

        hco::co_shutdown();
        runner.join();

        assert(responses.load() == N_CLIENTS);
        assert(all_ok.load());
        std::cout << "PASS: echo server pattern (" << N_CLIENTS << " clients)\n";
    }

    // 测试: 并发 Echo — 多个 server 协程处理不同客户端
    {
        constexpr int N_CLIENTS = 20;
        std::atomic<int> responses{0};
        std::atomic<bool> all_ok{true};

        hco::co_config cfg;
        cfg.worker_count = 4;
        hco::co_init(cfg);

        std::thread runner([]() { hco::co_run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        using Ch = hco::co_channel<std::string>;
        std::vector<std::unique_ptr<Ch>> req_channels;
        std::vector<std::unique_ptr<Ch>> rsp_channels;
        for (int i = 0; i < N_CLIENTS; ++i) {
            req_channels.push_back(std::make_unique<Ch>(1));
            rsp_channels.push_back(std::make_unique<Ch>(1));
        }

        // 每个连接一个 server handler 协程 (并发模式)
        for (int i = 0; i < N_CLIENTS; ++i) {
            hco::co_go([&, i]() {
                std::string msg;
                req_channels[i]->recv(msg);
                rsp_channels[i]->send(msg);
            });
        }

        // 客户端协程
        for (int i = 0; i < N_CLIENTS; ++i) {
            hco::co_go([&, i]() {
                std::string msg = "concurrent-" + std::to_string(i);
                req_channels[i]->send(msg);
                std::string echo;
                rsp_channels[i]->recv(echo);
                if (echo != msg) {
                    all_ok.store(false);
                }
                responses.fetch_add(1);
            });
        }

        auto start = std::chrono::steady_clock::now();
        while (responses.load() < N_CLIENTS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count() > 5000) {
                break;
            }
        }

        hco::co_shutdown();
        runner.join();

        assert(responses.load() == N_CLIENTS);
        assert(all_ok.load());
        std::cout << "PASS: concurrent echo (" << N_CLIENTS << " handlers)\n";
    }

    std::cout << "All echo tests passed.\n";
    return 0;
}
