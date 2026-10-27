/* Force-included into vfs_prefetch_offthread_test's private copy of
 * vfs_implementation.o, which is built with
 * -DVFS_WIN32_PREFETCH_HOOK=vfs_prefetch_test_hook.  Nothing else
 * includes this, and no other object in the tree is built that way. */

#ifndef _PREFETCH_HOOK_H
#define _PREFETCH_HOOK_H

#include <stddef.h>

/* Stands in for PrefetchVirtualMemory on one range. */
int vfs_prefetch_test_hook(void *addr, size_t len);

#endif
