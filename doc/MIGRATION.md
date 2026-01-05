# Migration Guide: proc_t to pids_stack API

## 概述 / Overview

本文档提供了从 procps-ng v3.3.16 (使用 `proc_t` API) 迁移到 v4.0.4 (使用 `pids_stack` API) 的完整指南。

This document provides a complete guide for migrating from procps-ng v3.3.16 (using `proc_t` API) to v4.0.4 (using `pids_stack` API).

## 背景 / Background

在 procps-ng 的开发历史中，commit 77dc22b 引入了重大重构，将旧的 `struct proc_t` 结构体改为新的 `struct pids_stack` 结构体。这个变化提供了更灵活、更高效的进程信息访问方式，但也意味着基于旧API开发的代码需要进行迁移。

In the development history of procps-ng, commit 77dc22b introduced a major refactoring that changed the old `struct proc_t` structure to the new `struct pids_stack` structure. This change provides a more flexible and efficient way to access process information, but it also means that code developed based on the old API needs to be migrated.

## 迁移策略 / Migration Strategy

我们提供了两种迁移策略：

We provide two migration strategies:

### 策略一：使用兼容层（推荐用于快速迁移）
### Strategy 1: Use Compatibility Layer (Recommended for Quick Migration)

兼容层提供了与旧 API 相似的接口，可以让您以最小的代码修改完成迁移。

The compatibility layer provides an interface similar to the old API, allowing you to complete the migration with minimal code changes.

**优点 / Advantages:**
- 代码修改量最小 / Minimal code changes
- 快速完成迁移 / Quick migration
- 保持代码可读性 / Maintain code readability

**缺点 / Disadvantages:**
- 性能可能不是最优 / Performance may not be optimal
- 增加了一层抽象 / Adds an abstraction layer
- 不能使用新API的所有特性 / Cannot use all features of the new API

### 策略二：直接使用新API（推荐用于新代码）
### Strategy 2: Use New API Directly (Recommended for New Code)

直接使用新的 pids API，充分利用其灵活性和性能优势。

Use the new pids API directly to take full advantage of its flexibility and performance.

**优点 / Advantages:**
- 最佳性能 / Best performance
- 使用新API的全部特性 / Use all features of the new API
- 面向未来 / Future-proof

**缺点 / Disadvantages:**
- 需要重写更多代码 / Requires rewriting more code
- 学习曲线较陡 / Steeper learning curve

## 代码示例 / Code Examples

### 示例1：使用兼容层迁移
### Example 1: Migration Using Compatibility Layer

#### 旧代码 (v3.3.16) / Old Code (v3.3.16)

```c
#include <proc/readproc.h>
#include <stdio.h>

int main() {
    PROCTAB *pt;
    proc_t *proc;
    
    /* 打开进程表，请求内存和状态信息 */
    pt = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLUSR);
    
    /* 遍历所有进程 */
    while ((proc = readproc(pt, NULL))) {
        printf("PID: %d\n", proc->tid);
        printf("User: %s\n", proc->euser ? proc->euser : "?");
        printf("Command: %s\n", proc->cmd ? proc->cmd : "?");
        printf("Memory (KB): %lu\n", proc->vm_rss);
        printf("Priority: %d\n", proc->priority);
        printf("\n");
        
        freeproc(proc);
    }
    
    closeproc(pt);
    return 0;
}
```

#### 新代码 - 使用兼容层 (v4.0.4) / New Code - Using Compatibility Layer (v4.0.4)

```c
#include <libproc2/procps_compat.h>
#include <stdio.h>

int main() {
    procps_compat_proctab *pt;
    proc_t *proc;
    
    /* 打开进程表，请求内存和状态信息 */
    pt = procps_compat_openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLUSR);
    if (!pt) {
        fprintf(stderr, "Failed to open proc table\n");
        return 1;
    }
    
    /* 遍历所有进程 */
    while ((proc = procps_compat_readproc(pt))) {
        printf("PID: %d\n", proc->tid);
        printf("User: %s\n", proc->euser ? proc->euser : "?");
        printf("Command: %s\n", proc->cmd ? proc->cmd : "?");
        printf("Memory (KB): %lu\n", proc->vm_rss);
        printf("Priority: %d\n", proc->priority);
        printf("\n");
        
        procps_compat_freeproc(proc);
    }
    
    procps_compat_closeproc(pt);
    return 0;
}
```

**变化说明 / Changes:**
1. 头文件从 `<proc/readproc.h>` 改为 `<libproc2/procps_compat.h>`
2. `PROCTAB` 类型改为 `procps_compat_proctab`
3. `openproc()` 改为 `procps_compat_openproc()`
4. `readproc()` 改为 `procps_compat_readproc()`
5. `freeproc()` 改为 `procps_compat_freeproc()`
6. `closeproc()` 改为 `procps_compat_closeproc()`

### 示例2：直接使用新API
### Example 2: Using New API Directly

```c
#include <libproc2/pids.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    struct pids_info *info = NULL;
    struct pids_fetch *fetch;
    
    /* 定义我们需要的信息项 */
    enum pids_item items[] = {
        PIDS_ID_PID,
        PIDS_ID_EUSER,
        PIDS_CMD,
        PIDS_VM_RSS,
        PIDS_PRIORITY
    };
    int num_items = sizeof(items) / sizeof(items[0]);
    
    /* 创建pids信息上下文 */
    if (procps_pids_new(&info, items, num_items) < 0) {
        fprintf(stderr, "Failed to create pids info\n");
        return 1;
    }
    
    /* 获取所有进程信息 */
    fetch = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
    if (!fetch) {
        fprintf(stderr, "Failed to reap processes\n");
        procps_pids_unref(&info);
        return 1;
    }
    
    /* 遍历所有进程 */
    for (int i = 0; i < fetch->counts->total; i++) {
        struct pids_stack *stack = fetch->stacks[i];
        
        printf("PID: %d\n", PIDS_VAL(0, s_int, stack));
        printf("User: %s\n", PIDS_VAL(1, str, stack));
        printf("Command: %s\n", PIDS_VAL(2, str, stack));
        printf("Memory (KB): %lu\n", PIDS_VAL(3, ul_int, stack));
        printf("Priority: %d\n", PIDS_VAL(4, s_int, stack));
        printf("\n");
    }
    
    /* 释放资源 */
    procps_pids_unref(&info);
    return 0;
}
```

### 示例3：混合使用 - 转换函数
### Example 3: Mixed Approach - Conversion Function

如果您的代码中有很多函数接受 `proc_t*` 参数，可以使用转换函数：

If your code has many functions that accept `proc_t*` parameters, you can use conversion functions:

```c
#include <libproc2/pids.h>
#include <libproc2/procps_compat.h>
#include <stdio.h>

/* 假设这是您现有的函数，接受proc_t参数 */
void print_process_info(proc_t *proc) {
    printf("PID: %d, CMD: %s, Memory: %lu KB\n",
           proc->tid, proc->cmd ? proc->cmd : "?", proc->vm_rss);
}

int main() {
    struct pids_info *info = NULL;
    struct pids_fetch *fetch;
    proc_t proc;
    
    enum pids_item items[] = {
        PIDS_ID_TID,
        PIDS_CMD,
        PIDS_VM_RSS
    };
    int num_items = sizeof(items) / sizeof(items[0]);
    
    if (procps_pids_new(&info, items, num_items) < 0) {
        fprintf(stderr, "Failed to create pids info\n");
        return 1;
    }
    
    fetch = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
    if (!fetch) {
        procps_pids_unref(&info);
        return 1;
    }
    
    for (int i = 0; i < fetch->counts->total; i++) {
        /* 将pids_stack转换为proc_t */
        if (procps_compat_stack_to_proc_t(
                fetch->stacks[i], &proc, items, num_items) == 0) {
            print_process_info(&proc);
            
            /* 清理proc_t中分配的字符串 */
            free(proc.cmd);
        }
    }
    
    procps_pids_unref(&info);
    return 0;
}
```

## API 对照表 / API Comparison

### 数据结构 / Data Structures

| 旧API (v3.3.16) | 新API (v4.0.4) | 说明 / Description |
|-----------------|----------------|-------------------|
| `struct proc_t` | `struct pids_stack` | 进程信息结构 / Process info structure |
| `PROCTAB` | `struct pids_info` | 进程表上下文 / Process table context |
| N/A | `struct pids_fetch` | 批量获取结果 / Batch fetch result |
| N/A | `enum pids_item` | 信息项枚举 / Item enumeration |

### 函数映射 / Function Mapping

| 旧API函数 | 兼容层函数 | 新API函数 | 说明 / Description |
|----------|-----------|----------|-------------------|
| `openproc()` | `procps_compat_openproc()` | `procps_pids_new()` | 初始化 / Initialize |
| `readproc()` | `procps_compat_readproc()` | `procps_pids_get()` / `procps_pids_reap()` | 读取进程 / Read process |
| `closeproc()` | `procps_compat_closeproc()` | `procps_pids_unref()` | 关闭 / Close |
| `freeproc()` | `procps_compat_freeproc()` | N/A (自动管理 / Auto-managed) | 释放内存 / Free memory |

### 字段映射 / Field Mapping

| proc_t 字段 | pids_item 枚举 | 类型 / Type |
|------------|---------------|------------|
| `tid` | `PIDS_ID_TID` | s_int |
| `ppid` | `PIDS_ID_PPID` | s_int |
| `tgid` | `PIDS_ID_TGID` | s_int |
| `state` | `PIDS_STATE` | s_ch |
| `utime` | `PIDS_TICS_USER` | ull_int |
| `stime` | `PIDS_TICS_SYSTEM` | ull_int |
| `priority` | `PIDS_PRIORITY` | s_int |
| `nice` | `PIDS_NICE` | s_int |
| `vm_rss` | `PIDS_VM_RSS` | ul_int |
| `vm_size` | `PIDS_VM_SIZE` | ul_int |
| `cmd` | `PIDS_CMD` | str |
| `cmdline` | `PIDS_CMDLINE` | str |
| `euser` | `PIDS_ID_EUSER` | str |
| `euid` | `PIDS_ID_EUID` | u_int |

完整的字段映射请参考 `library/include/procps_compat.h` 中的 `procps_compat_flags_to_items()` 函数。

For a complete field mapping, please refer to the `procps_compat_flags_to_items()` function in `library/include/procps_compat.h`.

## 编译说明 / Build Instructions

### 使用兼容层 / Using Compatibility Layer

```bash
gcc -o myapp myapp.c -lproc2
```

### 使用新API / Using New API

```bash
gcc -o myapp myapp.c -lproc2
```

确保安装了 procps-ng v4.0.4 或更高版本的开发文件。

Make sure you have the development files for procps-ng v4.0.4 or later installed.

## 性能考虑 / Performance Considerations

1. **新API更高效** / **New API is More Efficient**: 新的 pids API 允许您只请求需要的字段，减少了不必要的系统调用和内存分配。

2. **兼容层开销** / **Compatibility Layer Overhead**: 兼容层会先获取所有进程，然后逐个返回。对于大量进程的系统，这可能导致初始延迟。

3. **内存使用** / **Memory Usage**: 新API的内存管理更加精确，只分配实际需要的内存。

## 常见问题 / FAQ

### Q1: 兼容层是否支持所有旧API功能？
### Q1: Does the compatibility layer support all old API features?

A: 兼容层支持大部分常用功能。某些高级功能（如线程支持、实时过滤）可能需要直接使用新API。

A: The compatibility layer supports most common features. Some advanced features (such as thread support, real-time filtering) may require using the new API directly.

### Q2: 我应该使用哪种迁移策略？
### Q2: Which migration strategy should I use?

A: 
- 如果需要快速迁移现有代码，使用兼容层。
- 如果正在编写新代码或进行重大重构，直接使用新API。
- 如果有很多现有函数依赖 `proc_t`，使用转换函数。

A:
- If you need to quickly migrate existing code, use the compatibility layer.
- If you are writing new code or doing a major refactoring, use the new API directly.
- If you have many existing functions that depend on `proc_t`, use conversion functions.

### Q3: 兼容层的性能如何？
### Q3: What is the performance of the compatibility layer?

A: 兼容层在初始化时会获取所有进程信息，因此对于有大量进程的系统，可能会有初始延迟。但对于大多数应用，这个开销是可以接受的。

A: The compatibility layer fetches all process information at initialization, so there may be an initial delay for systems with many processes. However, for most applications, this overhead is acceptable.

## 更多资源 / Additional Resources

- procps_pids(3) - 新API的完整文档 / Complete documentation for the new API
- library/include/pids.h - 新API的头文件 / Header file for the new API
- library/include/readproc.h - 旧API的头文件（仍然可用）/ Header file for the old API (still available)
- library/include/procps_compat.h - 兼容层头文件 / Compatibility layer header file

## 贡献 / Contributing

如果您在迁移过程中发现问题或有改进建议，欢迎提交 issue 或 pull request。

If you find issues or have suggestions for improvements during migration, please submit an issue or pull request.
