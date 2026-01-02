# libproc2 pids 库核心函数分析

## 概述

本文档详细分析 libproc2 库中与进程信息收集相关的核心函数链，这些函数被 `top` 工具的 `tasks_refresh` 函数调用，用于从 `/proc` 文件系统中获取和处理进程信息。

## 函数调用链

```
tasks_refresh (top.c)
    ↓
procps_pids_reap (pids.c:1544) [公共 API]
    ↓
pids_stacks_fetch (pids.c:1223) [内部函数]
    ↓
    ├─ readproc/readeither (readproc.c) [读取 /proc]
    ├─ pids_proc_tally (pids.c:1129) [统计进程状态]
    │   └─ pids_make_hist (pids.c:750) [历史记录]
    └─ pids_assign_results (pids.c:922) [赋值结果]
```

## 1. procps_pids_reap - 公共接口函数

### 函数签名

```c
PROCPS_EXPORT struct pids_fetch *procps_pids_reap (
    struct pids_info *info,
    enum pids_fetch_type which)
```

**位置**: `library/pids.c:1544-1578`

### 功能描述

`procps_pids_reap` 是 libproc2 库提供的公共 API，用于获取系统中所有进程（或线程）的信息。这是 `top` 工具与底层库交互的主要入口点。

### 参数说明

- **info**: `struct pids_info *` - 上下文指针，包含配置和状态信息
- **which**: `enum pids_fetch_type` - 获取类型
  - `PIDS_FETCH_TASKS_ONLY`: 仅获取进程
  - `PIDS_FETCH_THREADS_TOO`: 获取进程和线程

### 返回值

- 成功: 返回 `struct pids_fetch *` 指针，包含进程列表和统计信息
- 失败: 返回 `NULL`，并设置 `errno`

### 详细实现

```c
struct pids_fetch *procps_pids_reap (
    struct pids_info *info,
    enum pids_fetch_type which)
{
    struct timespec ts;
    int rc;

    // 1. 参数验证
    errno = EINVAL;
    if (info == NULL)
        return NULL;
    if (which != PIDS_FETCH_TASKS_ONLY && which != PIDS_FETCH_THREADS_TOO)
        return NULL;
    if (!info->maxitems)
        return NULL;
    errno = 0;

    // 2. 容器检查（如果启用）
    if (info->containers_yes)
        pids_containers_check();

    // 3. 打开 /proc 目录
    if (!pids_oldproc_open(&info->fetch_PT, info->oldflags))
        return NULL;
    
    // 4. 设置读取函数指针
    info->read_something = which ? readeither : readproc;

    // 5. 获取系统启动时间（用于计算进程运行时间）
    info->boot_tics = 0;
    if (0 >= clock_gettime(CLOCK_BOOTTIME, &ts))
        info->boot_tics = (ts.tv_sec + ts.tv_nsec * 1.0e-9) * info->hertz;

    // 6. 核心：调用 pids_stacks_fetch 获取所有进程信息
    rc = pids_stacks_fetch(info);

    // 7. 关闭 /proc 目录
    pids_oldproc_close(&info->fetch_PT);
    
    // 8. 返回结果
    return (rc > 0) ? &info->fetch.results : NULL;
}
```

### 关键步骤

1. **参数验证**: 确保传入的参数有效
2. **容器支持**: 检查容器环境（Docker 等）
3. **打开 /proc**: 使用旧版 `readproc` 接口打开 /proc 文件系统
4. **设置读取函数**:
   - `readproc`: 仅读取进程
   - `readeither`: 读取进程和线程
5. **计算启动时间**: 用于后续计算进程的运行时长
6. **获取进程数据**: 调用核心函数 `pids_stacks_fetch`
7. **清理资源**: 关闭 /proc 目录句柄
8. **返回结果**: 返回包含进程信息的结构体

### 与 procps_pids_select 的区别

```c
PROCPS_EXPORT struct pids_fetch *procps_pids_select (
    struct pids_info *info,
    unsigned *these,
    int numthese,
    enum pids_select_type which)
```

- **procps_pids_reap**: 获取所有进程
- **procps_pids_select**: 仅获取指定 PID/UID 的进程
- 两者都调用 `pids_stacks_fetch`，但 `select` 会先设置过滤条件

## 2. pids_stacks_fetch - 核心获取函数

### 函数签名

```c
static int pids_stacks_fetch (struct pids_info *info)
```

**位置**: `library/pids.c:1223-1281`

### 功能描述

`pids_stacks_fetch` 是内部核心函数，负责：
1. 遍历 /proc 目录中的所有进程
2. 为每个进程分配数据结构
3. 调用统计和赋值函数
4. 管理动态内存分配

### 实现原理

#### 阶段 1: 初始化

```c
#define n_alloc  info->fetch.n_alloc   // 已分配的栈数量
#define n_inuse  info->fetch.n_inuse   // 正在使用的栈数量
#define n_saved  info->fetch.n_alloc_save

struct stacks_extent *ext;

// 首次调用时初始化
if (!info->fetch.anchor) {
    // 分配初始指针数组（1024 个）
    if (!(info->fetch.anchor = calloc(STACKS_INIT, sizeof(void *))))
        return -1;
    // 分配实际的数据栈
    if (!(ext = pids_stacks_alloc(info, STACKS_INIT)))
        return -1;
    // 复制栈指针到 anchor 数组
    memcpy(info->fetch.anchor, ext->stacks, sizeof(void *) * STACKS_INIT);
    n_alloc = STACKS_INIT;
}
```

**数据结构**:
- `anchor`: 指针数组，指向各个进程的数据栈
- `STACKS_INIT = 1024`: 初始分配的栈数量
- 使用两级分配：指针数组 + 实际数据栈

#### 阶段 2: 历史切换和计数器重置

```c
pids_toggle_history(info);
memset(&info->fetch.counts, 0, sizeof(struct pids_counts));
```

- **历史切换**: 交换新旧历史数据，用于计算增量（如 CPU 使用率）
- **计数器重置**: 清零进程状态计数器

#### 阶段 3: 遍历进程

```c
n_inuse = 0;
while (info->read_something(info->fetch_PT, &info->fetch_proc)) {
    // 3.1 检查是否需要扩展数组
    if (!(n_inuse < n_alloc)) {
        n_alloc += STACKS_GROW;  // 每次增长 128
        if (!(info->fetch.anchor = realloc(info->fetch.anchor, sizeof(void *) * n_alloc))
        || (!(ext = pids_stacks_alloc(info, STACKS_GROW))))
            return -1;
        memcpy(info->fetch.anchor + n_inuse, ext->stacks, sizeof(void *) * STACKS_GROW);
    }
    
    // 3.2 统计进程状态
    if (!pids_proc_tally(info, &info->fetch.counts, &info->fetch_proc))
        return -1;
    
    // 3.3 赋值结果到栈
    if (!pids_assign_results(info, info->fetch.anchor[n_inuse++], &info->fetch_proc))
        return -1;
}
```

**关键点**:
- `read_something`: 函数指针，指向 `readproc` 或 `readeither`
- **按需增长**: 初始 1024，每次不足时增长 128
- 对每个进程调用：
  1. `pids_proc_tally`: 统计状态
  2. `pids_assign_results`: 填充数据

#### 阶段 4: 结果最终化

```c
// 分配结果数组（如果需要）
if (n_saved < n_inuse + 1) {
    n_saved = n_inuse + 1;
    if (!(info->fetch.results.stacks = realloc(info->fetch.results.stacks, 
                                                sizeof(void *) * n_saved)))
        return -1;
}

// 复制栈指针到结果数组
memcpy(info->fetch.results.stacks, info->fetch.anchor, sizeof(void *) * n_inuse);
info->fetch.results.stacks[n_inuse] = NULL;  // NULL 结尾

return n_inuse;
```

**双缓冲机制**:
- `anchor`: 内部工作数组
- `results.stacks`: 暴露给用户的数组（NULL 结尾）
- 这样可以保护内部数据结构不被用户修改

### 内存管理策略

```
初始状态: anchor[1024]
    ↓
进程数超过 1024
    ↓
realloc(anchor, 1024 + 128)
    ↓
继续增长...
    ↓
最终: anchor[1024 + 128 * N]
```

- **初始大小**: 1024 个进程栈
- **增长策略**: 每次增加 128 个
- **内存效率**: 只在需要时增长，从不收缩

## 3. pids_proc_tally - 进程状态统计

### 函数签名

```c
static inline int pids_proc_tally (
    struct pids_info *info,
    struct pids_counts *counts,
    proc_t *p)
```

**位置**: `library/pids.c:1129-1164`

### 功能描述

统计进程的状态并分类计数，同时调用历史记录函数。

### 实现细节

```c
static inline int pids_proc_tally (
    struct pids_info *info,
    struct pids_counts *counts,
    proc_t *p)
{
    // 根据进程状态分类计数
    switch (p->state) {
        case 'R':  // Running
            ++counts->running;
            break;
        case 'D':  // Disk sleep (不可中断睡眠)
            ++counts->disk_sleep;
            break;
        case 'S':  // Sleeping (可中断睡眠)
            ++counts->sleeping;
            break;
        case 't':  // Tracing stop
        case 'T':  // Stopped
            ++counts->stopped;
            break;
        case 'Z':  // Zombie (僵尸进程)
            ++counts->zombied;
            break;
        default:
            // 'I' (idle) - 空闲
            // 'P' (parked) - 停放
            // 'X' (dead) - 死亡（很少见到）
            ++counts->other;
            break;
    }
    ++counts->total;

    // 如果启用历史记录，则保存数据
    if (info->history_yes)
        return pids_make_hist(info, p);
    return 1;
}
```

### 进程状态分类

| 状态 | 含义 | 计数器字段 |
|------|------|-----------|
| R | Running（运行中） | running |
| D | Disk Sleep（不可中断睡眠，通常等待 I/O） | disk_sleep |
| S | Sleeping（可中断睡眠） | sleeping |
| T/t | Stopped（停止/跟踪停止） | stopped |
| Z | Zombie（僵尸进程） | zombied |
| I/P/X | Idle/Parked/Dead | other |

### counts 结构体

```c
struct pids_counts {
    int total;        // 总进程数
    int running;      // 运行中
    int sleeping;     // 睡眠中
    int disk_sleep;   // 不可中断睡眠
    int stopped;      // 停止
    int zombied;      // 僵尸进程
    int other;        // 其他状态
};
```

这些计数在 `top` 的摘要行中显示，例如：
```
Tasks: 325 total,   2 running, 323 sleeping,   0 stopped,   0 zombie
```

## 4. pids_make_hist - 历史数据记录

### 函数签名

```c
static inline int pids_make_hist (
    struct pids_info *info,
    proc_t *p)
```

**位置**: `library/pids.c:750-783`

### 功能描述

为每个进程维护历史数据，用于计算增量值（如 CPU 使用率、页错误增量）。

### 实现原理

```c
static inline int pids_make_hist (
    struct pids_info *info,
    proc_t *p)
{
    TIC_t tics;
    HST_t *h;
    int slot = info->hist->num_tasks;

    // 1. 检查是否需要扩展历史数组
    if (slot + 1 >= Hr(HHist_siz)) {
        Hr(HHist_siz) += NEWOLD_GROW;  // 增加 128
        Hr(PHist_sav) = realloc(Hr(PHist_sav), sizeof(HST_t) * Hr(HHist_siz));
        Hr(PHist_new) = realloc(Hr(PHist_new), sizeof(HST_t) * Hr(HHist_siz));
        if (!Hr(PHist_sav) || !Hr(PHist_new))
            return 0;
    }
    
    // 2. 保存当前进程的数据到新历史记录
    Hr(PHist_new[slot].pid)  = p->tid;
    Hr(PHist_new[slot].maj)  = p->maj_flt;    // 主页错误数
    Hr(PHist_new[slot].min)  = p->min_flt;    // 次页错误数
    Hr(PHist_new[slot].tics) = tics = (p->utime + p->stime);  // CPU 时间

    // 3. 将新记录添加到哈希表
    pids_histput(info, slot);

    // 4. 查找上次记录，计算增量
    if ((h = pids_histget(info, p->tid))) {
        tics -= h->tics;                      // CPU 增量
        p->maj_delta = p->maj_flt - h->maj;   // 主页错误增量
        p->min_delta = p->min_flt - h->min;   // 次页错误增量
    }
    
    // 5. 保存 CPU 时间增量（用于计算 %CPU）
    p->pcpu = tics;

    info->hist->num_tasks++;
    return 1;
}
```

### 历史数据结构

```c
typedef struct HST_t {
    TIC_t tics;              // CPU 时间（用户时间 + 系统时间）
    unsigned long maj, min;  // 主/次页错误计数
    int pid;                 // 进程 ID（哈希键）
    int lnk;                 // 哈希链表下一个节点
} HST_t;

struct history_info {
    int    num_tasks;        // 当前任务数量
    int    HHist_siz;        // 历史数组大小
    HST_t *PHist_sav;        // 旧历史数据
    HST_t *PHist_new;        // 新历史数据
    int    HHash_one[HHASH_SIZE];   // 哈希表 1
    int    HHash_two[HHASH_SIZE];   // 哈希表 2
    int    HHash_nul[HHASH_SIZE];   // 空哈希表模板
    int   *PHash_sav;        // 指向旧哈希表
    int   *PHash_new;        // 指向新哈希表
};
```

### 双缓冲机制

```
第 N 次刷新:
  PHist_sav = 历史数据 N-1
  PHist_new = 历史数据 N (正在填充)
  PHash_sav = 哈希表 N-1
  PHash_new = 哈希表 N (正在填充)

pids_toggle_history() 调用后:
  PHist_sav = 历史数据 N
  PHist_new = 历史数据 N-1 (将被覆盖)
  PHash_sav = 哈希表 N
  PHash_new = 哈希表 N-1 (将被覆盖)
```

### 哈希表机制

```c
#define HHASH_SIZE  4096
#define _HASH_PID_(pid) (pid & (HHASH_SIZE - 1))

// 插入哈希表
static inline void pids_histput (
    struct pids_info *info,
    unsigned this)
{
    int V = _HASH_PID_(Hr(PHist_new[this].pid));
    Hr(PHist_new[this].lnk) = Hr(PHash_new[V]);
    Hr(PHash_new[V]) = this;
}

// 从哈希表查找
static inline HST_t *pids_histget (
    struct pids_info *info,
    int pid)
{
    int V = Hr(PHash_sav[_HASH_PID_(pid)]);
    
    while (-1 < V) {
        if (Hr(PHist_sav[V].pid) == pid)
            return &Hr(PHist_sav[V]);
        V = Hr(PHist_sav[V].lnk);
    }
    return NULL;
}
```

**哈希表特性**:
- **大小**: 4096 个桶
- **哈希函数**: PID & 4095 (简单模运算)
- **冲突解决**: 链地址法
- **查找时间**: O(1) 平均，O(n) 最坏

### 增量计算

CPU 使用率百分比的计算：
```c
// pids_make_hist 中：
p->pcpu = tics;  // 本次和上次之间的 CPU tics 增量

// top 显示时：
cpu_percent = (pcpu * 100.0) / (elapsed_tics * num_cpus);
```

这就是为什么 `top` 能够显示每个进程的 CPU 使用率百分比。

## 5. pids_toggle_history - 历史数据切换

### 函数签名

```c
static inline void pids_toggle_history (struct pids_info *info)
```

**位置**: `library/pids.c:786-801`

### 功能描述

在每次刷新开始时交换新旧历史缓冲区，实现高效的双缓冲。

### 实现细节

```c
static inline void pids_toggle_history (struct pids_info *info)
{
    void *v;

    // 交换历史数据指针
    v = Hr(PHist_sav);
    Hr(PHist_sav) = Hr(PHist_new);
    Hr(PHist_new) = v;

    // 交换哈希表指针
    v = Hr(PHash_sav);
    Hr(PHash_sav) = Hr(PHash_new);
    Hr(PHash_new) = v;
    
    // 清空新哈希表
    memcpy(Hr(PHash_new), Hr(HHash_nul), sizeof(Hr(HHash_nul)));

    // 重置任务计数
    info->hist->num_tasks = 0;
}
```

### 优势

1. **零拷贝**: 只交换指针，不复制数据
2. **内存高效**: 重用已分配的内存
3. **性能优化**: O(1) 时间复杂度

## 6. pids_assign_results - 结果赋值

### 函数签名

```c
static inline int pids_assign_results (
    struct pids_info *info,
    struct pids_stack *stack,
    proc_t *p)
```

**位置**: `library/pids.c:922-937`

### 功能描述

将从 `/proc` 读取的原始进程数据 (`proc_t`) 转换为用户请求的字段格式。

### 实现原理

```c
static inline int pids_assign_results (
    struct pids_info *info,
    struct pids_stack *stack,
    proc_t *p)
{
    struct pids_result *this = stack->head;
    SET_t *that = &info->func_array[0];

    info->seterr = 0;
    
    // 遍历所有需要的字段
    while (*that) {
        (*that)(info, this, p);  // 调用字段设置函数
        ++this;
        ++that;
    }
    
    return !info->seterr;
}
```

### 函数指针数组机制

```c
typedef void (*SET_t)(struct pids_info *, struct pids_result *, proc_t *);

struct pids_info {
    ...
    SET_t *func_array;  // 字段设置函数指针数组
    ...
};
```

**工作流程**:
1. 用户在 `procps_pids_new()` 时指定需要的字段（如 PID, CPU, MEM）
2. 库构建 `func_array`，每个元素是对应字段的设置函数
3. `pids_assign_results` 遍历数组，调用每个设置函数
4. 每个设置函数从 `proc_t` 提取数据填充到 `pids_result`

### 示例字段设置函数

```c
// 设置 PID 字段
static void setsfunc_pid(struct pids_info *info, struct pids_result *r, proc_t *p) {
    r->result.s_int = p->tid;
}

// 设置 CPU 字段
static void setsfunc_pcpu(struct pids_info *info, struct pids_result *r, proc_t *p) {
    r->result.real = p->pcpu;  // 已在 pids_make_hist 中计算
}
```

## 完整执行流程

### 时序图

```
top 调用 tasks_refresh
    ↓
调用 procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY)
    ↓
    ├─ 打开 /proc
    ├─ 设置 read_something = readproc
    ├─ 获取 boot_tics
    ↓
    调用 pids_stacks_fetch(info)
        ↓
        ├─ 初始化 anchor[1024]
        ├─ 调用 pids_toggle_history() [交换缓冲区]
        ├─ 清零 counts
        ↓
        while (readproc(fetch_PT, &fetch_proc)) {  [遍历 /proc]
            ↓
            ├─ 检查空间，必要时扩展 anchor
            ↓
            调用 pids_proc_tally(info, &counts, &fetch_proc)
                ↓
                ├─ 根据 state 更新 counts
                ↓
                调用 pids_make_hist(info, &fetch_proc)
                    ↓
                    ├─ 保存当前 tics, maj_flt, min_flt
                    ├─ 调用 pids_histput() [插入哈希表]
                    ├─ 调用 pids_histget() [查找旧数据]
                    └─ 计算增量: pcpu, maj_delta, min_delta
            ↓
            调用 pids_assign_results(info, anchor[n], &fetch_proc)
                ↓
                for (每个请求的字段) {
                    调用字段设置函数(info, result, &fetch_proc)
                }
        }
        ↓
        └─ 复制结果到 results.stacks
    ↓
    ├─ 关闭 /proc
    └─ 返回 &info->fetch.results
    ↓
返回到 tasks_refresh
```

### 数据流

```
/proc/[pid]/stat, /proc/[pid]/status, ...
    ↓ [readproc]
proc_t 结构 (原始数据)
    ↓ [pids_proc_tally]
pids_counts 结构 (状态统计)
    ↓ [pids_make_hist]
历史记录 + 增量计算
    ↓ [pids_assign_results]
pids_result 结构 (格式化结果)
    ↓
pids_fetch 结构 (返回给用户)
    ↓
top 的 Winstk[].ppt 数组
```

## 性能考虑

### 1. 内存分配策略

- **初始分配**: 1024 个进程栈（适合大多数系统）
- **增长策略**: 每次增加 128（避免频繁重分配）
- **零收缩**: 从不释放内存（适合长期运行的守护进程）

### 2. 哈希表优化

- **大小**: 4096 个桶（足够大以减少冲突）
- **哈希函数**: 简单快速的位运算
- **查找**: O(1) 平均时间复杂度

### 3. 双缓冲机制

- **零拷贝**: 只交换指针
- **并发友好**: 新旧数据完全隔离
- **内存稳定**: 重用已分配内存

### 4. 函数内联

关键函数使用 `inline` 减少函数调用开销：
- `pids_proc_tally`
- `pids_make_hist`
- `pids_toggle_history`
- `pids_assign_results`

## 常见问题

### Q1: 为什么需要历史数据？

**答**: 计算增量值：
- CPU 使用率 = (当前 CPU tics - 上次 CPU tics) / 时间增量
- 页错误率 = (当前页错误 - 上次页错误) / 时间增量

### Q2: 为什么使用双缓冲？

**答**: 
- 避免数据竞争
- 提高性能（零拷贝）
- 简化代码逻辑

### Q3: 内存会无限增长吗？

**答**: 不会。数组大小会增长到容纳当前最大进程数，然后稳定。即使进程数减少，数组也不收缩（为下次增长做准备）。

### Q4: 哈希冲突怎么处理？

**答**: 使用链地址法。每个哈希桶是一个链表，通过 `lnk` 字段连接。

## 总结

这五个函数构成了 libproc2 的核心数据收集引擎：

1. **procps_pids_reap**: 公共 API，协调整个流程
2. **pids_stacks_fetch**: 核心引擎，遍历进程并管理内存
3. **pids_proc_tally**: 状态统计，为摘要显示提供数据
4. **pids_make_hist**: 历史记录，支持增量计算
5. **pids_assign_results**: 数据转换，格式化用户请求的字段

它们的设计体现了：
- **高性能**: 内联函数、哈希表、双缓冲
- **内存高效**: 按需分配、重用机制
- **灵活性**: 函数指针、可配置字段
- **可靠性**: 完善的错误处理

这些函数为 `top`、`ps`、`pidof` 等工具提供了统一、高效的进程信息接口。
