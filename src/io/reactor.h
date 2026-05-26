// src/io/reactor.h
// I/O 事件反应器: Linux epoll 实现, Windows 桩, 协程化 fd 事件监听
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <cstdint>
#include <vector>
#include <mutex>
#include <unordered_map>

// epoll 仅在 Linux 可用, Windows 提供兼容常量
#ifdef __linux__
#include <sys/epoll.h>
#else
#define EPOLLIN  0x001
#define EPOLLOUT 0x004
#define EPOLLERR 0x008
#endif

namespace hco {

struct Coroutine;

struct IoEvent {
    Coroutine* co = nullptr;
    int fd = -1;
    uint32_t events = 0;  // EPOLLIN / EPOLLOUT / EPOLLERR
};

class Reactor {
public:
    Reactor();
    ~Reactor();

    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;

    // 注册 fd 事件, 关联到协程 (协程挂起后调用)
    void add(int fd, uint32_t events, Coroutine* co);

    // 移除 fd 的所有监听
    void remove(int fd);

    // 轮询事件, 返回被唤醒的协程列表, timeout_ms 为等待时长
    std::vector<Coroutine*> poll(int timeout_ms);

private:
#ifdef __linux__
    int epfd_ = -1;
    std::vector<epoll_event> events_buf_;
#else
    // Windows: 桩实现, 不执行实际 I/O
#endif
    std::mutex mtx_;
    std::unordered_map<int, IoEvent> pending_;  // fd -> event (用于唤醒时查找)
};

} // namespace hco
