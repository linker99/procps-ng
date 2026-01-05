/*
 * procps_compat.h - Compatibility layer for migrating from proc_t to pids_stack
 *
 * Copyright © 2025 procps-ng project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef PROCPS_COMPAT_H
#define PROCPS_COMPAT_H

/*
 * This header provides a compatibility layer for applications that were
 * built using procps-ng v3.3.16 (or earlier) with the old proc_t API and
 * need to be ported to procps-ng v4.0.4 (or later) with the new pids_stack API.
 *
 * MIGRATION STRATEGY:
 * 
 * 1. For new code: Use the new pids API directly (include <libproc2/pids.h>)
 * 2. For legacy code migration: Use this compatibility layer as a bridge
 * 
 * This compatibility layer provides two approaches:
 * 
 * Approach A: Direct Conversion Helper Functions
 * -----------------------------------------------
 * Convert pids_stack to proc_t structure for easier integration:
 *   - procps_compat_stack_to_proc_t()
 *   
 * Approach B: Wrapper Functions (More Seamless)
 * ----------------------------------------------
 * Provides wrapper functions that mimic the old API:
 *   - procps_compat_openproc()
 *   - procps_compat_readproc()
 *   - procps_compat_closeproc()
 *
 * EXAMPLE USAGE:
 *
 * Old code (v3.3.16):
 * -------------------
 *   #include <proc/readproc.h>
 *   
 *   PROCTAB *pt = openproc(PROC_FILLMEM | PROC_FILLSTAT);
 *   proc_t *proc;
 *   while ((proc = readproc(pt, NULL))) {
 *       printf("PID: %d, CMD: %s\n", proc->tid, proc->cmd);
 *       freeproc(proc);
 *   }
 *   closeproc(pt);
 *
 * New code with compatibility layer (v4.0.4):
 * -------------------------------------------
 *   #include <libproc2/procps_compat.h>
 *   
 *   procps_compat_proctab *pt = procps_compat_openproc(PROC_FILLMEM | PROC_FILLSTAT);
 *   proc_t *proc;
 *   while ((proc = procps_compat_readproc(pt))) {
 *       printf("PID: %d, CMD: %s\n", proc->tid, proc->cmd);
 *       procps_compat_freeproc(proc);
 *   }
 *   procps_compat_closeproc(pt);
 *
 * Direct new API usage (recommended for new code):
 * -------------------------------------------------
 *   #include <libproc2/pids.h>
 *   
 *   enum pids_item items[] = { PIDS_ID_PID, PIDS_CMD };
 *   struct pids_info *info = NULL;
 *   procps_pids_new(&info, items, 2);
 *   struct pids_fetch *fetch = procps_pids_reap(info, PIDS_FETCH_TASKS_ONLY);
 *   for (int i = 0; i < fetch->counts->total; i++) {
 *       printf("PID: %d, CMD: %s\n",
 *              PIDS_VAL(0, s_int, fetch->stacks[i]),
 *              PIDS_VAL(1, str, fetch->stacks[i]));
 *   }
 *   procps_pids_unref(&info);
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "pids.h"
#include "readproc.h"

/* Compatibility context structure */
typedef struct procps_compat_proctab {
    struct pids_info *info;
    struct pids_fetch *fetch;
    int current_index;
    unsigned flags;
    enum pids_item *items;
    int num_items;
} procps_compat_proctab;

/*
 * Convert a pids_stack to proc_t structure
 * 
 * This function fills a proc_t structure with data from a pids_stack.
 * The proc_t structure should be allocated and initialized to zero by the caller.
 * 
 * IMPORTANT: Memory Management
 * - String fields (cmd, euser, etc.) are duplicated with strdup() and must be freed
 * - If you allocated the proc_t yourself, free string fields manually:
 *     free(proc.cmd); free(proc.euser); ... free(proc.exe);
 * - If you got the proc_t from procps_compat_readproc(), use procps_compat_freeproc()
 *   which handles all string fields automatically
 * 
 * Parameters:
 *   stack - The pids_stack to convert from
 *   proc  - The proc_t structure to fill (should be allocated by caller)
 *   items - Array of pids_item enums that were used to create the stack
 *   num_items - Number of items in the items array
 *
 * Returns:
 *   0 on success, -1 on error
 * 
 * Limitations:
 *   Vector fields (environ_v, cmdline_v, cgroup_v) are NOT populated.
 *   Use the string versions (environ, cmdline, cgroup) or use the new API directly.
 */
int procps_compat_stack_to_proc_t(
    struct pids_stack *stack,
    proc_t *proc,
    enum pids_item *items,
    int num_items);

/*
 * Initialize process table reading with compatibility mode
 *
 * This function provides a similar interface to the old openproc() function.
 * It creates a compatibility context that can be used with procps_compat_readproc().
 *
 * Parameters:
 *   flags - PROC_* flags indicating what information to retrieve
 *           (same as old API: PROC_FILLMEM, PROC_FILLSTAT, etc.)
 *
 * Returns:
 *   Pointer to compatibility context on success, NULL on error
 */
procps_compat_proctab *procps_compat_openproc(unsigned flags);

/*
 * Read next process information in compatibility mode
 *
 * This function provides a similar interface to the old readproc() function.
 * It returns a dynamically allocated proc_t structure that should be freed
 * with procps_compat_freeproc().
 *
 * Parameters:
 *   pt - The compatibility context returned by procps_compat_openproc()
 *
 * Returns:
 *   Pointer to proc_t structure on success, NULL when no more processes
 */
proc_t *procps_compat_readproc(procps_compat_proctab *pt);

/*
 * Free a proc_t structure allocated by compatibility layer
 *
 * This function frees all dynamically allocated strings within the proc_t
 * structure and then frees the proc_t itself. Use this ONLY for proc_t
 * structures returned by procps_compat_readproc().
 * 
 * If you used procps_compat_stack_to_proc_t() with your own proc_t structure,
 * you must manually free the string fields, NOT call this function.
 * 
 * Parameters:
 *   proc - The proc_t structure to free (must be from procps_compat_readproc)
 *
 * Limitations:
 *   Vector fields (environ_v, cmdline_v, cgroup_v) are not freed as they are
 *   not populated by the compatibility layer. If you need these fields, use
 *   the new API directly.
 */
void procps_compat_freeproc(proc_t *proc);

/*
 * Close the compatibility context and free resources
 *
 * Parameters:
 *   pt - The compatibility context to close
 */
void procps_compat_closeproc(procps_compat_proctab *pt);

/*
 * Helper function to map old PROC_* flags to pids_item array
 *
 * This is used internally but exposed for advanced users who want
 * to understand the mapping between old and new APIs.
 *
 * Parameters:
 *   flags - Old PROC_* flags
 *   items - Output array to store pids_item enums
 *   max_items - Maximum size of items array
 *
 * Returns:
 *   Number of items written to the items array
 */
int procps_compat_flags_to_items(
    unsigned flags,
    enum pids_item *items,
    int max_items);

#ifdef __cplusplus
}
#endif

#endif /* PROCPS_COMPAT_H */
