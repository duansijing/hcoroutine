// src/common/fcontext/make_x86_64_sysv_elf_gas.S
// x86-64 SysV fcontext 创建: 在栈上构建初始寄存器帧, 入口函数写入 rip 槽
// 基于 boost.context 实现
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine

.file "make_x86_64_sysv_elf_gas.S"
.text
.globl make_fcontext
.align 16

make_fcontext:
    // 参数: %rdi = sp (栈顶), %rsi = size (未使用), %rdx = fn (入口函数)
    // 返回: %rax = fcontext_t (新上下文)

    // 栈顶地址 16 字节对齐
    andq  $-16, %rdi

    // 预留 fcontext 结构空间 (与 jump_fcontext 保存布局一致)
    // 6 个寄存器 (48B) + rip 槽 (8B) = 56B, 对齐到 64B
    // 额外预留 8B 给 jump_fcontext 中的 add $8,%rsp, 避免 RSP 触及 guard page
    leaq  -0x48(%rdi), %rax    // RAX = sp - 72 (64 + 8 预留)

    // 清零整个结构 (可选，此处省略以简化)

    // 保存入口函数到偏移 0x30 处 (跳转地址槽, jump_fcontext 第 7 次 pop 读取)
    movq  %rdx, 0x30(%rax)

    // 返回 fcontext 基地址
    ret
