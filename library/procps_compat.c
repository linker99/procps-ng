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

#include "procps_compat.h"
#include "pids.h"
#include "readproc.h"

/* Helper function to map old PROC_* flags to pids_item array */
int procps_compat_flags_to_items(unsigned flags, enum pids_item *items, int max_items)
{
    int count = 0;
    
    if (!items || max_items < 1)
        return 0;

    /* Basic items always included */
    if (count < max_items) items[count++] = PIDS_ID_PID;
    if (count < max_items) items[count++] = PIDS_ID_TID;
    if (count < max_items) items[count++] = PIDS_ID_PPID;
    if (count < max_items) items[count++] = PIDS_ID_TGID;
    if (count < max_items) items[count++] = PIDS_STATE;
    
    /* PROC_FILLSTAT items */
    if (flags & PROC_FILLSTAT) {
        if (count < max_items) items[count++] = PIDS_TICS_USER;
        if (count < max_items) items[count++] = PIDS_TICS_SYSTEM;
        if (count < max_items) items[count++] = PIDS_TICS_USER_C;
        if (count < max_items) items[count++] = PIDS_TICS_SYSTEM_C;
        if (count < max_items) items[count++] = PIDS_TICS_BEGAN;
        if (count < max_items) items[count++] = PIDS_TICS_BLKIO;
        if (count < max_items) items[count++] = PIDS_TICS_GUEST;
        if (count < max_items) items[count++] = PIDS_TICS_GUEST_C;
        if (count < max_items) items[count++] = PIDS_PRIORITY;
        if (count < max_items) items[count++] = PIDS_NICE;
        if (count < max_items) items[count++] = PIDS_NLWP;
        if (count < max_items) items[count++] = PIDS_TTY;
        if (count < max_items) items[count++] = PIDS_ID_PGRP;
        if (count < max_items) items[count++] = PIDS_ID_SESSION;
        if (count < max_items) items[count++] = PIDS_ID_TPGID;
        if (count < max_items) items[count++] = PIDS_PRIORITY_RT;
        if (count < max_items) items[count++] = PIDS_SCHED_CLASS;
        if (count < max_items) items[count++] = PIDS_PROCESSOR;
        if (count < max_items) items[count++] = PIDS_VSIZE_BYTES;
        if (count < max_items) items[count++] = PIDS_RSS;
        if (count < max_items) items[count++] = PIDS_RSS_RLIM;
        if (count < max_items) items[count++] = PIDS_FLAGS;
        if (count < max_items) items[count++] = PIDS_FLT_MIN;
        if (count < max_items) items[count++] = PIDS_FLT_MAJ;
        if (count < max_items) items[count++] = PIDS_FLT_MIN_C;
        if (count < max_items) items[count++] = PIDS_FLT_MAJ_C;
        if (count < max_items) items[count++] = PIDS_ADDR_CODE_START;
        if (count < max_items) items[count++] = PIDS_ADDR_CODE_END;
        if (count < max_items) items[count++] = PIDS_ADDR_STACK_START;
        if (count < max_items) items[count++] = PIDS_ADDR_CURR_ESP;
        if (count < max_items) items[count++] = PIDS_ADDR_CURR_EIP;
        if (count < max_items) items[count++] = PIDS_WCHAN_NAME;
        if (count < max_items) items[count++] = PIDS_EXIT_SIGNAL;
    }
    
    /* PROC_FILLMEM items */
    if (flags & PROC_FILLMEM) {
        if (count < max_items) items[count++] = PIDS_MEM_VIRT_PGS;
        if (count < max_items) items[count++] = PIDS_MEM_RES_PGS;
        if (count < max_items) items[count++] = PIDS_MEM_SHR_PGS;
        if (count < max_items) items[count++] = PIDS_MEM_CODE_PGS;
        if (count < max_items) items[count++] = PIDS_MEM_DATA_PGS;
        if (count < max_items) items[count++] = PIDS_MEM_VIRT;
        if (count < max_items) items[count++] = PIDS_MEM_RES;
        if (count < max_items) items[count++] = PIDS_MEM_SHR;
        if (count < max_items) items[count++] = PIDS_MEM_CODE;
        if (count < max_items) items[count++] = PIDS_MEM_DATA;
    }
    
    /* PROC_FILLSTATUS items */
    if (flags & PROC_FILLSTATUS) {
        if (count < max_items) items[count++] = PIDS_VM_SIZE;
        if (count < max_items) items[count++] = PIDS_VM_RSS;
        if (count < max_items) items[count++] = PIDS_VM_RSS_ANON;
        if (count < max_items) items[count++] = PIDS_VM_RSS_FILE;
        if (count < max_items) items[count++] = PIDS_VM_RSS_SHARED;
        if (count < max_items) items[count++] = PIDS_VM_DATA;
        if (count < max_items) items[count++] = PIDS_VM_STACK;
        if (count < max_items) items[count++] = PIDS_VM_SWAP;
        if (count < max_items) items[count++] = PIDS_VM_EXE;
        if (count < max_items) items[count++] = PIDS_VM_LIB;
        if (count < max_items) items[count++] = PIDS_VM_RSS_LOCKED;
        if (count < max_items) items[count++] = PIDS_SIGBLOCKED;
        if (count < max_items) items[count++] = PIDS_SIGCATCH;
        if (count < max_items) items[count++] = PIDS_SIGIGNORE;
        if (count < max_items) items[count++] = PIDS_SIGNALS;
        if (count < max_items) items[count++] = PIDS_SIGPENDING;
        if (count < max_items) items[count++] = PIDS_CAPS_PERMITTED;
        if (count < max_items) items[count++] = PIDS_ID_EUID;
        if (count < max_items) items[count++] = PIDS_ID_RUID;
        if (count < max_items) items[count++] = PIDS_ID_SUID;
        if (count < max_items) items[count++] = PIDS_ID_FUID;
        if (count < max_items) items[count++] = PIDS_ID_EGID;
        if (count < max_items) items[count++] = PIDS_ID_RGID;
        if (count < max_items) items[count++] = PIDS_ID_SGID;
        if (count < max_items) items[count++] = PIDS_ID_FGID;
    }
    
    /* PROC_FILLARG items */
    if (flags & PROC_FILLARG) {
        if (count < max_items) items[count++] = PIDS_CMDLINE;
        if (count < max_items) items[count++] = PIDS_CMDLINE_V;
    }
    
    /* PROC_FILLENV items */
    if (flags & PROC_FILLENV) {
        if (count < max_items) items[count++] = PIDS_ENVIRON;
        if (count < max_items) items[count++] = PIDS_ENVIRON_V;
    }
    
    /* PROC_FILLUSR items */
    if (flags & PROC_FILLUSR) {
        if (count < max_items) items[count++] = PIDS_ID_EUSER;
        if (count < max_items) items[count++] = PIDS_ID_RUSER;
        if (count < max_items) items[count++] = PIDS_ID_SUSER;
        if (count < max_items) items[count++] = PIDS_ID_FUSER;
    }
    
    /* PROC_FILLGRP items */
    if (flags & PROC_FILLGRP) {
        if (count < max_items) items[count++] = PIDS_ID_EGROUP;
        if (count < max_items) items[count++] = PIDS_ID_RGROUP;
        if (count < max_items) items[count++] = PIDS_ID_SGROUP;
        if (count < max_items) items[count++] = PIDS_ID_FGROUP;
    }
    
    /* PROC_FILLCGROUP items */
    if (flags & PROC_FILLCGROUP) {
        if (count < max_items) items[count++] = PIDS_CGROUP;
        if (count < max_items) items[count++] = PIDS_CGROUP_V;
        if (count < max_items) items[count++] = PIDS_CGNAME;
    }
    
    /* PROC_FILLOOM items */
    if (flags & PROC_FILLOOM) {
        if (count < max_items) items[count++] = PIDS_OOM_SCORE;
        if (count < max_items) items[count++] = PIDS_OOM_ADJ;
    }
    
    /* PROC_FILLNS items */
    if (flags & PROC_FILLNS) {
        if (count < max_items) items[count++] = PIDS_NS_IPC;
        if (count < max_items) items[count++] = PIDS_NS_MNT;
        if (count < max_items) items[count++] = PIDS_NS_NET;
        if (count < max_items) items[count++] = PIDS_NS_PID;
        if (count < max_items) items[count++] = PIDS_NS_USER;
        if (count < max_items) items[count++] = PIDS_NS_UTS;
    }
    
    /* PROC_FILLSYSTEMD items */
    if (flags & PROC_FILLSYSTEMD) {
        if (count < max_items) items[count++] = PIDS_SD_MACH;
        if (count < max_items) items[count++] = PIDS_SD_OUID;
        if (count < max_items) items[count++] = PIDS_SD_SEAT;
        if (count < max_items) items[count++] = PIDS_SD_SESS;
        if (count < max_items) items[count++] = PIDS_SD_SLICE;
        if (count < max_items) items[count++] = PIDS_SD_UNIT;
        if (count < max_items) items[count++] = PIDS_SD_UUNIT;
    }
    
    /* PROC_FILL_LXC items */
    if (flags & PROC_FILL_LXC) {
        if (count < max_items) items[count++] = PIDS_LXCNAME;
    }
    
    /* PROC_FILL_LUID items */
    if (flags & PROC_FILL_LUID) {
        if (count < max_items) items[count++] = PIDS_ID_LOGIN;
    }
    
    /* PROC_FILL_EXE items */
    if (flags & PROC_FILL_EXE) {
        if (count < max_items) items[count++] = PIDS_EXE;
    }
    
    /* PROC_FILLIO items */
    if (flags & PROC_FILLIO) {
        if (count < max_items) items[count++] = PIDS_IO_READ_BYTES;
        if (count < max_items) items[count++] = PIDS_IO_WRITE_BYTES;
        if (count < max_items) items[count++] = PIDS_IO_READ_CHARS;
        if (count < max_items) items[count++] = PIDS_IO_WRITE_CHARS;
        if (count < max_items) items[count++] = PIDS_IO_READ_OPS;
        if (count < max_items) items[count++] = PIDS_IO_WRITE_OPS;
        if (count < max_items) items[count++] = PIDS_IO_WRITE_CBYTES;
    }
    
    /* PROC_FILLSMAPS items */
    if (flags & PROC_FILLSMAPS) {
        if (count < max_items) items[count++] = PIDS_SMAP_RSS;
        if (count < max_items) items[count++] = PIDS_SMAP_PSS;
        if (count < max_items) items[count++] = PIDS_SMAP_PSS_ANON;
        if (count < max_items) items[count++] = PIDS_SMAP_PSS_FILE;
        if (count < max_items) items[count++] = PIDS_SMAP_PSS_SHMEM;
        if (count < max_items) items[count++] = PIDS_SMAP_SHR_CLEAN;
        if (count < max_items) items[count++] = PIDS_SMAP_SHR_DIRTY;
        if (count < max_items) items[count++] = PIDS_SMAP_PRV_CLEAN;
        if (count < max_items) items[count++] = PIDS_SMAP_PRV_DIRTY;
        if (count < max_items) items[count++] = PIDS_SMAP_REFERENCED;
        if (count < max_items) items[count++] = PIDS_SMAP_ANONYMOUS;
        if (count < max_items) items[count++] = PIDS_SMAP_LAZY_FREE;
        if (count < max_items) items[count++] = PIDS_SMAP_HUGE_ANON;
        if (count < max_items) items[count++] = PIDS_SMAP_HUGE_SHMEM;
        if (count < max_items) items[count++] = PIDS_SMAP_HUGE_FILE;
        if (count < max_items) items[count++] = PIDS_SMAP_HUGE_TLBSHR;
        if (count < max_items) items[count++] = PIDS_SMAP_HUGE_TLBPRV;
        if (count < max_items) items[count++] = PIDS_SMAP_SWAP;
        if (count < max_items) items[count++] = PIDS_SMAP_SWAP_PSS;
        if (count < max_items) items[count++] = PIDS_SMAP_LOCKED;
    }
    
    /* PROC_FILLAUTOGRP items */
    if (flags & PROC_FILLAUTOGRP) {
        if (count < max_items) items[count++] = PIDS_AUTOGRP_ID;
        if (count < max_items) items[count++] = PIDS_AUTOGRP_NICE;
    }
    
    /* PROC_FILL_SUPGRP items */
    if (flags & PROC_FILL_SUPGRP) {
        if (count < max_items) items[count++] = PIDS_SUPGIDS;
        if (count < max_items) items[count++] = PIDS_SUPGROUPS;
    }
    
    /* PROC_FILL_DOCKER items */
    if (flags & PROC_FILL_DOCKER) {
        if (count < max_items) items[count++] = PIDS_DOCKER_ID;
        if (count < max_items) items[count++] = PIDS_DOCKER_ID_64;
    }
    
    /* PROC_FILL_FDS items */
    if (flags & PROC_FILL_FDS) {
        if (count < max_items) items[count++] = PIDS_OPEN_FILES;
    }
    
    /* Always include CMD for convenience */
    if (count < max_items) items[count++] = PIDS_CMD;
    
    return count;
}

/* Helper to find an item in the items array and return its index */
static int find_item_index(enum pids_item *items, int num_items, enum pids_item item)
{
    for (int i = 0; i < num_items; i++) {
        if (items[i] == item)
            return i;
    }
    return -1;
}

/* Helper macro to safely copy string value from stack */
#define COPY_STR(item_enum, dest) do { \
    int idx = find_item_index(items, num_items, item_enum); \
    if (idx >= 0 && PIDS_VAL(idx, str, stack)) { \
        dest = strdup(PIDS_VAL(idx, str, stack)); \
    } else { \
        dest = NULL; \
    } \
} while(0)

/* Convert a pids_stack to proc_t structure */
int procps_compat_stack_to_proc_t(
    struct pids_stack *stack,
    proc_t *proc,
    enum pids_item *items,
    int num_items)
{
    int idx;
    
    if (!stack || !proc || !items)
        return -1;
    
    /* Initialize proc_t to zero */
    memset(proc, 0, sizeof(proc_t));
    
    /* Fill in basic process IDs */
    idx = find_item_index(items, num_items, PIDS_ID_TID);
    if (idx >= 0) proc->tid = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ID_PPID);
    if (idx >= 0) proc->ppid = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ID_TGID);
    if (idx >= 0) proc->tgid = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_STATE);
    if (idx >= 0) proc->state = PIDS_VAL(idx, s_ch, stack);
    
    /* Fill in time-related fields */
    idx = find_item_index(items, num_items, PIDS_TICS_USER);
    if (idx >= 0) proc->utime = PIDS_VAL(idx, ull_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_TICS_SYSTEM);
    if (idx >= 0) proc->stime = PIDS_VAL(idx, ull_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_TICS_USER_C);
    if (idx >= 0) proc->cutime = PIDS_VAL(idx, ull_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_TICS_SYSTEM_C);
    if (idx >= 0) proc->cstime = PIDS_VAL(idx, ull_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_TICS_BEGAN);
    if (idx >= 0) proc->start_time = PIDS_VAL(idx, ull_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_TICS_BLKIO);
    if (idx >= 0) proc->blkio_tics = PIDS_VAL(idx, ull_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_TICS_GUEST);
    if (idx >= 0) proc->gtime = PIDS_VAL(idx, ull_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_TICS_GUEST_C);
    if (idx >= 0) proc->cgtime = PIDS_VAL(idx, ull_int, stack);
    
    /* Fill in priority and scheduling */
    idx = find_item_index(items, num_items, PIDS_PRIORITY);
    if (idx >= 0) proc->priority = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_NICE);
    if (idx >= 0) proc->nice = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_PRIORITY_RT);
    if (idx >= 0) proc->rtprio = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SCHED_CLASS);
    if (idx >= 0) proc->sched = PIDS_VAL(idx, s_int, stack);
    
    /* Fill in memory fields */
    idx = find_item_index(items, num_items, PIDS_MEM_VIRT_PGS);
    if (idx >= 0) proc->size = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_MEM_RES_PGS);
    if (idx >= 0) proc->resident = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_MEM_SHR_PGS);
    if (idx >= 0) proc->share = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_MEM_CODE_PGS);
    if (idx >= 0) proc->trs = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_MEM_DATA_PGS);
    if (idx >= 0) proc->drs = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_VM_SIZE);
    if (idx >= 0) proc->vm_size = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_VM_RSS);
    if (idx >= 0) proc->vm_rss = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_VM_RSS_ANON);
    if (idx >= 0) proc->vm_rss_anon = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_VM_RSS_FILE);
    if (idx >= 0) proc->vm_rss_file = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_VM_RSS_SHARED);
    if (idx >= 0) proc->vm_rss_shared = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_VM_DATA);
    if (idx >= 0) proc->vm_data = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_VM_STACK);
    if (idx >= 0) proc->vm_stack = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_VM_SWAP);
    if (idx >= 0) proc->vm_swap = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_VM_EXE);
    if (idx >= 0) proc->vm_exe = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_VM_LIB);
    if (idx >= 0) proc->vm_lib = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_RSS);
    if (idx >= 0) proc->rss = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_RSS_RLIM);
    if (idx >= 0) proc->rss_rlim = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_VSIZE_BYTES);
    if (idx >= 0) proc->vsize = PIDS_VAL(idx, ul_int, stack);
    
    /* Fill in other fields */
    idx = find_item_index(items, num_items, PIDS_FLAGS);
    if (idx >= 0) proc->flags = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_FLT_MIN);
    if (idx >= 0) proc->min_flt = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_FLT_MAJ);
    if (idx >= 0) proc->maj_flt = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_FLT_MIN_C);
    if (idx >= 0) proc->cmin_flt = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_FLT_MAJ_C);
    if (idx >= 0) proc->cmaj_flt = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_NLWP);
    if (idx >= 0) proc->nlwp = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_TTY);
    if (idx >= 0) proc->tty = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ID_PGRP);
    if (idx >= 0) proc->pgrp = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ID_SESSION);
    if (idx >= 0) proc->session = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ID_TPGID);
    if (idx >= 0) proc->tpgid = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_EXIT_SIGNAL);
    if (idx >= 0) proc->exit_signal = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_PROCESSOR);
    if (idx >= 0) proc->processor = PIDS_VAL(idx, s_int, stack);
    
    /* Fill in address fields */
    idx = find_item_index(items, num_items, PIDS_ADDR_CODE_START);
    if (idx >= 0) proc->start_code = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ADDR_CODE_END);
    if (idx >= 0) proc->end_code = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ADDR_STACK_START);
    if (idx >= 0) proc->start_stack = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ADDR_CURR_ESP);
    if (idx >= 0) proc->kstk_esp = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ADDR_CURR_EIP);
    if (idx >= 0) proc->kstk_eip = PIDS_VAL(idx, ul_int, stack);
    
    /* Fill in UIDs and GIDs */
    idx = find_item_index(items, num_items, PIDS_ID_EUID);
    if (idx >= 0) proc->euid = PIDS_VAL(idx, u_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ID_EGID);
    if (idx >= 0) proc->egid = PIDS_VAL(idx, u_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ID_RUID);
    if (idx >= 0) proc->ruid = PIDS_VAL(idx, u_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ID_RGID);
    if (idx >= 0) proc->rgid = PIDS_VAL(idx, u_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ID_SUID);
    if (idx >= 0) proc->suid = PIDS_VAL(idx, u_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ID_SGID);
    if (idx >= 0) proc->sgid = PIDS_VAL(idx, u_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ID_FUID);
    if (idx >= 0) proc->fuid = PIDS_VAL(idx, u_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ID_FGID);
    if (idx >= 0) proc->fgid = PIDS_VAL(idx, u_int, stack);
    
    /* Fill in OOM fields */
    idx = find_item_index(items, num_items, PIDS_OOM_SCORE);
    if (idx >= 0) proc->oom_score = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_OOM_ADJ);
    if (idx >= 0) proc->oom_adj = PIDS_VAL(idx, s_int, stack);
    
    /* Fill in IO fields */
    idx = find_item_index(items, num_items, PIDS_IO_READ_CHARS);
    if (idx >= 0) proc->rchar = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_IO_WRITE_CHARS);
    if (idx >= 0) proc->wchar = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_IO_READ_OPS);
    if (idx >= 0) proc->syscr = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_IO_WRITE_OPS);
    if (idx >= 0) proc->syscw = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_IO_READ_BYTES);
    if (idx >= 0) proc->read_bytes = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_IO_WRITE_BYTES);
    if (idx >= 0) proc->write_bytes = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_IO_WRITE_CBYTES);
    if (idx >= 0) proc->cancelled_write_bytes = PIDS_VAL(idx, ul_int, stack);
    
    /* Fill in autogroup fields */
    idx = find_item_index(items, num_items, PIDS_AUTOGRP_ID);
    if (idx >= 0) proc->autogrp_id = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_AUTOGRP_NICE);
    if (idx >= 0) proc->autogrp_nice = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_ID_LOGIN);
    if (idx >= 0) proc->luid = PIDS_VAL(idx, s_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_OPEN_FILES);
    if (idx >= 0) proc->fds = PIDS_VAL(idx, s_int, stack);
    
    /* Fill in smaps fields */
    idx = find_item_index(items, num_items, PIDS_SMAP_RSS);
    if (idx >= 0) proc->smap_Rss = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_PSS);
    if (idx >= 0) proc->smap_Pss = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_PSS_ANON);
    if (idx >= 0) proc->smap_Pss_Anon = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_PSS_FILE);
    if (idx >= 0) proc->smap_Pss_File = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_PSS_SHMEM);
    if (idx >= 0) proc->smap_Pss_Shmem = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_SHR_CLEAN);
    if (idx >= 0) proc->smap_Shared_Clean = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_SHR_DIRTY);
    if (idx >= 0) proc->smap_Shared_Dirty = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_PRV_CLEAN);
    if (idx >= 0) proc->smap_Private_Clean = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_PRV_DIRTY);
    if (idx >= 0) proc->smap_Private_Dirty = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_REFERENCED);
    if (idx >= 0) proc->smap_Referenced = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_ANONYMOUS);
    if (idx >= 0) proc->smap_Anonymous = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_LAZY_FREE);
    if (idx >= 0) proc->smap_LazyFree = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_HUGE_ANON);
    if (idx >= 0) proc->smap_AnonHugePages = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_HUGE_SHMEM);
    if (idx >= 0) proc->smap_ShmemPmdMapped = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_HUGE_FILE);
    if (idx >= 0) proc->smap_FilePmdMapped = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_HUGE_TLBSHR);
    if (idx >= 0) proc->smap_Shared_Hugetlb = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_HUGE_TLBPRV);
    if (idx >= 0) proc->smap_Private_Hugetlb = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_SWAP);
    if (idx >= 0) proc->smap_Swap = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_SWAP_PSS);
    if (idx >= 0) proc->smap_SwapPss = PIDS_VAL(idx, ul_int, stack);
    
    idx = find_item_index(items, num_items, PIDS_SMAP_LOCKED);
    if (idx >= 0) proc->smap_Locked = PIDS_VAL(idx, ul_int, stack);
    
    /* Fill in string fields - need to duplicate strings */
    COPY_STR(PIDS_CMD, proc->cmd);
    COPY_STR(PIDS_CMDLINE, proc->cmdline);
    COPY_STR(PIDS_ENVIRON, proc->environ);
    COPY_STR(PIDS_CGROUP, proc->cgroup);
    COPY_STR(PIDS_CGNAME, proc->cgname);
    COPY_STR(PIDS_SUPGIDS, proc->supgid);
    COPY_STR(PIDS_SUPGROUPS, proc->supgrp);
    COPY_STR(PIDS_ID_EUSER, proc->euser);
    COPY_STR(PIDS_ID_RUSER, proc->ruser);
    COPY_STR(PIDS_ID_SUSER, proc->suser);
    COPY_STR(PIDS_ID_FUSER, proc->fuser);
    COPY_STR(PIDS_ID_EGROUP, proc->egroup);
    COPY_STR(PIDS_ID_RGROUP, proc->rgroup);
    COPY_STR(PIDS_ID_SGROUP, proc->sgroup);
    COPY_STR(PIDS_ID_FGROUP, proc->fgroup);
    COPY_STR(PIDS_SD_MACH, proc->sd_mach);
    COPY_STR(PIDS_SD_OUID, proc->sd_ouid);
    COPY_STR(PIDS_SD_SEAT, proc->sd_seat);
    COPY_STR(PIDS_SD_SESS, proc->sd_sess);
    COPY_STR(PIDS_SD_SLICE, proc->sd_slice);
    COPY_STR(PIDS_SD_UNIT, proc->sd_unit);
    COPY_STR(PIDS_SD_UUNIT, proc->sd_uunit);
    COPY_STR(PIDS_DOCKER_ID, proc->dockerid);
    COPY_STR(PIDS_DOCKER_ID_64, proc->dockerid_64);
    COPY_STR(PIDS_LXCNAME, proc->lxcname);
    COPY_STR(PIDS_EXE, proc->exe);
    
    /* Copy signal masks as strings if available */
    idx = find_item_index(items, num_items, PIDS_SIGNALS);
    if (idx >= 0 && PIDS_VAL(idx, str, stack)) {
        strncpy(proc->signal, PIDS_VAL(idx, str, stack), sizeof(proc->signal) - 1);
        proc->signal[sizeof(proc->signal) - 1] = '\0';
    }
    
    idx = find_item_index(items, num_items, PIDS_SIGBLOCKED);
    if (idx >= 0 && PIDS_VAL(idx, str, stack)) {
        strncpy(proc->blocked, PIDS_VAL(idx, str, stack), sizeof(proc->blocked) - 1);
        proc->blocked[sizeof(proc->blocked) - 1] = '\0';
    }
    
    idx = find_item_index(items, num_items, PIDS_SIGIGNORE);
    if (idx >= 0 && PIDS_VAL(idx, str, stack)) {
        strncpy(proc->sigignore, PIDS_VAL(idx, str, stack), sizeof(proc->sigignore) - 1);
        proc->sigignore[sizeof(proc->sigignore) - 1] = '\0';
    }
    
    idx = find_item_index(items, num_items, PIDS_SIGCATCH);
    if (idx >= 0 && PIDS_VAL(idx, str, stack)) {
        strncpy(proc->sigcatch, PIDS_VAL(idx, str, stack), sizeof(proc->sigcatch) - 1);
        proc->sigcatch[sizeof(proc->sigcatch) - 1] = '\0';
    }
    
    idx = find_item_index(items, num_items, PIDS_SIGPENDING);
    if (idx >= 0 && PIDS_VAL(idx, str, stack)) {
        strncpy(proc->_sigpnd, PIDS_VAL(idx, str, stack), sizeof(proc->_sigpnd) - 1);
        proc->_sigpnd[sizeof(proc->_sigpnd) - 1] = '\0';
    }
    
    idx = find_item_index(items, num_items, PIDS_CAPS_PERMITTED);
    if (idx >= 0 && PIDS_VAL(idx, str, stack)) {
        strncpy(proc->capprm, PIDS_VAL(idx, str, stack), sizeof(proc->capprm) - 1);
        proc->capprm[sizeof(proc->capprm) - 1] = '\0';
    }
    
    return 0;
}

/* Maximum number of pids_item enums we support in compatibility mode */
#define COMPAT_MAX_ITEMS 256

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
