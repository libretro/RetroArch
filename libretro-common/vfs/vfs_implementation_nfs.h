/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_implementation_nfs.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef VFS_IMPLEMENTATION_NFS_H
#define VFS_IMPLEMENTATION_NFS_H

#include <stdint.h>
#include <boolean.h>
#include <vfs/vfs.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* nfs:// backend over the built-in NFSv3 client (net/net_nfs3.c).
 * URL shape: nfs://server/export/path, or nfs://server/path with the
 * export from the settings; the subdir, when set, sits between the
 * export and the path. */

#define RETRO_NFS_DEFAULT_TIMEOUT      5
#define RETRO_NFS_DEFAULT_NUM_CONTEXTS 4

#define RETRO_NFS_DIRENT_FILE 0
#define RETRO_NFS_DIRENT_DIR  1

struct nfs_settings
{
   const char *server_address;
   const char *export_path;
   const char *subdir;
   unsigned    timeout;         /* seconds */
   unsigned    num_contexts;    /* connection pool size */
   unsigned    nfs_port;        /* 0: ask the portmapper */
   unsigned    mount_port;      /* 0: ask the portmapper */
   unsigned    version;         /* 0/3: NFSv3; 4: NFSv4.0 (no portmapper,
                                 * export is the server's pseudo path) */
};
typedef struct nfs_settings nfs_settings_t;

struct nfs_dirent
{
   char    name[256];
   int     type;                /* RETRO_NFS_DIRENT_* */
   int64_t size;
};

typedef struct
{
   void *ctx;                   /* struct rnfs_ctx, from the pool */
   void *dir;                   /* struct rnfs_dir */
   struct nfs_dirent ent;
} nfs_dir_handle;

bool nfs_init_cfg(const struct nfs_settings *new_cfg);

/* File operations */
bool retro_vfs_file_open_nfs(libretro_vfs_implementation_file *stream,
      const char *path, unsigned mode, unsigned hints);
int64_t retro_vfs_file_read_nfs(libretro_vfs_implementation_file *stream,
      void *s, uint64_t len);
int64_t retro_vfs_file_write_nfs(libretro_vfs_implementation_file *stream,
      const void *s, uint64_t len);
int64_t retro_vfs_file_seek_nfs(libretro_vfs_implementation_file *stream,
      int64_t offset, int whence);
int64_t retro_vfs_file_tell_nfs(libretro_vfs_implementation_file *stream);
int retro_vfs_file_close_nfs(libretro_vfs_implementation_file *stream);

/* Directory operations */
nfs_dir_handle *retro_vfs_opendir_nfs(const char *path, bool include_hidden);
struct nfs_dirent *retro_vfs_readdir_nfs(nfs_dir_handle *dh);
int retro_vfs_closedir_nfs(nfs_dir_handle *dh);

/* Stat / error / lifecycle */
int retro_vfs_stat_nfs(const char *path, int64_t *size);
int retro_vfs_file_error_nfs(libretro_vfs_implementation_file *stream);
void nfs_shutdown(void);
bool nfs_probe_connection(void);
const char *nfs_get_last_error(void);

RETRO_END_DECLS

#endif /* VFS_IMPLEMENTATION_NFS_H */
