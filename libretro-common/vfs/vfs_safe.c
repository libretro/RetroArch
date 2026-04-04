/* Task #18827 - VFS Crash Fix */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vfs/vfs.h>

int vfs_safe_open(const char* path, int flags) {
    if (!path) {
        fprintf(stderr, "VFS Error: null path\n");
        return -1;
    }
    size_t len = strlen(path);
    if (len == 0 || len > VFS_MAX_PATH) {
        fprintf(stderr, "VFS Error: invalid path length\n");
        return -1;
    }
    char safe_path[VFS_MAX_PATH];
    strncpy(safe_path, path, VFS_MAX_PATH - 1);
    safe_path[VFS_MAX_PATH - 1] = '\0';
    return vfs_open(safe_path, flags);
}

void vfs_safe_close(vfs_handle_t* handle) {
    if (!handle) return;
    if (handle->fd >= 0) {
        vfs_close(handle->fd);
        handle->fd = -1;
    }
    free(handle);
}

int vfs_crash_fix_init(void) {
    vfs_register_safe_ops(&vfs_safe_open, &vfs_safe_close);
    return 0;
}