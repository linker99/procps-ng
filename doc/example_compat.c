/*
 * example_compat.c - Example program demonstrating the compatibility layer
 *
 * This example shows how to use the procps_compat layer to migrate
 * code from the old proc_t API to the new pids_stack API.
 *
 * Compile with:
 *   gcc -o example_compat example_compat.c -I../library/include -L../library/.libs -lproc2
 *
 * Or if installed system-wide:
 *   gcc -o example_compat example_compat.c -lproc2
 */

#include <stdio.h>
#include <stdlib.h>
#include <libproc2/procps_compat.h>

int main(int argc, char *argv[])
{
    procps_compat_proctab *pt;
    proc_t *proc;
    int count = 0;
    int max_display = 10;  /* Show first 10 processes */
    
    printf("=== Process Information Demo (Using Compatibility Layer) ===\n\n");
    
    /* Open process table with memory, stat, and user information */
    pt = procps_compat_openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLUSR);
    if (!pt) {
        fprintf(stderr, "Error: Failed to open process table\n");
        return 1;
    }
    
    printf("%-8s %-10s %-8s %-8s %-20s\n",
           "PID", "USER", "PRIORITY", "MEM(KB)", "COMMAND");
    printf("------------------------------------------------------------------------\n");
    
    /* Read processes one by one */
    while ((proc = procps_compat_readproc(pt))) {
        /* Display process information */
        printf("%-8d %-10s %-8d %-8lu %-20s\n",
               proc->tid,
               proc->euser ? proc->euser : "?",
               proc->priority,
               proc->vm_rss,
               proc->cmd ? proc->cmd : "?");
        
        /* Free the process structure */
        procps_compat_freeproc(proc);
        
        count++;
        
        /* Limit output for demonstration */
        if (count >= max_display && argc < 2) {
            printf("... (showing first %d processes, use any argument to show all)\n", max_display);
            break;
        }
    }
    
    /* Close the process table */
    procps_compat_closeproc(pt);
    
    printf("\nTotal processes shown: %d\n", count);
    
    return 0;
}
