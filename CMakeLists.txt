cmake_minimum_required(VERSION 3.14)
project(hcoroutine VERSION 1.0.0 LANGUAGES CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ASM support (fcontext)
enable_language(ASM)

find_package(Threads REQUIRED)

# header file directories
include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR})

# fcontext assembly source files — selected by architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(CONTEXT_ASM
        src/common/fcontext/jump_arm64_aapcs_elf_gas.S
        src/common/fcontext/make_arm64_aapcs_elf_gas.S
        src/common/fcontext/ontop_arm64_aapcs_elf_gas.S
    )
else()
    set(CONTEXT_ASM
        src/common/fcontext/jump_x86_64_sysv_elf_gas.S
        src/common/fcontext/make_x86_64_sysv_elf_gas.S
        src/common/fcontext/ontop_x86_64_sysv_elf_gas.S
    )
endif()

# core library
add_library(hcoroutine STATIC
    src/coroutine/stack.cpp
    src/coroutine/coroutine.cpp
    src/scheduler/worker.cpp
    src/scheduler/scheduler.cpp
    src/sync/mutex.cpp
    src/sync/cond.cpp
    src/sync/rwlock.cpp
    src/sync/waitgroup.cpp
    src/sync/channel.cpp
    src/io/timer_wheel.cpp
    src/io/reactor.cpp
    src/io/io.cpp
    ${CONTEXT_ASM}
)

target_link_libraries(hcoroutine PRIVATE Threads::Threads)

# tests
enable_testing()
add_subdirectory(test)
