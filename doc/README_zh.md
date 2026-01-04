# proc_t 到 pids.h API 迁移指南

## 概述

本文档帮助开发者将代码从旧的 `proc_t` 结构体 API（使用 `readproc.h`）迁移到新的 `pids.h` API。

这个重大的 API 重新设计是在 2015 年 8 月的提交 77dc22b 左右引入的，提高了性能、灵活性和可维护性。

## 关于 commit 77dc22b

**提交信息:**
```
commit 77dc22b9101af39fc30306e36d65cad2b396cc9e
Author: Jim Warner <james.warner@comcast.net>
Date:   Wed Aug 19 00:00:00 2015 -0500

    top: exploit those new library task/threads provisions
    
    This patch adapts top to exploit the new <proc/pids.h>
    interface.
```

这个提交是 procps-ng 项目中的一个重要里程碑，它将 `top` 工具从旧的 `proc_t` 结构体 API 迁移到新的 `<proc/pids.h>` 接口。

## 主要变化

### 旧 API (proc_t / readproc.h) 的特点
- **基于结构体**: 数据存储在一个大的 `proc_t` 结构体中
- **基于标志的选择**: 使用 `PROC_FILL*` 标志选择要读取的数据
- **直接字段访问**: 直接访问字段如 `p->pid`、`p->cmdline`
- **基于回调的排序**: 自定义排序回调函数
- **固定内存布局**: 所有字段都会被分配，无论是否需要

### 新 API (pids.h) 的特点
- **基于结果栈**: 数据以结果项栈的形式返回
- **基于项目的选择**: 明确请求特定的 `PIDS_*` 项目
- **基于宏的访问**: 使用 `PIDS_VAL()` 宏提取值
- **内置排序**: 库提供排序功能
- **高效内存**: 仅获取请求的项目

## 快速示例对比

### 旧代码 (proc_t)
```c
#include <proc/readproc.h>

PROCTAB *pt;
proc_t *proc = NULL;

pt = openproc(PROC_FILLSTAT | PROC_FILLMEM);
while ((proc = readproc(pt, proc)) != NULL) {
    printf("PID: %d, CMD: %s, RSS: %lu\n",
           proc->tid, proc->cmd, proc->vm_rss);
}
closeproc(pt);
```

### 新代码 (pids.h)
```c
#include <proc/pids.h>

struct pids_info *info = NULL;
struct pids_fetch *fetched;

enum pids_item items[] = {
    PIDS_ID_PID,
    PIDS_CMD,
    PIDS_MEM_RES
};

procps_pids_new(&info, items, 3);
fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);

for (int i = 0; i < fetched->counts->total; i++) {
    struct pids_stack *stack = fetched->stacks[i];
    printf("PID: %d, CMD: %s, RSS: %lu\n",
           PIDS_VAL(0, s_int, stack),
           PIDS_VAL(1, str, stack),
           PIDS_VAL(2, ul_int, stack));
}

procps_pids_unref(&info);
```

## 字段映射快速查询表

| proc_t 字段 | pids.h 枚举 | 类型 |
|-------------|------------|------|
| `tid` | `PIDS_ID_TID` | `s_int` |
| `tgid` | `PIDS_ID_TGID` | `s_int` |
| `ppid` | `PIDS_ID_PPID` | `s_int` |
| `state` | `PIDS_STATE` | `s_ch` |
| `cmd` | `PIDS_CMD` | `str` |
| `cmdline` | `PIDS_CMDLINE` | `str` |
| `euser` | `PIDS_ID_EUSER` | `str` |
| `euid` | `PIDS_ID_EUID` | `u_int` |
| `vm_size` | `PIDS_MEM_VIRT` | `ul_int` |
| `vm_rss` | `PIDS_MEM_RES` | `ul_int` |
| `vm_swap` | `PIDS_VM_SWAP` | `ul_int` |
| `utime` | `PIDS_TIME_USER` | `ull_int` |
| `stime` | `PIDS_TIME_SYSTEM` | `ull_int` |
| `nice` | `PIDS_NICE` | `s_int` |
| `priority` | `PIDS_PRIORITY` | `s_int` |

## 完整文档

详细的迁移指南和示例代码请参考以下文档：

1. **完整迁移指南**: `MIGRATION_GUIDE_proc_t_to_pids.md` (英文)
   - API 详细对比
   - 完整的字段映射表
   - 多个迁移示例
   - 最佳实践
   - 常见陷阱

2. **快速参考**: `QUICK_REFERENCE_proc_t_to_pids.md` (英文)
   - 快速查找表
   - 代码模板
   - 常见错误

3. **示例代码**: `migration_example.c`
   - 可编译的示例代码
   - 新旧 API 并行对比
   - 5 个实际使用场景

## 迁移步骤

1. **包含正确的头文件**
   ```c
   // 旧: #include <proc/readproc.h>
   // 新: #include <proc/pids.h>
   ```

2. **定义需要的项目**
   ```c
   enum pids_item items[] = {
       PIDS_ID_PID,
       PIDS_CMD,
       PIDS_MEM_RES,
       // 其他需要的字段...
   };
   ```

3. **初始化上下文**
   ```c
   struct pids_info *info = NULL;
   procps_pids_new(&info, items, numitems);
   ```

4. **获取进程数据**
   ```c
   struct pids_fetch *fetched;
   fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
   ```

5. **遍历结果**
   ```c
   for (int i = 0; i < fetched->counts->total; i++) {
       struct pids_stack *stack = fetched->stacks[i];
       // 使用 PIDS_VAL(index, type, stack) 访问值
   }
   ```

6. **清理资源**
   ```c
   procps_pids_unref(&info);
   ```

## 编译选项

- **旧 API**: 使用 `-lproc` 链接
- **新 API**: 使用 `-lproc2` 链接

## 示例：过滤特定 PID

### 旧代码
```c
pid_t pids[] = {1234, 0};  // 0 结尾
pt = openproc(PROC_FILLSTAT | PROC_PID, pids);
```

### 新代码
```c
unsigned int pids[] = {1234};
fetched = procps_pids_select(info, pids, 1, PIDS_SELECT_PID);
```

## 示例：包含线程

### 旧代码
```c
while ((proc = readeither(pt, proc)) != NULL) {
    printf("TID: %d, TGID: %d\n", proc->tid, proc->tgid);
}
```

### 新代码
```c
fetched = procps_pids_reap(info, PIDS_FETCH_THREADS_TOO);
for (int i = 0; i < fetched->counts->total; i++) {
    struct pids_stack *stack = fetched->stacks[i];
    printf("TID: %d, TGID: %d\n",
           PIDS_VAL(tid_idx, s_int, stack),
           PIDS_VAL(tgid_idx, s_int, stack));
}
```

## 性能优势

新 API 提供了几个性能优势：

1. **减少内存使用**: 仅分配请求的项目
2. **更好的缓存**: 库可以缓存和重用数据
3. **高效排序**: 内置排序已优化
4. **批量操作**: `reap()` 一次获取所有进程
5. **选择性读取**: 只从 /proc 读取需要的内容

## 常见问题

### Q: 我的代码使用了 proc_t 结构体，如何适配？
A: 按照本指南的步骤，将每个 proc_t 字段映射到相应的 PIDS_* 枚举。参考 `MIGRATION_GUIDE_proc_t_to_pids.md` 中的完整映射表。

### Q: 我可以同时支持两个 API 吗？
A: 可以，您可以使用条件编译创建一个兼容层。参考完整迁移指南中的"兼容层"部分。

### Q: 新 API 的性能如何？
A: 新 API 通常更快，因为它只获取您请求的数据，并且内置了优化的排序和缓存机制。

### Q: 我需要修改 Makefile 吗？
A: 是的，将链接标志从 `-lproc` 改为 `-lproc2`。

## 获取帮助

如果遇到迁移问题：

1. 查看本目录中的完整文档
2. 参考 `library/include/pids.h` 中的 API 文档
3. 查看 `src/ps/` 和 `src/top/` 中的实际应用示例
4. 在 procps-ng 项目页面提交问题

## 文档文件列表

- `README_zh.md` (本文件) - 中文概述
- `MIGRATION_GUIDE_proc_t_to_pids.md` - 完整迁移指南（英文）
- `QUICK_REFERENCE_proc_t_to_pids.md` - 快速参考（英文）
- `migration_example.c` - 可编译的示例代码

## 总结

虽然从 `proc_t` 迁移到 `pids.h` 需要一些代码更改，但新 API 具有以下优势：

- ✅ 更高效：仅获取需要的数据
- ✅ 更灵活：轻松添加/删除项目
- ✅ 更易维护：更清晰的关注点分离
- ✅ 更健壮：更好的错误处理

参考本指南和示例，您可以成功将代码迁移到新的 API。从简单的案例开始，逐步重构更复杂的代码。新 API 的设计使编写正确、高效的进程监控工具变得更容易。
