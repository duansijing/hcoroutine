// src/io/io.cpp
// 协程化异步 I/O 实现: co_read / co_write / co_accept / co_connect
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include <hcoroutine/io.h>
#include "src/coroutine/coroutine.h"
#include "src/scheduler/worker.h"

#ifdef __linux__
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#endif

namespace hco {

#ifdef __linux__

static bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

// 挂起当前协程等待 fd 就绪
static void wait_for_fd(int fd) {
    Coroutine* self = coroutine_self();
    if (!self) return;
    self->reason = SuspendReason::IO_WAIT;
    self->wait_fd = fd;
    coroutine_suspend();
}

int co_read(int fd, void* buf, size_t count) {
    set_nonblocking(fd);
    while (true) {
        ssize_t n = ::read(fd, buf, count);
        if (n >= 0) return static_cast<int>(n);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            wait_for_fd(fd);
            continue;
        }
        return static_cast<int>(n);  // 错误
    }
}

int co_write(int fd, const void* buf, size_t count) {
    set_nonblocking(fd);
    while (true) {
        ssize_t n = ::write(fd, buf, count);
        if (n >= 0) return static_cast<int>(n);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            wait_for_fd(fd);
            continue;
        }
        return static_cast<int>(n);
    }
}

int co_accept(int fd, struct sockaddr* addr, socklen_t* addrlen) {
    set_nonblocking(fd);
    while (true) {
        int client = ::accept(fd, addr, addrlen);
        if (client >= 0) {
            set_nonblocking(client);
            return client;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            wait_for_fd(fd);
            continue;
        }
        return client;
    }
}

int co_connect(int fd, const struct sockaddr* addr, socklen_t addrlen) {
    set_nonblocking(fd);
    int ret = ::connect(fd, addr, addrlen);
    if (ret == 0) return 0;
    if (errno == EINPROGRESS) {
        wait_for_fd(fd);
        // 检查连接是否成功
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
        return (err == 0) ? 0 : -1;
    }
    return ret;
}

#else
// Windows: 桩实现
int co_read(int, void*, size_t) { return -1; }
int co_write(int, const void*, size_t) { return -1; }
#endif

} // namespace hco
