# proc_t to pids.h Migration Documentation

This directory contains comprehensive documentation and examples for migrating code from the old `proc_t` structure API to the new `pids.h` API.

## Background

Around August 2015 (commit 77dc22b), procps-ng underwent a major API redesign. The old `readproc.h` API based on the `proc_t` structure was replaced with a new, more efficient `pids.h` API. This change improved:

- **Performance**: Only requested data is fetched
- **Flexibility**: Easy to add/remove data items
- **Maintainability**: Cleaner code structure
- **Memory efficiency**: Reduced memory footprint

## Files in this Directory

### 1. README_zh.md (Chinese)
**Chinese overview document** for developers who prefer Chinese documentation. Provides:
- Quick introduction to the migration
- Common usage examples
- Link to detailed English documentation

### 2. MIGRATION_GUIDE_proc_t_to_pids.md (English)
**Comprehensive migration guide** covering:
- Detailed API comparison
- Complete field mapping table (proc_t → PIDS_* enums)
- Multiple migration examples
- Best practices
- Common pitfalls
- Performance considerations

### 3. QUICK_REFERENCE_proc_t_to_pids.md (English)
**Quick reference sheet** for developers actively migrating code:
- Side-by-side API comparison
- Quick lookup tables
- Code templates
- Common mistakes to avoid
- Type reference for PIDS_VAL()

### 4. migration_example.c
**Working example code** demonstrating migration:
- Compiles with both old and new APIs (using `#ifdef USE_OLD_API`)
- 5 practical examples:
  1. List all processes
  2. Show memory usage
  3. Filter by specific PID
  4. Display CPU time
  5. Work with threads
- Can be compiled and run to see actual behavior

## Quick Start

### For New Development
If you're starting a new project, use the new API directly:

```c
#include <pids.h>

struct pids_info *info = NULL;
enum pids_item items[] = {PIDS_ID_PID, PIDS_CMD, PIDS_MEM_RES};

procps_pids_new(&info, items, 3);
// ... use the API ...
procps_pids_unref(&info);
```

### For Existing Code Migration
1. Read `README_zh.md` (Chinese) or `MIGRATION_GUIDE_proc_t_to_pids.md` (English)
2. Consult `QUICK_REFERENCE_proc_t_to_pids.md` for field mappings
3. Study `migration_example.c` for practical examples
4. Test your migrated code thoroughly

## Compiling the Example

### Prerequisites
Build the procps-ng library first:
```bash
./autogen.sh
./configure
make
```

### Compile the example with NEW API:
```bash
gcc -o migration_example_new migration_example.c \
    -I../library/include \
    -L../library/.libs \
    -lproc2 \
    -Wl,-rpath,../library/.libs
```

### Compile the example with OLD API (if available):
```bash
gcc -DUSE_OLD_API -o migration_example_old migration_example.c \
    -I../library/include \
    -L../library/.libs \
    -lproc \
    -Wl,-rpath,../library/.libs
```

### Run the example:
```bash
./migration_example_new
```

## Key Differences Summary

| Aspect | Old API (proc_t) | New API (pids.h) |
|--------|------------------|------------------|
| **Header** | `<readproc.h>` | `<pids.h>` |
| **Context** | `PROCTAB *` | `struct pids_info *` |
| **Data** | `proc_t *` | `struct pids_stack *` |
| **Init** | `openproc(flags)` | `procps_pids_new(&info, items, n)` |
| **Read** | `readproc(pt, p)` | `procps_pids_reap(info, type)` |
| **Access** | `proc->field` | `PIDS_VAL(idx, type, stack)` |
| **Cleanup** | `closeproc(pt)` | `procps_pids_unref(&info)` |
| **Library** | `-lproc` | `-lproc2` |

## Field Mapping Quick Reference

Common field mappings:

```c
proc_t field        →  PIDS_* enum        (type)
-------------------------------------------------
tid                 →  PIDS_ID_TID        (s_int)
ppid                →  PIDS_ID_PPID       (s_int)
state               →  PIDS_STATE         (s_ch)
cmd                 →  PIDS_CMD           (str)
vm_size             →  PIDS_MEM_VIRT      (ul_int)
vm_rss              →  PIDS_MEM_RES       (ul_int)
utime               →  PIDS_TICS_USER     (ull_int)
stime               →  PIDS_TICS_SYSTEM   (ull_int)
```

See the full migration guide for complete mappings.

## Getting Help

1. Start with the appropriate README (Chinese or English)
2. Consult the quick reference for specific field mappings
3. Study the working example code
4. Check the full migration guide for detailed explanations
5. Look at real-world usage in `src/ps/` and `src/top/`

## Contributing

If you find errors or have suggestions for improvement:
1. Check the procps-ng issue tracker
2. Submit corrections via pull request
3. Share your migration experiences

## License

These documentation files are part of the procps-ng project and follow the same license (GPL/LGPL).

---

**Note**: The documentation assumes you're working with procps-ng version 4.x or later. Earlier versions may have different API details.
