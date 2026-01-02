# tasks_refresh 函数原理分析

## 概述

`tasks_refresh` 是 procps-ng 项目中 `top` 工具的核心函数之一，位于 `src/top/top.c` 文件中（第 2836-2893 行）。此函数负责与 libprocps 的 `<pids>` API 交互，定期刷新进程/线程信息，并更新各个窗口的任务数据指针数组。

## 函数签名

```c
static void *tasks_refresh (void *unused)
```

- **返回值**: `void*` - 返回 NULL（作为 pthread 线程函数的标准签名）
- **参数**: `void *unused` - 未使用的参数（线程函数需要此签名）

## 主要职责

1. **时间计算**: 计算帧间隔时间，用于 CPU 使用率等指标的准确计算
2. **进程数据获取**: 从内核通过 libprocps API 获取最新的进程/线程信息
3. **内存管理**: 动态调整窗口数据结构的大小以容纳进程信息
4. **数据同步**: 将获取的进程数据同步到所有窗口（最多4个）

## 核心数据结构

### 全局变量（前提条件）

```c
static struct pids_info *Pids_ctx;           // libprocps pids API 上下文
static struct pids_fetch *Pids_reap;         // 存储获取的进程数据
static WIN_t Winstk[GROUPSMAX];              // 4个窗口数组 (GROUPSMAX=4)
```

### 窗口结构 (WIN_t)

```c
struct WIN_t {
    struct pids_stack **ppt;  // 指向进程堆栈的指针数组
    // ... 其他字段
};
```

### 进程获取结果 (pids_fetch)

```c
struct pids_fetch {
    struct pids_counts *counts;  // 进程统计信息
    struct pids_stack **stacks;  // 进程堆栈指针数组
};

struct pids_counts {
    int total;                   // 总进程数
    int running, sleeping, disk_sleep, stopped, zombied, other;
};
```

## 线程模式

该函数支持两种运行模式：

### 1. 线程模式 (THREADED_TSK 定义时)

- 作为独立后台线程运行
- 使用信号量进行同步：
  - `Semaphore_tasks_beg`: 等待主线程信号开始刷新
  - `Semaphore_tasks_end`: 通知主线程刷新完成
- 在无限循环中持续运行 (`while(1)`)

### 2. 同步模式 (THREADED_TSK 未定义时)

- 由主线程直接调用
- 执行一次后返回 (`while(0)`)

## 详细工作流程

### 第一步: 时间计算与帧缩放

```c
if (0 != clock_gettime(CLOCK_BOOTTIME, &ts))
    Frame_etscale = 0;
else {
    uptime_cur = (ts.tv_sec + ts.tv_nsec * 1.0e-9);
    et = uptime_cur - uptime_sav;
    if (et < 0.01) et = 0.005;
    uptime_sav = uptime_cur;
    // if in Solaris mode, adjust our scaling for all cpus
    Frame_etscale = 100.0f / ((float)Hertz * (float)et * (Rc.mode_irixps ? 1 : Cpu_cnt));
}
```

**目的**: 
- 获取系统启动时间
- 计算距离上次刷新的时间间隔 (et)
- 计算 `Frame_etscale` - CPU 使用率的缩放因子
  - 在 Irix 模式下 (mode_irixps=true)：不除以 CPU 数量，每个 CPU 可达 100%
  - 在 Solaris 模式下 (mode_irixps=false)：除以 CPU 数量，所有 CPU 总和为 100%

**关键变量**:
- `Hertz`: 系统时钟频率（通常是 100）
- `et`: 经过的时间（秒）
- `Cpu_cnt`: CPU 核心数

### 第二步: 确定获取模式

```c
what = Thread_mode ? PIDS_FETCH_THREADS_TOO : PIDS_FETCH_TASKS_ONLY;
if (Monpidsidx) {
    what |= PIDS_SELECT_PID;
    Pids_reap = procps_pids_select(Pids_ctx, (unsigned *)Monpids, Monpidsidx, what);
} else
    Pids_reap = procps_pids_reap(Pids_ctx, what);
```

**决策逻辑**:
1. **线程模式** (`Thread_mode`): 
   - `PIDS_FETCH_THREADS_TOO`: 获取进程和线程
   - `PIDS_FETCH_TASKS_ONLY`: 仅获取进程
   
2. **监控特定 PID** (`Monpidsidx > 0`):
   - 使用 `procps_pids_select()`: 仅获取指定的 PID 列表
   - `Monpids` 数组包含要监控的 PID
   
3. **获取所有进程**:
   - 使用 `procps_pids_reap()`: 获取系统中所有进程

### 第三步: 动态内存分配

#### 宏定义
```c
#define nALIGN(n,m) (((n + m - 1) / m) * m)     // 通用对齐
#define nALGN2(n,m) ((n + m - 1) & ~(m - 1))    // 2的幂对齐
#define n_reap  Pids_reap->counts->total        // 当前进程总数
```

#### 分配逻辑

```c
if (n_alloc < n_reap) {
    // 需要扩展数组
    n_alloc = nALGN2(n_reap, 128);  // 对齐到128的倍数
    for (i = 0; i < GROUPSMAX; i++) {
        Winstk[i].ppt = alloc_r(Winstk[i].ppt, sizeof(void *) * n_alloc);
        memcpy(Winstk[i].ppt, Pids_reap->stacks, sizeof(void *) * PIDSmaxt);
    }
} else {
    // 数组足够大，直接复制
    for (i = 0; i < GROUPSMAX; i++)
        memcpy(Winstk[i].ppt, Pids_reap->stacks, sizeof(void *) * PIDSmaxt);
}
```

**内存对齐**:
- 使用 `nALGN2(n_reap, 128)` 将分配大小对齐到 128 的倍数
- 这样可以减少频繁的内存重新分配
- 采用2的幂对齐，使用位操作提高效率

**为什么有4个窗口**:
- `top` 支持最多4个独立的窗口视图（GROUPSMAX = 4）
- 每个窗口可以有不同的进程显示配置
- 用户可以通过键盘切换窗口（1-4键）

### 第四步: 线程同步（仅线程模式）

```c
#ifdef THREADED_TSK
    sem_post(&Semaphore_tasks_end);  // 通知主线程完成
} while (1);                         // 继续循环
#else
} while (0);                         // 仅执行一次
#endif
```

## 调用场景

### 1. 初始化阶段

在 `before()` 函数中（第 3791-3793 行）创建后台线程：

```c
#ifdef THREADED_TSK
if (0 != pthread_create(&Thread_id_tasks, NULL, tasks_refresh, NULL))
    error_exit(fmtmk(N_fmt(X_THREADINGS_fmt), __LINE__, strerror(errno)));
pthread_setname_np(Thread_id_tasks, "update tasks");
#endif
```

### 2. 主显示循环

在 `frame_make()` 函数中（第 7546-7549 行）：

```c
#ifdef THREADED_TSK
    sem_post(&Semaphore_tasks_beg);  // 触发后台线程刷新
#else
    tasks_refresh(NULL);              // 直接调用
#endif
```

### 3. 强制刷新

在 `usleep_refresh()` 函数中（第 2899-2906 行）：

```c
static void usleep_refresh (void) {
#ifdef THREADED_TSK
    sem_post(&Semaphore_tasks_beg);
    sem_wait(&Semaphore_tasks_end);
#else
    tasks_refresh(NULL);
#endif
    usleep(LIB_USLEEP);
}
```

**用途**: 强制执行一次刷新并等待完成，然后休眠以避免数据偏差

## 性能优化要点

### 1. 内存对齐
- 使用128字节对齐减少内存碎片
- 位操作 `& ~(m-1)` 比除法运算更快

### 2. 条件编译
- 通过 `#ifdef THREADED_TSK` 支持两种模式
- 避免运行时条件判断开销

### 3. 静态变量
- `n_alloc` 和 `uptime_sav` 声明为 static
- 保持状态跨调用，避免重复计算

### 4. 预分配策略
- 不是每次都重新分配，只在需要时扩展
- 扩展时多分配一些空间（128对齐）

## 错误处理

```c
if (!Pids_reap)
    error_exit(fmtmk(N_fmt(LIB_errorpid_fmt), __LINE__, strerror(errno)));
```

如果无法获取进程数据，程序会立即终止并显示错误信息。

## 数据流图

```
[系统 /proc 文件系统]
         ↓
[libprocps pids API]
         ↓
[procps_pids_reap/select] → [Pids_reap]
         ↓
[tasks_refresh 函数]
         ↓
[Winstk[0..3].ppt 数组] → [各窗口进程数据]
         ↓
[显示函数使用这些数据渲染界面]
```

## 依赖关系

### 前置条件（由其他函数初始化）
1. `Pids_ctx` - 在程序启动时通过 `procps_pids_new()` 创建
2. `Winstk[]` - 在 `wins_stage_1()` 和 `wins_stage_2()` 中初始化
3. `Hertz` - 从系统获取的时钟频率
4. 线程模式下的信号量 - 在 `before()` 中初始化

### 后续使用（由此函数填充数据）
1. 进程列表显示
2. CPU 使用率计算（使用 `Frame_etscale`）
3. 任务统计信息显示（使用 `Pids_reap->counts`）

## 并发安全

### 线程模式下的同步机制

```
主线程                     tasks_refresh 线程
  │                              │
  ├── sem_post(beg) ─────────→ sem_wait(beg)
  │                              ├── 计算时间
  │                              ├── 获取进程数据
  │                              ├── 更新窗口数据
  │                              └── sem_post(end)
  ├── sem_wait(end) ←──────────┘
  │                              │
  └── 使用刷新的数据              └── 继续循环
```

## 总结

`tasks_refresh` 函数是 `top` 工具的核心数据获取引擎，它：

1. **桥接系统和应用**: 通过 libprocps API 从内核获取进程信息
2. **支持多种模式**: 可以获取进程或线程，全部或选定的 PID
3. **动态适应**: 根据进程数量自动调整内存分配
4. **高效同步**: 在线程模式下使用信号量实现异步数据刷新
5. **时间管理**: 精确计算帧间隔，确保 CPU 使用率等指标的准确性

该函数设计巧妙地平衡了性能、灵活性和正确性，是理解 `top` 工具工作原理的关键入口点。
