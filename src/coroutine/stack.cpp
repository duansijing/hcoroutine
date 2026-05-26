// src/coroutine/stack.cpp
// Copy-on-Grow 栈实现: Windows VirtualAlloc / Linux mmap
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include "stack.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <cstring>
#include <stdexcept>

namespace hco {

static constexpr long PAGE_SIZE = 4096;

static size_t align_up(size_t n, size_t align) {
    return (n + align - 1) & ~(align - 1);
}

Stack::Stack(size_t initial_size, size_t max_size)
    : stack_ptr_(nullptr)
    , size_(align_up(initial_size, PAGE_SIZE))
    , max_size_(align_up(max_size, PAGE_SIZE))
    , guard_page_(nullptr)
{
    if (size_ > max_size_) {
        size_ = max_size_;
    }

    size_t total = size_ + PAGE_SIZE; // 栈 + guard page

#ifdef _WIN32
    void* mem = VirtualAlloc(nullptr, total,
                             MEM_RESERVE | MEM_COMMIT,
                             PAGE_READWRITE);
    if (!mem) {
        throw std::bad_alloc();
    }

    stack_ptr_ = mem;
    guard_page_ = static_cast<char*>(mem) + size_;

    // guard page 设为不可访问
    DWORD old_prot;
    VirtualProtect(guard_page_, PAGE_SIZE,
                   PAGE_NOACCESS, &old_prot);
#else
    void* mem = mmap(nullptr, total,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS,
                     -1, 0);
    if (mem == MAP_FAILED) {
        throw std::bad_alloc();
    }

    stack_ptr_ = mem;
    guard_page_ = static_cast<char*>(mem) + size_;

    mprotect(guard_page_, PAGE_SIZE, PROT_NONE);
#endif
}

Stack::~Stack() {
    if (stack_ptr_) {
#ifdef _WIN32
        VirtualFree(stack_ptr_, 0, MEM_RELEASE);
#else
        // 恢复 guard page 权限以便 munmap
        mprotect(guard_page_, PAGE_SIZE, PROT_READ | PROT_WRITE);
        munmap(stack_ptr_, size_ + PAGE_SIZE);
#endif
        stack_ptr_ = nullptr;
        guard_page_ = nullptr;
    }
}

void* Stack::stack_top() const {
    return static_cast<char*>(stack_ptr_) + size_;
}

bool Stack::try_grow() {
    size_t new_size = size_ * 2;
    if (new_size > max_size_) {
        return false;
    }

#ifdef _WIN32
    // 恢复旧 guard page 权限
    DWORD old_prot;
    VirtualProtect(guard_page_, PAGE_SIZE,
                   PAGE_READWRITE, &old_prot);

    size_t new_total = new_size + PAGE_SIZE;
    void* new_mem = VirtualAlloc(nullptr, new_total,
                                 MEM_RESERVE | MEM_COMMIT,
                                 PAGE_READWRITE);
    if (!new_mem) {
        VirtualProtect(guard_page_, PAGE_SIZE,
                       PAGE_NOACCESS, &old_prot);
        return false;
    }

    char* new_stack_bottom = static_cast<char*>(new_mem);
    char* old_stack_bottom = static_cast<char*>(stack_ptr_);
    size_t offset = new_size - size_;
    memcpy(new_stack_bottom + offset, old_stack_bottom, size_);

    VirtualFree(stack_ptr_, 0, MEM_RELEASE);

    stack_ptr_ = new_mem;
    size_ = new_size;
    guard_page_ = new_stack_bottom + new_size;

    VirtualProtect(guard_page_, PAGE_SIZE,
                   PAGE_NOACCESS, &old_prot);
#else
    mprotect(guard_page_, PAGE_SIZE, PROT_READ | PROT_WRITE);

    size_t new_total = new_size + PAGE_SIZE;
    void* new_mem = mmap(nullptr, new_total,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS,
                         -1, 0);
    if (new_mem == MAP_FAILED) {
        mprotect(guard_page_, PAGE_SIZE, PROT_NONE);
        return false;
    }

    char* new_stack_bottom = static_cast<char*>(new_mem);
    char* old_stack_bottom = static_cast<char*>(stack_ptr_);
    size_t offset = new_size - size_;
    memcpy(new_stack_bottom + offset, old_stack_bottom, size_);

    munmap(stack_ptr_, size_ + PAGE_SIZE);

    stack_ptr_ = new_mem;
    size_ = new_size;
    guard_page_ = new_stack_bottom + new_size;

    mprotect(guard_page_, PAGE_SIZE, PROT_NONE);
#endif
    return true;
}

} // namespace hco
