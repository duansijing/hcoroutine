// include/hcoroutine/io.h
// 协程化异步 I/O API: co_read / co_write / co_accept / co_connect
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <hcoroutine/types.h>
#include <cstddef>

namespace hco {

// 协程化异步 I/O: 在协程内调用, 若 fd 未就绪则挂起等待

// 异步读取 (返回实际读取字节数, < 0 表示错误)
int co_read(int fd, void* buf, size_t count);

// 异步写入 (返回实际写入字节数, < 0 表示错误)
int co_write(int fd, const void* buf, size_t count);

#ifdef __linux__
#include <sys/socket.h>

// 异步 accept (返回新 fd, < 0 表示错误)
int co_accept(int fd, struct sockaddr* addr, socklen_t* addrlen);

// 异步 connect (返回 0 成功, < 0 表示错误)
int co_connect(int fd, const struct sockaddr* addr, socklen_t addrlen);
#endif

} // namespace hco
