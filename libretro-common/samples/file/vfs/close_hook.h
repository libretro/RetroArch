/* Force-included into vfs_close_result_test's private copy of
 * vfs_implementation.o.  The system header that declares close() is
 * pulled in first, so its prototype keeps the real name; every call in
 * the unit after that goes through vfs_close_result_close(), which the
 * test defines.  Nothing else includes this. */

#ifndef _VFS_CLOSE_HOOK_H
#define _VFS_CLOSE_HOOK_H

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

int vfs_close_result_close(int fd);

#define close vfs_close_result_close

#endif
