/*
 * Migration Example: proc_t to pids.h
 * 
 * This file demonstrates side-by-side comparison of old proc_t API
 * and new pids.h API for common operations.
 *
 * Compile old API version (if old API is installed):
 *   gcc -DUSE_OLD_API migration_example.c -lproc -o migration_old
 *
 * Compile new API version:
 *   gcc migration_example.c -I../library/include -L../library/.libs -lproc2 -Wl,-rpath,../library/.libs -o migration_new
 *
 * Or if installed system-wide:
 *   gcc migration_example.c -lproc2 -o migration_new
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef USE_OLD_API
/******************************************************************************
 * OLD API (proc_t / readproc.h)
 ******************************************************************************/

#include <readproc.h>

void example1_list_all_processes(void) {
    PROCTAB *pt;
    proc_t *proc = NULL;
    
    printf("=== Example 1: List All Processes (OLD API) ===\n");
    
    pt = openproc(PROC_FILLSTAT | PROC_FILLSTATUS);
    if (!pt) {
        perror("openproc");
        return;
    }
    
    printf("%-8s %-20s %s\n", "PID", "USER", "COMMAND");
    printf("%-8s %-20s %s\n", "---", "----", "-------");
    
    while ((proc = readproc(pt, proc)) != NULL) {
        printf("%-8d %-20s %s\n", 
               proc->tid, 
               proc->euser ? proc->euser : "?",
               proc->cmd);
    }
    
    closeproc(pt);
}

void example2_show_memory(void) {
    PROCTAB *pt;
    proc_t *proc = NULL;
    
    printf("\n=== Example 2: Show Memory Usage (OLD API) ===\n");
    
    pt = openproc(PROC_FILLSTAT | PROC_FILLMEM | PROC_FILLSTATUS);
    if (!pt) {
        perror("openproc");
        return;
    }
    
    printf("%-8s %10s %10s %10s %s\n", "PID", "VIRT", "RES", "SHR", "CMD");
    printf("%-8s %10s %10s %10s %s\n", "---", "----", "---", "---", "---");
    
    while ((proc = readproc(pt, proc)) != NULL) {
        printf("%-8d %10lu %10lu %10lu %s\n",
               proc->tid,
               proc->vm_size,
               proc->vm_rss,
               proc->share * 4,  // Convert pages to KB
               proc->cmd);
    }
    
    closeproc(pt);
}

void example3_filter_by_pid(pid_t target_pid) {
    PROCTAB *pt;
    proc_t *proc = NULL;
    pid_t pids[2] = {target_pid, 0};
    
    printf("\n=== Example 3: Filter by PID %d (OLD API) ===\n", target_pid);
    
    pt = openproc(PROC_FILLSTAT | PROC_FILLMEM | PROC_PID, pids);
    if (!pt) {
        perror("openproc");
        return;
    }
    
    proc = readproc(pt, proc);
    if (proc) {
        printf("Found process:\n");
        printf("  PID:     %d\n", proc->tid);
        printf("  PPID:    %d\n", proc->ppid);
        printf("  Command: %s\n", proc->cmd);
        printf("  State:   %c\n", proc->state);
        printf("  VmSize:  %lu KB\n", proc->vm_size);
        printf("  VmRSS:   %lu KB\n", proc->vm_rss);
    } else {
        printf("Process %d not found\n", target_pid);
    }
    
    closeproc(pt);
}

void example4_cpu_time(void) {
    PROCTAB *pt;
    proc_t *proc = NULL;
    
    printf("\n=== Example 4: CPU Time (OLD API) ===\n");
    
    pt = openproc(PROC_FILLSTAT);
    if (!pt) {
        perror("openproc");
        return;
    }
    
    printf("%-8s %15s %15s %15s\n", "PID", "USER TIME", "SYS TIME", "TOTAL");
    printf("%-8s %15s %15s %15s\n", "---", "---------", "--------", "-----");
    
    int count = 0;
    while ((proc = readproc(pt, proc)) != NULL && count++ < 10) {
        unsigned long long total = proc->utime + proc->stime;
        printf("%-8d %15llu %15llu %15llu\n",
               proc->tid, proc->utime, proc->stime, total);
    }
    
    closeproc(pt);
}

void example5_with_threads(pid_t target_pid) {
    PROCTAB *pt;
    proc_t *proc = NULL;
    pid_t pids[2] = {target_pid, 0};
    
    printf("\n=== Example 5: Show Threads for PID %d (OLD API) ===\n", target_pid);
    
    pt = openproc(PROC_FILLSTAT | PROC_PID, pids);
    if (!pt) {
        perror("openproc");
        return;
    }
    
    printf("%-8s %-8s %-8s %s\n", "TID", "TGID", "STATE", "CMD");
    printf("%-8s %-8s %-8s %s\n", "---", "----", "-----", "---");
    
    while ((proc = readeither(pt, proc)) != NULL) {
        if (proc->tgid == target_pid) {
            printf("%-8d %-8d %-8c %s\n",
                   proc->tid, proc->tgid, proc->state, proc->cmd);
        }
    }
    
    closeproc(pt);
}

#else
/******************************************************************************
 * NEW API (pids.h)
 ******************************************************************************/

#include <pids.h>

// Helper enum for clearer code
enum {
    E_PID = 0,
    E_PPID,
    E_USER,
    E_CMD,
    E_STATE,
    E_VIRT,
    E_RES,
    E_SHR,
    E_UTIME,
    E_STIME,
    E_TIME_ALL,
    E_TID,
    E_TGID
};

void example1_list_all_processes(void) {
    struct pids_info *info = NULL;
    struct pids_fetch *fetched;
    
    printf("=== Example 1: List All Processes (NEW API) ===\n");
    
    enum pids_item items[] = {
        PIDS_ID_PID,
        PIDS_ID_EUSER,
        PIDS_CMD
    };
    int numitems = sizeof(items) / sizeof(items[0]);
    
    if (procps_pids_new(&info, items, numitems) < 0) {
        perror("procps_pids_new");
        return;
    }
    
    fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
    if (!fetched) {
        fprintf(stderr, "Failed to reap processes\n");
        procps_pids_unref(&info);
        return;
    }
    
    printf("%-8s %-20s %s\n", "PID", "USER", "COMMAND");
    printf("%-8s %-20s %s\n", "---", "----", "-------");
    
    for (int i = 0; i < fetched->counts->total; i++) {
        struct pids_stack *stack = fetched->stacks[i];
        printf("%-8d %-20s %s\n",
               PIDS_VAL(0, s_int, stack),   // PIDS_ID_PID
               PIDS_VAL(1, str, stack),     // PIDS_ID_EUSER
               PIDS_VAL(2, str, stack));    // PIDS_CMD
    }
    
    procps_pids_unref(&info);
}

void example2_show_memory(void) {
    struct pids_info *info = NULL;
    struct pids_fetch *fetched;
    
    printf("\n=== Example 2: Show Memory Usage (NEW API) ===\n");
    
    enum pids_item items[] = {
        PIDS_ID_PID,
        PIDS_MEM_VIRT,
        PIDS_MEM_RES,
        PIDS_MEM_SHR,
        PIDS_CMD
    };
    int numitems = sizeof(items) / sizeof(items[0]);
    
    if (procps_pids_new(&info, items, numitems) < 0) {
        perror("procps_pids_new");
        return;
    }
    
    fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
    if (!fetched) {
        fprintf(stderr, "Failed to reap processes\n");
        procps_pids_unref(&info);
        return;
    }
    
    printf("%-8s %10s %10s %10s %s\n", "PID", "VIRT", "RES", "SHR", "CMD");
    printf("%-8s %10s %10s %10s %s\n", "---", "----", "---", "---", "---");
    
    for (int i = 0; i < fetched->counts->total; i++) {
        struct pids_stack *stack = fetched->stacks[i];
        printf("%-8d %10lu %10lu %10lu %s\n",
               PIDS_VAL(0, s_int, stack),
               PIDS_VAL(1, ul_int, stack),
               PIDS_VAL(2, ul_int, stack),
               PIDS_VAL(3, ul_int, stack),
               PIDS_VAL(4, str, stack));
    }
    
    procps_pids_unref(&info);
}

void example3_filter_by_pid(pid_t target_pid) {
    struct pids_info *info = NULL;
    struct pids_fetch *fetched;
    unsigned int pids[1] = {(unsigned int)target_pid};
    
    printf("\n=== Example 3: Filter by PID %d (NEW API) ===\n", target_pid);
    
    enum pids_item items[] = {
        PIDS_ID_PID,
        PIDS_ID_PPID,
        PIDS_CMD,
        PIDS_STATE,
        PIDS_MEM_VIRT,
        PIDS_MEM_RES
    };
    int numitems = sizeof(items) / sizeof(items[0]);
    
    if (procps_pids_new(&info, items, numitems) < 0) {
        perror("procps_pids_new");
        return;
    }
    
    fetched = procps_pids_select(info, pids, 1, PIDS_SELECT_PID);
    if (!fetched || fetched->counts->total == 0) {
        printf("Process %d not found\n", target_pid);
        procps_pids_unref(&info);
        return;
    }
    
    struct pids_stack *stack = fetched->stacks[0];
    printf("Found process:\n");
    printf("  PID:     %d\n", PIDS_VAL(0, s_int, stack));
    printf("  PPID:    %d\n", PIDS_VAL(1, s_int, stack));
    printf("  Command: %s\n", PIDS_VAL(2, str, stack));
    printf("  State:   %c\n", PIDS_VAL(3, s_ch, stack));
    printf("  VmSize:  %lu KB\n", PIDS_VAL(4, ul_int, stack));
    printf("  VmRSS:   %lu KB\n", PIDS_VAL(5, ul_int, stack));
    
    procps_pids_unref(&info);
}

void example4_cpu_time(void) {
    struct pids_info *info = NULL;
    struct pids_fetch *fetched;
    
    printf("\n=== Example 4: CPU Time (NEW API) ===\n");
    
    enum pids_item items[] = {
        PIDS_ID_PID,
        PIDS_TICS_USER,
        PIDS_TICS_SYSTEM,
        PIDS_TICS_ALL
    };
    int numitems = sizeof(items) / sizeof(items[0]);
    
    if (procps_pids_new(&info, items, numitems) < 0) {
        perror("procps_pids_new");
        return;
    }
    
    fetched = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
    if (!fetched) {
        fprintf(stderr, "Failed to reap processes\n");
        procps_pids_unref(&info);
        return;
    }
    
    printf("%-8s %15s %15s %15s\n", "PID", "USER TIME", "SYS TIME", "TOTAL");
    printf("%-8s %15s %15s %15s\n", "---", "---------", "--------", "-----");
    
    int limit = (fetched->counts->total < 10) ? fetched->counts->total : 10;
    for (int i = 0; i < limit; i++) {
        struct pids_stack *stack = fetched->stacks[i];
        printf("%-8d %15llu %15llu %15llu\n",
               PIDS_VAL(0, s_int, stack),
               PIDS_VAL(1, ull_int, stack),
               PIDS_VAL(2, ull_int, stack),
               PIDS_VAL(3, ull_int, stack));
    }
    
    procps_pids_unref(&info);
}

void example5_with_threads(pid_t target_pid) {
    struct pids_info *info = NULL;
    struct pids_fetch *fetched;
    unsigned int pids[1] = {(unsigned int)target_pid};
    
    printf("\n=== Example 5: Show Threads for PID %d (NEW API) ===\n", target_pid);
    
    enum pids_item items[] = {
        PIDS_ID_TID,
        PIDS_ID_TGID,
        PIDS_STATE,
        PIDS_CMD
    };
    int numitems = sizeof(items) / sizeof(items[0]);
    
    if (procps_pids_new(&info, items, numitems) < 0) {
        perror("procps_pids_new");
        return;
    }
    
    fetched = procps_pids_select(info, pids, 1, PIDS_SELECT_PID_THREADS);
    if (!fetched) {
        fprintf(stderr, "Failed to select process\n");
        procps_pids_unref(&info);
        return;
    }
    
    printf("%-8s %-8s %-8s %s\n", "TID", "TGID", "STATE", "CMD");
    printf("%-8s %-8s %-8s %s\n", "---", "----", "-----", "---");
    
    for (int i = 0; i < fetched->counts->total; i++) {
        struct pids_stack *stack = fetched->stacks[i];
        printf("%-8d %-8d %-8c %s\n",
               PIDS_VAL(0, s_int, stack),
               PIDS_VAL(1, s_int, stack),
               PIDS_VAL(2, s_ch, stack),
               PIDS_VAL(3, str, stack));
    }
    
    procps_pids_unref(&info);
}

#endif

/******************************************************************************
 * Main Function
 ******************************************************************************/

int main(int argc, char *argv[]) {
    pid_t test_pid = getpid();  // Use our own PID for testing
    
#ifdef USE_OLD_API
    printf("Running with OLD API (proc_t / readproc.h)\n");
    printf("===========================================\n\n");
#else
    printf("Running with NEW API (pids.h)\n");
    printf("=============================\n\n");
#endif
    
    // Example 1: List all processes
    example1_list_all_processes();
    
    // Example 2: Show memory usage
    example2_show_memory();
    
    // Example 3: Filter by PID
    example3_filter_by_pid(test_pid);
    
    // Example 4: CPU time
    example4_cpu_time();
    
    // Example 5: With threads
    example5_with_threads(test_pid);
    
    printf("\n");
    return 0;
}
