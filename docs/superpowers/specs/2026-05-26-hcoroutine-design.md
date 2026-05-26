# HCoroutine 高性能协程库 — 设计文档

## 概述

HCoroutine 是一个基于 pthread 构建的 C++ 高性能有栈协程库，用于后端服务进程的任务统一调度。采用对称 N:M 调度模型（Work-Stealing），支持 I/O 与 CPU 混合场景，提供显式异步 API 和丰富的协程间同步原语。

### 设计目标

- 百万级协程并发，内存开销可控（Copy-on-Grow 栈）
- 多核 Work-Stealing 调度，无中心瓶颈
- 支持任务优先级调度 + 防饥饿机制
- 显式异步 API，行为可控无魔法
- Linux x86-64 + ARM64，C++17 标准

## 模块划分与逻辑视图

### 子模块一览

| 模块 | 文件 | 职责 | 依赖 |
|------|------|------|------|
| **Scheduler** | `scheduler/` | 全局调度入口，Worker 生命周期管理，CPU 绑定，优雅关闭 | Coroutine, Reactor, Sync |
| **Worker** | `scheduler/worker.cpp` | 线程主循环：取任务 → 切换协程 → epoll_wait → 窃取 | Coroutine, Reactor |
| **GlobalQueue** | `scheduler/global_queue.cpp` | MPMC 全局任务队列，新协程的入口 | Queue (SPMC/MPMC) |
| **WorkStealing** | `scheduler/work_stealing.cpp` | 随机 Victim 选择 + CAS 窃取策略 | Worker |
| **Coroutine** | `coroutine/` | 协程实体：ID 分配、状态转移、fcontext 封装 | Stack, fcontext |
| **Stack** | `coroutine/stack.cpp` | Copy-on-Grow 栈：mmap 分配、guard page、SIGSEGV 处理、扩容 | Platform (mmap) |
| **fcontext** | `common/fcontext/` | boost.context 汇编 (x86-64 SysV + ARM64 AAPCS)：jump_fcontext / make_fcontext | 无 |
| **Reactor** | `io/reactor.cpp` | epoll 实例管理：fd 注册/注销，事件分发，fd→协程映射 | Coroutine |
| **TimerWheel** | `io/timer_wheel.cpp` | 分层时间轮：O(1) 插入/删除，到期回调 | Coroutine |
| **Channel** | `sync/channel.cpp` | 有缓冲/无缓冲通道：send/recv 阻塞语义 | Coroutine, Mutex, Cond |
| **Mutex** | `sync/mutex.cpp` | 协程级互斥锁：lock 挂起协程而非阻塞线程 | Coroutine |
| **Cond** | `sync/cond.cpp` | 协程级条件变量：wait 挂起 + notify 唤醒 | Coroutine, Mutex |
| **RWLock** | `sync/rwlock.cpp` | 读多写少优化锁 | Coroutine, Mutex |
| **WaitGroup** | `sync/waitgroup.cpp` | 栅栏同步：计数归零唤醒等待者 | Coroutine, Cond |

### 逻辑视图

```
                          ┌─────────────────────────────┐
                          │         用户代码             │
                          │  co_go() / co_read() / ...  │
                          └────────────┬────────────────┘
                                       │ 公共 API
        ┌──────────────────────────────┼──────────────────────────────┐
        │                      ┌───────┴───────┐                      │
        │                      │   Scheduler   │                      │
        │                      │  (入口/总控)   │                      │
        │                      └───┬───┬───┬───┘                      │
        │              ┌───────────┘   │   └───────────┐              │
        │              ▼               ▼               ▼              │
        │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐        │
        │  │   Worker 0   │ │   Worker 1   │ │   Worker N   │  ...   │
        │  │ ┌──────────┐ │ │ ┌──────────┐ │ │ ┌──────────┐ │        │
        │  │ │Local PQ  │ │ │ │Local PQ  │ │ │ │Local PQ  │ │        │
        │  │ │(min-heap)│ │ │ │(min-heap)│ │ │ │(min-heap)│ │        │
        │  │ └────┬─────┘ │ │ └────┬─────┘ │ │ └────┬─────┘ │        │
        │  │      ▼       │ │      ▼       │ │      ▼       │        │
        │  │ ┌──────────┐ │ │ ┌──────────┐ │ │ ┌──────────┐ │        │
        │  │ │Coroutine │ │ │ │Coroutine │ │ │ │Coroutine │ │        │
        │  │ │ 执行/切换 │ │ │ │ 执行/切换 │ │ │ │ 执行/切换 │ │        │
        │  │ └────┬─────┘ │ │ └────┬─────┘ │ │ └────┬─────┘ │        │
        │  │      ▼       │ │      ▼       │ │      ▼       │        │
        │  │ ┌──────────┐ │ │ ┌──────────┐ │ │ ┌──────────┐ │        │
        │  │ │ Reactor  │ │ │ │ Reactor  │ │ │ │ Reactor  │ │        │
        │  │ │(epoll)   │◄├─┼─┤(epoll)   │◄├─┼─┤(epoll)   │ │        │
        │  │ │TimerWheel│ │ │ │TimerWheel│ │ │ │TimerWheel│ │        │
        │  │ └──────────┘ │ │ └──────────┘ │ │ └──────────┘ │        │
        │  └──────┬───────┘ └──────┬───────┘ └──────┬───────┘        │
        │         │                │                │                │
        │         └──── Work-Stealing (CAS 窃取) ────┘                │
        │                         │                                   │
        │                ┌────────┴────────┐                          │
        │                │  Global Queue   │                          │
        │                │  (MPMC spinlock)│                          │
        │                └────────┬────────┘                          │
        │                         │                                   │
        │                ┌────────┴────────┐                          │
        │                │   协程同步原语    │                          │
        │                │ Channel/Mutex/  │                          │
        │                │ Cond/RWLock/WG  │                          │
        │                └────────┬────────┘                          │
        │                         │                                   │
        │                ┌────────┴────────┐                          │
        │                │   Platform      │                          │
        │                │ fcontext/pthread│                          │
        │                │ mmap/epoll      │                          │
        │                └─────────────────┘                          │
        └────────────────────────────────────────────────────────────┘
```

### 核心数据流

#### 流程 1：协程创建与首次调度

```
co_go(task, opts)
  │
  ├─1─→ Coroutine::create(task, opts)     // 分配 ID，创建 fcontext，分配初始栈
  ├─2─→ GlobalQueue::push(coroutine)       // 新协程进入全局队列
  │
  │   [某 Worker 主循环]
  ├─3─→ Worker::try_fetch()               // 优先 Local → Global → Steal
  ├─4─→ Coroutine::resume()               // jump_fcontext: 调度器 → 协程
  │
  │   [协程执行中]
  └─5─→ task() 开始执行
```

#### 流程 2：I/O 挂起与唤醒

```
co_read(fd, buf, len, timeout)
  │
  │   [当前 Worker 上运行的协程 C]
  ├─1─→ Reactor::add_event(fd, EPOLLIN)    // 注册到本 Worker 的 epoll 实例
  ├─2─→ C.reason = IO_WAIT                 // 记录挂起原因
  ├─3─→ C.wait_fd = fd                     // 记录等待的 fd
  ├─4─→ fd_map[fd] = C                     // 建立 fd→协程 映射
  ├─5─→ Coroutine::suspend()               // jump_fcontext: 协程 → 调度器
  ├─6─→ Worker 取下一个协程执行             // C 处于 SUSPENDED

  │   [epoll 通知 fd 可读]
  ├─7─→ Reactor::handle_events()           // epoll_wait 返回就绪 fd 列表
  ├─8─→ C = fd_map[ready_fd]               // 根据 fd 查找协程
  ├─9─→ C.state = READY                    // 恢复为就绪态
  └─10→ Worker::enqueue(C)                 // C 重新入队，等待再次被调度
```

#### 流程 3：Work-Stealing

```
Worker-1 (空闲)
  │
  ├─1─→ 检查 Local Queue — 空
  ├─2─→ 检查 Global Queue — 空
  ├─3─→ random_select_victim() → Worker-3 (最忙)
  ├─4─→ CAS steal Worker-3.queue.back()    // 从队尾原子窃取
  │     ├─ 成功 → enqueue_local(协程) → resume
  │     └─ 失败 → 重试 or 换 Victim
  │
  └─5─→ 全部 Worker 均空 → epoll_wait(timeout) 休眠
```

#### 流程 4：同步原语 — 以 Mutex 为例

```
co_mutex::lock()
  │
  │   [协程 C1 持有锁, C2 尝试获取]
  ├─1─→ C2 发现 mutex 已被 C1 持有
  ├─2─→ C2.reason = MUTEX_WAIT
  ├─3─→ mutex.wait_queue.push(C2)          // C2 加入 mutex 等待队列
  ├─4─→ Coroutine::suspend()               // C2 挂起 → Worker 调度其他协程

  │   [C1 释放锁]
  ├─5─→ co_mutex::unlock()
  ├─6─→ C_wake = mutex.wait_queue.pop()    // 从等待队列取出 C2
  ├─7─→ C_wake.state = READY
  └─8─→ Worker::enqueue(C_wake)            // C2 重新入队
```

#### 流程 5：优雅关闭

```
co_shutdown() / SIGINT
  │
  ├─1─→ Scheduler::shutting_down = true
  ├─2─→ co_go() 返回 INVALID_HANDLE        // 拒绝新协程
  ├─3─→ 等待所有 Worker 队列为空            // GlobalQueue + 所有 Local Queue
  ├─4─→ 遍历所有 SUSPENDED 协程             // 回收资源
  │     ├─ Stack::destroy()                // munmap 栈 + guard page
  │     └─ Coroutine::destroy()            // 释放上下文
  ├─5─→ 销毁 epoll 实例 + TimerWheel
  ├─6─→ 销毁同步原语（释放等待队列中的协程）
  └─7─→ Worker 线程 pthread_join → 进程退出
```

## 核心设计决策

| 决策项 | 选择 | 原因 |
|--------|------|------|
| 协程模型 | 有栈协程 (Stackful) | 可在任意调用深度挂起，对业务代码侵入最小 |
| 调度模型 | 对称 N:M Work-Stealing | 多核扩展性好，无中心瓶颈 |
| 上下文切换 | boost.context (fcontext) | 切换延迟 ~10-20ns，跨平台覆盖，可提取独立使用 |
| 栈管理 | Copy-on-Grow（4KB 初始） | 百万协程内存节省 8-10 倍 |
| I/O 模型 | 显式异步 API (co_read 系列) | 行为清晰可预期，无系统调用 hook 带来的副作用 |
| 平台 | Linux x86-64 + ARM64 | 聚焦性能优化，epoll 深度集成，覆盖服务器主流架构 |
| C++ 标准 | C++17 | 支持 optional/variant/string_view 等基础设施 |
| 协程取消 | 不支持强制取消 | 简化状态管理，依赖优雅关闭清理 |

## API 设计

### 协程生命周期

```cpp
// 启动协程，返回句柄
co_handle co_go(std::function<void()> task, co_options opts = {});

// 等待协程完成（阻塞当前协程，不阻塞 pthread）
void co_join(co_handle h);

// 让出执行权
void co_yield();

// 休眠（毫秒）
void co_sleep(uint64_t ms);

// 获取当前协程 ID
co_handle co_self();
```

### 启动选项

```cpp
struct co_options {
    int    priority   = 0;        // -20(低) ~ +20(高)
    size_t stack_size = 4096;     // 初始栈大小
    size_t max_stack  = 1048576;  // 最大栈大小
};
```

### 同步原语

| 原语 | 关键方法 |
|------|---------|
| `co_channel<T>` | send / recv / try_send / try_recv / close |
| `co_mutex` | lock / unlock / try_lock (+ RAII co_lock_guard) |
| `co_cond` | wait / notify_one / notify_all / wait_for |
| `co_rwlock` | read_lock / read_unlock / write_lock / write_unlock |
| `co_waitgroup` | add / done / wait |

### I/O API

```cpp
ssize_t co_read(int fd, void* buf, size_t count, int timeout_ms = -1);
ssize_t co_write(int fd, const void* buf, size_t count, int timeout_ms = -1);
int     co_accept(int fd, sockaddr* addr, socklen_t* len, int timeout_ms = -1);
int     co_connect(int fd, const sockaddr* addr, socklen_t len, int timeout_ms = -1);
```

### 调度器控制

```cpp
void co_init(const co_config& cfg);
void co_run();       // 启动事件循环（阻塞至 shutdown）
void co_shutdown();  // 优雅关闭

struct co_config {
    int               worker_count     = 0;   // 0 = CPU 核心数（或 cpu_ids 长度）
    std::vector<int>  cpu_ids          = {};  // 指定绑定的 CPU 核心 ID 列表
                                              // 空 = 自动 0 ~ worker_count-1
    int               event_timeout_ms = 10;  // epoll 超时
};
```

## 调度器设计

### Work-Stealing 流程

1. Worker 优先从本地 Local Queue 取任务
2. Local Queue 空时，从 Global Queue 批量拉取
3. 都为空时，随机选择 Victim Worker 窃取其队尾任务
4. 反复无任务后，Worker 进入 epoll_wait 休眠

### 优先级调度

- 优先级范围：-20（最低）~ +20（最高）
- 每个 Worker 本地优先级队列（最小堆）
- 同优先级 FIFO 保证公平
- **防饥饿 Aging**：低优先级协程超过 100ms 未执行，临时提升优先级

### 关键数据结构

- **Local Queue**：SPMC 无锁队列（仅 owner push，steal 端用 CAS）
- **Global Queue**：MPMC spinlock 保护（竞争频率低）
- **epoll 实例**：每 Worker 独立，避免全局竞争

## 协程层设计

### Coroutine 核心字段

```cpp
struct Coroutine {
    co_handle      id;           // 唯一 ID
    fcontext_t     ctx;          // boost.context 上下文
    CoroutineState state;        // READY / RUNNING / SUSPENDED / DEAD
    int            priority;     // -20 ~ +20

    // Copy-on-Grow 栈
    void*          stack_ptr;    // mmap 分配的栈底
    size_t         stack_size;   // 当前大小
    size_t         max_stack;    // 上限
    void*          guard_page;   // 保护页 (PROT_NONE)

    // 挂起上下文
    SuspendReason  reason;       // IO_WAIT / SLEEP / MUTEX / YIELD
    union { int wait_fd; uint64_t wake_time; };
};
```

### 状态机

```
READY → RUNNING → SUSPENDED → READY → ... → DEAD
                    ↑ I/O 就绪 / 超时 / 锁释放
```

### Copy-on-Grow 栈流程

1. 初始分配 4KB 栈 + 4KB guard page（mmap PROT_NONE）
2. 栈溢出触发 SIGSEGV → 信号处理器判断 guard page 命中
3. 分配 2× 大小新栈（mmap），memcpy 拷贝旧栈内容
4. 更新 fcontext 中的 rsp 寄存器指向新栈
5. munmap 旧栈，新 guard page 映射到新栈末尾
6. 超 max_stack 上限 → 抛出异常

## I/O 层设计

### Epoll Reactor

- 每个 Worker 拥有独立的 epoll 实例
- co_read/co_write → 注册 fd 到当前 Worker epoll → 挂起协程
- Worker 空闲时调用 epoll_wait → fd 就绪 → 唤醒对应协程 → 重新入队
- fd 与协程的映射：hash table (fd → co_handle)

### Timer Wheel

- 分层时间轮，O(1) 插入/删除
- 精度 1ms，最大单级 60s（超时使用多级轮盘）
- epoll_wait timeout 动态计算为最近的 timer 到期时间
- 用途：co_sleep、co_* timeout、co_cond.wait_for

## 优雅关闭

1. 外部信号或 co_shutdown() 触发
2. 停止接受新协程（co_go 返回错误）
3. 等待所有 Worker 队列中的协程执行完毕
4. 释放所有协程栈、epoll fd、定时器
5. 各 Worker 线程 join，进程正常退出

## 目录结构

```
include/                # 对外头文件
  hcoroutine/
    coroutine.h         # 协程生命周期 API
    channel.h           # co_channel<T>
    mutex.h             # co_mutex / co_cond / co_rwlock
    waitgroup.h         # co_waitgroup
    io.h                # co_read / co_write / co_accept / co_connect
    scheduler.h         # co_init / co_run / co_shutdown / co_config

src/
  common/               # 公共基础设施
    fcontext/           # boost.context 汇编 (x86-64 + ARM64)
    queue/              # 无锁队列 (SPMC / MPMC)
    utils/              # 工具函数
  features/
    scheduler/          # 调度器实现
      global_queue.cpp
      worker.cpp
      work_stealing.cpp
    coroutine/          # 协程实体
      coroutine.cpp
      stack.cpp         # Copy-on-Grow 栈管理
      context.cpp       # fcontext 封装
    io/
      reactor.cpp       # epoll Reactor
      timer_wheel.cpp   # 分层时间轮
    sync/               # 同步原语
      channel.cpp
      mutex.cpp
      cond.cpp
      rwlock.cpp
      waitgroup.cpp

test/                   # 测试
  unit/
    scheduler_test.cpp
    coroutine_test.cpp
    channel_test.cpp
    mutex_test.cpp
    io_test.cpp
  integration/
    echo_server_test.cpp
    priority_test.cpp

docs/
  superpowers/specs/    # 设计文档
```

## 性能指标（目标）

| 指标 | 目标值 |
|------|--------|
| 协程创建/销毁 | < 1us |
| 协程切换延迟 | < 20ns |
| 单 Worker 协程调度 | > 10M switches/s |
| 4 核 Work-Stealing 扩展效率 | > 80% |
| 10 万协程内存占用 | < 2GB |

## 测试策略

### 单元测试

- 每个同步原语独立测试（channel 关闭/缓冲、mutex 竞争、cond 通知丢失、rwlock 读写公平性）
- 协程生命周期测试（创建/挂起/恢复/销毁）
- 栈溢出与扩容测试
- Timer Wheel 精度与边界测试
- epoll Reactor 事件分发测试

### 集成测试

- Echo Server：多连接并发，验证 I/O 协程调度正确性
- 优先级调度测试：混合高低优先级任务，验证高优先先执行 + Aging 生效
- Work-Stealing 测试：不均匀负载场景，验证任务被正确窃取
- 优雅关闭测试：大量协程执行中触发 shutdown，验证资源完全释放
- 压力测试：百万协程创建 / 高 QPS I/O / CPU 满载调度稳定性
