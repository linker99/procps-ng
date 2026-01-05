/*
 * test_compat.c - Test program for the compatibility layer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "procps_compat.h"

int main() {
    procps_compat_proctab *pt;
    proc_t *proc;
    int count = 0;
    int passed = 0;
    int failed = 0;
    
    printf("Testing procps compatibility layer...\n\n");
    
    /* Test 1: Open process table */
    printf("Test 1: Opening process table... ");
    pt = procps_compat_openproc(PROC_FILLSTAT | PROC_FILLUSR);
    if (pt) {
        printf("PASSED\n");
        passed++;
    } else {
        printf("FAILED\n");
        failed++;
        return 1;
    }
    
    /* Test 2: Read at least one process */
    printf("Test 2: Reading processes... ");
    proc = procps_compat_readproc(pt);
    if (proc) {
        printf("PASSED (PID: %d)\n", proc->tid);
        passed++;
        
        /* Test 3: Verify fields are populated */
        printf("Test 3: Verifying process fields... ");
        if (proc->tid > 0) {
            printf("PASSED\n");
            printf("  - PID: %d\n", proc->tid);
            printf("  - TGID: %d\n", proc->tgid);
            printf("  - Priority: %d\n", proc->priority);
            if (proc->euser) {
                printf("  - User: %s\n", proc->euser);
            }
            if (proc->cmd) {
                printf("  - Command: %s\n", proc->cmd);
            }
            passed++;
        } else {
            printf("FAILED (invalid PID)\n");
            failed++;
        }
        
        /* Test 4: Free process structure */
        printf("Test 4: Freeing process structure... ");
        procps_compat_freeproc(proc);
        printf("PASSED\n");
        passed++;
        
    } else {
        printf("FAILED\n");
        failed++;
    }
    
    /* Test 5: Read multiple processes */
    printf("Test 5: Reading multiple processes... ");
    count = 0;
    while ((proc = procps_compat_readproc(pt)) && count < 5) {
        count++;
        procps_compat_freeproc(proc);
    }
    if (count > 0) {
        printf("PASSED (read %d processes)\n", count);
        passed++;
    } else {
        printf("FAILED\n");
        failed++;
    }
    
    /* Test 6: Close process table */
    printf("Test 6: Closing process table... ");
    procps_compat_closeproc(pt);
    printf("PASSED\n");
    passed++;
    
    /* Summary */
    printf("\n========================================\n");
    printf("Test Results:\n");
    printf("  Passed: %d\n", passed);
    printf("  Failed: %d\n", failed);
    printf("========================================\n");
    
    return (failed == 0) ? 0 : 1;
}
