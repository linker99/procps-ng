# Double Buffering Mechanism - Detailed Explanation

## Overview

Double buffering is a core technique in the libproc2 pids library for managing historical data. It allows the system to collect new data while still accessing data from the previous collection, enabling calculation of delta values (such as CPU usage).

## Why Double Buffering?

### Problem Background

To calculate a process's CPU usage percentage, we need the CPU time delta between two time points:

```
CPU Usage = (Current CPU Time - Previous CPU Time) / Time Interval
```

This means we need to:
1. **Save previous data**: For comparison
2. **Collect current data**: Latest state
3. **Calculate delta**: Difference between the two

### Problems with Traditional Approach

Using a single buffer approach:

```c
// ❌ Wrong: Single buffer approach
struct process_data old_data[MAX_PROCS];
struct process_data new_data[MAX_PROCS];

// First refresh
collect_data(new_data);

// Second refresh
memcpy(old_data, new_data, sizeof(new_data));  // Copy entire array!
collect_data(new_data);
calculate_delta(old_data, new_data);
```

**Problems**:
- Must copy entire array on each refresh (potentially hundreds or thousands of processes)
- Memory copying is time-consuming, affecting performance
- Requires additional memory bandwidth

## Double Buffering Principle

### Core Idea

**Don't copy data, just swap pointers!**

```c
// Double buffering approach
struct process_data *buffer_A;  // Buffer A
struct process_data *buffer_B;  // Buffer B
struct process_data *old_ptr;   // Pointer to old data
struct process_data *new_ptr;   // Pointer to new data

// Initialize
old_ptr = buffer_A;
new_ptr = buffer_B;

// First refresh
collect_data(new_ptr);          // Write to buffer_B

// Swap pointers (zero-copy!)
void *temp = old_ptr;
old_ptr = new_ptr;              // Now old_ptr -> buffer_B
new_ptr = temp;                 // Now new_ptr -> buffer_A

// Second refresh
collect_data(new_ptr);          // Write to buffer_A
calculate_delta(old_ptr, new_ptr);  // Compare buffer_B and buffer_A
```

### Time Complexity Comparison

| Operation | Single Buffer (Copy) | Double Buffer (Swap) |
|-----------|---------------------|---------------------|
| Memory Copy | O(n) | O(1) |
| Pointer Swap | - | O(1) |
| Total Time | O(n) | O(1) |

Where n is the number of processes. For a system with 1000 processes, double buffering is 1000x faster!

## Implementation in libproc2

### Data Structure

```c
struct history_info {
    int    num_tasks;           // Current number of tasks (index)
    int    HHist_siz;          // Maximum size of history array
    
    // Double-buffered history data arrays
    HST_t *PHist_sav;          // "saved" - previous refresh data
    HST_t *PHist_new;          // "new" - current refresh data
    
    // Fixed two actual hash tables
    int    HHash_one[4096];    // Hash table 1
    int    HHash_two[4096];    // Hash table 2
    int    HHash_nul[4096];    // Empty hash table template (all -1)
    
    // Double-buffered hash table pointers
    int   *PHash_sav;          // Points to old hash table (one or two)
    int   *PHash_new;          // Points to new hash table (one or two)
};
```

### Detailed Double Buffering Flow

#### Initialization Phase (pids_config_history)

```c
static void pids_config_history (struct pids_info *info)
{
    // 1. Create empty hash table template (all buckets initialized to -1)
    for (i = 0; i < 4096; i++)
        info->hist->HHash_nul[i] = -1;
    
    // 2. Initialize both actual hash tables
    memcpy(info->hist->HHash_one, info->hist->HHash_nul, sizeof(HHash_nul));
    memcpy(info->hist->HHash_two, info->hist->HHash_nul, sizeof(HHash_nul));
    
    // 3. Set initial pointer state
    info->hist->PHash_sav = info->hist->HHash_one;  // sav -> hash table 1
    info->hist->PHash_new = info->hist->HHash_two;  // new -> hash table 2
}
```

**Initial State Diagram**:

```
PHist_sav → [empty array, allocated later]
PHist_new → [empty array, allocated later]

PHash_sav → HHash_one[4096] = {-1, -1, -1, ...}
PHash_new → HHash_two[4096] = {-1, -1, -1, ...}
```

#### First Refresh (N=1)

```c
// Called at the start of pids_stacks_fetch
pids_toggle_history(info);  // No-op this time (first run)

// Collect process information
while (read_process(...)) {
    pids_make_hist(info, process);
    // Save process data to PHist_new
    // Save PID hash to PHash_new
}
```

**After First Refresh**:

```
PHist_sav → [empty array]
PHist_new → [Process1 data, Process2 data, ..., ProcessN data]

PHash_sav → HHash_one[4096] = {-1, -1, -1, ...}  (still empty)
PHash_new → HHash_two[4096] = {0, 3, -1, 5, ...}  (has data)
```

#### Second Refresh (N=2) - Call pids_toggle_history at Start

```c
static inline void pids_toggle_history (struct pids_info *info)
{
    void *v;
    
    // Swap history data pointers
    v = Hr(PHist_sav);
    Hr(PHist_sav) = Hr(PHist_new);  // sav now points to 1st refresh data
    Hr(PHist_new) = v;              // new now points to empty array
    
    // Swap hash table pointers
    v = Hr(PHash_sav);
    Hr(PHash_sav) = Hr(PHash_new);  // sav now points to 1st refresh hash
    Hr(PHash_new) = v;              // new now points to empty hash
    
    // Clear new hash table (prepare for new data)
    memcpy(Hr(PHash_new), Hr(HHash_nul), sizeof(Hr(HHash_nul)));
    
    // Reset task counter
    info->hist->num_tasks = 0;
}
```

**State After Swap**:

```
Before swap:
  PHist_sav → [empty]
  PHist_new → [1st refresh data]
  PHash_sav → HHash_one (empty)
  PHash_new → HHash_two (1st refresh hash)

After swap:
  PHist_sav → [1st refresh data]  ← Now can be used to find old values!
  PHist_new → [empty]             ← Ready to receive 2nd refresh data
  PHash_sav → HHash_two (1st refresh hash)  ← Used to find old values
  PHash_new → HHash_one (cleared)           ← Ready for 2nd refresh hash
```

#### Second Refresh - Collect Data

```c
// Collect process information
while (read_process(...)) {
    pids_make_hist(info, process);
    
    // 1. Save current process data to PHist_new
    PHist_new[slot].pid  = process->tid;
    PHist_new[slot].tics = process->utime + process->stime;
    PHist_new[slot].maj  = process->maj_flt;
    PHist_new[slot].min  = process->min_flt;
    
    // 2. Insert new data into new hash table
    pids_histput(info, slot);  // Uses PHash_new
    
    // 3. Find previous data from old hash table
    h = pids_histget(info, process->tid);  // Uses PHash_sav
    
    // 4. Calculate deltas
    if (h) {
        process->pcpu = current_tics - h->tics;           // CPU delta
        process->maj_delta = process->maj_flt - h->maj;   // Major fault delta
        process->min_delta = process->min_flt - h->min;   // Minor fault delta
    }
}
```

**After Second Refresh**:

```
PHist_sav → [1st refresh data]  (read-only, for comparison)
PHist_new → [2nd refresh data]  (newly written)

PHash_sav → HHash_two (1st refresh hash)  (read-only)
PHash_new → HHash_one (2nd refresh hash)  (newly written)
```

#### Third Refresh - Swap Again

```c
pids_toggle_history(info);

// After swap:
PHist_sav → [2nd refresh data]  ← Previous becomes "old data"
PHist_new → [1st refresh data]  ← Ready to be overwritten

PHash_sav → HHash_one (2nd refresh hash)
PHash_new → HHash_two (cleared, ready for 3rd refresh)
```

### Complete Timeline Diagram

```
Timeline:
─────────────────────────────────────────────────────────>
  T0          T1          T2          T3          T4
  Init      Refresh1    Refresh2    Refresh3    Refresh4

Data Flow:

T0: 
  sav → [empty]
  new → [empty]

T1 (collect 1st batch):
  sav → [empty]
  new → [data1]

T2 before (toggle):
  sav → [data1]  ← pointer swap
  new → [empty]  ← pointer swap

T2 (collect 2nd batch):
  sav → [data1]  ← read-only, for comparison
  new → [data2]  ← newly written

T3 before (toggle):
  sav → [data2]  ← pointer swap
  new → [data1]  ← pointer swap (will be overwritten)

T3 (collect 3rd batch):
  sav → [data2]  ← read-only, for comparison
  new → [data3]  ← overwrites original data1

T4 before (toggle):
  sav → [data3]  ← pointer swap
  new → [data2]  ← pointer swap (will be overwritten)

...repeating cycle...
```

## Key Features

### 1. Zero-Copy

```c
// ❌ Wrong: Must copy thousands of structures
memcpy(old_data, new_data, sizeof(HST_t) * num_processes);

// ✅ Correct: Only swap two pointers
void *temp = old_ptr;
old_ptr = new_ptr;
new_ptr = temp;
```

**Performance Difference**:
- Copy 1000 process data (32 bytes each) = 32 KB data copy
- Swap pointers = Only 16 bytes (two 8-byte pointers)
- **Speed improvement**: ~2000x faster

### 2. Memory Reuse

Two buffers A and B are used alternately, never freed:

```
Refresh 1: Write to A
Refresh 2: Write to B, read from A
Refresh 3: Write to A, read from B  ← Reuse A
Refresh 4: Write to B, read from A  ← Reuse B
```

**Advantages**:
- Avoid frequent malloc/free
- Reduce memory fragmentation
- Stable memory usage

### 3. Concurrency-Friendly

In a multi-threaded environment (though currently single-threaded):

```c
// Thread 1: Read old data
read_thread() {
    data = PHist_sav[index];  // Read old data
}

// Thread 2: Write new data
write_thread() {
    PHist_new[index] = data;  // Write new data
}

// No conflict! Read and write are on different buffers
```

### 4. Hash Tables Also Double-Buffered

Not only are data arrays double-buffered, hash tables are too:

```c
// When looking up old data, use old hash table
pids_histget(info, pid) {
    int V = PHash_sav[hash(pid)];  // Use old hash table
    while (V != -1) {
        if (PHist_sav[V].pid == pid)
            return &PHist_sav[V];  // Return old data
        V = PHist_sav[V].lnk;
    }
}

// When inserting new data, use new hash table
pids_histput(info, slot) {
    int V = hash(PHist_new[slot].pid);
    PHist_new[slot].lnk = PHash_new[V];  // Use new hash table
    PHash_new[V] = slot;
}
```

**Why do hash tables also need double buffering?**
- Old hash table indexes old data array
- New hash table indexes new data array
- Both are independent, no interference

## Practical Example

### Calculating CPU Usage

Assume two refreshes, 3 seconds apart:

**First Refresh (T=0 seconds)**:
```
Process 1234:
  utime = 100 tics
  stime = 50 tics
  total = 150 tics

Save to PHist_new[0]:
  pid  = 1234
  tics = 150
```

**Second Refresh (T=3 seconds)**:

1. Swap pointers first (pids_toggle_history):
   ```
   PHist_sav now points to first refresh data
   PHist_new ready to receive second refresh data
   ```

2. Read current values:
   ```
   Process 1234:
     utime = 130 tics
     stime = 60 tics
     total = 190 tics
   ```

3. Save to PHist_new[0]:
   ```
   pid  = 1234
   tics = 190
   ```

4. Find old value from PHist_sav and calculate delta:
   ```c
   h = pids_histget(info, 1234);  // Find PHist_sav[0]
   tics_delta = 190 - 150 = 40 tics
   process->pcpu = 40;  // Save delta
   ```

5. Calculate CPU usage:
   ```
   CPU% = (40 tics / (3 seconds * 100 HZ)) * 100%
        = (40 / 300) * 100%
        = 13.3%
   ```

### Handling Process Termination

**Problem**: What if a process exits between two refreshes?

**Solution**: pids_histget returns NULL

```c
h = pids_histget(info, pid);
if (h) {
    // Process existed in last refresh, calculate delta
    tics_delta = current_tics - h->tics;
} else {
    // Process is new, use full tics
    tics_delta = current_tics;
}
process->pcpu = tics_delta;
```

## Performance Analysis

### Memory Overhead

Assuming a system with 1000 processes:

```
Single buffer approach:
  Historical data: 1000 * 32 bytes = 32 KB
  Copy per refresh: 32 KB
  
Double buffer approach:
  Historical data A: 1000 * 32 bytes = 32 KB
  Historical data B: 1000 * 32 bytes = 32 KB
  Hash table one: 4096 * 4 bytes = 16 KB
  Hash table two: 4096 * 4 bytes = 16 KB
  Total: 96 KB
  Copy per refresh: 16 KB (only copy empty hash table template)
```

**Conclusion**: 
- Uses 64 KB more memory (negligible on modern systems)
- Reduces copy from 32 KB to 16 KB
- Pointer swap operations are almost free (3 pointer assignments)

### Time Overhead

```
System with 1000 processes:

Single buffer:
  memcpy(32 KB) ≈ 8000 CPU cycles

Double buffer:
  Pointer swaps × 2 ≈ 6 CPU cycles
  memcpy(16 KB) ≈ 4000 CPU cycles
  Total: 4006 CPU cycles
  
Performance improvement: 8000 / 4006 ≈ 2x
```

## Common Misconceptions

### Misconception 1: "Double buffering requires twice the memory"

**Truth**: Yes, but it's worth it:
- Modern systems have plenty of memory
- Significant performance improvement (2-1000x)
- Avoids frequent memory allocation/deallocation

### Misconception 2: "Can use single buffer + temporary variable"

**Wrong Approach**:
```c
for (i = 0; i < num_procs; i++) {
    old_tics = history[i].tics;  // Read
    new_tics = read_current_tics();
    delta = new_tics - old_tics;
    history[i].tics = new_tics;  // Immediately overwrite
}
```

**Problems**: 
- Process order may change (PID 1234 might move from index 0 to index 5)
- Must search entire array to find matching PID
- Time complexity O(n²)

**Double buffer + Hash table**:
```c
// Lookup O(1)
old_data = hash_lookup(old_hash_table, pid);
// Calculate delta
delta = new_tics - old_data->tics;
// Save new data to new buffer
new_buffer[index] = new_data;
```

### Misconception 3: "Only graphics need double buffering"

**Truth**: Double buffering applies to any scenario requiring simultaneous access to old and new data:
- Graphics rendering (avoid flickering)
- Process monitoring (calculate deltas)
- Network statistics (calculate bandwidth)
- Log analysis (compare before/after)

## Summary

Double buffering is one of the key techniques for libproc2's high performance:

1. **Zero-Copy**: Avoids large data copying through pointer swaps
2. **Memory Stability**: Fixed memory usage, avoids fragmentation
3. **Simple Logic**: Read old, write new, no interference
4. **Excellent Performance**: O(1) swap operation, 2-1000x performance improvement

This design pattern is worth adopting in other scenarios requiring historical data comparison.
