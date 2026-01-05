# procps_compat - Compatibility Layer for proc_t to pids_stack Migration

## 概述 / Overview

这是一个帮助从 procps-ng v3.3.16 (使用 `proc_t` API) 迁移到 v4.0.4+ (使用 `pids_stack` API) 的兼容层。

This is a compatibility layer to help migrate from procps-ng v3.3.16 (using `proc_t` API) to v4.0.4+ (using `pids_stack` API).

## 为什么需要这个兼容层？/ Why is this compatibility layer needed?

在 procps-ng 的开发历史中，commit 77dc22b 引入了重大重构：

In the development history of procps-ng, commit 77dc22b introduced a major refactoring:

- **旧API**: 使用 `struct proc_t` 和 `openproc()/readproc()/closeproc()` 函数
- **新API**: 使用 `struct pids_stack` 和 `procps_pids_new()/procps_pids_reap()/procps_pids_unref()` 函数

- **Old API**: Uses `struct proc_t` and `openproc()/readproc()/closeproc()` functions
- **New API**: Uses `struct pids_stack` and `procps_pids_new()/procps_pids_reap()/procps_pids_unref()` functions

对于基于旧API开发的大型项目，直接迁移到新API需要大量的代码修改。这个兼容层提供了一个过渡方案。

For large projects developed based on the old API, migrating directly to the new API requires extensive code changes. This compatibility layer provides a transition solution.

## 核心功能 / Core Features

### 1. 包装函数 / Wrapper Functions

提供与旧API相似的接口：

Provides an interface similar to the old API:

```c
procps_compat_proctab *procps_compat_openproc(unsigned flags);
proc_t *procps_compat_readproc(procps_compat_proctab *pt);
void procps_compat_freeproc(proc_t *proc);
void procps_compat_closeproc(procps_compat_proctab *pt);
```

### 2. 转换函数 / Conversion Functions

允许在新旧API之间转换数据：

Allows converting data between old and new APIs:

```c
int procps_compat_stack_to_proc_t(
    struct pids_stack *stack,
    proc_t *proc,
    enum pids_item *items,
    int num_items);
```

### 3. 标志映射 / Flag Mapping

自动将旧的 `PROC_*` 标志映射到新的 `pids_item` 枚举：

Automatically maps old `PROC_*` flags to new `pids_item` enums:

```c
int procps_compat_flags_to_items(
    unsigned flags,
    enum pids_item *items,
    int max_items);
```

## 快速开始 / Quick Start

### 迁移前 (v3.3.16) / Before Migration (v3.3.16)

```c
#include <proc/readproc.h>

PROCTAB *pt = openproc(PROC_FILLMEM | PROC_FILLSTAT);
proc_t *proc;
while ((proc = readproc(pt, NULL))) {
    printf("PID: %d\n", proc->tid);
    freeproc(proc);
}
closeproc(pt);
```

### 迁移后 (v4.0.4+, 使用兼容层) / After Migration (v4.0.4+, using compatibility layer)

```c
#include <libproc2/procps_compat.h>

procps_compat_proctab *pt = procps_compat_openproc(PROC_FILLMEM | PROC_FILLSTAT);
proc_t *proc;
while ((proc = procps_compat_readproc(pt))) {
    printf("PID: %d\n", proc->tid);
    procps_compat_freeproc(proc);
}
procps_compat_closeproc(pt);
```

**主要变化 / Main Changes**:
1. 头文件: `<proc/readproc.h>` → `<libproc2/procps_compat.h>`
2. 类型: `PROCTAB` → `procps_compat_proctab`
3. 函数: 添加 `procps_compat_` 前缀

## 文件说明 / File Description

- **library/include/procps_compat.h** - 兼容层头文件 / Compatibility layer header
- **library/procps_compat.c** - 兼容层实现 / Compatibility layer implementation
- **doc/MIGRATION.md** - 详细的迁移指南（中英文）/ Detailed migration guide (Chinese & English)
- **doc/example_compat.c** - 使用兼容层的示例程序 / Example program using compatibility layer
- **doc/example_newapi.c** - 使用新API的示例程序 / Example program using new API
- **library/tests/test_compat.c** - 兼容层测试程序 / Compatibility layer test program

## 编译和测试 / Build and Test

### 编译库 / Build Library

```bash
./autogen.sh
./configure
make
```

### 编译示例程序 / Build Example Programs

```bash
# 使用兼容层的示例
gcc -o example_compat doc/example_compat.c -lproc2

# 使用新API的示例
gcc -o example_newapi doc/example_newapi.c -lproc2
```

### 运行测试 / Run Tests

```bash
# 编译测试程序
gcc -o test_compat library/tests/test_compat.c -I./library/include -L./library/.libs -lproc2

# 运行测试
./test_compat
```

## 性能考虑 / Performance Considerations

### 兼容层开销 / Compatibility Layer Overhead

兼容层在初始化时会一次性获取所有进程信息，这可能导致：

The compatibility layer fetches all process information at once during initialization, which may cause:

1. **初始延迟** / **Initial Delay**: 对于有大量进程的系统（数千个进程），初始化可能需要几十到几百毫秒
2. **内存使用** / **Memory Usage**: 所有进程信息会被缓存在内存中

### 何时使用新API / When to Use New API

建议在以下情况直接使用新API：

It is recommended to use the new API directly in the following cases:

- 编写新代码 / Writing new code
- 需要最佳性能 / Need best performance
- 需要实时过滤或增量读取 / Need real-time filtering or incremental reading
- 只需要特定字段的信息 / Only need specific field information

## 支持的标志 / Supported Flags

兼容层支持以下 `PROC_*` 标志：

The compatibility layer supports the following `PROC_*` flags:

| 标志 / Flag | 说明 / Description |
|------------|-------------------|
| `PROC_FILLSTAT` | 填充 stat 信息 / Fill stat information |
| `PROC_FILLMEM` | 填充内存信息 / Fill memory information |
| `PROC_FILLSTATUS` | 填充 status 信息 / Fill status information |
| `PROC_FILLARG` | 填充命令行参数 / Fill command line arguments |
| `PROC_FILLENV` | 填充环境变量 / Fill environment variables |
| `PROC_FILLUSR` | 填充用户信息 / Fill user information |
| `PROC_FILLGRP` | 填充组信息 / Fill group information |
| `PROC_FILLCGROUP` | 填充 cgroup 信息 / Fill cgroup information |
| `PROC_FILLOOM` | 填充 OOM 信息 / Fill OOM information |
| `PROC_FILLNS` | 填充命名空间信息 / Fill namespace information |
| `PROC_FILLSYSTEMD` | 填充 systemd 信息 / Fill systemd information |
| `PROC_FILL_LXC` | 填充 LXC 信息 / Fill LXC information |
| `PROC_FILL_LUID` | 填充登录 UID / Fill login UID |
| `PROC_FILL_EXE` | 填充可执行文件路径 / Fill executable path |
| `PROC_FILLIO` | 填充 I/O 统计 / Fill I/O statistics |
| `PROC_FILLSMAPS` | 填充 smaps 信息 / Fill smaps information |
| `PROC_FILLAUTOGRP` | 填充 autogroup 信息 / Fill autogroup information |
| `PROC_FILL_SUPGRP` | 填充补充组 / Fill supplementary groups |
| `PROC_FILL_DOCKER` | 填充 Docker 信息 / Fill Docker information |
| `PROC_FILL_FDS` | 填充文件描述符数量 / Fill file descriptor count |

## 限制和注意事项 / Limitations and Considerations

### 当前限制 / Current Limitations

1. **线程支持** / **Thread Support**: 兼容层当前只支持进程，不支持线程遍历。如需线程支持，请使用新API的 `PIDS_FETCH_THREADS_TOO` 选项。
2. **PID/UID 过滤** / **PID/UID Filtering**: 不支持 `PROC_PID` 和 `PROC_UID` 标志的过滤功能。如需过滤，请使用新API的 `procps_pids_select()` 函数。
3. **增量读取** / **Incremental Reading**: 所有进程信息在 `openproc` 时一次性读取，不支持增量读取。

### 迁移建议 / Migration Recommendations

1. **逐步迁移** / **Gradual Migration**: 先使用兼容层让代码运行起来，然后逐步将关键部分迁移到新API
2. **测试覆盖** / **Test Coverage**: 迁移后充分测试，特别是边界情况
3. **性能监控** / **Performance Monitoring**: 监控迁移后的性能变化，必要时优化
4. **文档更新** / **Documentation Update**: 更新项目文档，说明使用的API版本

## 示例代码 / Example Code

### 示例1: 基本使用 / Example 1: Basic Usage

```c
#include <libproc2/procps_compat.h>
#include <stdio.h>

int main() {
    procps_compat_proctab *pt = procps_compat_openproc(PROC_FILLSTAT | PROC_FILLUSR);
    proc_t *proc;
    
    while ((proc = procps_compat_readproc(pt))) {
        printf("PID: %5d  User: %-10s  Cmd: %s\n", 
               proc->tid,
               proc->euser ? proc->euser : "?",
               proc->cmd ? proc->cmd : "?");
        procps_compat_freeproc(proc);
    }
    
    procps_compat_closeproc(pt);
    return 0;
}
```

### 示例2: 混合使用 / Example 2: Mixed Usage

```c
#include <libproc2/pids.h>
#include <libproc2/procps_compat.h>

void process_info(proc_t *p) {
    // 使用proc_t的现有函数
    printf("PID: %d\n", p->tid);
}

int main() {
    struct pids_info *info = NULL;
    enum pids_item items[] = { PIDS_ID_TID, PIDS_CMD };
    
    procps_pids_new(&info, items, 2);
    struct pids_fetch *fetch = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
    
    for (int i = 0; i < fetch->counts->total; i++) {
        proc_t proc;
        procps_compat_stack_to_proc_t(fetch->stacks[i], &proc, items, 2);
        process_info(&proc);
        free(proc.cmd);
    }
    
    procps_pids_unref(&info);
    return 0;
}
```

## 进一步阅读 / Further Reading

- **doc/MIGRATION.md** - 完整的迁移指南 / Complete migration guide
- **man procps_pids(3)** - 新API文档 / New API documentation
- **library/include/pids.h** - 新API头文件 / New API header file
- **library/include/readproc.h** - 旧API头文件（仍然可用）/ Old API header file (still available)

## 贡献 / Contributing

欢迎提交问题报告和改进建议！

Issues and improvement suggestions are welcome!

## 许可证 / License

本兼容层遵循与 procps-ng 相同的 LGPL 2.1+ 许可证。

This compatibility layer follows the same LGPL 2.1+ license as procps-ng.
