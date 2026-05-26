# HCoroutine

基于 boost.context (fcontext) 的 C++17 高性能 N:M 协程调度库，面向后端服务的统一任务调度。

## 特性

- **Stackful 协程**: Copy-on-Grow 栈，支持 x86-64 SysV ELF + ARM64 AAPCS
- **N:M 调度**: 多 Worker 线程 + Work-Stealing（工作窃取）
- **优先级调度**: -20 ~ +20 共 41 级，Aging 机制防低优先级饥饿
- **同步原语**: co_mutex / co_cond / co_rwlock / co_waitgroup
- **协程通道**: co_channel&lt;T&gt; 带缓冲协程间通信，支持 close
- **定时器**: 毫秒级 Timer Wheel，支持 co_sleep
- **I/O**: Epoll Reactor (Linux)，co_read / co_write / co_accept / co_connect
- **跨平台**: Linux x86-64 + ARM64 原生，MinGW/Windows 开发编译

## 快速开始

### 构建

```sh
mkdir build && cd build
cmake .. -G "Unix Makefiles"
make -j$(nproc)
```

### 运行测试

```sh
cd build
ctest --output-on-failure
```

### 基本用法

```cpp
#include <hcoroutine/scheduler.h>
#include <hcoroutine/coroutine.h>
#include <hcoroutine/mutex.h>
#include <hcoroutine/channel.h>

int main() {
    // 1. 初始化调度器
    hco::co_config cfg;
    cfg.worker_count = 4;
    hco::co_init(cfg);

    std::thread t([]() { hco::co_run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 2. 创建通道
    hco::co_channel<int> ch(10);

    // 3. 生产者协程
    hco::co_go([&]() {
        for (int i = 0; i < 100; ++i) {
            ch.send(i);
        }
        ch.close();
    });

    // 4. 消费者协程
    hco::co_go([&]() {
        int val;
        while (ch.recv(val)) {
            std::cout << "received: " << val << std::endl;
        }
    });

    // 5. 关闭
    std::this_thread::sleep_for(std::chrono::seconds(1));
    hco::co_shutdown();
    t.join();
    return 0;
}
```

## API 概览

### 调度器 (scheduler.h)

| API | 说明 |
|-----|------|
| `co_init(cfg)` | 初始化调度器, 创建 Worker 线程 |
| `co_run()` | 启动 Worker 事件循环 |
| `co_shutdown()` | 安全关闭调度器 |

### 协程 (coroutine.h)

| API | 说明 |
|-----|------|
| `co_go(task, opts?)` | 创建协程并派发, 返回 handle |
| `co_join(handle)` | 等待协程完成 |
| `co_yield()` | 主动让出 CPU |
| `co_sleep(ms)` | 挂起指定毫秒 |
| `co_self()` | 返回当前协程 handle |

### 同步原语

| 类型 | 说明 |
|------|------|
| `co_mutex` | 互斥锁, 支持 try_lock |
| `co_lock_guard<T>` | RAII 锁守卫 |
| `co_cond` | 条件变量, wait/signal/broadcast |
| `co_rwlock` | 读写锁, 写者优先 |
| `co_waitgroup` | 等待组, add/done/wait |

### 通道 (co_channel&lt;T&gt;)

| 方法 | 说明 |
|------|------|
| `send(val)` | 发送, 满则挂起 |
| `recv(out)` | 接收, 返回 true; 关闭且空返回 false |
| `close()` | 关闭通道 |
| `is_closed()` | 查询关闭状态 |

### I/O (仅 Linux)

| API | 说明 |
|-----|------|
| `co_read(fd, buf, n)` | 协程感知的 read |
| `co_write(fd, buf, n)` | 协程感知的 write |
| `co_accept(fd, addr)` | 协程感知的 accept |
| `co_connect(fd, addr)` | 协程感知的 connect |

## 目录结构

```text
include/hcoroutine/    # 9 个公共头文件
src/
  common/              # fcontext 汇编 (6 文件) + SPMC 队列 + 优先级队列
  coroutine/           # 协程核心 (创建/挂起/恢复/销毁)
  scheduler/           # Worker + Scheduler
  sync/                # mutex/cond/rwlock/waitgroup/channel
  io/                  # TimerWheel + Reactor + I/O API
test/unit/             # 11 个单元/集成测试
```

## 调度模型

- **N:M 架构**: N 个协程映射到 M 个 Worker 线程（默认 `hardware_concurrency`）
- **SPMC 无锁队列**: CAS 实现 pop/steal，防止并发消费
- **工作窃取**: 空闲 Worker 随机从其他 Worker 窃取任务
- **41 级优先级**: 高优先级先出队，Aging 在等待 >100ms 时临时提升 +10
- **挂起分类处理**: YIELD→重新入队, SLEEP→TimerWheel, IO_WAIT→Reactor, MUTEX/COND/CHANNEL→由 waker 负责唤醒

## 许可证

MIT License — 详见各源文件头部声明。

## 依赖

- boost.context (fcontext) — 协程上下文切换
- pthread — 线程管理
- Linux: epoll — I/O 多路复用
- CMake ≥ 3.16
- GCC ≥ 8 或 Clang ≥ 10（C++17）
