// include/hcoroutine/scheduler.h
// 调度器公共 API: co_init / co_run / co_shutdown
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <hcoroutine/types.h>

namespace hco {

void co_init(const co_config& cfg = {});
void co_run();
void co_shutdown();

} // namespace hco
