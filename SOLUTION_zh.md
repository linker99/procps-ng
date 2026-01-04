# 关于 commit 77dc22b 的迁移指南

## 问题回答

您的问题是："分析这个commit 77dc22b。有一个项目是基于77dc22b补丁之前开发的，有些接口是使用了proc_t结构体这样的结构体，这些代码如何适配到77dc22b之后的版本呢？"

## 简短答案

commit 77dc22b (2015年8月) 是一个重大的 API 重新设计，将旧的 `proc_t` 结构体 API 替换为新的 `pids.h` API。我已经创建了完整的迁移文档来帮助您的项目进行适配。

## 提交的文档

在 `doc/` 目录下，我创建了以下文档：

### 1. README_zh.md
**中文概述文档** - 快速入门指南
- 介绍新旧 API 的区别
- 常用示例对比
- 快速参考表

### 2. MIGRATION_GUIDE_proc_t_to_pids.md
**完整迁移指南**（英文）- 详细的技术文档
- API 详细对比
- 完整的字段映射表（500+ 个映射）
- 多个迁移示例
- 最佳实践
- 常见陷阱

### 3. QUICK_REFERENCE_proc_t_to_pids.md
**快速参考**（英文）- 开发时的速查表
- 快速查找表
- 代码模板
- 类型参考

### 4. migration_example.c
**可编译运行的示例代码**
- 展示新旧 API 的使用方法
- 包含 5 个实际示例
- 可以编译并运行

### 5. README_migration.md
**文档总索引**（英文）
- 解释所有文档的用途
- 编译示例的说明

## 核心变化

### 旧 API (77dc22b 之前)
```c
#include <proc/readproc.h>

PROCTAB *pt;
proc_t *proc = NULL;

pt = openproc(PROC_FILLSTAT | PROC_FILLMEM);
while ((proc = readproc(pt, proc)) != NULL) {
    printf("PID: %d, RSS: %lu\n", proc->tid, proc->vm_rss);
}
closeproc(pt);
```

### 新 API (77dc22b 之后)
```c
#include <pids.h>

struct pids_info *info = NULL;
struct pids_fetch *fetched;

enum pids_item items[] = {PIDS_ID_PID, PIDS_MEM_RES};
procps_pids_new(&info, items, 2);

fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
for (int i = 0; i < fetched->counts->total; i++) {
    struct pids_stack *stack = fetched->stacks[i];
    printf("PID: %d, RSS: %lu\n",
           PIDS_VAL(0, s_int, stack),
           PIDS_VAL(1, ul_int, stack));
}

procps_pids_unref(&info);
```

## 字段映射示例

| proc_t 字段 | pids.h 枚举 | 类型 |
|-------------|------------|------|
| `tid` | `PIDS_ID_TID` | `s_int` |
| `ppid` | `PIDS_ID_PPID` | `s_int` |
| `state` | `PIDS_STATE` | `s_ch` |
| `cmd` | `PIDS_CMD` | `str` |
| `vm_size` | `PIDS_MEM_VIRT` | `ul_int` |
| `vm_rss` | `PIDS_MEM_RES` | `ul_int` |
| `utime` | `PIDS_TICS_USER` | `ull_int` |
| `stime` | `PIDS_TICS_SYSTEM` | `ull_int` |

（完整映射表请参阅 MIGRATION_GUIDE_proc_t_to_pids.md）

## 迁移步骤

1. **查看文档**: 先阅读 `README_zh.md` 了解概况
2. **映射字段**: 使用 `MIGRATION_GUIDE_proc_t_to_pids.md` 中的映射表
3. **参考示例**: 学习 `migration_example.c` 中的实际代码
4. **逐步迁移**: 从简单的功能开始，逐步重构
5. **测试验证**: 充分测试迁移后的代码

## 示例编译和运行

### 编译示例（新 API）
```bash
cd doc
gcc -o migration_example_new migration_example.c \
    -I../library/include \
    -L../library/.libs \
    -lproc2 \
    -Wl,-rpath,../library/.libs
```

### 运行示例
```bash
./migration_example_new
```

示例会展示：
1. 列出所有进程
2. 显示内存使用
3. 按 PID 过滤
4. CPU 时间
5. 线程处理

## 关键要点

1. **新 API 更高效**: 只获取需要的数据
2. **显式声明**: 必须明确声明需要哪些字段
3. **不同的访问方式**: 使用 `PIDS_VAL()` 宏而不是直接访问字段
4. **类型安全**: 需要指定正确的类型（s_int, ul_int, str 等）
5. **库名变化**: 链接时使用 `-lproc2` 而不是 `-lproc`

## 优势

新 API 带来的好处：
- ✅ 内存使用更少
- ✅ 性能更好
- ✅ 更容易维护
- ✅ 更好的错误处理
- ✅ 内置排序功能

## 获取帮助

如果在迁移过程中遇到问题：

1. 查看详细的迁移指南
2. 参考快速参考文档
3. 研究示例代码
4. 查看 `src/ps/` 和 `src/top/` 的实际应用

## 总结

commit 77dc22b 引入的新 API 虽然需要一些代码修改，但带来了显著的性能和可维护性提升。使用本文档提供的映射表和示例，您应该能够成功地将项目迁移到新 API。

建议从小的、简单的功能开始迁移，逐步扩展到更复杂的部分。新 API 的设计使得编写正确、高效的进程监控工具变得更容易。

---

**文档位置**: `/doc/` 目录
**示例代码**: `/doc/migration_example.c`
**测试状态**: ✅ 已编译并测试通过
