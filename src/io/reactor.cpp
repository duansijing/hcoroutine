// src/io/reactor.cpp
// Reactor 实现: epoll_create / epoll_ctl / epoll_wait (Linux), Windows 桩
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include "reactor.h"
#include "src/coroutine/coroutine.h"
#include <cstring>
#include <cerrno>

#ifdef __linux__
#include <unistd.h>
#endif

namespace hco {

Reactor::Reactor() {
#ifdef __linux__
    epfd_ = epoll_create1(0);
    events_buf_.resize(256);
#endif
}

Reactor::~Reactor() {
#ifdef __linux__
    if (epfd_ >= 0) close(epfd_);
#endif
}

void Reactor::add(int fd, uint32_t events, Coroutine* co) {
    std::lock_guard<std::mutex> lk(mtx_);
    pending_[fd] = {co, fd, events};

#ifdef __linux__
    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
#endif
}

void Reactor::remove(int fd) {
    std::lock_guard<std::mutex> lk(mtx_);
    pending_.erase(fd);

#ifdef __linux__
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
#endif
}

std::vector<Coroutine*> Reactor::poll(int timeout_ms) {
    std::vector<Coroutine*> ready;

#ifdef __linux__
    int n = epoll_wait(epfd_, events_buf_.data(),
                       static_cast<int>(events_buf_.size()), timeout_ms);
    if (n <= 0) return ready;

    std::lock_guard<std::mutex> lk(mtx_);
    for (int i = 0; i < n; ++i) {
        int fd = events_buf_[i].data.fd;
        auto it = pending_.find(fd);
        if (it != pending_.end()) {
            ready.push_back(it->second.co);
            pending_.erase(it);
            epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        }
    }
#endif
    // Windows: 返回空列表

    return ready;
}

} // namespace hco
