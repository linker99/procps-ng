# Quick Reference: proc_t to pids.h Migration

## Quick Comparison

### Initialization

| Old API | New API |
|---------|---------|
| `PROCTAB *pt = openproc(flags)` | `struct pids_info *info;`<br>`procps_pids_new(&info, items, numitems)` |

### Cleanup

| Old API | New API |
|---------|---------|
| `closeproc(pt)` | `procps_pids_unref(&info)` |

### Read All Processes

| Old API | New API |
|---------|---------|
| `while ((p = readproc(pt, p)))` | `fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY)`<br>then iterate `fetched->stacks[i]` |

### Read With Threads

| Old API | New API |
|---------|---------|
| `while ((p = readeither(pt, p)))` | `fetched = procps_pids_reap(info, PIDS_FETCH_THREADS_TOO)` |

### Filter by PID

| Old API | New API |
|---------|---------|
| `pid_t pids[] = {pid, 0};`<br>`openproc(flags \| PROC_PID, pids)` | `unsigned int pids[] = {pid};`<br>`procps_pids_select(info, pids, 1, PIDS_SELECT_PID)` |

### Filter by UID

| Old API | New API |
|---------|---------|
| `uid_t uids[] = {...};`<br>`openproc(flags \| PROC_UID, uids, n)` | `unsigned int uids[] = {...};`<br>`procps_pids_select(info, uids, n, PIDS_SELECT_UID)` |

## Common Field Mappings (Quick Lookup)

| proc_t field | pids.h enum | Access Type |
|--------------|-------------|-------------|
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
| `share` (pages) | `PIDS_MEM_SHR_PGS` | `ul_int` |
| N/A (KB) | `PIDS_MEM_SHR` | `ul_int` |
| `utime` | `PIDS_TICS_USER` | `ull_int` |
| `stime` | `PIDS_TICS_SYSTEM` | `ull_int` |
| `utime + stime` | `PIDS_TICS_ALL` | `ull_int` |
| `nice` | `PIDS_NICE` | `s_int` |
| `priority` | `PIDS_PRIORITY` | `s_int` |
| `processor` | `PIDS_PROCESSOR` | `s_int` |
| `nlwp` | `PIDS_NLWP` | `s_int` |

## Code Templates

### Template 1: Simple Process Listing

```c
struct pids_info *info = NULL;
struct pids_fetch *fetched;

enum pids_item items[] = {
    PIDS_ID_PID,
    PIDS_CMD,
    PIDS_STATE
};
int numitems = sizeof(items) / sizeof(items[0]);

if (procps_pids_new(&info, items, numitems) < 0) {
    // Handle error
    return;
}

fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
if (fetched) {
    for (int i = 0; i < fetched->counts->total; i++) {
        struct pids_stack *stack = fetched->stacks[i];
        int pid = PIDS_VAL(0, s_int, stack);
        char *cmd = PIDS_VAL(1, str, stack);
        char state = PIDS_VAL(2, s_ch, stack);
        
        // Use the values...
    }
}

procps_pids_unref(&info);
```

### Template 2: Filter by Specific PIDs

```c
struct pids_info *info = NULL;
struct pids_fetch *fetched;
unsigned int target_pids[] = {1234, 5678};
int num_pids = 2;

enum pids_item items[] = {
    PIDS_ID_PID,
    PIDS_CMD,
    PIDS_MEM_RES
};

procps_pids_new(&info, items, 3);

fetched = procps_pids_select(info, target_pids, num_pids, PIDS_SELECT_PID);
if (fetched) {
    for (int i = 0; i < fetched->counts->total; i++) {
        // Process each matched PID
        struct pids_stack *stack = fetched->stacks[i];
        // ... use stack ...
    }
}

procps_pids_unref(&info);
```

### Template 3: With Sorting

```c
struct pids_info *info = NULL;
struct pids_fetch *fetched;
struct pids_stack **sorted;

enum pids_item items[] = {
    PIDS_ID_PID,
    PIDS_MEM_RES,  // We'll sort by this
    PIDS_CMD
};

procps_pids_new(&info, items, 3);
fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);

if (fetched) {
    // Sort by memory (descending)
    sorted = procps_pids_sort(info, 
                              fetched->stacks,
                              fetched->counts->total,
                              PIDS_MEM_RES,
                              PIDS_SORT_DESCEND);
    
    // Use sorted array
    for (int i = 0; i < fetched->counts->total; i++) {
        struct pids_stack *stack = sorted[i];
        // ... use stack ...
    }
}

procps_pids_unref(&info);
```

### Template 4: Using Named Enums for Clarity

```c
// Define named indices for clarity
enum my_items {
    MY_PID = 0,
    MY_PPID,
    MY_USER,
    MY_CMD,
    MY_MEM,
    MY_STATE
};

// Map to actual items
enum pids_item items[] = {
    [MY_PID]   = PIDS_ID_PID,
    [MY_PPID]  = PIDS_ID_PPID,
    [MY_USER]  = PIDS_ID_EUSER,
    [MY_CMD]   = PIDS_CMD,
    [MY_MEM]   = PIDS_MEM_RES,
    [MY_STATE] = PIDS_STATE
};

struct pids_info *info = NULL;
procps_pids_new(&info, items, sizeof(items)/sizeof(items[0]));

struct pids_fetch *fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
if (fetched) {
    for (int i = 0; i < fetched->counts->total; i++) {
        struct pids_stack *stack = fetched->stacks[i];
        
        // Clear, self-documenting access
        int pid = PIDS_VAL(MY_PID, s_int, stack);
        int ppid = PIDS_VAL(MY_PPID, s_int, stack);
        char *user = PIDS_VAL(MY_USER, str, stack);
        char *cmd = PIDS_VAL(MY_CMD, str, stack);
        unsigned long mem = PIDS_VAL(MY_MEM, ul_int, stack);
        char state = PIDS_VAL(MY_STATE, s_ch, stack);
    }
}

procps_pids_unref(&info);
```

## Type Reference for PIDS_VAL

| Type Name | C Type | Used For |
|-----------|--------|----------|
| `s_ch` | `signed char` | Single character (like state: 'R', 'S', 'D') |
| `s_int` | `signed int` | Most integer IDs (PID, PPID, nice, etc.) |
| `u_int` | `unsigned int` | UIDs, GIDs |
| `ul_int` | `unsigned long` | Memory values, addresses |
| `ull_int` | `unsigned long long` | Time values, large counters |
| `str` | `char *` | Strings (command, user names) |
| `strv` | `char **` | String vectors (cmdline_v, environ_v) |
| `real` | `double` | Floating point (rarely used) |

## Migration Checklist

- [ ] Replace `#include <proc/readproc.h>` with `#include <proc/pids.h>`
- [ ] Replace `PROCTAB *pt` with `struct pids_info *info`
- [ ] Replace `proc_t *proc` with `struct pids_stack *stack`
- [ ] Define `enum pids_item items[]` array for needed fields
- [ ] Replace `openproc()` with `procps_pids_new(&info, items, numitems)`
- [ ] Replace `readproc()` loop with `procps_pids_reap()` + iteration
- [ ] Replace direct field access `proc->field` with `PIDS_VAL(index, type, stack)`
- [ ] Replace `closeproc()` with `procps_pids_unref(&info)`
- [ ] Update compilation flags from `-lproc` to `-lproc2`
- [ ] Test thoroughly with actual data

## Common Mistakes to Avoid

1. **Wrong type in PIDS_VAL**: Using `s_int` for memory (should be `ul_int`)
2. **Wrong index**: Forgetting items array order when accessing values
3. **Null checks**: Not checking if `fetched` or `fetched->stacks` is NULL
4. **Memory leaks**: Forgetting to call `procps_pids_unref(&info)`
5. **Freeing strings**: Don't free strings from PIDS_VAL - library owns them
6. **Array overflow**: Not checking `fetched->counts->total` before iteration
7. **Reusing old flags**: PROC_* flags don't exist in new API

## Performance Tips

1. Only request items you need - each item has a cost
2. Reuse `pids_info` context when making multiple queries
3. Use `procps_pids_reap()` to get all processes at once (more efficient than iteration)
4. Use built-in `procps_pids_sort()` instead of custom sorting
5. Consider `procps_pids_select()` when filtering by PID/UID

## Getting Help

- Full migration guide: `doc/MIGRATION_GUIDE_proc_t_to_pids.md`
- Working examples: `doc/migration_example.c`
- API reference: `library/include/pids.h`
- Real-world usage: `src/ps/`, `src/top/`

## Example Diff

```diff
- #include <proc/readproc.h>
+ #include <proc/pids.h>

- PROCTAB *pt;
- proc_t *proc = NULL;
+ struct pids_info *info = NULL;
+ struct pids_fetch *fetched;
+ enum pids_item items[] = {PIDS_ID_PID, PIDS_CMD};

- pt = openproc(PROC_FILLSTAT);
+ procps_pids_new(&info, items, 2);

- while ((proc = readproc(pt, proc)) != NULL) {
-     printf("%d %s\n", proc->tid, proc->cmd);
- }
+ fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
+ for (int i = 0; i < fetched->counts->total; i++) {
+     struct pids_stack *stack = fetched->stacks[i];
+     printf("%d %s\n", 
+            PIDS_VAL(0, s_int, stack),
+            PIDS_VAL(1, str, stack));
+ }

- closeproc(pt);
+ procps_pids_unref(&info);
```
