// src/common/fcontext/make_arm64_aapcs_elf_gas.S
// ARM64 AAPCS fcontext 创建: 构建初始栈帧, x0=sp, x1=size, x2=fn
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine

.file "make_arm64_aapcs_elf_gas.S"
.text
.globl make_fcontext
.type make_fcontext,@function
.align 4

make_fcontext:
    // 16-byte stack alignment
    and x0, x0, ~0xF

    // Reserve space: 8 GPRs (x19-x26) + lr + fp = 10*8 = 80 bytes
    sub x0, x0, #80

    // Store fn at position 64 (x26 slot)
    str x2, [x0, #64]

    // Store stack pointer at position 72
    add x1, x0, #80
    str x1, [x0, #72]

    // Return fcontext_t
    ret
.size make_fcontext,.-make_fcontext
