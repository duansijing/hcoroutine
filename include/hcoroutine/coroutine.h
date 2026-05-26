// include/hcoroutine/coroutine.h
// 协程公共 API: co_go / co_join / co_yield / co_sleep / co_self
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 HCoroutine
#pragma once

#include <hcoroutine/types.h>
#include <functional>

namespace hco {

co_handle co_go(std::function<void()> task, co_options opts = {});
void co_join(co_handle h);
void co_yield();
void co_sleep(uint64_t ms);
co_handle co_self();

} // namespace hco
