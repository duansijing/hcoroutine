// src/common/fcontext/jump_arm64_aapcs_elf_gas.S
// ARM64 AAPCS fcontext 上下文切换: x0=to (目标), x1=data (用户数据)
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine

.file "jump_arm64_aapcs_elf_gas.S"
.text
.globl jump_fcontext
.type jump_fcontext,@function
.align 4

jump_fcontext:
    // Save current context: x19-x30
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x29, x30, [sp, #-16]!

    // data -> x0 for transfer_t.data
    mov x0, x1

    // Switch to target context
    mov sp, x0

    // Restore target context
    ldp x29, x30, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16

    ret
.size jump_fcontext,.-jump_fcontext
