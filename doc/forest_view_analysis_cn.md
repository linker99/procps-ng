# Forest View（树形视图）函数详解

## 概述

Forest View 是 `top` 工具中的树形进程显示模式，通过 'V' 键切换。它以父子关系的树状结构显示进程，使用 ASCII 字符绘制树形分支，并支持折叠/展开子进程。

本文档详细分析三个核心函数：
- **forest_begin**: 构建树形结构
- **forest_config**: 配置焦点进程范围
- **forest_display**: 添加 ASCII 树形装饰

## 相关数据结构

### 全局变量

```c
static struct pids_stack **Seed_ppt;   // 临时窗口 ppt 指针（输入）
static struct pids_stack **Tree_ppt;   // 树形输出数组（forest_begin 重新分配）
static int Tree_idx;                   // 树形索引（frame_make 重置为 0）

static int *Hide_pid;                  // 可折叠进程数组
static int  Hide_tot;                  // 上述数组中使用的总数
```

### 额外的 PIDS_extra 字段

Forest View 使用三个额外的结果字段：

```c
eu_TREE_HID (s_ch):   // 隐藏状态
                      // 'x' = 已折叠（父进程标记）
                      // 'z' = 不可见（被折叠的子进程）
                      // 其他 = 正常显示

eu_TREE_LVL (s_int):  // 树形层级 (0-100)
                      // 0 = 根进程
                      // 1 = 第一级子进程
                      // 2 = 第二级子进程
                      // 等等...

eu_TREE_ADD (u_int):  // 累积子进程 CPU 时间
                      // 用于将被折叠的子进程 CPU 时间
                      // 累加到父进程上显示
```

### Hide_pid 数组说明

```c
Hide_pid[i] > 0:  // 正数 PID = 该进程的子进程已被折叠
Hide_pid[i] < 0:  // 负数 PID = 该进程的子进程已被展开
Hide_pid[i] = 0:  // 未使用的槽位
```

## 函数 1: forest_begin - 构建树形结构

### 函数签名

```c
static void forest_begin (WIN_t *q)
```

**位置**: `src/top/top.c:5053-5125`

### 功能描述

`forest_begin` 是树形视图的核心构建函数，负责：
1. 将平面的进程列表重组为树形结构
2. 计算每个进程的层级深度
3. 处理折叠/展开状态
4. 累计被折叠子进程的 CPU 时间

### 详细实现分析

#### 阶段 1: 初始化和内存管理

```c
static void forest_begin (WIN_t *q) {
   static int hwmsav;
   int i, j;

   Seed_ppt = q->ppt;                          // 保存原始指针数组
   
   if (!Tree_idx) {                            // 每帧只执行一次
      if (hwmsav < PIDSmaxt) {                 // 按需增长，从不收缩
         hwmsav = PIDSmaxt;
         Tree_ppt = alloc_r(Tree_ppt, sizeof(void *) * hwmsav);
      }
```

**关键点**:
- `Seed_ppt`: 指向窗口的原始进程指针数组（输入）
- `Tree_ppt`: 新分配的树形输出数组
- `hwmsav`: 高水位标记，跟踪最大进程数
- `Tree_idx`: 全局计数器，记录已处理的进程数
- **内存策略**: 只增长不收缩（避免频繁 realloc）

#### 阶段 2: 排序（可选）

```c
#ifndef TREE_SCANALL
      if (!(procps_pids_sort(Pids_ctx, Seed_ppt, PIDSmaxt
         , PIDS_TICS_BEGAN, PIDS_SORT_ASCEND)))
            error_exit(fmtmk(N_fmt(LIB_errorpid_fmt), __LINE__, strerror(errno)));
#endif
```

**目的**: 
- 按进程启动时间升序排序
- 确保父进程在子进程之前出现
- 优化 `forest_adds` 的搜索效率（只需向前扫描）

#### 阶段 3: 递归构建树形结构

```c
      for (i = 0; i < PIDSmaxt; i++) {         // 避免 hidepid 扭曲
         if (!PID_VAL(eu_TREE_LVL, s_int, Seed_ppt[i])) // 父进程层级为 0
            forest_adds(i, 0);                 // 添加父进程及其子进程
      }
```

**逻辑**:
1. 遍历所有进程
2. 找到根进程（`eu_TREE_LVL == 0`）
3. 调用 `forest_adds()` 递归添加该进程及其所有子孙

**forest_adds 递归函数**:

```c
static void forest_adds (const int self, int level) {
  #define rSv(E,X) PID_VAL(E, s_int, Seed_ppt[X])
  #define rSv_Lvl  Tree_ppt[Tree_idx]->head[eu_TREE_LVL].result.s_int
   int i;

   if (Tree_idx < PIDSmaxt) {               // 防止越界
      if (level > 100) level = 101;         // 嵌套深度限制
      Tree_ppt[Tree_idx] = Seed_ppt[self];  // 添加当前进程
      rSv_Lvl = level;                      // 记录层级
      ++Tree_idx;
      
      // 查找所有子进程
      for (i = self + 1; i < PIDSmaxt; i++) {
         // 子进程条件：
         // 1. TID 的 TGID == 父进程 PID（线程）
         // 2. PID 的 PPID == 父进程 PID 且 PID == TGID（子进程）
         if (rSv(EU_PID, self) == rSv(EU_TGD, i)
         || (rSv(EU_PID, self) == rSv(EU_PPD, i) && rSv(EU_PID, i) == rSv(EU_TGD, i)))
            forest_adds(i, level + 1);      // 递归添加子进程
      }
   }
}
```

**递归逻辑示例**:

假设有以下进程（已按启动时间排序）：

```
Index  PID   PPID  TGID
0      1     0     1     (init)
1      100   1     100   (systemd)
2      200   100   200   (子进程1)
3      201   100   201   (子进程2)
4      300   200   300   (孙进程)
```

执行流程：

```
1. i=0: PID=1, PPID=0, level=0 → 不是子进程
   调用 forest_adds(0, 0)
   → Tree_ppt[0] = Seed_ppt[0], level=0, Tree_idx=1
   → 查找子进程: PID 1 的子进程
   → 找到 i=1 (PPID=1)
   → 递归调用 forest_adds(1, 1)
      → Tree_ppt[1] = Seed_ppt[1], level=1, Tree_idx=2
      → 查找子进程: PID 100 的子进程
      → 找到 i=2 (PPID=100)
      → 递归调用 forest_adds(2, 2)
         → Tree_ppt[2] = Seed_ppt[2], level=2, Tree_idx=3
         → 查找子进程: PID 200 的子进程
         → 找到 i=4 (PPID=200)
         → 递归调用 forest_adds(4, 3)
            → Tree_ppt[4] = Seed_ppt[4], level=3, Tree_idx=4
      → 找到 i=3 (PPID=100)
      → 递归调用 forest_adds(3, 2)
         → Tree_ppt[3] = Seed_ppt[3], level=2, Tree_idx=5

结果 Tree_ppt 顺序:
Tree_ppt[0]: PID=1,   level=0
Tree_ppt[1]: PID=100, level=1
Tree_ppt[2]: PID=200, level=2
Tree_ppt[3]: PID=300, level=3  (注意：孙进程在子进程2之前)
Tree_ppt[4]: PID=201, level=2
```

#### 阶段 4: 处理折叠/展开

```c
      for (i = 0; i < Hide_tot; i++) {
       #define rSv(E,T,X)  Tree_ppt[X]->head[E].result.T
       #define rSv_Pid(X)  rSv(EU_PID, s_int, X)
       #define rSv_Lvl(X)  rSv(eu_TREE_LVL, s_int, X)
       #define rSv_Hid(X)  rSv(eu_TREE_HID, s_ch, X)
       #define rSv_Add(X)  rSv(eu_TREE_ADD, u_int, X)
       #define rSv_Cpu(X)  rSv(EU_CPU, u_int, X)

         if (Hide_pid[i] > 0) {              // 正数 = 折叠
            for (j = 0; j < PIDSmaxt; j++) {
               if (rSv_Pid(j) == Hide_pid[i]) {
                  int parent = j;
                  int children = 0;
                  int level = rSv_Lvl(parent);
                  
                  // 标记所有子进程为不可见
                  while (j+1 < PIDSmaxt && rSv_Lvl(j+1) > level) {
                     ++j;
                     rSv_Hid(j) = 'z';        // 'z' = 不可见
#ifndef TREE_VCPUOFF
                     rSv_Add(parent) += rSv_Cpu(j);  // 累加 CPU
#endif
                     children = 1;
                  }
                  
                  // 标记父进程为已折叠
                  if (children) rSv_Hid(parent) = 'x';  // 'x' = 已折叠
                  break;
               }
            }
         }
```

**折叠逻辑**:

假设 Tree_ppt 中有：
```
Index  PID   Level  CPU
0      100   1      5
1      200   2      3
2      300   3      2
3      201   2      4
```

如果用户折叠 PID=100（`Hide_pid[0] = 100`）：

```
执行前:
  Tree_ppt[0]: PID=100, level=1, hid=' ', cpu=5
  Tree_ppt[1]: PID=200, level=2, hid=' ', cpu=3
  Tree_ppt[2]: PID=300, level=3, hid=' ', cpu=2
  Tree_ppt[3]: PID=201, level=2, hid=' ', cpu=4

执行后:
  Tree_ppt[0]: PID=100, level=1, hid='x', cpu=5, add=9 (3+2+4)
  Tree_ppt[1]: PID=200, level=2, hid='z', cpu=3
  Tree_ppt[2]: PID=300, level=3, hid='z', cpu=2
  Tree_ppt[3]: PID=201, level=2, hid='z', cpu=4

显示效果:
  100  (5+9=14% CPU)  +`- command
  (200, 300, 201 被隐藏)
```

#### 阶段 5: 替换窗口数组

```c
       #undef rSv
       // ...其他 undef...
      }
   }
   q->ppt = Tree_ppt;                          // 替换！
   memcpy(Seed_ppt, Tree_ppt, sizeof(void *) * PIDSmaxt);
}
```

**关键点**:
- `q->ppt = Tree_ppt`: 窗口现在使用树形排序的数组
- `memcpy(Seed_ppt, Tree_ppt, ...)`: 复制回原数组（保持一致）

### 完整流程图

```
输入: q->ppt (平面进程列表)
  ↓
Seed_ppt = q->ppt
  ↓
分配/扩展 Tree_ppt 数组
  ↓
排序 Seed_ppt (按启动时间)
  ↓
for 每个根进程 (level=0):
  forest_adds(进程, 0)
    递归添加到 Tree_ppt
    设置每个进程的 level
  ↓
for 每个 Hide_pid:
  if 折叠 (>0):
    标记父进程 hid='x'
    标记所有子进程 hid='z'
    累加子进程 CPU 到父进程
  if 展开 (<0):
    清除标记
  ↓
q->ppt = Tree_ppt (替换数组)
  ↓
输出: q->ppt (树形进程列表)
```

## 函数 2: forest_config - 配置焦点范围

### 函数签名

```c
static void forest_config (WIN_t *q)
```

**位置**: `src/top/top.c:5132-5169`

### 功能描述

当用户使用 'F' 键聚焦某个进程时，`forest_config` 确定该进程及其子进程在数组中的范围（`focus_beg` 和 `focus_end`），以便滚动和显示操作只影响这个子树。

### 详细实现

```c
static void forest_config (WIN_t *q) {
  #define rSv(x) PID_VAL(eu_TREE_LVL, s_int, q->ppt[(x)])
   int i, level = 0;

   // 查找焦点 PID 在数组中的位置
   for (i = 0; i < PIDSmaxt; i++) {
      if (q->focus_pid == PID_VAL(EU_PID, s_int, q->ppt[i])) {
         level = rSv(i);               // 获取该进程的层级
         q->focus_beg = i;             // 焦点开始位置
         break;
      }
   }
   
   // 如果找不到，关闭焦点模式
   if (i == PIDSmaxt)
      q->focus_pid = q->begtask = 0;
   else {
#ifdef FOCUS_TREE_X
      q->focus_lvl = rSv(i);           // 保存层级（用于缩进调整）
#endif
      // 查找焦点进程的最后一个子孙
      while (i+1 < PIDSmaxt && rSv(i+1) > level)
         ++i;
      q->focus_end = i + 1;            // 设置结束位置（栅栏柱）
      
      // 确保滚动位置不在焦点之上
      if (q->begtask < q->focus_beg) {
         q->begtask = q->focus_beg;
         mkVIZoff(q)                   // 标记需要重绘
      }
      
#ifdef FOCUS_HARD_Y
      // 如果上方有进程结束，尝试保持焦点
      // （但在有很多子进程时允许滚动）
      if (q->begtask > q->focus_beg
      && (SCREEN_ROWS > (q->focus_end - q->focus_beg))) {
         q->begtask = q->focus_beg;
         mkVIZoff(q)
      }
#endif
   }
  #undef rSv
}
```

### 工作原理

假设树形结构如下：

```
Index  PID   Level
0      1     0
1      100   1
2      200   2     ← 用户聚焦此进程 (focus_pid=200)
3      300   3
4      301   3
5      201   2
6      101   1
```

执行 `forest_config` 后：

```c
// 查找 PID=200
i=2: PID=200 匹配
level = rSv(2) = 2
q->focus_beg = 2

// 查找子进程范围
i=2: level=2
i=3: rSv(3)=3 > 2 → 继续
i=4: rSv(4)=3 > 2 → 继续
i=5: rSv(5)=2 = 2 → 停止

q->focus_end = 5  (i+1)

结果:
  focus_beg = 2   (PID=200)
  focus_end = 5   (栅栏柱，不包含)
  
焦点范围包含:
  Index 2: PID=200 (level=2)
  Index 3: PID=300 (level=3) - 子进程
  Index 4: PID=301 (level=3) - 子进程
```

### 滚动调整逻辑

```c
if (q->begtask < q->focus_beg) {
   q->begtask = q->focus_beg;  // 强制从焦点开始显示
}
```

**示例场景**:

```
屏幕可见行数: 10
begtask = 0 (从第一个进程开始显示)
focus_beg = 5
focus_end = 10

调整前: 显示 Index 0-9
调整后: 显示 Index 5-14 (从焦点开始)
```

## 函数 3: forest_display - 添加树形装饰

### 函数签名

```c
static inline const char *forest_display (const WIN_t *q, int idx)
```

**位置**: `src/top/top.c:5175-5221`

### 功能描述

`forest_display` 为命令字符串添加 ASCII 树形分支字符（如 `` `- ``），根据进程的层级深度添加适当的缩进和装饰。

### 详细实现

```c
static inline const char *forest_display (const WIN_t *q, int idx) {
  #define rSv(E)   PID_VAL(E, str, p)
  #define rSv_Lvl  PID_VAL(eu_TREE_LVL, s_int, p)
  #define rSv_Hid  PID_VAL(eu_TREE_HID, s_ch, p)
  
  static char buf[MAXBUFSIZ];
  struct pids_stack *p = q->ppt[idx];
  
  // 获取命令字符串
  const char *which = (CHKw(q, Show_CMDLIN)) 
                      ? rSv(eu_CMDLINE)   // 完整命令行
                      : rSv(EU_CMD);      // 只有命令名
  int level = rSv_Lvl;

#ifdef FOCUS_TREE_X
  // 如果有焦点，调整层级（相对缩进）
  if (q->focus_pid) {
     if (idx >= q->focus_beg && idx < q->focus_end)
        level -= q->focus_lvl;
  }
#endif

  // 如果不是树形模式或是根进程，直接返回
  if (!CHKw(q, Show_FOREST) || level == 0) return which;
  
#ifndef TREE_VWINALL
  if (q == Curwin)            // 只在当前窗口显示树形
#endif
  
  // 处理已折叠的父进程
  if (rSv_Hid == 'x') {
#ifdef TREE_VALTMRK
     snprintf(buf, sizeof(buf), "%*s%s", (4 * level), "`+ ", which);
#else
     snprintf(buf, sizeof(buf), "+%*s%s", ((4 * level) - 1), "`- ", which);
#endif
     return buf;
  }
  
  // 处理嵌套过深的进程
  if (level > 100) {
     snprintf(buf, sizeof(buf), "%400s%s", " +  ", which);
     return buf;
  }
  
  // 正常进程：添加缩进和分支字符
#ifndef FOCUS_VIZOFF
  if (q->focus_pid) 
     snprintf(buf, sizeof(buf), "|%*s%s", ((4 * level) - 1), "`- ", which);
  else 
     snprintf(buf, sizeof(buf), "%*s%s", (4 * level), " `- ", which);
#else
  snprintf(buf, sizeof(buf), "%*s%s", (4 * level), " `- ", which);
#endif
  return buf;
}
```

### ASCII 艺术字符说明

```
 `- : 树枝（正常子进程）
 `+ : 已折叠的分支（有隐藏的子进程）
 |  : 焦点指示器（可选，FOCUS_VIZOFF 未定义时）
```

### 显示示例

**未折叠的树形视图**:

```
Level 0:  systemd
Level 1:   `- bash
Level 2:       `- top
Level 2:       `- vim
Level 1:   `- sshd
Level 2:       `- sshd
Level 3:           `- bash
```

**折叠后** (bash 的子进程被折叠):

```
Level 0:  systemd
Level 1:   `+ bash        (显示为 +`- 或 `+)
Level 1:   `- sshd
Level 2:       `- sshd
Level 3:           `- bash
```

### 缩进计算

```c
缩进空格数 = level * 4

Level 0: 0 空格
Level 1: 4 空格 + " `- "
Level 2: 8 空格 + " `- "
Level 3: 12 空格 + " `- "
```

**实际输出**:

```c
level=0: "systemd"
level=1: "    `- bash"           (4 空格)
level=2: "        `- top"        (8 空格)
level=3: "            `- vim"    (12 空格)
```

### 焦点模式下的相对缩进

当焦点在 level=2 的进程时：

```c
// 假设 focus_lvl = 2
level=2: level -= 2 → 0 → "top"          (无缩进)
level=3: level -= 2 → 1 → "    `- vim"   (4 空格)
level=4: level -= 2 → 2 → "        `- ..." (8 空格)
```

**效果**: 焦点进程显示为根，其子进程相对缩进。

## 调用时序

### 主流程

```c
window_show(WIN_t *q)
  ↓
if (CHKw(q, Show_FOREST)) {
  forest_begin(q);           // 构建树形结构
    ↓
  if (q->focus_pid) 
    forest_config(q);        // 配置焦点范围
}
  ↓
for (i = q->begtask; i < PIDSmaxt; i++) {
  task_show(q, i);
    ↓
    // 显示每个字段
    case EU_CMD:
      forest_display(q, i);  // 添加树形装饰
      ↓
      输出到屏幕
}
```

### 完整示例

假设有以下进程：

```
PID   PPID  CMD
1     0     systemd
100   1     bash
200   100   top
201   100   vim
```

**步骤 1**: `forest_begin(q)` 执行

```
输入 Seed_ppt:
  [0]: PID=1,   PPID=0
  [1]: PID=100, PPID=1
  [2]: PID=200, PPID=100
  [3]: PID=201, PPID=100

执行 forest_adds(0, 0):
  Tree_ppt[0] = PID=1, level=0
  → 找到子进程 PID=100
  → forest_adds(1, 1):
      Tree_ppt[1] = PID=100, level=1
      → 找到子进程 PID=200
      → forest_adds(2, 2):
          Tree_ppt[2] = PID=200, level=2
      → 找到子进程 PID=201
      → forest_adds(3, 2):
          Tree_ppt[3] = PID=201, level=2

输出 Tree_ppt:
  [0]: PID=1,   level=0
  [1]: PID=100, level=1
  [2]: PID=200, level=2
  [3]: PID=201, level=2
```

**步骤 2**: 用户按 'F' 聚焦 PID=100

```c
forest_config(q):
  查找 PID=100 → index=1
  focus_beg = 1
  level = 1
  
  查找子进程范围:
    index=2: level=2 > 1 → 继续
    index=3: level=2 > 1 → 继续
    index=4: 越界
  
  focus_end = 4
```

**步骤 3**: 显示进程

```c
for i=0 to 3:
  i=0: forest_display(q, 0)
       level=0 → return "systemd"
       
  i=1: forest_display(q, 1)
       level=1
       焦点调整: level -= 1 → 0
       return "bash"
       
  i=2: forest_display(q, 2)
       level=2
       焦点调整: level -= 1 → 1
       return "    `- top"
       
  i=3: forest_display(q, 3)
       level=2
       焦点调整: level -= 1 → 1
       return "    `- vim"
```

**最终屏幕输出**:

```
systemd
bash              ← 焦点进程，无缩进
    `- top        ← 相对缩进
    `- vim        ← 相对缩进
```

## 性能优化

### 1. 静态缓冲区

```c
static char buf[MAXBUFSIZ];  // forest_display 中
```

**优点**: 避免每次调用时分配/释放内存

### 2. 内联函数

```c
static inline const char *forest_display(...)
```

**优点**: 编译器可以将函数调用优化为内联代码

### 3. 按需排序

```c
#ifndef TREE_SCANALL
  procps_pids_sort(..., PIDS_TICS_BEGAN, PIDS_SORT_ASCEND);
  // 只扫描当前进程之后的部分
  for (i = self + 1; i < PIDSmaxt; i++)
#else
  // 扫描整个数组
  for (i = 0; i < PIDSmaxt; i++)
#endif
```

**优点**: 默认情况下，排序后的数组允许更快的子进程搜索

### 4. 内存高水位标记

```c
static int hwmsav;
if (hwmsav < PIDSmaxt) {
   hwmsav = PIDSmaxt;
   Tree_ppt = alloc_r(Tree_ppt, sizeof(void *) * hwmsav);
}
```

**优点**: 只增长不收缩，避免频繁 realloc

## 常见问题

### Q1: 为什么要排序？

**答**: 排序确保父进程在子进程之前，这样：
1. 递归搜索可以只向前扫描（性能优化）
2. 树形结构自然正确（父→子顺序）

### Q2: level > 100 时为什么特殊处理？

**答**: 防止无限递归或极深嵌套导致的问题：
- 恶意或异常的进程树
- 防止缓冲区溢出
- 提供视觉提示（400 个空格）

### Q3: 折叠后 CPU 累加的作用是什么？

**答**: 让用户看到父进程及其所有子进程的总 CPU 使用率：
```
未折叠:
  bash     5%
    top    10%
    vim    8%

折叠后:
  bash    23%  (5% + 10% + 8%)
    (top 和 vim 被隐藏)
```

### Q4: 为什么使用 'x' 和 'z' 标记？

**答**: 区分两种隐藏状态：
- 'x': 父进程，用户明确折叠（显示 `+）
- 'z': 子进程，因父进程折叠而被隐藏（完全不显示）

## 总结

这三个函数构成了 top 的树形视图核心：

1. **forest_begin**: 数据重组引擎
   - 递归构建树形结构
   - 计算层级深度
   - 处理折叠/展开
   - O(n²) 时间复杂度（n = 进程数）

2. **forest_config**: 焦点管理器
   - 确定焦点进程范围
   - 调整滚动位置
   - O(n) 时间复杂度

3. **forest_display**: 视觉装饰器
   - 添加 ASCII 树形字符
   - 计算缩进
   - O(1) 时间复杂度（每次调用）

设计亮点：
- **递归算法**: 优雅处理任意深度的进程树
- **内存复用**: 静态缓冲区和高水位标记
- **灵活配置**: 多个编译时选项（TREE_SCANALL, FOCUS_TREE_X 等）
- **用户友好**: 折叠/展开、焦点、相对缩进等功能

这些函数的实现展示了如何在有限的内存和 CPU 资源下，提供丰富的交互式进程树视图功能。
