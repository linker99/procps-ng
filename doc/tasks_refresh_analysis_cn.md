# tasks_refresh 函数原理分析

## 概述

`tasks_refresh` 函数是 procps-ng 项目中 `top` 工具的核心函数之一，负责从系统中收集进程/线程信息，并刷新所有窗口的任务数据。该函数位于 `src/top/top.c` 文件中（第 2836-2893 行）。

## 函数签名与位置

```c
static void *tasks_refresh (void *unused)
```

- **文件位置**: `src/top/top.c:2836-2893`
- **返回类型**: `void *` (支持作为 pthread 线程函数)
- **参数**: `void *unused` (未使用，为兼容线程接口)

## 函数主要功能

`tasks_refresh` 函数主要完成以下三个核心任务：

1. **计算时间增量和 CPU 缩放因子**：计算两次刷新之间的时间差，用于准确计算 CPU 使用率
2. **从系统获取进程/线程信息**：通过 libproc2 库的 `pids` API 获取当前所有进程或线程的信息
3. **更新窗口数据结构**：将获取的进程信息更新到所有窗口 (WIN_t) 的指针数组中

## 调用关系

### 上游调用者

`tasks_refresh` 函数在两种模式下被调用：

#### 1. 多线程模式 (THREADED_TSK 定义时)

在多线程模式下，`tasks_refresh` 作为独立线程运行：

- **初始化位置**: `before()` 函数 (`top.c:3791`)
  ```c
  if (0 != pthread_create(&Thread_id_tasks, NULL, tasks_refresh, NULL))
      error_exit(fmtmk(N_fmt(X_THREADINGS_fmt), __LINE__, strerror(errno)));
  pthread_setname_np(Thread_id_tasks, "update tasks");
  ```

- **同步触发**: `frame_make()` 函数 (`top.c:7546`)
  ```c
  #ifdef THREADED_TSK
     sem_post(&Semaphore_tasks_beg);  // 通知线程开始工作
  #else
     tasks_refresh(NULL);              // 直接调用
  #endif
  ```

- **等待完成**: `summary_show()` 函数 (`top.c:7054-7055`)
  ```c
  #ifdef THREADED_TSK
     sem_wait(&Semaphore_tasks_end);  // 等待线程完成
  #endif
  ```

#### 2. 单线程模式 (未定义 THREADED_TSK 时)

在单线程模式下，`tasks_refresh` 被直接调用：

- **主要调用点**: `frame_make()` 函数 (`top.c:7548`)
- **初始化调用**: `usleep_refresh()` 函数 (`top.c:2904`)

### 下游被调用的函数

`tasks_refresh` 主要调用以下 libproc2 库函数：

1. **procps_pids_reap()** (`library/pids.c:1544`)
   - 功能：获取系统中所有进程/线程的信息
   - 调用位置：`top.c:2866`
   
2. **procps_pids_select()** (`library/pids.c:1642`)
   - 功能：获取指定 PID 集合的进程信息（监控模式）
   - 调用位置：`top.c:2864`

## 详细实现原理

### 1. 线程同步机制

```c
do {
#ifdef THREADED_TSK
    sem_wait(&Semaphore_tasks_beg);  // 等待主线程信号
#endif
    
    // ... 执行数据收集和更新 ...
    
#ifdef THREADED_TSK
    sem_post(&Semaphore_tasks_end);  // 通知主线程完成
} while (1);                         // 多线程模式：无限循环
#else
} while (0);                         // 单线程模式：执行一次
#endif
```

- **多线程模式**: 使用信号量 (`Semaphore_tasks_beg` 和 `Semaphore_tasks_end`) 实现主线程与刷新线程的同步
- **单线程模式**: 直接执行一次后返回

### 2. 时间计算与 CPU 缩放

```c
if (0 != clock_gettime(CLOCK_BOOTTIME, &ts))
    Frame_etscale = 0;
else {
    uptime_cur = (ts.tv_sec + ts.tv_nsec * 1.0e-9);
    et = uptime_cur - uptime_sav;
    if (et < 0.01) et = 0.005;
    uptime_sav = uptime_cur;
    // 根据 Solaris 模式调整 CPU 缩放
    Frame_etscale = 100.0f / ((float)Hertz * (float)et * (Rc.mode_irixps ? 1 : Cpu_cnt));
}
```

**关键变量说明**:
- `uptime_sav`: 静态变量，保存上次的系统运行时间
- `et`: 时间增量（elapsed time），两次刷新之间的秒数
- `Frame_etscale`: 全局变量，CPU 使用率缩放因子
- `Rc.mode_irixps`: Irix 模式标志（影响多核 CPU 显示方式）

**计算公式**:
- Irix 模式: `Frame_etscale = 100.0 / (Hertz * et)`
- Solaris 模式: `Frame_etscale = 100.0 / (Hertz * et * Cpu_cnt)`

### 3. 进程信息收集

```c
what = Thread_mode ? PIDS_FETCH_THREADS_TOO : PIDS_FETCH_TASKS_ONLY;
if (Monpidsidx) {
    what |= PIDS_SELECT_PID;
    Pids_reap = procps_pids_select(Pids_ctx, (unsigned *)Monpids, Monpidsidx, what);
} else
    Pids_reap = procps_pids_reap(Pids_ctx, what);
```

**两种收集模式**:
1. **完整收集** (`procps_pids_reap`): 获取系统中所有进程/线程
2. **选择性收集** (`procps_pids_select`): 仅获取指定 PID 列表的进程（用于监控特定进程）

**参数说明**:
- `Pids_ctx`: libproc2 库的上下文，在 `before()` 函数中初始化
- `what`: 获取类型标志
  - `PIDS_FETCH_TASKS_ONLY`: 只获取进程
  - `PIDS_FETCH_THREADS_TOO`: 获取进程和线程
  - `PIDS_SELECT_PID`: 按 PID 选择

**返回值**:
- `Pids_reap`: `struct pids_fetch *` 类型，包含：
  - `counts->total`: 总进程/线程数
  - `counts->running`: 运行中的数量
  - `counts->sleeping`: 睡眠状态的数量
  - `counts->stopped`: 停止状态的数量
  - `counts->zombied`: 僵尸进程数量
  - `stacks`: 进程信息栈数组

### 4. 窗口数据更新

```c
// 宏定义
#define n_reap  Pids_reap->counts->total

// 动态分配策略
if (n_alloc < n_reap) {
    n_alloc = nALGN2(n_reap, 128);  // 对齐到 128 的倍数
    for (i = 0; i < GROUPSMAX; i++) {
        Winstk[i].ppt = alloc_r(Winstk[i].ppt, sizeof(void *) * n_alloc);
        memcpy(Winstk[i].ppt, Pids_reap->stacks, sizeof(void *) * PIDSmaxt);
    }
} else {
    for (i = 0; i < GROUPSMAX; i++)
        memcpy(Winstk[i].ppt, Pids_reap->stacks, sizeof(void *) * PIDSmaxt);
}
```

**内存管理策略**:
- `n_alloc`: 静态变量，当前已分配的数组大小
- `n_reap`: 本次获取到的进程总数
- **按需增长**: 只在需要更多空间时重新分配
- **对齐优化**: 使用 `nALGN2(n, 128)` 对齐到 128 的倍数，提高内存访问效率

**数据结构**:
- `Winstk[GROUPSMAX]`: 窗口数组，`GROUPSMAX = 4`（最多支持 4 个窗口）
- `Winstk[i].ppt`: 每个窗口的进程栈指针数组 (`struct pids_stack **`)
- `Pids_reap->stacks`: libproc2 返回的进程信息栈数组

## 相关数据结构

### WIN_t 结构 (窗口)

定义在 `top.h:363-410`：

```c
typedef struct WIN_t {
    // 字段配置
    FLG_t  pflgsall [PFLAGSSIZ];     // 所有活动字段
    FLG_t  procflgs [PFLAGSSIZ];     // 显示字段子集
    RCW_t  rc;                       // 保存到配置文件的设置
    
    // 窗口状态
    int    winnum;                   // 窗口编号
    int    winlines;                 // 当前窗口行数
    int    begtask;                  // 滚动起始位置
    
    // 进程数据
    struct pids_stack **ppt;         // 进程栈指针数组 (关键!)
    
    // 链表
    struct WIN_t *next, *prev;       // 窗口链表
} WIN_t;
```

### 全局变量

```c
static WIN_t  Winstk[GROUPSMAX];     // 4 个窗口数组
static WIN_t *Curwin;                // 当前活动窗口
static float  Frame_etscale;         // CPU 缩放因子
static struct pids_info *Pids_ctx;   // libproc2 上下文
static struct pids_fetch *Pids_reap; // 进程信息结果
```

## 执行流程图

```
程序启动 (main)
    ↓
before() - 初始化
    ↓
    ├─ procps_pids_new(&Pids_ctx, ...)  // 初始化 pids 库
    ↓
    └─ pthread_create(..., tasks_refresh, NULL)  // [多线程模式]
         └─ while(1) { sem_wait → 工作 → sem_post }

主循环 (frame_make)
    ↓
    ├─ [多线程模式]
    │   sem_post(&Semaphore_tasks_beg)  // 通知刷新线程
    │   (线程异步执行 tasks_refresh)
    │       ↓
    │       ├─ 计算时间增量 (uptime, et)
    │       ├─ 计算 Frame_etscale
    │       ├─ procps_pids_reap/select()
    │       └─ 更新 Winstk[].ppt
    │   
    ├─ [单线程模式]
    │   tasks_refresh(NULL)  // 直接调用
    │
    ↓
summary_show()
    ↓
    ├─ sem_wait(&Semaphore_tasks_end)  // 等待刷新完成
    ├─ 显示进程统计 (PIDSmaxt, running, sleeping...)
    ↓
window_show()
    ├─ 排序进程列表 (procps_pids_sort)
    ├─ 显示进程详细信息
    └─ 使用 Curwin->ppt 数组
```

## 性能优化要点

### 1. 内存分配优化
- 使用静态变量 `n_alloc` 避免频繁分配
- 对齐到 128 字节边界提高缓存效率
- 只在需要时增长，从不收缩

### 2. 多线程优化
- 数据收集与显示并行，减少延迟
- 使用信号量实现无锁同步
- 线程名称设置便于调试 (`"update tasks"`)

### 3. 时间计算优化
- 使用 `CLOCK_BOOTTIME` 避免系统时间调整影响
- 最小时间增量保护 (`if (et < 0.01) et = 0.005`)

## 与其他组件的交互

### 1. 与 libproc2 库交互

```
tasks_refresh
    ↓
procps_pids_reap()  [library/pids.c]
    ↓
pids_stacks_fetch()
    ↓
openproc() / readproc()  [library/readproc.c]
    ↓
读取 /proc 文件系统
```

### 2. 与显示系统交互

```
tasks_refresh 更新 Winstk[].ppt
    ↓
window_show() 读取 Curwin->ppt
    ↓
procps_pids_sort() 排序
    ↓
task_show() 显示每个进程
```

### 3. 与配置系统交互

- `Thread_mode`: 控制是否获取线程信息
- `Monpids/Monpidsidx`: 监控特定进程列表
- `Rc.mode_irixps`: 影响 CPU 计算模式

## 错误处理

```c
if (!Pids_reap)
    error_exit(fmtmk(N_fmt(LIB_errorpid_fmt), __LINE__, strerror(errno)));
```

- 检查 `procps_pids_reap/select` 返回值
- 失败时调用 `error_exit()` 终止程序
- 显示错误信息和行号便于调试

## 线程安全性

### 多线程模式下的保护

1. **信号量同步**: 
   - `Semaphore_tasks_beg`: 主线程 → 刷新线程
   - `Semaphore_tasks_end`: 刷新线程 → 主线程

2. **数据访问顺序**:
   - 刷新线程写入 `Pids_reap` 和 `Winstk[].ppt`
   - 主线程在 `sem_wait` 后读取数据
   - 保证顺序一致性

3. **静态变量**:
   - `uptime_sav`, `n_alloc`: 仅在刷新线程中访问
   - 无竞争条件

## 使用场景

### 常规模式
- 每次屏幕刷新时调用
- 默认 3 秒刷新间隔 (可配置)
- 显示所有进程/线程

### 监控模式
- 用户通过 'p' 命令指定监控 PID
- `Monpids` 数组存储 PID 列表
- 使用 `procps_pids_select()` 仅获取指定进程

### 线程模式
- 用户通过 'H' 命令切换
- `Thread_mode` 标志控制
- 显示线程级别信息

## 相关函数

- `cpus_refresh()`: 刷新 CPU 统计信息 (类似功能)
- `memory_refresh()`: 刷新内存统计信息 (类似功能)
- `usleep_refresh()`: 初始化时强制刷新并等待
- `frame_make()`: 主显示循环，调用本函数
- `summary_show()`: 显示摘要信息，使用本函数数据
- `window_show()`: 显示进程列表，使用本函数数据

## 总结

`tasks_refresh` 函数是 `top` 工具的数据引擎：

1. **职责单一**: 专注于数据收集和更新
2. **设计灵活**: 支持单线程和多线程两种模式
3. **性能优化**: 内存对齐、按需分配、并行处理
4. **可靠性高**: 完善的错误处理和线程同步
5. **扩展性好**: 支持多窗口、进程/线程切换、PID 监控

该函数通过与 libproc2 库的 `pids` API 交互，高效地从 `/proc` 文件系统中提取进程信息，并维护一个实时更新的进程列表供上层显示函数使用。其设计充分考虑了性能、并发和可维护性，是系统监控工具的典型实现。
