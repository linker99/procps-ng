# Forest View Functions - Detailed Analysis

## Overview

Forest View is a tree-structured process display mode in the `top` utility, toggled with the 'V' key. It displays processes in a parent-child hierarchical tree structure using ASCII characters to draw tree branches, and supports collapsing/expanding child processes.

This document provides detailed analysis of three core functions:
- **forest_begin**: Builds the tree structure
- **forest_config**: Configures focus process range  
- **forest_display**: Adds ASCII tree decoration

## Related Data Structures

### Global Variables

```c
static struct pids_stack **Seed_ppt;   // Temporary window ppt pointer (input)
static struct pids_stack **Tree_ppt;   // Tree output array (resized by forest_begin)
static int Tree_idx;                   // Tree index (reset to 0 by frame_make)

static int *Hide_pid;                  // Collapsible process array
static int  Hide_tot;                  // Total used in above array
```

### Additional PIDS_extra Fields

Forest View uses three additional result fields:

```c
eu_TREE_HID (s_ch):   // Hidden state
                      // 'x' = collapsed (parent process marker)
                      // 'z' = unseen (collapsed child process)
                      // other = normal display

eu_TREE_LVL (s_int):  // Tree level (0-100)
                      // 0 = root process
                      // 1 = first-level child
                      // 2 = second-level child
                      // etc...

eu_TREE_ADD (u_int):  // Accumulated child CPU time
                      // Used to accumulate CPU time from
                      // collapsed children to parent for display
```

### Hide_pid Array Description

```c
Hide_pid[i] > 0:  // Positive PID = process's children are collapsed
Hide_pid[i] < 0:  // Negative PID = process's children are expanded
Hide_pid[i] = 0:  // Unused slot
```

## Function 1: forest_begin - Build Tree Structure

### Function Signature

```c
static void forest_begin (WIN_t *q)
```

**Location**: `src/top/top.c:5053-5125`

### Function Description

`forest_begin` is the core tree view builder, responsible for:
1. Reorganizing flat process list into tree structure
2. Calculating depth level for each process
3. Handling collapse/expand state
4. Accumulating CPU time from collapsed children

### Detailed Implementation Analysis

#### Phase 1: Initialization and Memory Management

```c
static void forest_begin (WIN_t *q) {
   static int hwmsav;
   int i, j;

   Seed_ppt = q->ppt;                          // Save original pointer array
   
   if (!Tree_idx) {                            // Execute once per frame
      if (hwmsav < PIDSmaxt) {                 // Grow on demand, never shrink
         hwmsav = PIDSmaxt;
         Tree_ppt = alloc_r(Tree_ppt, sizeof(void *) * hwmsav);
      }
```

**Key Points**:
- `Seed_ppt`: Points to window's original process pointer array (input)
- `Tree_ppt`: Newly allocated tree output array
- `hwmsav`: High water mark, tracks maximum process count
- `Tree_idx`: Global counter, records processed process count
- **Memory Strategy**: Only grows, never shrinks (avoids frequent realloc)

#### Phase 2: Sorting (Optional)

```c
#ifndef TREE_SCANALL
      if (!(procps_pids_sort(Pids_ctx, Seed_ppt, PIDSmaxt
         , PIDS_TICS_BEGAN, PIDS_SORT_ASCEND)))
            error_exit(fmtmk(N_fmt(LIB_errorpid_fmt), __LINE__, strerror(errno)));
#endif
```

**Purpose**: 
- Sort by process start time in ascending order
- Ensures parent processes appear before children
- Optimizes `forest_adds` search efficiency (only forward scan needed)

#### Phase 3: Recursively Build Tree Structure

```c
      for (i = 0; i < PIDSmaxt; i++) {         // Avoid hidepid distortions
         if (!PID_VAL(eu_TREE_LVL, s_int, Seed_ppt[i])) // Parents at level 0
            forest_adds(i, 0);                 // Add parent + children
      }
```

**Logic**:
1. Iterate through all processes
2. Find root processes (`eu_TREE_LVL == 0`)
3. Call `forest_adds()` recursively to add process and all descendants

**forest_adds Recursive Function**:

```c
static void forest_adds (const int self, int level) {
  #define rSv(E,X) PID_VAL(E, s_int, Seed_ppt[X])
  #define rSv_Lvl  Tree_ppt[Tree_idx]->head[eu_TREE_LVL].result.s_int
   int i;

   if (Tree_idx < PIDSmaxt) {               // Prevent overflow
      if (level > 100) level = 101;         // Nesting depth limit
      Tree_ppt[Tree_idx] = Seed_ppt[self];  // Add current process
      rSv_Lvl = level;                      // Record level
      ++Tree_idx;
      
      // Find all child processes
      for (i = self + 1; i < PIDSmaxt; i++) {
         // Child process conditions:
         // 1. TID's TGID == parent PID (thread)
         // 2. PID's PPID == parent PID and PID == TGID (child process)
         if (rSv(EU_PID, self) == rSv(EU_TGD, i)
         || (rSv(EU_PID, self) == rSv(EU_PPD, i) && rSv(EU_PID, i) == rSv(EU_TGD, i)))
            forest_adds(i, level + 1);      // Recursively add child
      }
   }
}
```

**Recursion Logic Example**:

Assume the following processes (sorted by start time):

```
Index  PID   PPID  TGID
0      1     0     1     (init)
1      100   1     100   (systemd)
2      200   100   200   (child1)
3      201   100   201   (child2)
4      300   200   300   (grandchild)
```

Execution flow:

```
1. i=0: PID=1, PPID=0, level=0 → not a child
   Call forest_adds(0, 0)
   → Tree_ppt[0] = Seed_ppt[0], level=0, Tree_idx=1
   → Find children: Children of PID 1
   → Found i=1 (PPID=1)
   → Recursive call forest_adds(1, 1)
      → Tree_ppt[1] = Seed_ppt[1], level=1, Tree_idx=2
      → Find children: Children of PID 100
      → Found i=2 (PPID=100)
      → Recursive call forest_adds(2, 2)
         → Tree_ppt[2] = Seed_ppt[2], level=2, Tree_idx=3
         → Find children: Children of PID 200
         → Found i=4 (PPID=200)
         → Recursive call forest_adds(4, 3)
            → Tree_ppt[4] = Seed_ppt[4], level=3, Tree_idx=4
      → Found i=3 (PPID=100)
      → Recursive call forest_adds(3, 2)
         → Tree_ppt[3] = Seed_ppt[3], level=2, Tree_idx=5

Result Tree_ppt order:
Tree_ppt[0]: PID=1,   level=0
Tree_ppt[1]: PID=100, level=1
Tree_ppt[2]: PID=200, level=2
Tree_ppt[3]: PID=300, level=3  (Note: grandchild before child2)
Tree_ppt[4]: PID=201, level=2
```

#### Phase 4: Handle Collapse/Expand

```c
      for (i = 0; i < Hide_tot; i++) {
       #define rSv(E,T,X)  Tree_ppt[X]->head[E].result.T
       #define rSv_Pid(X)  rSv(EU_PID, s_int, X)
       #define rSv_Lvl(X)  rSv(eu_TREE_LVL, s_int, X)
       #define rSv_Hid(X)  rSv(eu_TREE_HID, s_ch, X)
       #define rSv_Add(X)  rSv(eu_TREE_ADD, u_int, X)
       #define rSv_Cpu(X)  rSv(EU_CPU, u_int, X)

         if (Hide_pid[i] > 0) {              // Positive = collapsed
            for (j = 0; j < PIDSmaxt; j++) {
               if (rSv_Pid(j) == Hide_pid[i]) {
                  int parent = j;
                  int children = 0;
                  int level = rSv_Lvl(parent);
                  
                  // Mark all children as unseen
                  while (j+1 < PIDSmaxt && rSv_Lvl(j+1) > level) {
                     ++j;
                     rSv_Hid(j) = 'z';        // 'z' = unseen
#ifndef TREE_VCPUOFF
                     rSv_Add(parent) += rSv_Cpu(j);  // Accumulate CPU
#endif
                     children = 1;
                  }
                  
                  // Mark parent as collapsed
                  if (children) rSv_Hid(parent) = 'x';  // 'x' = collapsed
                  break;
               }
            }
         }
```

**Collapse Logic**:

Assume Tree_ppt contains:
```
Index  PID   Level  CPU
0      100   1      5
1      200   2      3
2      300   3      2
3      201   2      4
```

If user collapses PID=100 (`Hide_pid[0] = 100`):

```
Before:
  Tree_ppt[0]: PID=100, level=1, hid=' ', cpu=5
  Tree_ppt[1]: PID=200, level=2, hid=' ', cpu=3
  Tree_ppt[2]: PID=300, level=3, hid=' ', cpu=2
  Tree_ppt[3]: PID=201, level=2, hid=' ', cpu=4

After:
  Tree_ppt[0]: PID=100, level=1, hid='x', cpu=5, add=9 (3+2+4)
  Tree_ppt[1]: PID=200, level=2, hid='z', cpu=3
  Tree_ppt[2]: PID=300, level=3, hid='z', cpu=2
  Tree_ppt[3]: PID=201, level=2, hid='z', cpu=4

Display effect:
  100  (5+9=14% CPU)  +`- command
  (200, 300, 201 are hidden)
```

#### Phase 5: Replace Window Array

```c
       #undef rSv
       // ...other undefs...
      }
   }
   q->ppt = Tree_ppt;                          // Replace!
   memcpy(Seed_ppt, Tree_ppt, sizeof(void *) * PIDSmaxt);
}
```

**Key Points**:
- `q->ppt = Tree_ppt`: Window now uses tree-sorted array
- `memcpy(Seed_ppt, Tree_ppt, ...)`: Copy back to original (keep consistent)

### Complete Flow Diagram

```
Input: q->ppt (flat process list)
  ↓
Seed_ppt = q->ppt
  ↓
Allocate/expand Tree_ppt array
  ↓
Sort Seed_ppt (by start time)
  ↓
for each root process (level=0):
  forest_adds(process, 0)
    Recursively add to Tree_ppt
    Set level for each process
  ↓
for each Hide_pid:
  if collapsed (>0):
    Mark parent hid='x'
    Mark all children hid='z'
    Accumulate child CPU to parent
  if expanded (<0):
    Clear marks
  ↓
q->ppt = Tree_ppt (replace array)
  ↓
Output: q->ppt (tree process list)
```

## Function 2: forest_config - Configure Focus Range

### Function Signature

```c
static void forest_config (WIN_t *q)
```

**Location**: `src/top/top.c:5132-5169`

### Function Description

When user focuses on a process with 'F' key, `forest_config` determines the range of that process and its children in the array (`focus_beg` and `focus_end`), so scrolling and display operations only affect this subtree.

### Detailed Implementation

```c
static void forest_config (WIN_t *q) {
  #define rSv(x) PID_VAL(eu_TREE_LVL, s_int, q->ppt[(x)])
   int i, level = 0;

   // Find focus PID position in array
   for (i = 0; i < PIDSmaxt; i++) {
      if (q->focus_pid == PID_VAL(EU_PID, s_int, q->ppt[i])) {
         level = rSv(i);               // Get process level
         q->focus_beg = i;             // Focus begin position
         break;
      }
   }
   
   // If not found, turn off focus mode
   if (i == PIDSmaxt)
      q->focus_pid = q->begtask = 0;
   else {
#ifdef FOCUS_TREE_X
      q->focus_lvl = rSv(i);           // Save level (for indent adjustment)
#endif
      // Find last descendant of focus process
      while (i+1 < PIDSmaxt && rSv(i+1) > level)
         ++i;
      q->focus_end = i + 1;            // Set end position (fencepost)
      
      // Ensure scroll position is not above focus
      if (q->begtask < q->focus_beg) {
         q->begtask = q->focus_beg;
         mkVIZoff(q)                   // Mark for redraw
      }
      
#ifdef FOCUS_HARD_Y
      // If processes above ended, try to maintain focus
      // (but allow scrolling when there are many children)
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

### How It Works

Assume tree structure:

```
Index  PID   Level
0      1     0
1      100   1
2      200   2     ← User focuses this (focus_pid=200)
3      300   3
4      301   3
5      201   2
6      101   1
```

After executing `forest_config`:

```c
// Find PID=200
i=2: PID=200 matches
level = rSv(2) = 2
q->focus_beg = 2

// Find child range
i=2: level=2
i=3: rSv(3)=3 > 2 → continue
i=4: rSv(4)=3 > 2 → continue
i=5: rSv(5)=2 = 2 → stop

q->focus_end = 5  (i+1)

Result:
  focus_beg = 2   (PID=200)
  focus_end = 5   (fencepost, not included)
  
Focus range includes:
  Index 2: PID=200 (level=2)
  Index 3: PID=300 (level=3) - child
  Index 4: PID=301 (level=3) - child
```

### Scroll Adjustment Logic

```c
if (q->begtask < q->focus_beg) {
   q->begtask = q->focus_beg;  // Force display from focus
}
```

**Example Scenario**:

```
Screen visible rows: 10
begtask = 0 (display from first process)
focus_beg = 5
focus_end = 10

Before adjustment: Display Index 0-9
After adjustment: Display Index 5-14 (from focus)
```

## Function 3: forest_display - Add Tree Decoration

### Function Signature

```c
static inline const char *forest_display (const WIN_t *q, int idx)
```

**Location**: `src/top/top.c:5175-5221`

### Function Description

`forest_display` adds ASCII tree branch characters (like `` `- ``) to command strings, with appropriate indentation and decoration based on process depth level.

### Detailed Implementation

```c
static inline const char *forest_display (const WIN_t *q, int idx) {
  #define rSv(E)   PID_VAL(E, str, p)
  #define rSv_Lvl  PID_VAL(eu_TREE_LVL, s_int, p)
  #define rSv_Hid  PID_VAL(eu_TREE_HID, s_ch, p)
  
  static char buf[MAXBUFSIZ];
  struct pids_stack *p = q->ppt[idx];
  
  // Get command string
  const char *which = (CHKw(q, Show_CMDLIN)) 
                      ? rSv(eu_CMDLINE)   // Full command line
                      : rSv(EU_CMD);      // Command name only
  int level = rSv_Lvl;

#ifdef FOCUS_TREE_X
  // If focused, adjust level (relative indent)
  if (q->focus_pid) {
     if (idx >= q->focus_beg && idx < q->focus_end)
        level -= q->focus_lvl;
  }
#endif

  // If not tree mode or root process, return directly
  if (!CHKw(q, Show_FOREST) || level == 0) return which;
  
#ifndef TREE_VWINALL
  if (q == Curwin)            // Only show tree in current window
#endif
  
  // Handle collapsed parent
  if (rSv_Hid == 'x') {
#ifdef TREE_VALTMRK
     snprintf(buf, sizeof(buf), "%*s%s", (4 * level), "`+ ", which);
#else
     snprintf(buf, sizeof(buf), "+%*s%s", ((4 * level) - 1), "`- ", which);
#endif
     return buf;
  }
  
  // Handle deeply nested processes
  if (level > 100) {
     snprintf(buf, sizeof(buf), "%400s%s", " +  ", which);
     return buf;
  }
  
  // Normal process: add indent and branch character
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

### ASCII Art Characters Explained

```
 `- : Tree branch (normal child process)
 `+ : Collapsed branch (has hidden children)
 |  : Focus indicator (optional, when FOCUS_VIZOFF not defined)
```

### Display Examples

**Uncollapsed tree view**:

```
Level 0:  systemd
Level 1:   `- bash
Level 2:       `- top
Level 2:       `- vim
Level 1:   `- sshd
Level 2:       `- sshd
Level 3:           `- bash
```

**After collapse** (bash's children collapsed):

```
Level 0:  systemd
Level 1:   `+ bash        (shows as +`- or `+)
Level 1:   `- sshd
Level 2:       `- sshd
Level 3:           `- bash
```

### Indent Calculation

```c
Indent spaces = level * 4

Level 0: 0 spaces
Level 1: 4 spaces + " `- "
Level 2: 8 spaces + " `- "
Level 3: 12 spaces + " `- "
```

**Actual output**:

```c
level=0: "systemd"
level=1: "    `- bash"           (4 spaces)
level=2: "        `- top"        (8 spaces)
level=3: "            `- vim"    (12 spaces)
```

### Relative Indent in Focus Mode

When focus is on level=2 process:

```c
// Assume focus_lvl = 2
level=2: level -= 2 → 0 → "top"          (no indent)
level=3: level -= 2 → 1 → "    `- vim"   (4 spaces)
level=4: level -= 2 → 2 → "        `- ..." (8 spaces)
```

**Effect**: Focus process displays as root, children indented relatively.

## Call Sequence

### Main Flow

```c
window_show(WIN_t *q)
  ↓
if (CHKw(q, Show_FOREST)) {
  forest_begin(q);           // Build tree structure
    ↓
  if (q->focus_pid) 
    forest_config(q);        // Configure focus range
}
  ↓
for (i = q->begtask; i < PIDSmaxt; i++) {
  task_show(q, i);
    ↓
    // Display each field
    case EU_CMD:
      forest_display(q, i);  // Add tree decoration
      ↓
      Output to screen
}
```

### Complete Example

Assume the following processes:

```
PID   PPID  CMD
1     0     systemd
100   1     bash
200   100   top
201   100   vim
```

**Step 1**: `forest_begin(q)` executes

```
Input Seed_ppt:
  [0]: PID=1,   PPID=0
  [1]: PID=100, PPID=1
  [2]: PID=200, PPID=100
  [3]: PID=201, PPID=100

Execute forest_adds(0, 0):
  Tree_ppt[0] = PID=1, level=0
  → Find child PID=100
  → forest_adds(1, 1):
      Tree_ppt[1] = PID=100, level=1
      → Find child PID=200
      → forest_adds(2, 2):
          Tree_ppt[2] = PID=200, level=2
      → Find child PID=201
      → forest_adds(3, 2):
          Tree_ppt[3] = PID=201, level=2

Output Tree_ppt:
  [0]: PID=1,   level=0
  [1]: PID=100, level=1
  [2]: PID=200, level=2
  [3]: PID=201, level=2
```

**Step 2**: User presses 'F' to focus PID=100

```c
forest_config(q):
  Find PID=100 → index=1
  focus_beg = 1
  level = 1
  
  Find child range:
    index=2: level=2 > 1 → continue
    index=3: level=2 > 1 → continue
    index=4: out of bounds
  
  focus_end = 4
```

**Step 3**: Display processes

```c
for i=0 to 3:
  i=0: forest_display(q, 0)
       level=0 → return "systemd"
       
  i=1: forest_display(q, 1)
       level=1
       Focus adjust: level -= 1 → 0
       return "bash"
       
  i=2: forest_display(q, 2)
       level=2
       Focus adjust: level -= 1 → 1
       return "    `- top"
       
  i=3: forest_display(q, 3)
       level=2
       Focus adjust: level -= 1 → 1
       return "    `- vim"
```

**Final screen output**:

```
systemd
bash              ← Focus process, no indent
    `- top        ← Relative indent
    `- vim        ← Relative indent
```

## Performance Optimizations

### 1. Static Buffer

```c
static char buf[MAXBUFSIZ];  // In forest_display
```

**Advantage**: Avoids allocating/freeing memory on each call

### 2. Inline Function

```c
static inline const char *forest_display(...)
```

**Advantage**: Compiler can optimize function call to inline code

### 3. On-Demand Sorting

```c
#ifndef TREE_SCANALL
  procps_pids_sort(..., PIDS_TICS_BEGAN, PIDS_SORT_ASCEND);
  // Only scan after current process
  for (i = self + 1; i < PIDSmaxt; i++)
#else
  // Scan entire array
  for (i = 0; i < PIDSmaxt; i++)
#endif
```

**Advantage**: By default, sorted array allows faster child search

### 4. Memory High Water Mark

```c
static int hwmsav;
if (hwmsav < PIDSmaxt) {
   hwmsav = PIDSmaxt;
   Tree_ppt = alloc_r(Tree_ppt, sizeof(void *) * hwmsav);
}
```

**Advantage**: Only grows, never shrinks, avoids frequent realloc

## FAQ

### Q1: Why sort?

**A**: Sorting ensures parents appear before children, allowing:
1. Recursive search to only scan forward (performance optimization)
2. Natural tree structure correctness (parent→child order)

### Q2: Why special handling for level > 100?

**A**: Prevents issues from infinite recursion or extremely deep nesting:
- Malicious or abnormal process trees
- Prevent buffer overflow
- Provide visual cue (400 spaces)

### Q3: What's the purpose of CPU accumulation after collapse?

**A**: Shows user total CPU usage of parent plus all children:
```
Uncollapsed:
  bash     5%
    top    10%
    vim    8%

Collapsed:
  bash    23%  (5% + 10% + 8%)
    (top and vim hidden)
```

### Q4: Why use 'x' and 'z' markers?

**A**: Distinguish two hidden states:
- 'x': Parent process, user explicitly collapsed (shows `+)
- 'z': Child process, hidden because parent collapsed (completely hidden)

## Summary

These three functions form the core of top's tree view:

1. **forest_begin**: Data reorganization engine
   - Recursively build tree structure
   - Calculate depth levels
   - Handle collapse/expand
   - O(n²) time complexity (n = number of processes)

2. **forest_config**: Focus manager
   - Determine focus process range
   - Adjust scroll position
   - O(n) time complexity

3. **forest_display**: Visual decorator
   - Add ASCII tree characters
   - Calculate indentation
   - O(1) time complexity (per call)

Design highlights:
- **Recursive algorithm**: Elegantly handles arbitrary depth process trees
- **Memory reuse**: Static buffers and high water marks
- **Flexible configuration**: Multiple compile-time options (TREE_SCANALL, FOCUS_TREE_X, etc.)
- **User-friendly**: Collapse/expand, focus, relative indent features

These function implementations demonstrate how to provide rich interactive process tree view functionality with limited memory and CPU resources.
