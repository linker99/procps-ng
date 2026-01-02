# Analysis of libproc2 pids Core Functions

## Overview

This document provides detailed analysis of the core functions in the libproc2 library related to process information collection. These functions are called by the `tasks_refresh` function in the `top` utility to retrieve and process process information from the `/proc` filesystem.

## Function Call Chain

```
tasks_refresh (top.c)
    ↓
procps_pids_reap (pids.c:1544) [Public API]
    ↓
pids_stacks_fetch (pids.c:1223) [Internal Function]
    ↓
    ├─ readproc/readeither (readproc.c) [Read /proc]
    ├─ pids_proc_tally (pids.c:1129) [Tally Process States]
    │   └─ pids_make_hist (pids.c:750) [Historical Records]
    └─ pids_assign_results (pids.c:922) [Assign Results]
```

## 1. procps_pids_reap - Public Interface Function

### Function Signature

```c
PROCPS_EXPORT struct pids_fetch *procps_pids_reap (
    struct pids_info *info,
    enum pids_fetch_type which)
```

**Location**: `library/pids.c:1544-1578`

### Function Description

`procps_pids_reap` is a public API provided by the libproc2 library to retrieve information about all processes (or threads) in the system. This is the primary entry point for the `top` utility to interact with the underlying library.

### Parameters

- **info**: `struct pids_info *` - Context pointer containing configuration and state information
- **which**: `enum pids_fetch_type` - Fetch type
  - `PIDS_FETCH_TASKS_ONLY`: Fetch only processes
  - `PIDS_FETCH_THREADS_TOO`: Fetch both processes and threads

### Return Value

- Success: Returns `struct pids_fetch *` pointer containing process list and statistics
- Failure: Returns `NULL` and sets `errno`

### Detailed Implementation

```c
struct pids_fetch *procps_pids_reap (
    struct pids_info *info,
    enum pids_fetch_type which)
{
    struct timespec ts;
    int rc;

    // 1. Parameter validation
    errno = EINVAL;
    if (info == NULL)
        return NULL;
    if (which != PIDS_FETCH_TASKS_ONLY && which != PIDS_FETCH_THREADS_TOO)
        return NULL;
    if (!info->maxitems)
        return NULL;
    errno = 0;

    // 2. Container check (if enabled)
    if (info->containers_yes)
        pids_containers_check();

    // 3. Open /proc directory
    if (!pids_oldproc_open(&info->fetch_PT, info->oldflags))
        return NULL;
    
    // 4. Set read function pointer
    info->read_something = which ? readeither : readproc;

    // 5. Get system boot time (for calculating process runtime)
    info->boot_tics = 0;
    if (0 >= clock_gettime(CLOCK_BOOTTIME, &ts))
        info->boot_tics = (ts.tv_sec + ts.tv_nsec * 1.0e-9) * info->hertz;

    // 6. Core: Call pids_stacks_fetch to retrieve all process information
    rc = pids_stacks_fetch(info);

    // 7. Close /proc directory
    pids_oldproc_close(&info->fetch_PT);
    
    // 8. Return results
    return (rc > 0) ? &info->fetch.results : NULL;
}
```

### Key Steps

1. **Parameter Validation**: Ensures passed parameters are valid
2. **Container Support**: Checks container environment (Docker, etc.)
3. **Open /proc**: Uses legacy `readproc` interface to open /proc filesystem
4. **Set Read Function**:
   - `readproc`: Read only processes
   - `readeither`: Read processes and threads
5. **Calculate Boot Time**: Used for calculating process runtime later
6. **Fetch Process Data**: Calls core function `pids_stacks_fetch`
7. **Cleanup Resources**: Closes /proc directory handle
8. **Return Results**: Returns structure containing process information

### Difference from procps_pids_select

```c
PROCPS_EXPORT struct pids_fetch *procps_pids_select (
    struct pids_info *info,
    unsigned *these,
    int numthese,
    enum pids_select_type which)
```

- **procps_pids_reap**: Fetches all processes
- **procps_pids_select**: Fetches only specified PIDs/UIDs
- Both call `pids_stacks_fetch`, but `select` sets filter criteria first

## 2. pids_stacks_fetch - Core Fetch Function

### Function Signature

```c
static int pids_stacks_fetch (struct pids_info *info)
```

**Location**: `library/pids.c:1223-1281`

### Function Description

`pids_stacks_fetch` is the internal core function responsible for:
1. Iterating through all processes in /proc directory
2. Allocating data structures for each process
3. Calling tally and assignment functions
4. Managing dynamic memory allocation

### Implementation Principles

#### Phase 1: Initialization

```c
#define n_alloc  info->fetch.n_alloc   // Number of stacks allocated
#define n_inuse  info->fetch.n_inuse   // Number of stacks in use
#define n_saved  info->fetch.n_alloc_save

struct stacks_extent *ext;

// Initialize on first call
if (!info->fetch.anchor) {
    // Allocate initial pointer array (1024 entries)
    if (!(info->fetch.anchor = calloc(STACKS_INIT, sizeof(void *))))
        return -1;
    // Allocate actual data stacks
    if (!(ext = pids_stacks_alloc(info, STACKS_INIT)))
        return -1;
    // Copy stack pointers to anchor array
    memcpy(info->fetch.anchor, ext->stacks, sizeof(void *) * STACKS_INIT);
    n_alloc = STACKS_INIT;
}
```

**Data Structures**:
- `anchor`: Pointer array pointing to each process's data stack
- `STACKS_INIT = 1024`: Initial number of stacks allocated
- Two-level allocation: pointer array + actual data stacks

#### Phase 2: History Toggle and Counter Reset

```c
pids_toggle_history(info);
memset(&info->fetch.counts, 0, sizeof(struct pids_counts));
```

- **History Toggle**: Swaps new/old historical data for calculating deltas (e.g., CPU usage)
- **Counter Reset**: Zeros out process state counters

#### Phase 3: Process Iteration

```c
n_inuse = 0;
while (info->read_something(info->fetch_PT, &info->fetch_proc)) {
    // 3.1 Check if array expansion needed
    if (!(n_inuse < n_alloc)) {
        n_alloc += STACKS_GROW;  // Grow by 128 each time
        if (!(info->fetch.anchor = realloc(info->fetch.anchor, sizeof(void *) * n_alloc))
        || (!(ext = pids_stacks_alloc(info, STACKS_GROW))))
            return -1;
        memcpy(info->fetch.anchor + n_inuse, ext->stacks, sizeof(void *) * STACKS_GROW);
    }
    
    // 3.2 Tally process state
    if (!pids_proc_tally(info, &info->fetch.counts, &info->fetch_proc))
        return -1;
    
    // 3.3 Assign results to stack
    if (!pids_assign_results(info, info->fetch.anchor[n_inuse++], &info->fetch_proc))
        return -1;
}
```

**Key Points**:
- `read_something`: Function pointer to `readproc` or `readeither`
- **Grow on Demand**: Initial 1024, grows by 128 when insufficient
- For each process, calls:
  1. `pids_proc_tally`: Tally state
  2. `pids_assign_results`: Fill data

#### Phase 4: Results Finalization

```c
// Allocate results array (if needed)
if (n_saved < n_inuse + 1) {
    n_saved = n_inuse + 1;
    if (!(info->fetch.results.stacks = realloc(info->fetch.results.stacks, 
                                                sizeof(void *) * n_saved)))
        return -1;
}

// Copy stack pointers to results array
memcpy(info->fetch.results.stacks, info->fetch.anchor, sizeof(void *) * n_inuse);
info->fetch.results.stacks[n_inuse] = NULL;  // NULL-terminate

return n_inuse;
```

**Double Buffering Mechanism**:
- `anchor`: Internal working array
- `results.stacks`: Array exposed to users (NULL-terminated)
- This protects internal data structures from user modification

### Memory Management Strategy

```
Initial state: anchor[1024]
    ↓
Process count exceeds 1024
    ↓
realloc(anchor, 1024 + 128)
    ↓
Continue growing...
    ↓
Final: anchor[1024 + 128 * N]
```

- **Initial Size**: 1024 process stacks
- **Growth Strategy**: Increase by 128 each time
- **Memory Efficiency**: Only grows when needed, never shrinks

## 3. pids_proc_tally - Process State Tallying

### Function Signature

```c
static inline int pids_proc_tally (
    struct pids_info *info,
    struct pids_counts *counts,
    proc_t *p)
```

**Location**: `library/pids.c:1129-1164`

### Function Description

Tallies process states and categorizes counts, while calling the historical record function.

### Implementation Details

```c
static inline int pids_proc_tally (
    struct pids_info *info,
    struct pids_counts *counts,
    proc_t *p)
{
    // Categorize and count by process state
    switch (p->state) {
        case 'R':  // Running
            ++counts->running;
            break;
        case 'D':  // Disk sleep (uninterruptible sleep)
            ++counts->disk_sleep;
            break;
        case 'S':  // Sleeping (interruptible sleep)
            ++counts->sleeping;
            break;
        case 't':  // Tracing stop
        case 'T':  // Stopped
            ++counts->stopped;
            break;
        case 'Z':  // Zombie
            ++counts->zombied;
            break;
        default:
            // 'I' (idle)
            // 'P' (parked)
            // 'X' (dead - rarely seen)
            ++counts->other;
            break;
    }
    ++counts->total;

    // If history is enabled, save data
    if (info->history_yes)
        return pids_make_hist(info, p);
    return 1;
}
```

### Process State Categories

| State | Meaning | Counter Field |
|-------|---------|--------------|
| R | Running | running |
| D | Disk Sleep (uninterruptible sleep, usually waiting for I/O) | disk_sleep |
| S | Sleeping (interruptible sleep) | sleeping |
| T/t | Stopped (stopped/tracing stop) | stopped |
| Z | Zombie | zombied |
| I/P/X | Idle/Parked/Dead | other |

### counts Structure

```c
struct pids_counts {
    int total;        // Total processes
    int running;      // Running
    int sleeping;     // Sleeping
    int disk_sleep;   // Uninterruptible sleep
    int stopped;      // Stopped
    int zombied;      // Zombie processes
    int other;        // Other states
};
```

These counts are displayed in `top`'s summary line, for example:
```
Tasks: 325 total,   2 running, 323 sleeping,   0 stopped,   0 zombie
```

## 4. pids_make_hist - Historical Data Recording

### Function Signature

```c
static inline int pids_make_hist (
    struct pids_info *info,
    proc_t *p)
```

**Location**: `library/pids.c:750-783`

### Function Description

Maintains historical data for each process to calculate delta values (such as CPU usage, page fault deltas).

### Implementation Principles

```c
static inline int pids_make_hist (
    struct pids_info *info,
    proc_t *p)
{
    TIC_t tics;
    HST_t *h;
    int slot = info->hist->num_tasks;

    // 1. Check if history array expansion needed
    if (slot + 1 >= Hr(HHist_siz)) {
        Hr(HHist_siz) += NEWOLD_GROW;  // Increase by 128
        Hr(PHist_sav) = realloc(Hr(PHist_sav), sizeof(HST_t) * Hr(HHist_siz));
        Hr(PHist_new) = realloc(Hr(PHist_new), sizeof(HST_t) * Hr(HHist_siz));
        if (!Hr(PHist_sav) || !Hr(PHist_new))
            return 0;
    }
    
    // 2. Save current process data to new history record
    Hr(PHist_new[slot].pid)  = p->tid;
    Hr(PHist_new[slot].maj)  = p->maj_flt;    // Major page faults
    Hr(PHist_new[slot].min)  = p->min_flt;    // Minor page faults
    Hr(PHist_new[slot].tics) = tics = (p->utime + p->stime);  // CPU time

    // 3. Add new record to hash table
    pids_histput(info, slot);

    // 4. Find previous record, calculate deltas
    if ((h = pids_histget(info, p->tid))) {
        tics -= h->tics;                      // CPU delta
        p->maj_delta = p->maj_flt - h->maj;   // Major fault delta
        p->min_delta = p->min_flt - h->min;   // Minor fault delta
    }
    
    // 5. Save CPU time delta (used for calculating %CPU)
    p->pcpu = tics;

    info->hist->num_tasks++;
    return 1;
}
```

### Historical Data Structures

```c
typedef struct HST_t {
    TIC_t tics;              // CPU time (user time + system time)
    unsigned long maj, min;  // Major/minor page fault counts
    int pid;                 // Process ID (hash key)
    int lnk;                 // Next node in hash chain
} HST_t;

struct history_info {
    int    num_tasks;        // Current number of tasks
    int    HHist_siz;        // Size of history array
    HST_t *PHist_sav;        // Old historical data
    HST_t *PHist_new;        // New historical data
    int    HHash_one[HHASH_SIZE];   // Hash table 1
    int    HHash_two[HHASH_SIZE];   // Hash table 2
    int    HHash_nul[HHASH_SIZE];   // Empty hash table template
    int   *PHash_sav;        // Pointer to old hash table
    int   *PHash_new;        // Pointer to new hash table
};
```

### Double Buffering Mechanism

```
Refresh N:
  PHist_sav = Historical data N-1
  PHist_new = Historical data N (being filled)
  PHash_sav = Hash table N-1
  PHash_new = Hash table N (being filled)

After pids_toggle_history() call:
  PHist_sav = Historical data N
  PHist_new = Historical data N-1 (will be overwritten)
  PHash_sav = Hash table N
  PHash_new = Hash table N-1 (will be overwritten)
```

### Hash Table Mechanism

```c
#define HHASH_SIZE  4096
#define _HASH_PID_(pid) (pid & (HHASH_SIZE - 1))

// Insert into hash table
static inline void pids_histput (
    struct pids_info *info,
    unsigned this)
{
    int V = _HASH_PID_(Hr(PHist_new[this].pid));
    Hr(PHist_new[this].lnk) = Hr(PHash_new[V]);
    Hr(PHash_new[V]) = this;
}

// Search hash table
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

**Hash Table Properties**:
- **Size**: 4096 buckets
- **Hash Function**: PID & 4095 (simple modulo)
- **Collision Resolution**: Chaining method
- **Search Time**: O(1) average, O(n) worst case

### Delta Calculations

CPU usage percentage calculation:
```c
// In pids_make_hist:
p->pcpu = tics;  // CPU tics delta between this and last refresh

// When displaying in top:
cpu_percent = (pcpu * 100.0) / (elapsed_tics * num_cpus);
```

This is how `top` can display CPU usage percentage for each process.

## 5. pids_toggle_history - Historical Data Toggle

### Function Signature

```c
static inline void pids_toggle_history (struct pids_info *info)
```

**Location**: `library/pids.c:786-801`

### Function Description

Swaps new/old history buffers at the start of each refresh, implementing efficient double buffering.

### Implementation Details

```c
static inline void pids_toggle_history (struct pids_info *info)
{
    void *v;

    // Swap history data pointers
    v = Hr(PHist_sav);
    Hr(PHist_sav) = Hr(PHist_new);
    Hr(PHist_new) = v;

    // Swap hash table pointers
    v = Hr(PHash_sav);
    Hr(PHash_sav) = Hr(PHash_new);
    Hr(PHash_new) = v;
    
    // Clear new hash table
    memcpy(Hr(PHash_new), Hr(HHash_nul), sizeof(Hr(HHash_nul)));

    // Reset task count
    info->hist->num_tasks = 0;
}
```

### Advantages

1. **Zero-Copy**: Only swaps pointers, no data copying
2. **Memory Efficient**: Reuses allocated memory
3. **Performance Optimized**: O(1) time complexity

## 6. pids_assign_results - Result Assignment

### Function Signature

```c
static inline int pids_assign_results (
    struct pids_info *info,
    struct pids_stack *stack,
    proc_t *p)
```

**Location**: `library/pids.c:922-937`

### Function Description

Converts raw process data read from `/proc` (`proc_t`) into user-requested field format.

### Implementation Principles

```c
static inline int pids_assign_results (
    struct pids_info *info,
    struct pids_stack *stack,
    proc_t *p)
{
    struct pids_result *this = stack->head;
    SET_t *that = &info->func_array[0];

    info->seterr = 0;
    
    // Iterate through all needed fields
    while (*that) {
        (*that)(info, this, p);  // Call field setter function
        ++this;
        ++that;
    }
    
    return !info->seterr;
}
```

### Function Pointer Array Mechanism

```c
typedef void (*SET_t)(struct pids_info *, struct pids_result *, proc_t *);

struct pids_info {
    ...
    SET_t *func_array;  // Field setter function pointer array
    ...
};
```

**Workflow**:
1. User specifies needed fields (e.g., PID, CPU, MEM) in `procps_pids_new()`
2. Library builds `func_array`, each element is the setter function for that field
3. `pids_assign_results` iterates the array, calling each setter function
4. Each setter function extracts data from `proc_t` and fills `pids_result`

### Example Field Setter Functions

```c
// Set PID field
static void setsfunc_pid(struct pids_info *info, struct pids_result *r, proc_t *p) {
    r->result.s_int = p->tid;
}

// Set CPU field
static void setsfunc_pcpu(struct pids_info *info, struct pids_result *r, proc_t *p) {
    r->result.real = p->pcpu;  // Already calculated in pids_make_hist
}
```

## Complete Execution Flow

### Sequence Diagram

```
top calls tasks_refresh
    ↓
Calls procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY)
    ↓
    ├─ Open /proc
    ├─ Set read_something = readproc
    ├─ Get boot_tics
    ↓
    Call pids_stacks_fetch(info)
        ↓
        ├─ Initialize anchor[1024]
        ├─ Call pids_toggle_history() [swap buffers]
        ├─ Zero counts
        ↓
        while (readproc(fetch_PT, &fetch_proc)) {  [iterate /proc]
            ↓
            ├─ Check space, expand anchor if necessary
            ↓
            Call pids_proc_tally(info, &counts, &fetch_proc)
                ↓
                ├─ Update counts based on state
                ↓
                Call pids_make_hist(info, &fetch_proc)
                    ↓
                    ├─ Save current tics, maj_flt, min_flt
                    ├─ Call pids_histput() [insert into hash table]
                    ├─ Call pids_histget() [find old data]
                    └─ Calculate deltas: pcpu, maj_delta, min_delta
            ↓
            Call pids_assign_results(info, anchor[n], &fetch_proc)
                ↓
                for (each requested field) {
                    Call field setter function(info, result, &fetch_proc)
                }
        }
        ↓
        └─ Copy results to results.stacks
    ↓
    ├─ Close /proc
    └─ Return &info->fetch.results
    ↓
Return to tasks_refresh
```

### Data Flow

```
/proc/[pid]/stat, /proc/[pid]/status, ...
    ↓ [readproc]
proc_t structure (raw data)
    ↓ [pids_proc_tally]
pids_counts structure (state statistics)
    ↓ [pids_make_hist]
Historical records + delta calculations
    ↓ [pids_assign_results]
pids_result structure (formatted results)
    ↓
pids_fetch structure (returned to user)
    ↓
top's Winstk[].ppt array
```

## Performance Considerations

### 1. Memory Allocation Strategy

- **Initial Allocation**: 1024 process stacks (suitable for most systems)
- **Growth Strategy**: Increase by 128 each time (avoids frequent reallocation)
- **Zero Shrinkage**: Never frees memory (suitable for long-running daemons)

### 2. Hash Table Optimization

- **Size**: 4096 buckets (large enough to reduce collisions)
- **Hash Function**: Simple fast bitwise operation
- **Lookup**: O(1) average time complexity

### 3. Double Buffering Mechanism

- **Zero-Copy**: Only swaps pointers
- **Concurrency-Friendly**: New/old data completely isolated
- **Memory Stable**: Reuses allocated memory

### 4. Function Inlining

Key functions use `inline` to reduce function call overhead:
- `pids_proc_tally`
- `pids_make_hist`
- `pids_toggle_history`
- `pids_assign_results`

## FAQ

### Q1: Why is historical data needed?

**A**: To calculate delta values:
- CPU usage = (current CPU tics - last CPU tics) / time delta
- Page fault rate = (current page faults - last page faults) / time delta

### Q2: Why use double buffering?

**A**: 
- Avoids data races
- Improves performance (zero-copy)
- Simplifies code logic

### Q3: Will memory grow indefinitely?

**A**: No. Array size grows to accommodate current maximum process count, then stabilizes. Even if process count decreases, the array doesn't shrink (prepared for next growth).

### Q4: How are hash collisions handled?

**A**: Using chaining method. Each hash bucket is a linked list, connected via the `lnk` field.

## Summary

These five functions constitute the core data collection engine of libproc2:

1. **procps_pids_reap**: Public API, coordinates the entire process
2. **pids_stacks_fetch**: Core engine, iterates processes and manages memory
3. **pids_proc_tally**: State tallying, provides data for summary display
4. **pids_make_hist**: Historical records, supports delta calculations
5. **pids_assign_results**: Data conversion, formats user-requested fields

Their design demonstrates:
- **High Performance**: Inline functions, hash tables, double buffering
- **Memory Efficient**: On-demand allocation, reuse mechanisms
- **Flexibility**: Function pointers, configurable fields
- **Reliability**: Comprehensive error handling

These functions provide a unified, efficient process information interface for tools like `top`, `ps`, and `pidof`.
