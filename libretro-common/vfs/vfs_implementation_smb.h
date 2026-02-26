#ifndef VFS_IMPLEMENTATION_SMB_H
#define VFS_IMPLEMENTATION_SMB_H

#include <stdint.h>
#include <vfs/vfs.h>

#ifdef __cplusplus
extern "C" {
#endif

/* System headers may lack SMB2_SEC_ defines but
 * will clash with deps/libsmb2 if provided here
 */
#define RETRO_SMB2_SEC_UNDEFINED 0
#define RETRO_SMB2_SEC_NTLMSSP 1
#define RETRO_SMB2_SEC_KRB5 2

struct smb_settings {
   const char *server_address;
   const char *share;
   const char *username;
   const char *password;
   const char *workgroup;
   unsigned    timeout;
   unsigned    num_contexts;
   unsigned    auth_mode;
   const char *subdir;
};

typedef struct smb_settings smb_settings_t;

struct smbc_dirent {
   char name[256];
   int  type;     /* file vs directory */
   int64_t size;  /* file size */
};

typedef struct {
   struct smb2_context *ctx;
   struct smb2dir *dir;
} smb_dir_handle;

bool smb_init_cfg(const struct smb_settings *new_cfg);

/* File operations */
bool    retro_vfs_file_open_smb(libretro_vfs_implementation_file *stream,
        const char *path, unsigned mode, unsigned hints);
int64_t retro_vfs_file_read_smb(libretro_vfs_implementation_file *stream,
        void *s, uint64_t len);
int64_t retro_vfs_file_write_smb(libretro_vfs_implementation_file *stream,
        const void *s, uint64_t len);
int64_t retro_vfs_file_seek_smb(libretro_vfs_implementation_file *stream,
        int64_t offset, int whence);
int64_t retro_vfs_file_tell_smb(libretro_vfs_implementation_file *stream);
int     retro_vfs_file_close_smb(libretro_vfs_implementation_file *stream);

/* Directory operations */
smb_dir_handle* retro_vfs_opendir_smb(const char *path, bool include_hidden);
struct smbc_dirent* retro_vfs_readdir_smb(smb_dir_handle* dh);
int                 retro_vfs_closedir_smb(smb_dir_handle* dh);

/* Stat */
int retro_vfs_stat_smb(const char *path, int32_t *size);

/* Errors */
int retro_vfs_file_error_smb(libretro_vfs_implementation_file *stream);

/* Context management */
void smb_shutdown();

#ifdef __cplusplus
}
#endif

#endif /* VFS_IMPLEMENTATION_SMB_H */
