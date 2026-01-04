# Migration Guide: proc_t to pids.h API

## Overview

This guide helps developers migrate code from the old `proc_t` structure API (using `readproc.h`) to the new `pids.h` API introduced around commit 77dc22b (August 2015). This was a major API redesign that improved performance, flexibility, and maintainability.

## Table of Contents

1. [Key Differences](#key-differences)
2. [API Comparison](#api-comparison)
3. [Field Mapping](#field-mapping)
4. [Migration Examples](#migration-examples)
5. [Function Signature Changes](#function-signature-changes)
6. [Best Practices](#best-practices)

## Key Differences

### Old API (proc_t / readproc.h)
- **Structure-based**: Data stored in a large `proc_t` structure
- **Flag-based selection**: Use `PROC_FILL*` flags to select what data to read
- **Direct field access**: Access fields directly like `p->pid`, `p->cmdline`
- **Callback-based sorting**: Custom sort callback functions
- **Fixed memory layout**: All fields allocated whether needed or not

### New API (pids.h)
- **Result stack-based**: Data returned as stacks of result items
- **Item-based selection**: Explicitly request specific `PIDS_*` items
- **Macro-based access**: Use `PIDS_VAL()` macro to extract values
- **Built-in sorting**: Library provides sorting functionality
- **Efficient memory**: Only requested items are fetched

## API Comparison

### Initialization

#### Old API (proc_t)
```c
#include <proc/readproc.h>

PROCTAB *pt;
proc_t *proc;

// Open process table with flags
pt = openproc(PROC_FILLSTAT | PROC_FILLMEM | PROC_FILLSTATUS);

// Allocate proc_t (or pass NULL for auto-allocation)
proc = NULL;
```

#### New API (pids.h)
```c
#include <proc/pids.h>

struct pids_info *info = NULL;
enum pids_item items[] = {
    PIDS_ID_PID,
    PIDS_ID_PPID,
    PIDS_CMD,
    PIDS_MEM_RES,
    PIDS_TIME_ALL,
    PIDS_STATE
};
int numitems = sizeof(items) / sizeof(items[0]);

// Initialize the pids context
if (procps_pids_new(&info, items, numitems) < 0) {
    // Handle error
}
```

### Reading Processes

#### Old API (proc_t)
```c
// Read processes one by one
while ((proc = readproc(pt, proc)) != NULL) {
    printf("PID: %d, CMD: %s, RSS: %lu\n",
           proc->tid,
           proc->cmd,
           proc->vm_rss);
}

// Cleanup
closeproc(pt);
```

#### New API (pids.h)
```c
struct pids_fetch *fetched;

// Reap all processes at once
fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);

// Iterate through results
for (int i = 0; i < fetched->counts->total; i++) {
    struct pids_stack *stack = fetched->stacks[i];
    
    printf("PID: %d, CMD: %s, RSS: %lu\n",
           PIDS_VAL(0, s_int, stack),   // PIDS_ID_PID
           PIDS_VAL(2, str, stack),     // PIDS_CMD
           PIDS_VAL(3, ul_int, stack)); // PIDS_MEM_RES
}

// Cleanup
procps_pids_unref(&info);
```

### Reading with Threads

#### Old API (proc_t)
```c
// Use readeither() instead of readproc()
pt = openproc(PROC_FILLSTAT);

while ((proc = readeither(pt, proc)) != NULL) {
    printf("TID: %d, TGID: %d\n", proc->tid, proc->tgid);
}
```

#### New API (pids.h)
```c
// Use PIDS_FETCH_THREADS_TOO
fetched = procps_pids_reap(info, PIDS_FETCH_THREADS_TOO);

for (int i = 0; i < fetched->counts->total; i++) {
    struct pids_stack *stack = fetched->stacks[i];
    // Access thread data using PIDS_ID_TID and PIDS_ID_TGID
}
```

### Filtering by PID

#### Old API (proc_t)
```c
pid_t pids[] = {1234, 5678, 0};  // 0-terminated
pt = openproc(PROC_FILLSTAT | PROC_PID, pids);
```

#### New API (pids.h)
```c
unsigned int pids[] = {1234, 5678};
int numpids = 2;

fetched = procps_pids_select(info, pids, numpids, PIDS_SELECT_PID);
```

### Filtering by UID

#### Old API (proc_t)
```c
uid_t uids[] = {1000, 1001};
int nuids = 2;
pt = openproc(PROC_FILLSTAT | PROC_UID, uids, nuids);
```

#### New API (pids.h)
```c
unsigned int uids[] = {1000, 1001};
int numuids = 2;

fetched = procps_pids_select(info, uids, numuids, PIDS_SELECT_UID);
```

## Field Mapping

This table maps `proc_t` structure fields to their corresponding `PIDS_*` enumerators:

### Process Identification
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `tid` | `PIDS_ID_TID` | s_int | Task/Thread ID |
| `tgid` | `PIDS_ID_TGID` | s_int | Thread Group ID (process ID) |
| `ppid` | `PIDS_ID_PPID` | s_int | Parent Process ID |
| `pgrp` | `PIDS_ID_PGRP` | s_int | Process Group ID |
| `session` | `PIDS_ID_SESSION` | s_int | Session ID |
| `tpgid` | `PIDS_ID_TPGID` | s_int | Terminal Process Group ID |

### User/Group Information
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `euid` | `PIDS_ID_EUID` | u_int | Effective User ID |
| `egid` | `PIDS_ID_EGID` | u_int | Effective Group ID |
| `ruid` | `PIDS_ID_RUID` | u_int | Real User ID |
| `rgid` | `PIDS_ID_RGID` | u_int | Real Group ID |
| `suid` | `PIDS_ID_SUID` | u_int | Saved User ID |
| `sgid` | `PIDS_ID_SGID` | u_int | Saved Group ID |
| `fuid` | `PIDS_ID_FUID` | u_int | Filesystem User ID |
| `fgid` | `PIDS_ID_FGID` | u_int | Filesystem Group ID |
| `euser` | `PIDS_ID_EUSER` | str | Effective User Name |
| `egroup` | `PIDS_ID_EGROUP` | str | Effective Group Name |
| `ruser` | `PIDS_ID_RUSER` | str | Real User Name |
| `rgroup` | `PIDS_ID_RGROUP` | str | Real Group Name |
| `suser` | `PIDS_ID_SUSER` | str | Saved User Name |
| `sgroup` | `PIDS_ID_SGROUP` | str | Saved Group Name |
| `fuser` | `PIDS_ID_FUSER` | str | Filesystem User Name |
| `fgroup` | `PIDS_ID_FGROUP` | str | Filesystem Group Name |
| `luid` | `PIDS_ID_LOGIN` | s_int | Login UID |

### Process State
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `state` | `PIDS_STATE` | s_ch | Process State (R, S, D, Z, T) |
| `nice` | `PIDS_NICE` | s_int | Nice Value |
| `priority` | `PIDS_PRIORITY` | s_int | Priority |
| `rtprio` | `PIDS_PRIORITY_RT` | s_int | Real-time Priority |
| `sched` | `PIDS_SCHED_CLASS` | s_int | Scheduling Class |
| N/A | `PIDS_SCHED_CLASSSTR` | str | Scheduling Class (string) |
| `processor` | `PIDS_PROCESSOR` | s_int | CPU Number |
| N/A | `PIDS_PROCESSOR_NODE` | s_int | NUMA Node |

### Time Information
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `utime` | `PIDS_TICS_USER` | ull_int | User Time (in ticks/jiffies) |
| `stime` | `PIDS_TICS_SYSTEM` | ull_int | System Time (in ticks/jiffies) |
| `cutime` | `PIDS_TICS_USER_C` | ull_int | Cumulative User Time |
| `cstime` | `PIDS_TICS_SYSTEM_C` | ull_int | Cumulative System Time |
| `start_time` | `PIDS_TICS_BEGAN` | ull_int | Start Time (in ticks since boot) |
| N/A | `PIDS_TICS_ALL` | ull_int | utime + stime |
| N/A | `PIDS_TICS_ALL_C` | ull_int | utime + stime + cutime + cstime |
| `gtime` | `PIDS_TICS_GUEST` | ull_int | Guest Time |
| `cgtime` | `PIDS_TICS_GUEST_C` | ull_int | Cumulative Guest Time |
| `blkio_tics` | `PIDS_TICS_BLKIO` | ull_int | Block I/O Time |
| N/A | `PIDS_TIME_START` | real | Start time in seconds (converted) |
| N/A | `PIDS_TIME_ELAPSED` | real | Elapsed time in seconds |
| N/A | `PIDS_TIME_ALL` | real | Total CPU time in seconds |
| N/A | `PIDS_TIME_ALL_C` | real | Total CPU time including children |

### Memory Information
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `size` | `PIDS_MEM_VIRT_PGS` | ul_int | Virtual Memory (pages) |
| `vm_size` | `PIDS_MEM_VIRT` | ul_int | Virtual Memory (KiB) |
| `resident` | `PIDS_MEM_RES_PGS` | ul_int | Resident Memory (pages) |
| `vm_rss` | `PIDS_MEM_RES` | ul_int | Resident Memory (KiB) |
| `share` | `PIDS_MEM_SHR_PGS` | ul_int | Shared Memory (pages) |
| N/A | `PIDS_MEM_SHR` | ul_int | Shared Memory (KiB) |
| `trs` | `PIDS_MEM_CODE_PGS` | ul_int | Code Memory (pages) |
| `vm_exe` | `PIDS_MEM_CODE` | ul_int | Code Memory (KiB) |
| `drs` | `PIDS_MEM_DATA_PGS` | ul_int | Data Memory (pages) |
| `vm_data` | `PIDS_MEM_DATA` | ul_int | Data Memory (KiB) |
| `vm_stack` | `PIDS_VM_STACK` | ul_int | Stack Size (KiB) |
| `vm_swap` | `PIDS_VM_SWAP` | ul_int | Swap Usage (KiB) |
| `vm_lock` | `PIDS_VM_LOCKED` | ul_int | Locked Memory (KiB) |
| `rss` | `PIDS_RSS` | ul_int | RSS from stat |
| `rss_rlim` | `PIDS_RSS_RLIM` | ul_int | RSS Limit |

### Page Faults
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `maj_flt` | `PIDS_FLT_MAJ` | ul_int | Major Page Faults |
| `min_flt` | `PIDS_FLT_MIN` | ul_int | Minor Page Faults |
| `maj_delta` | `PIDS_FLT_MAJ_DELTA` | s_int | Major Fault Delta |
| `min_delta` | `PIDS_FLT_MIN_DELTA` | s_int | Minor Fault Delta |
| N/A | `PIDS_FLT_MAJ_C` | ul_int | maj_flt + cmaj_flt |
| N/A | `PIDS_FLT_MIN_C` | ul_int | min_flt + cmin_flt |

### Command and Environment
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `cmd` | `PIDS_CMD` | str | Command Name |
| `cmdline` | `PIDS_CMDLINE` | str | Command Line |
| `cmdline_v` | `PIDS_CMDLINE_V` | strv | Command Line Vector |
| `environ` | `PIDS_ENVIRON` | str | Environment |
| `environ_v` | `PIDS_ENVIRON_V` | strv | Environment Vector |
| `exe` | `PIDS_EXE` | str | Executable Path |

### I/O Statistics
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `rchar` | `PIDS_IO_READ_CHARS` | ul_int | Characters Read |
| `wchar` | `PIDS_IO_WRITE_CHARS` | ul_int | Characters Written |
| `syscr` | `PIDS_IO_READ_OPS` | ul_int | Read Operations |
| `syscw` | `PIDS_IO_WRITE_OPS` | ul_int | Write Operations |
| `read_bytes` | `PIDS_IO_READ_BYTES` | ul_int | Bytes Read |
| `write_bytes` | `PIDS_IO_WRITE_BYTES` | ul_int | Bytes Written |
| `cancelled_write_bytes` | `PIDS_IO_WRITE_CBYTES` | ul_int | Cancelled Write Bytes |

### Signals
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `signal` | `PIDS_SIGNALS` | str | Pending Signals (ShdPnd) |
| `_sigpnd` | `PIDS_SIGPENDING` | str | Per-task Pending Signals |
| `blocked` | `PIDS_SIGBLOCKED` | str | Blocked Signals |
| `sigignore` | `PIDS_SIGIGNORE` | str | Ignored Signals |
| `sigcatch` | `PIDS_SIGCATCH` | str | Caught Signals |

### Namespaces
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `ns.ipc` | `PIDS_NS_IPC` | ul_int | IPC Namespace |
| `ns.mnt` | `PIDS_NS_MNT` | ul_int | Mount Namespace |
| `ns.net` | `PIDS_NS_NET` | ul_int | Network Namespace |
| `ns.pid` | `PIDS_NS_PID` | ul_int | PID Namespace |
| `ns.user` | `PIDS_NS_USER` | ul_int | User Namespace |
| `ns.uts` | `PIDS_NS_UTS` | ul_int | UTS Namespace |
| N/A | `PIDS_NS_CGROUP` | ul_int | Cgroup Namespace |
| N/A | `PIDS_NS_TIME` | ul_int | Time Namespace |

### Container Information
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `lxcname` | `PIDS_LXCNAME` | str | LXC Container Name |
| `dockerid` | `PIDS_DOCKER_ID` | str | Docker Container ID (short) |
| `dockerid_64` | `PIDS_DOCKER_ID_64` | str | Docker Container ID (full) |
| `cgroup` | `PIDS_CGROUP` | str | Cgroup |
| `cgroup_v` | `PIDS_CGROUP_V` | strv | Cgroup Vector |
| `cgname` | `PIDS_CGNAME` | str | Cgroup Name |

### systemd Information
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `sd_mach` | `PIDS_SD_MACH` | str | systemd Machine |
| `sd_ouid` | `PIDS_SD_OUID` | str | systemd Owner UID |
| `sd_seat` | `PIDS_SD_SEAT` | str | systemd Seat |
| `sd_sess` | `PIDS_SD_SESS` | str | systemd Session |
| `sd_slice` | `PIDS_SD_SLICE` | str | systemd Slice |
| `sd_unit` | `PIDS_SD_UNIT` | str | systemd Unit |
| `sd_uunit` | `PIDS_SD_UUNIT` | str | systemd User Unit |

### Other Fields
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `tty` | `PIDS_TTY` | s_int | TTY Number |
| N/A | `PIDS_TTY_NAME` | str | TTY Name |
| N/A | `PIDS_TTY_NUMBER` | s_int | TTY Number (major/minor) |
| `flags` | `PIDS_FLAGS` | ul_int | Process Flags |
| `nlwp` | `PIDS_NLWP` | s_int | Number of Threads |
| `wchan` | `PIDS_WCHAN` | ul_int | Wait Channel Address |
| N/A | `PIDS_WCHAN_NAME` | str | Wait Channel Name |
| `oom_score` | `PIDS_OOM_SCORE` | s_int | OOM Score |
| `oom_adj` | `PIDS_OOM_ADJ` | s_int | OOM Adjustment |
| `autogrp_id` | `PIDS_AUTOGRP_ID` | s_int | Autogroup ID |
| `autogrp_nice` | `PIDS_AUTOGRP_NICE` | s_int | Autogroup Nice |
| `exit_signal` | `PIDS_EXIT_SIGNAL` | s_int | Exit Signal |
| `fds` | `PIDS_OPEN_FILES` | s_int | Open File Descriptors |
| `capprm` | `PIDS_CAPS_PERMITTED` | str | Permitted Capabilities |

### Address Information
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `start_code` | `PIDS_ADDR_CODE_START` | ul_int | Code Start Address |
| `end_code` | `PIDS_ADDR_CODE_END` | ul_int | Code End Address |
| `start_stack` | `PIDS_ADDR_STACK_START` | ul_int | Stack Start Address |
| `kstk_esp` | `PIDS_ADDR_CURR_ESP` | ul_int | Kernel Stack ESP |
| `kstk_eip` | `PIDS_ADDR_CURR_EIP` | ul_int | Kernel Stack EIP |

### smaps_rollup Fields
| proc_t field | pids.h enum | Type | Notes |
|--------------|-------------|------|-------|
| `smap_Rss` | `PIDS_SMAP_RSS` | ul_int | RSS |
| `smap_Pss` | `PIDS_SMAP_PSS` | ul_int | PSS |
| `smap_Pss_Anon` | `PIDS_SMAP_PSS_ANON` | ul_int | PSS Anonymous |
| `smap_Pss_File` | `PIDS_SMAP_PSS_FILE` | ul_int | PSS File |
| `smap_Pss_Shmem` | `PIDS_SMAP_PSS_SHMEM` | ul_int | PSS Shared Memory |
| `smap_Shared_Clean` | `PIDS_SMAP_SHR_CLEAN` | ul_int | Shared Clean |
| `smap_Shared_Dirty` | `PIDS_SMAP_SHR_DIRTY` | ul_int | Shared Dirty |
| `smap_Private_Clean` | `PIDS_SMAP_PRV_CLEAN` | ul_int | Private Clean |
| `smap_Private_Dirty` | `PIDS_SMAP_PRV_DIRTY` | ul_int | Private Dirty |
| `smap_Referenced` | `PIDS_SMAP_REFERENCED` | ul_int | Referenced |
| `smap_Anonymous` | `PIDS_SMAP_ANONYMOUS` | ul_int | Anonymous |
| `smap_LazyFree` | `PIDS_SMAP_LAZY_FREE` | ul_int | Lazy Free |
| `smap_AnonHugePages` | `PIDS_SMAP_HUGE_ANON` | ul_int | Anonymous Huge Pages |
| `smap_ShmemPmdMapped` | `PIDS_SMAP_HUGE_SHMEM` | ul_int | Shmem PMD Mapped |
| `smap_FilePmdMapped` | `PIDS_SMAP_HUGE_FILE` | ul_int | File PMD Mapped |
| `smap_Shared_Hugetlb` | `PIDS_SMAP_HUGE_TLBSHR` | ul_int | Shared Hugetlb |
| `smap_Private_Hugetlb` | `PIDS_SMAP_HUGE_TLBPRV` | ul_int | Private Hugetlb |
| `smap_Swap` | `PIDS_SMAP_SWAP` | ul_int | Swap |
| `smap_SwapPss` | `PIDS_SMAP_SWAP_PSS` | ul_int | Swap PSS |
| `smap_Locked` | `PIDS_SMAP_LOCKED` | ul_int | Locked |

## Migration Examples

### Example 1: Simple Process Listing

#### Old Code (proc_t)
```c
#include <proc/readproc.h>
#include <stdio.h>

void list_processes_old(void) {
    PROCTAB *pt;
    proc_t *proc = NULL;
    
    pt = openproc(PROC_FILLSTAT | PROC_FILLSTATUS);
    if (!pt) {
        perror("openproc");
        return;
    }
    
    while ((proc = readproc(pt, proc)) != NULL) {
        printf("PID: %5d  CMD: %s\n", proc->tid, proc->cmd);
    }
    
    closeproc(pt);
}
```

#### New Code (pids.h)
```c
#include <proc/pids.h>
#include <stdio.h>

void list_processes_new(void) {
    struct pids_info *info = NULL;
    struct pids_fetch *fetched;
    
    enum pids_item items[] = {
        PIDS_ID_PID,
        PIDS_CMD
    };
    int numitems = sizeof(items) / sizeof(items[0]);
    
    if (procps_pids_new(&info, items, numitems) < 0) {
        perror("procps_pids_new");
        return;
    }
    
    fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
    if (!fetched) {
        procps_pids_unref(&info);
        return;
    }
    
    for (int i = 0; i < fetched->counts->total; i++) {
        struct pids_stack *stack = fetched->stacks[i];
        printf("PID: %5d  CMD: %s\n",
               PIDS_VAL(0, s_int, stack),   // PIDS_ID_PID
               PIDS_VAL(1, str, stack));    // PIDS_CMD
    }
    
    procps_pids_unref(&info);
}
```

### Example 2: Process with Memory Information

#### Old Code (proc_t)
```c
void show_process_memory_old(pid_t pid) {
    PROCTAB *pt;
    proc_t *proc = NULL;
    pid_t pids[2] = {pid, 0};
    
    pt = openproc(PROC_FILLSTAT | PROC_FILLMEM | PROC_PID, pids);
    if (!pt) return;
    
    proc = readproc(pt, proc);
    if (proc) {
        printf("Process %d:\n", proc->tid);
        printf("  Virtual: %lu KB\n", proc->vm_size);
        printf("  Resident: %lu KB\n", proc->vm_rss);
        printf("  Shared: %lu KB\n", proc->share * 4);  // pages to KB
    }
    
    closeproc(pt);
}
```

#### New Code (pids.h)
```c
void show_process_memory_new(pid_t pid) {
    struct pids_info *info = NULL;
    struct pids_fetch *fetched;
    unsigned int pids[1] = {(unsigned int)pid};
    
    enum pids_item items[] = {
        PIDS_ID_PID,
        PIDS_MEM_VIRT,
        PIDS_MEM_RES,
        PIDS_MEM_SHR
    };
    int numitems = sizeof(items) / sizeof(items[0]);
    
    if (procps_pids_new(&info, items, numitems) < 0) return;
    
    fetched = procps_pids_select(info, pids, 1, PIDS_SELECT_PID);
    if (fetched && fetched->counts->total > 0) {
        struct pids_stack *stack = fetched->stacks[0];
        printf("Process %d:\n", PIDS_VAL(0, s_int, stack));
        printf("  Virtual: %lu KB\n", PIDS_VAL(1, ul_int, stack));
        printf("  Resident: %lu KB\n", PIDS_VAL(2, ul_int, stack));
        printf("  Shared: %lu KB\n", PIDS_VAL(3, ul_int, stack));
    }
    
    procps_pids_unref(&info);
}
```

### Example 3: CPU Usage Calculation

#### Old Code (proc_t)
```c
void calculate_cpu_old(void) {
    PROCTAB *pt;
    proc_t *proc = NULL;
    
    pt = openproc(PROC_FILLSTAT);
    if (!pt) return;
    
    while ((proc = readproc(pt, proc)) != NULL) {
        unsigned long long total_time = proc->utime + proc->stime;
        printf("PID %d: %llu jiffies\n", proc->tid, total_time);
    }
    
    closeproc(pt);
}
```

#### New Code (pids.h)
```c
void calculate_cpu_new(void) {
    struct pids_info *info = NULL;
    struct pids_fetch *fetched;
    
    enum pids_item items[] = {
        PIDS_ID_PID,
        PIDS_TIME_ALL  // This is utime + stime
    };
    int numitems = sizeof(items) / sizeof(items[0]);
    
    if (procps_pids_new(&info, items, numitems) < 0) return;
    
    fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
    if (fetched) {
        for (int i = 0; i < fetched->counts->total; i++) {
            struct pids_stack *stack = fetched->stacks[i];
            printf("PID %d: %llu jiffies\n",
                   PIDS_VAL(0, s_int, stack),
                   PIDS_VAL(1, ull_int, stack));
        }
    }
    
    procps_pids_unref(&info);
}
```

### Example 4: Filtering and Sorting

#### Old Code (proc_t)
```c
// Custom sort callback
static int cmp_mem(const proc_t **a, const proc_t **b) {
    return (*b)->vm_rss - (*a)->vm_rss;  // Descending order
}

void top_memory_users_old(void) {
    PROCTAB *pt;
    proc_t *proc = NULL;
    proc_t **procs = NULL;
    int count = 0, capacity = 100;
    
    procs = malloc(capacity * sizeof(proc_t *));
    pt = openproc(PROC_FILLSTAT | PROC_FILLMEM);
    
    // Collect all processes
    while ((proc = readproc(pt, NULL)) != NULL) {
        if (count >= capacity) {
            capacity *= 2;
            procs = realloc(procs, capacity * sizeof(proc_t *));
        }
        procs[count++] = proc;
    }
    
    // Sort (custom implementation needed)
    qsort(procs, count, sizeof(proc_t *), 
          (int(*)(const void *, const void *))cmp_mem);
    
    // Print top 10
    for (int i = 0; i < 10 && i < count; i++) {
        printf("%5d %10lu %s\n", procs[i]->tid, 
               procs[i]->vm_rss, procs[i]->cmd);
    }
    
    // Cleanup
    for (int i = 0; i < count; i++) {
        freeproc(procs[i]);
    }
    free(procs);
    closeproc(pt);
}
```

#### New Code (pids.h)
```c
void top_memory_users_new(void) {
    struct pids_info *info = NULL;
    struct pids_fetch *fetched;
    struct pids_stack **sorted;
    
    enum pids_item items[] = {
        PIDS_ID_PID,
        PIDS_MEM_RES,
        PIDS_CMD
    };
    int numitems = sizeof(items) / sizeof(items[0]);
    
    if (procps_pids_new(&info, items, numitems) < 0) return;
    
    fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
    if (!fetched) {
        procps_pids_unref(&info);
        return;
    }
    
    // Sort by memory (descending)
    sorted = procps_pids_sort(info, fetched->stacks, 
                              fetched->counts->total,
                              PIDS_MEM_RES, 
                              PIDS_SORT_DESCEND);
    
    // Print top 10
    int limit = (fetched->counts->total < 10) ? fetched->counts->total : 10;
    for (int i = 0; i < limit; i++) {
        struct pids_stack *stack = sorted[i];
        printf("%5d %10lu %s\n",
               PIDS_VAL(0, s_int, stack),
               PIDS_VAL(1, ul_int, stack),
               PIDS_VAL(2, str, stack));
    }
    
    procps_pids_unref(&info);
}
```

### Example 5: Working with Threads

#### Old Code (proc_t)
```c
void show_threads_old(pid_t pid) {
    PROCTAB *pt;
    proc_t *proc = NULL;
    pid_t pids[2] = {pid, 0};
    
    pt = openproc(PROC_FILLSTAT | PROC_PID, pids);
    if (!pt) return;
    
    printf("Threads for process %d:\n", pid);
    while ((proc = readeither(pt, proc)) != NULL) {
        if (proc->tgid == pid) {
            printf("  TID: %d, State: %c\n", proc->tid, proc->state);
        }
    }
    
    closeproc(pt);
}
```

#### New Code (pids.h)
```c
void show_threads_new(pid_t pid) {
    struct pids_info *info = NULL;
    struct pids_fetch *fetched;
    unsigned int pids[1] = {(unsigned int)pid};
    
    enum pids_item items[] = {
        PIDS_ID_TGID,
        PIDS_ID_TID,
        PIDS_STATE
    };
    int numitems = sizeof(items) / sizeof(items[0]);
    
    if (procps_pids_new(&info, items, numitems) < 0) return;
    
    fetched = procps_pids_select(info, pids, 1, PIDS_SELECT_PID_THREADS);
    if (fetched) {
        printf("Threads for process %d:\n", pid);
        for (int i = 0; i < fetched->counts->total; i++) {
            struct pids_stack *stack = fetched->stacks[i];
            printf("  TID: %d, State: %c\n",
                   PIDS_VAL(1, s_int, stack),
                   PIDS_VAL(2, s_ch, stack));
        }
    }
    
    procps_pids_unref(&info);
}
```

## Function Signature Changes

### Passing Process Data to Functions

In the old API, functions typically received a `proc_t *` pointer directly. In the new API, there are two common patterns:

#### Pattern 1: Pass the pids_stack directly

**Old API:**
```c
static void display_process(const proc_t *p) {
    printf("PID: %d, CMD: %s\n", p->tid, p->cmd);
}

// Usage
while ((proc = readproc(pt, proc)) != NULL) {
    display_process(proc);
}
```

**New API:**
```c
static void display_process(struct pids_stack *stack, int pid_idx, int cmd_idx) {
    printf("PID: %d, CMD: %s\n",
           PIDS_VAL(pid_idx, s_int, stack),
           PIDS_VAL(cmd_idx, str, stack));
}

// Usage
for (int i = 0; i < fetched->counts->total; i++) {
    display_process(fetched->stacks[i], MY_PID, MY_CMD);
}
```

#### Pattern 2: Pass an index and use a window/context structure

This pattern is used in `top` where process data is stored in a window structure.

**Old API:**
```c
static inline const char *forest_display(const WIN_t *q, const proc_t *p) {
    // Access fields directly from proc_t
    const char *cmd = p->cmd;
    int level = p->some_field;
    // ... process display logic ...
    return formatted_string;
}

// Usage
forest_display(window, proc);
```

**New API:**
```c
static inline const char *forest_display(const WIN_t *q, int idx) {
    // Extract pids_stack from the window's array
    struct pids_stack *p = q->ppt[idx];
    
    // Access fields using PIDS_VAL with predefined indices
    const char *cmd = PIDS_VAL(eu_CMD, str, p);
    int level = PIDS_VAL(eu_TREE_LVL, s_int, p);
    // ... process display logic ...
    return formatted_string;
}

// Usage
forest_display(window, process_index);
```

**Key Points:**
- The window structure (`WIN_t`) now contains an array of `pids_stack` pointers: `q->ppt[idx]`
- Instead of passing the process structure, pass the index
- Inside the function, retrieve the stack: `struct pids_stack *p = q->ppt[idx]`
- Use `PIDS_VAL()` macro to access fields
- Define enums for field indices (e.g., `eu_CMD`, `eu_TREE_LVL`) for clarity

#### Pattern 3: Create wrapper structures (for complex cases)

If you need to pass process data between many functions, consider creating a wrapper:

```c
// Define a context structure
typedef struct {
    struct pids_stack *stack;
    // Store commonly used indices
    int pid_idx;
    int cmd_idx;
    int mem_idx;
} proc_context_t;

static void init_proc_context(proc_context_t *ctx, struct pids_stack *stack) {
    ctx->stack = stack;
    ctx->pid_idx = MY_PID;
    ctx->cmd_idx = MY_CMD;
    ctx->mem_idx = MY_MEM;
}

static void display_process(const proc_context_t *ctx) {
    printf("PID: %d, CMD: %s, MEM: %lu\n",
           PIDS_VAL(ctx->pid_idx, s_int, ctx->stack),
           PIDS_VAL(ctx->cmd_idx, str, ctx->stack),
           PIDS_VAL(ctx->mem_idx, ul_int, ctx->stack));
}
```

### Storing Process References

**Old API:**
```c
// Store array of proc_t pointers
proc_t **saved_procs = malloc(count * sizeof(proc_t *));
for (int i = 0; i < count; i++) {
    saved_procs[i] = readproc(pt, NULL);
}
```

**New API:**
```c
// Store array of pids_stack pointers from fetched results
struct pids_stack **saved_stacks = fetched->stacks;
// Note: These are owned by the pids_info context
// Don't free them individually; they're freed when you call procps_pids_unref()
```

## Best Practices

### 1. Request Only What You Need
The new API is designed to be efficient. Only request the items you actually need:

```c
// Bad: Requesting everything
enum pids_item items[] = {
    PIDS_ID_PID, PIDS_ID_PPID, PIDS_CMD, PIDS_STATE,
    PIDS_MEM_RES, PIDS_MEM_VIRT, /* ... 50 more items ... */
};

// Good: Request only what you use
enum pids_item items[] = {
    PIDS_ID_PID,
    PIDS_CMD,
    PIDS_MEM_RES
};
```

### 2. Reuse the Context
Don't create a new context for each query. Reuse it:

```c
// Bad: Creating context repeatedly
for (int i = 0; i < 100; i++) {
    struct pids_info *info;
    procps_pids_new(&info, items, numitems);
    // ... use it ...
    procps_pids_unref(&info);
}

// Good: Create once, use many times
struct pids_info *info;
procps_pids_new(&info, items, numitems);
for (int i = 0; i < 100; i++) {
    // ... use it ...
}
procps_pids_unref(&info);
```

### 3. Use Enums for Item Access
Instead of magic numbers, use enums to track item positions:

```c
enum my_items {
    MY_PID = 0,
    MY_CMD,
    MY_MEM,
    MY_STATE
};

enum pids_item items[] = {
    [MY_PID]   = PIDS_ID_PID,
    [MY_CMD]   = PIDS_CMD,
    [MY_MEM]   = PIDS_MEM_RES,
    [MY_STATE] = PIDS_STATE
};

// Access is clearer
int pid = PIDS_VAL(MY_PID, s_int, stack);
char *cmd = PIDS_VAL(MY_CMD, str, stack);
```

### 4. Check Return Values
Always check for errors:

```c
struct pids_info *info = NULL;
if (procps_pids_new(&info, items, numitems) < 0) {
    fprintf(stderr, "Failed to initialize pids context\n");
    return -1;
}

struct pids_fetch *fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
if (!fetched || !fetched->stacks) {
    fprintf(stderr, "Failed to reap processes\n");
    procps_pids_unref(&info);
    return -1;
}
```

### 5. Use Appropriate Fetch Type
Choose the right fetch type for your needs:

```c
// For processes only (like readproc)
fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);

// For processes and threads (like readeither)
fetched = procps_pids_reap(info, PIDS_FETCH_THREADS_TOO);
```

### 6. Understand Item Types
Use the correct type accessor for each item:

```c
// s_int for signed integers
int pid = PIDS_VAL(idx, s_int, stack);

// ul_int for unsigned long
unsigned long mem = PIDS_VAL(idx, ul_int, stack);

// str for strings
char *cmd = PIDS_VAL(idx, str, stack);

// s_ch for signed char
char state = PIDS_VAL(idx, s_ch, stack);

// ull_int for unsigned long long
unsigned long long time = PIDS_VAL(idx, ull_int, stack);
```

### 7. Memory Management
The new API handles memory automatically. Don't free strings or stacks:

```c
// Bad: Don't do this!
char *cmd = PIDS_VAL(MY_CMD, str, stack);
free(cmd);  // WRONG! Library owns this memory

// Good: Just use the values
char *cmd = PIDS_VAL(MY_CMD, str, stack);
printf("%s\n", cmd);  // OK

// Cleanup is automatic when you unref
procps_pids_unref(&info);
```

### 8. Reset Items If Needed
You can change the items being fetched:

```c
struct pids_info *info;
enum pids_item items1[] = {PIDS_ID_PID, PIDS_CMD};
procps_pids_new(&info, items1, 2);

// ... use it ...

// Change what we're fetching
enum pids_item items2[] = {PIDS_ID_PID, PIDS_MEM_RES, PIDS_STATE};
procps_pids_reset(info, items2, 3);

// ... use it with new items ...

procps_pids_unref(&info);
```

## Common Pitfalls

### 1. Wrong Type Accessor
```c
// Wrong: Using wrong type
int mem = PIDS_VAL(idx, s_int, stack);  // PIDS_MEM_RES is ul_int!

// Correct:
unsigned long mem = PIDS_VAL(idx, ul_int, stack);
```

### 2. Incorrect Index
```c
enum pids_item items[] = {PIDS_ID_PID, PIDS_CMD, PIDS_STATE};

// Wrong: Hardcoded wrong index
char *cmd = PIDS_VAL(0, str, stack);  // That's PID, not CMD!

// Correct: Use the right index
char *cmd = PIDS_VAL(1, str, stack);  // CMD is at index 1

// Better: Use enum
enum { MY_PID, MY_CMD, MY_STATE };
char *cmd = PIDS_VAL(MY_CMD, str, stack);
```

### 3. Forgetting to Unref
```c
// Wrong: Memory leak
struct pids_info *info;
procps_pids_new(&info, items, numitems);
// ... forgot to call procps_pids_unref(&info)

// Correct:
struct pids_info *info;
procps_pids_new(&info, items, numitems);
// ... use it ...
procps_pids_unref(&info);  // Always cleanup
```

### 4. Assuming Item Order
```c
// Fragile: Depends on declaration order
enum pids_item items[] = {PIDS_CMD, PIDS_ID_PID};  // Note: CMD first!
int pid = PIDS_VAL(0, s_int, stack);  // WRONG! Index 0 is CMD, not PID

// Robust: Use explicit enum
enum { MY_CMD = 0, MY_PID };
enum pids_item items[] = {
    [MY_CMD] = PIDS_CMD,
    [MY_PID] = PIDS_ID_PID
};
int pid = PIDS_VAL(MY_PID, s_int, stack);  // Always correct
```

## Compatibility Layer

If you need to support both APIs temporarily, you can create a compatibility layer:

```c
#ifdef USE_OLD_API
#include <proc/readproc.h>
typedef struct {
    PROCTAB *pt;
    proc_t *proc;
} proc_iter_t;
#else
#include <proc/pids.h>
typedef struct {
    struct pids_info *info;
    struct pids_fetch *fetched;
    int current;
} proc_iter_t;
#endif

// Initialize iterator
proc_iter_t *proc_iter_init(void);

// Get next process
int proc_iter_next(proc_iter_t *iter, int *pid, char **cmd);

// Cleanup
void proc_iter_free(proc_iter_t *iter);
```

This allows gradual migration while maintaining compatibility.

## Performance Considerations

The new API offers several performance benefits:

1. **Reduced Memory**: Only requested items are allocated
2. **Better Caching**: Library can cache and reuse data
3. **Efficient Sorting**: Built-in sorting is optimized
4. **Batch Operations**: `reap()` gets all processes at once
5. **Selective Reading**: Only read what you need from /proc

## Conclusion

The migration from `proc_t` to `pids.h` requires some code changes, but the new API is:

- **More efficient**: Only fetch what you need
- **More flexible**: Easy to add/remove items
- **More maintainable**: Clearer separation of concerns
- **More robust**: Better error handling

Use this guide and the examples to migrate your code. Start with simple cases and gradually refactor more complex code. The new API's design makes it easier to write correct, efficient process monitoring tools.

## Additional Resources

- See `library/include/pids.h` for the complete API
- Check `src/ps/` and `src/top/` for real-world usage examples
- Review `library/pids.c` for implementation details
- Read the procps-ng NEWS file for version-specific changes

## Questions or Issues?

If you encounter migration issues not covered here, please:

1. Check the procps-ng repository examples
2. Review the library header files for detailed documentation
3. Look at how `ps` and `top` were migrated
4. Open an issue on the procps-ng project page
