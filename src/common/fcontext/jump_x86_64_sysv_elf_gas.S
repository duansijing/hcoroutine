// src/common/fcontext/jump_x86_64_sysv_elf_gas.S
// x86-64 SysV fcontext 上下文切换: 保存/恢复 callee-saved 寄存器, 跳转目标
// 基于 boost.context 实现, 适配本项目 fcontext 56 字节布局
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
//
// fcontext 布局 (56 字节, make_fcontext 预留 64 字节):
//   offset 0x00: r12
//   offset 0x08: r13
//   offset 0x10: r14
//   offset 0x18: r15
//   offset 0x20: rbx
//   offset 0x28: rbp
//   offset 0x30: rip (入口函数地址 或 返回地址)
//
// jump_fcontext(to, data):
//   1. 在栈上保存当前 6 个 callee-saved 寄存器 + rip
//   2. 切换栈到 to
//   3. 恢复目标的 6 个寄存器 + rip
//   4. 设置 transfer_t (RDI=fctx=保存区基址, RSI=data) 并跳转

.file "jump_x86_64_sysv_elf_gas.S"
.text
.globl jump_fcontext
.align 16

jump_fcontext:
    // ---- 保存当前上下文 ----
    // 预留 56 字节
    leaq  -0x38(%rsp), %rsp

    // 保存 callee-saved 寄存器到固定偏移
    movq  %r12, (%rsp)       // offset 0x00
    movq  %r13, 0x8(%rsp)    // offset 0x08
    movq  %r14, 0x10(%rsp)   // offset 0x10
    movq  %r15, 0x18(%rsp)   // offset 0x18
    movq  %rbx, 0x20(%rsp)   // offset 0x20
    movq  %rbp, 0x28(%rsp)   // offset 0x28

    // 计算返回地址: 调用 jump_fcontext 时的返回地址位于 [RSP + 0x38]
    // (32 字节 shadow space + 返回地址在栈上的位置... 实际只需 RSP + 56)
    // 当从 jump_fcontext 恢复到此处时，会 pop 这个值并 jmp 回去
    movq  0x38(%rsp), %rcx   // RCX = 返回地址
    movq  %rcx, 0x30(%rsp)   // offset 0x30 = 返回地址

    // 计算 from fctx (当前保存区的基地址)
    movq  %rsp, %r8          // R8 = from fctx = 当前 RSP (保存区基址)

    // 临时保存 data 和 to
    movq  %rsi, %r9          // R9 = data
    movq  %rdi, %r10         // R10 = to

    // ---- 切换到目标上下文 ----
    movq  %r10, %rsp         // RSP = to (目标保存区基址)

    // ---- 恢复目标上下文 ----
    popq  %r12               // offset 0x00
    popq  %r13               // offset 0x08
    popq  %r14               // offset 0x10
    popq  %r15               // offset 0x18
    popq  %rbx               // offset 0x20
    popq  %rbp               // offset 0x28
    popq  %rcx               // offset 0x30 = 目标 rip (make_fcontext: fn; 恢复: 返回地址)

    // ---- 设置 transfer_t 并跳转 ----
    // 首次入口 (make_fcontext): 目标函数签名 void fn(transfer_t)
    //   SysV ABI struct 参数: RDI = fctx (8B), RSI = data (8B)
    // 挂起恢复: jump_fcontext "返回" transfer_t
    //   SysV ABI struct 返回: RAX = fctx, RDX = data
    movq  %r8, %rdi          // RDI = from fctx (首次入口参数)
    movq  %r8, %rax          // RAX = from fctx (恢复返回值)
    movq  %r9, %rsi          // RSI = data (首次入口参数)
    movq  %r9, %rdx          // RDX = data (恢复返回值)

    // 因为用 jmp 而非 ret 跳转, call 指令压入的返回地址残留在栈上
    // 需要手动弹出以保持 RSP 正确, 避免破坏调用者的栈帧布局
    addq  $8, %rsp

    jmp   *%rcx              // 跳转到目标: 首次为入口函数, 恢复时返回到调用者
