// src/common/fcontext/ontop_arm64_aapcs_elf_gas.S
// ARM64 AAPCS fcontext ontop: 在目标上下文执行回调后返回
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine

.file "ontop_arm64_aapcs_elf_gas.S"
.text
.globl ontop_fcontext
.type ontop_fcontext,@function
.align 4

ontop_fcontext:
    // x0 = to, x1 = data, x2 = fn
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x29, x30, [sp, #-16]!

    mov x3, sp
    mov sp, x0
    mov x0, x1           // data -> arg for fn

    // Push fn as return address
    mov x30, x2

    ldp x29, x30, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16

    ret
.size ontop_fcontext,.-ontop_fcontext
