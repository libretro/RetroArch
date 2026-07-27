#ifndef VFS_IMPLEMENTATION_NFS_H
#define VFS_IMPLEMENTATION_NFS_H

#include <stdint.h>
#include <boolean.h>
#include <vfs/vfs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct nfs_settings {
   const char *server_address;
   const char *export_path;
   const char *subdir;
   unsigned    timeout;
   unsigned    num_contexts;
};

typedef struct nfs_settings nfs_settings_t;

struct nfs_dirent {
   char name[256];
   int  type;     /* file vs directory */
   int64_t size;  /* file size */
};

typedef struct {
   struct nfs_context *ctx;
   struct nfsdir *dir;
} nfs_dir_handle;

bool nfs_init_cfg(const struct nfs_settings *new_cfg);

/* File operations (read-only) */
bool    retro_vfs_file_open_nfs(libretro_vfs_implementation_file *stream,
        const char *path, unsigned mode, unsigned hints);
int64_t retro_vfs_file_read_nfs(libretro_vfs_implementation_file *stream,
        void *s, uint64_t len);
int64_t retro_vfs_file_seek_nfs(libretro_vfs_implementation_file *stream,
        int64_t offset, int whence);
int64_t retro_vfs_file_tell_nfs(libretro_vfs_implementation_file *stream);
int     retro_vfs_file_close_nfs(libretro_vfs_implementation_file *stream);

/* Directory operations */
nfs_dir_handle* retro_vfs_opendir_nfs(const char *path, bool include_hidden);
struct nfs_dirent* retro_vfs_readdir_nfs(nfs_dir_handle* dh);
int                 retro_vfs_closedir_nfs(nfs_dir_handle* dh);

/* Stat */
int retro_vfs_stat_nfs(const char *path, int64_t *size);

/* Errors */
int retro_vfs_file_error_nfs(libretro_vfs_implementation_file *stream);

/* Context management */
void nfs_shutdown(void);

/* Mount with current settings; returns false on failure.
 * nfs_get_last_error() has a short reason after failure. */
bool nfs_probe_connection(void);
const char *nfs_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* VFS_IMPLEMENTATION_NFS_H */
