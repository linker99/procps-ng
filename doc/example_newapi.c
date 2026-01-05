/*
 * example_newapi.c - Example program using the new pids API directly
 *
 * This example shows how to use the new pids API for better performance
 * and more flexible access to process information.
 *
 * Compile with:
 *   gcc -o example_newapi example_newapi.c -I../library/include -L../library/.libs -lproc2
 *
 * Or if installed system-wide:
 *   gcc -o example_newapi example_newapi.c -lproc2
 */

#include <stdio.h>
#include <stdlib.h>
#include "pids.h"

/* Define the process information items we want to retrieve */
enum rel_items {
    EU_PID,
    EU_USER,
    EU_PRIORITY,
    EU_MEM,
    EU_CMD
};

int main(int argc, char *argv[])
{
    struct pids_info *info = NULL;
    struct pids_fetch *fetch;
    int max_display = 10;
    
    /* Define which items we want to fetch */
    enum pids_item items[] = {
        PIDS_ID_PID,      /* EU_PID */
        PIDS_ID_EUSER,    /* EU_USER */
        PIDS_PRIORITY,    /* EU_PRIORITY */
        PIDS_VM_RSS,      /* EU_MEM */
        PIDS_CMD          /* EU_CMD */
    };
    int num_items = sizeof(items) / sizeof(items[0]);
    
    printf("=== Process Information Demo (Using New API) ===\n\n");
    
    /* Create pids information context */
    if (procps_pids_new(&info, items, num_items) < 0) {
        fprintf(stderr, "Error: Failed to create pids info\n");
        return 1;
    }
    
    /* Fetch all processes at once */
    fetch = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
    if (!fetch) {
        fprintf(stderr, "Error: Failed to fetch process information\n");
        procps_pids_unref(&info);
        return 1;
    }
    
    printf("%-8s %-10s %-8s %-8s %-20s\n",
           "PID", "USER", "PRIORITY", "MEM(KB)", "COMMAND");
    printf("------------------------------------------------------------------------\n");
    
    /* Iterate through all processes */
    for (int i = 0; i < fetch->counts->total; i++) {
        struct pids_stack *stack = fetch->stacks[i];
        
        /* Access values using the PIDS_VAL macro and relative enum */
        printf("%-8d %-10s %-8d %-8lu %-20s\n",
               PIDS_VAL(EU_PID, s_int, stack),
               PIDS_VAL(EU_USER, str, stack),
               PIDS_VAL(EU_PRIORITY, s_int, stack),
               PIDS_VAL(EU_MEM, ul_int, stack),
               PIDS_VAL(EU_CMD, str, stack));
        
        /* Limit output for demonstration */
        if (i >= max_display - 1 && argc < 2) {
            printf("... (showing first %d processes, use any argument to show all)\n", max_display);
            break;
        }
    }
    
    printf("\nTotal processes: %d\n", fetch->counts->total);
    printf("Running: %d, Sleeping: %d, Stopped: %d, Zombie: %d\n",
           fetch->counts->running,
           fetch->counts->sleeping,
           fetch->counts->stopped,
           fetch->counts->zombied);
    
    /* Clean up */
    procps_pids_unref(&info);
    
    return 0;
}
