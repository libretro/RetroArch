#ifndef VFS_IMPLEMENTATION_SMB_H
#define VFS_IMPLEMENTATION_SMB_H

#include <stdint.h>
#include <vfs/vfs.h>

#ifdef __cplusplus
extern "C" {
#endif

/* define smb2_sec enum if older system headers do not */
#ifndef SMB2_SEC_UNDEFINED
#define SMB2_SEC_UNDEFINED 0
#endif
#ifndef SMB2_SEC_NTLMSSP
#define SMB2_SEC_NTLMSSP 1
#endif
#ifndef SMB2_SEC_KRB5
#define SMB2_SEC_KRB5 2
#endif

struct smbc_dirent {
   char name[256];
   int  type;     /* file vs directory */
   int64_t size;  /* file size */
};

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
intptr_t            retro_vfs_opendir_smb(const char *path, bool include_hidden);
struct smbc_dirent* retro_vfs_readdir_smb(intptr_t dh);
int                 retro_vfs_closedir_smb(intptr_t dh);

/* Stat */
int retro_vfs_stat_smb(const char *path, int32_t *size);

/* Errors */
int retro_vfs_file_error_smb(libretro_vfs_implementation_file *stream);

/* Context management */
void smb_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* VFS_IMPLEMENTATION_SMB_H */
