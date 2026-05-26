// src/common/fcontext/ontop_x86_64_sysv_elf_gas.S
// x86-64 SysV fcontext ontop: 在目标上下文上执行回调后返回
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine

.file "ontop_x86_64_sysv_elf_gas.S"
.text
.globl ontop_fcontext
.align 16

ontop_fcontext:
    // 参数: %rdi = to, %rsi = data, %rdx = fn
    // 保存当前寄存器
    pushq %rbp
    pushq %rbx
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12

    leaq -8(%rsp), %rsp
    movq %rsi, %rax            // data
    movq %rdx, %r10            // fn 暂存到 r10

    // 切换到目标上下文
    movq %rdi, %rsp

    popq %r12
    popq %r13
    popq %r14
    popq %r15
    popq %rbx
    popq %rbp

    popq %rcx                  // 目标上下文 rip
    movq %rax, %rdi            // transfer_t.data
    xorl %eax, %eax            // transfer_t.fctx
    // 将 fn 地址临时存到 rip 位置，这样目标上下文执行时会先调用 fn
    pushq %r10
    jmp *%rcx
