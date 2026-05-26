// src/common/fcontext/fcontext.h
// fcontext 上下文切换 API 声明: make_fcontext / jump_fcontext / ontop_fcontext
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <cstddef>

namespace hco {

// fcontext_t 是一个指向栈顶的指针，跳转后恢复该位置的寄存器
using fcontext_t = void*;

// 转移数据结构，fcontext 跳转时传入
struct transfer_t {
    fcontext_t  fctx;   // 来源上下文
    void*       data;   // 用户数据
};

// 汇编实现使用 SysV 调用约定，需要显式标注 (MinGW-w64 默认使用 MS x64 ABI)
#if defined(_WIN32) && defined(__GNUC__)
#define HCO_SYSV_ABI __attribute__((sysv_abi))
#define HCO_MS_ABI   __attribute__((ms_abi))
#else
#define HCO_SYSV_ABI
#define HCO_MS_ABI
#endif

// 协程入口回调类型 (SysV ABI, 与 fcontext 汇编匹配)
typedef void (*fn_t)(transfer_t) HCO_SYSV_ABI;

// 创建上下文：在 stack 上构建初始寄存器帧，入口为 fn
// sp: 栈顶地址，size: 栈大小，fn: 协程入口函数
extern "C" HCO_SYSV_ABI fcontext_t make_fcontext(void* sp, size_t size, fn_t fn);

// 跳转到目标上下文
// to: 目标上下文，data: 传递给目标的用户数据 (→ transfer_t.data)
extern "C" HCO_SYSV_ABI transfer_t jump_fcontext(fcontext_t to, void* data);

// 在目标上下文上执行 fn 后返回（用于回调模式）
extern "C" HCO_SYSV_ABI transfer_t ontop_fcontext(fcontext_t to, void* data, fn_t fn);

} // namespace hco
