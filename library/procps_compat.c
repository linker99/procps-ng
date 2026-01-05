/*
 * procps_compat.c - Compatibility layer implementation
 *
 * Copyright © 2025 procps-ng project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>

#include "procps_compat.h"
#include "pids.h"
#include "readproc.h"

/* Maximum number of pids_item enums we support in compatibility mode */
#define COMPAT_MAX_ITEMS 256

/*
 * NOTE: This compatibility layer now uses the library's native
 * procps_pids_stack_to_proc() function for conversion, which is
 * exported as part of the library API. This provides a cleaner
 * and more maintainable solution than the previous data-driven
 * approach, while keeping the same functionality.
 */

/* Mapping structure for flag to items conversion */
struct flag_item_map {
    unsigned flag;
    enum pids_item item;
};

/* Static mapping table for PROC_* flags to pids_item enums */
static const struct flag_item_map flag_mappings[] = {
    /* PROC_FILLSTAT items */
    {PROC_FILLSTAT, PIDS_TICS_USER},
    {PROC_FILLSTAT, PIDS_TICS_SYSTEM},
    {PROC_FILLSTAT, PIDS_TICS_USER_C},
    {PROC_FILLSTAT, PIDS_TICS_SYSTEM_C},
    {PROC_FILLSTAT, PIDS_TICS_BEGAN},
    {PROC_FILLSTAT, PIDS_TICS_BLKIO},
    {PROC_FILLSTAT, PIDS_TICS_GUEST},
    {PROC_FILLSTAT, PIDS_TICS_GUEST_C},
    {PROC_FILLSTAT, PIDS_PRIORITY},
    {PROC_FILLSTAT, PIDS_NICE},
    {PROC_FILLSTAT, PIDS_NLWP},
    {PROC_FILLSTAT, PIDS_TTY},
    {PROC_FILLSTAT, PIDS_ID_PGRP},
    {PROC_FILLSTAT, PIDS_ID_SESSION},
    {PROC_FILLSTAT, PIDS_ID_TPGID},
    {PROC_FILLSTAT, PIDS_PRIORITY_RT},
    {PROC_FILLSTAT, PIDS_SCHED_CLASS},
    {PROC_FILLSTAT, PIDS_PROCESSOR},
    {PROC_FILLSTAT, PIDS_VSIZE_BYTES},
    {PROC_FILLSTAT, PIDS_RSS},
    {PROC_FILLSTAT, PIDS_RSS_RLIM},
    {PROC_FILLSTAT, PIDS_FLAGS},
    {PROC_FILLSTAT, PIDS_FLT_MIN},
    {PROC_FILLSTAT, PIDS_FLT_MAJ},
    {PROC_FILLSTAT, PIDS_FLT_MIN_C},
    {PROC_FILLSTAT, PIDS_FLT_MAJ_C},
    {PROC_FILLSTAT, PIDS_ADDR_CODE_START},
    {PROC_FILLSTAT, PIDS_ADDR_CODE_END},
    {PROC_FILLSTAT, PIDS_ADDR_STACK_START},
    {PROC_FILLSTAT, PIDS_ADDR_CURR_ESP},
    {PROC_FILLSTAT, PIDS_ADDR_CURR_EIP},
    {PROC_FILLSTAT, PIDS_WCHAN_NAME},
    {PROC_FILLSTAT, PIDS_EXIT_SIGNAL},
    
    /* PROC_FILLMEM items */
    {PROC_FILLMEM, PIDS_MEM_VIRT_PGS},
    {PROC_FILLMEM, PIDS_MEM_RES_PGS},
    {PROC_FILLMEM, PIDS_MEM_SHR_PGS},
    {PROC_FILLMEM, PIDS_MEM_CODE_PGS},
    {PROC_FILLMEM, PIDS_MEM_DATA_PGS},
    {PROC_FILLMEM, PIDS_MEM_VIRT},
    {PROC_FILLMEM, PIDS_MEM_RES},
    {PROC_FILLMEM, PIDS_MEM_SHR},
    {PROC_FILLMEM, PIDS_MEM_CODE},
    {PROC_FILLMEM, PIDS_MEM_DATA},
    
    /* PROC_FILLSTATUS items */
    {PROC_FILLSTATUS, PIDS_VM_SIZE},
    {PROC_FILLSTATUS, PIDS_VM_RSS},
    {PROC_FILLSTATUS, PIDS_VM_RSS_ANON},
    {PROC_FILLSTATUS, PIDS_VM_RSS_FILE},
    {PROC_FILLSTATUS, PIDS_VM_RSS_SHARED},
    {PROC_FILLSTATUS, PIDS_VM_DATA},
    {PROC_FILLSTATUS, PIDS_VM_STACK},
    {PROC_FILLSTATUS, PIDS_VM_SWAP},
    {PROC_FILLSTATUS, PIDS_VM_EXE},
    {PROC_FILLSTATUS, PIDS_VM_LIB},
    {PROC_FILLSTATUS, PIDS_VM_RSS_LOCKED},
    {PROC_FILLSTATUS, PIDS_SIGBLOCKED},
    {PROC_FILLSTATUS, PIDS_SIGCATCH},
    {PROC_FILLSTATUS, PIDS_SIGIGNORE},
    {PROC_FILLSTATUS, PIDS_SIGNALS},
    {PROC_FILLSTATUS, PIDS_SIGPENDING},
    {PROC_FILLSTATUS, PIDS_CAPS_PERMITTED},
    {PROC_FILLSTATUS, PIDS_ID_EUID},
    {PROC_FILLSTATUS, PIDS_ID_RUID},
    {PROC_FILLSTATUS, PIDS_ID_SUID},
    {PROC_FILLSTATUS, PIDS_ID_FUID},
    {PROC_FILLSTATUS, PIDS_ID_EGID},
    {PROC_FILLSTATUS, PIDS_ID_RGID},
    {PROC_FILLSTATUS, PIDS_ID_SGID},
    {PROC_FILLSTATUS, PIDS_ID_FGID},
    
    /* PROC_FILLARG items */
    {PROC_FILLARG, PIDS_CMDLINE},
    {PROC_FILLARG, PIDS_CMDLINE_V},
    
    /* PROC_FILLENV items */
    {PROC_FILLENV, PIDS_ENVIRON},
    {PROC_FILLENV, PIDS_ENVIRON_V},
    
    /* PROC_FILLUSR items */
    {PROC_FILLUSR, PIDS_ID_EUSER},
    {PROC_FILLUSR, PIDS_ID_RUSER},
    {PROC_FILLUSR, PIDS_ID_SUSER},
    {PROC_FILLUSR, PIDS_ID_FUSER},
    
    /* PROC_FILLGRP items */
    {PROC_FILLGRP, PIDS_ID_EGROUP},
    {PROC_FILLGRP, PIDS_ID_RGROUP},
    {PROC_FILLGRP, PIDS_ID_SGROUP},
    {PROC_FILLGRP, PIDS_ID_FGROUP},
    
    /* PROC_FILLCGROUP items */
    {PROC_FILLCGROUP, PIDS_CGROUP},
    {PROC_FILLCGROUP, PIDS_CGROUP_V},
    {PROC_FILLCGROUP, PIDS_CGNAME},
    
    /* PROC_FILLOOM items */
    {PROC_FILLOOM, PIDS_OOM_SCORE},
    {PROC_FILLOOM, PIDS_OOM_ADJ},
    
    /* PROC_FILLNS items */
    {PROC_FILLNS, PIDS_NS_IPC},
    {PROC_FILLNS, PIDS_NS_MNT},
    {PROC_FILLNS, PIDS_NS_NET},
    {PROC_FILLNS, PIDS_NS_PID},
    {PROC_FILLNS, PIDS_NS_USER},
    {PROC_FILLNS, PIDS_NS_UTS},
    
    /* PROC_FILLSYSTEMD items */
    {PROC_FILLSYSTEMD, PIDS_SD_MACH},
    {PROC_FILLSYSTEMD, PIDS_SD_OUID},
    {PROC_FILLSYSTEMD, PIDS_SD_SEAT},
    {PROC_FILLSYSTEMD, PIDS_SD_SESS},
    {PROC_FILLSYSTEMD, PIDS_SD_SLICE},
    {PROC_FILLSYSTEMD, PIDS_SD_UNIT},
    {PROC_FILLSYSTEMD, PIDS_SD_UUNIT},
    
    /* PROC_FILL_LXC items */
    {PROC_FILL_LXC, PIDS_LXCNAME},
    
    /* PROC_FILL_LUID items */
    {PROC_FILL_LUID, PIDS_ID_LOGIN},
    
    /* PROC_FILL_EXE items */
    {PROC_FILL_EXE, PIDS_EXE},
    
    /* PROC_FILLIO items */
    {PROC_FILLIO, PIDS_IO_READ_BYTES},
    {PROC_FILLIO, PIDS_IO_WRITE_BYTES},
    {PROC_FILLIO, PIDS_IO_READ_CHARS},
    {PROC_FILLIO, PIDS_IO_WRITE_CHARS},
    {PROC_FILLIO, PIDS_IO_READ_OPS},
    {PROC_FILLIO, PIDS_IO_WRITE_OPS},
    {PROC_FILLIO, PIDS_IO_WRITE_CBYTES},
    
    /* PROC_FILLSMAPS items */
    {PROC_FILLSMAPS, PIDS_SMAP_RSS},
    {PROC_FILLSMAPS, PIDS_SMAP_PSS},
    {PROC_FILLSMAPS, PIDS_SMAP_PSS_ANON},
    {PROC_FILLSMAPS, PIDS_SMAP_PSS_FILE},
    {PROC_FILLSMAPS, PIDS_SMAP_PSS_SHMEM},
    {PROC_FILLSMAPS, PIDS_SMAP_SHR_CLEAN},
    {PROC_FILLSMAPS, PIDS_SMAP_SHR_DIRTY},
    {PROC_FILLSMAPS, PIDS_SMAP_PRV_CLEAN},
    {PROC_FILLSMAPS, PIDS_SMAP_PRV_DIRTY},
    {PROC_FILLSMAPS, PIDS_SMAP_REFERENCED},
    {PROC_FILLSMAPS, PIDS_SMAP_ANONYMOUS},
    {PROC_FILLSMAPS, PIDS_SMAP_LAZY_FREE},
    {PROC_FILLSMAPS, PIDS_SMAP_HUGE_ANON},
    {PROC_FILLSMAPS, PIDS_SMAP_HUGE_SHMEM},
    {PROC_FILLSMAPS, PIDS_SMAP_HUGE_FILE},
    {PROC_FILLSMAPS, PIDS_SMAP_HUGE_TLBSHR},
    {PROC_FILLSMAPS, PIDS_SMAP_HUGE_TLBPRV},
    {PROC_FILLSMAPS, PIDS_SMAP_SWAP},
    {PROC_FILLSMAPS, PIDS_SMAP_SWAP_PSS},
    {PROC_FILLSMAPS, PIDS_SMAP_LOCKED},
    
    /* PROC_FILLAUTOGRP items */
    {PROC_FILLAUTOGRP, PIDS_AUTOGRP_ID},
    {PROC_FILLAUTOGRP, PIDS_AUTOGRP_NICE},
    
    /* PROC_FILL_SUPGRP items */
    {PROC_FILL_SUPGRP, PIDS_SUPGIDS},
    {PROC_FILL_SUPGRP, PIDS_SUPGROUPS},
    
    /* PROC_FILL_DOCKER items */
    {PROC_FILL_DOCKER, PIDS_DOCKER_ID},
    {PROC_FILL_DOCKER, PIDS_DOCKER_ID_64},
    
    /* PROC_FILL_FDS items */
    {PROC_FILL_FDS, PIDS_OPEN_FILES},
};

/* Helper function to map old PROC_* flags to pids_item array */
int procps_compat_flags_to_items(unsigned flags, enum pids_item *items, int max_items)
{
    int count = 0;
    size_t i;
    
    if (!items || max_items < 1)
        return 0;

    /* Basic items always included */
    if (count < max_items) items[count++] = PIDS_ID_PID;
    if (count < max_items) items[count++] = PIDS_ID_TID;
    if (count < max_items) items[count++] = PIDS_ID_PPID;
    if (count < max_items) items[count++] = PIDS_ID_TGID;
    if (count < max_items) items[count++] = PIDS_STATE;
    
    /* Add items based on flags using the mapping table */
    for (i = 0; i < sizeof(flag_mappings) / sizeof(flag_mappings[0]); i++) {
        if (count >= max_items)
            break;
        if (flags & flag_mappings[i].flag) {
            items[count++] = flag_mappings[i].item;
        }
    }
    
    /* Always include CMD for convenience */
    if (count < max_items) items[count++] = PIDS_CMD;
    
    return count;
}

/* Convert a pids_stack to proc_t structure */
int procps_compat_stack_to_proc_t(
    struct pids_stack *stack,
    proc_t *proc,
    enum pids_item *items,
    int num_items)
{
    /* Use the library's native conversion function */
    return procps_pids_stack_to_proc(stack, proc, items, num_items);
}

/* Initialize process table reading with compatibility mode */
procps_compat_proctab *procps_compat_openproc(unsigned flags)
{
    procps_compat_proctab *pt;
    enum pids_item items[COMPAT_MAX_ITEMS];
    int num_items;
    
    pt = calloc(1, sizeof(procps_compat_proctab));
    if (!pt)
        return NULL;
    
    pt->flags = flags;
    
    /* Convert flags to items array */
    num_items = procps_compat_flags_to_items(flags, items, COMPAT_MAX_ITEMS);
    if (num_items <= 0) {
        free(pt);
        return NULL;
    }
    
    /* Allocate and store the items array */
    pt->items = malloc(num_items * sizeof(enum pids_item));
    if (!pt->items) {
        free(pt);
        return NULL;
    }
    memcpy(pt->items, items, num_items * sizeof(enum pids_item));
    pt->num_items = num_items;
    
    /* Create the pids_info context */
    if (procps_pids_new(&pt->info, pt->items, pt->num_items) < 0) {
        free(pt->items);
        free(pt);
        return NULL;
    }
    
    /* Reap all processes */
    pt->fetch = procps_pids_reap(pt->info, PIDS_FETCH_TASKS_ONLY);
    if (!pt->fetch) {
        procps_pids_unref(&pt->info);
        free(pt->items);
        free(pt);
        return NULL;
    }
    
    pt->current_index = 0;
    
    return pt;
}

/* Read next process information in compatibility mode */
proc_t *procps_compat_readproc(procps_compat_proctab *pt)
{
    proc_t *proc;
    
    if (!pt || !pt->fetch)
        return NULL;
    
    /* Check if we've reached the end */
    if (pt->current_index >= pt->fetch->counts->total)
        return NULL;
    
    /* Allocate proc_t structure */
    proc = calloc(1, sizeof(proc_t));
    if (!proc)
        return NULL;
    
    /* Convert the current stack to proc_t */
    if (procps_compat_stack_to_proc_t(
            pt->fetch->stacks[pt->current_index],
            proc,
            pt->items,
            pt->num_items) < 0) {
        free(proc);
        return NULL;
    }
    
    pt->current_index++;
    
    return proc;
}

/* Free a proc_t structure allocated by compatibility layer */
void procps_compat_freeproc(proc_t *proc)
{
    if (!proc)
        return;
    
    /* Free all dynamically allocated strings */
    free(proc->cmd);
    free(proc->cmdline);
    free(proc->environ);
    free(proc->cgroup);
    free(proc->cgname);
    free(proc->supgid);
    free(proc->supgrp);
    free(proc->euser);
    free(proc->ruser);
    free(proc->suser);
    free(proc->fuser);
    free(proc->egroup);
    free(proc->rgroup);
    free(proc->sgroup);
    free(proc->fgroup);
    free(proc->sd_mach);
    free(proc->sd_ouid);
    free(proc->sd_seat);
    free(proc->sd_sess);
    free(proc->sd_slice);
    free(proc->sd_unit);
    free(proc->sd_uunit);
    free(proc->dockerid);
    free(proc->dockerid_64);
    free(proc->lxcname);
    free(proc->exe);
    
    /* Note: environ_v, cmdline_v, cgroup_v would need special handling
     * if they were filled by the compatibility layer. These vector fields
     * require parsing the string versions into arrays, which is complex
     * and rarely needed. Users needing these fields should:
     * 1. Use PIDS_ENVIRON_V, PIDS_CMDLINE_V, PIDS_CGROUP_V directly with new API
     * 2. Parse the string versions (environ, cmdline, cgroup) themselves
     * 3. Use the old readproc.c functions if they are available in their version
     */
    
    free(proc);
}

/* Close the compatibility context and free resources */
void procps_compat_closeproc(procps_compat_proctab *pt)
{
    if (!pt)
        return;
    
    if (pt->info)
        procps_pids_unref(&pt->info);
    
    free(pt->items);
    free(pt);
}
