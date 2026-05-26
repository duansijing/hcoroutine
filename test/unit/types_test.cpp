// test/unit/types_test.cpp
// 基础类型定义单元测试
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#include <hcoroutine/types.h>

int main() {
    // verify default values
    hco::co_options opts;
    if (opts.priority != 0) return 1;
    if (opts.stack_size != 65536) return 2;
    if (opts.max_stack != 1048576) return 3;

    // verify handle constant
    if (hco::INVALID_HANDLE != 0) return 4;

    // verify config default worker_count=0 uses CPU core count logic
    hco::co_config cfg;
    if (cfg.worker_count != 0) return 5;
    if (!cfg.cpu_ids.empty()) return 6;
    if (cfg.event_timeout_ms != 10) return 7;

    return 0;
}
