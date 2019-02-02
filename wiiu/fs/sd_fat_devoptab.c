/***************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include <errno.h>
#include <sys/statvfs.h>
#include <sys/dirent.h>
#include <sys/iosupport.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <stdio.h>
#include "fs_utils.h"
#include <wiiu/os/mutex.h>
#include <wiiu/fs.h>

#define FS_ALIGNMENT            0x40
#define FS_ALIGN(x)             (((x) + FS_ALIGNMENT - 1) & ~(FS_ALIGNMENT - 1))

typedef struct __attribute__((packed))
{
    uint32_t flag;
    uint32_t permission;
    uint32_t owner_id;
    uint32_t group_id;
    uint32_t size;
    uint32_t alloc_size;
    uint64_t quota_size;
    uint32_t ent_id;
    uint64_t ctime;
    uint64_t mtime;
    uint8_t attributes[48];
} FSStat__;

typedef struct
{
   FSStat__ stat;
   char name[256];
} FSDirEntry;

typedef struct _sd_fat_private_t {
    char *mount_path;
    void *pClient;
    void *pCmd;
    void *pMutex;
} sd_fat_private_t;

typedef struct _sd_fat_file_state_t {
    sd_fat_private_t *dev;
    int fd;                                     /* File descriptor */
    int flags;                                  /* Opening flags */
    bool read;                                  /* True if allowed to read from file */
    bool write;                                 /* True if allowed to write to file */
    bool append;                                /* True if allowed to append to file */
    u64 pos;                                    /* Current position within the file (in bytes) */
    u64 len;                                    /* Total length of the file (in bytes) */
    struct _sd_fat_file_state_t *prevOpenFile;  /* The previous entry in a double-linked FILO list of open files */
    struct _sd_fat_file_state_t *nextOpenFile;  /* The next entry in a double-linked FILO list of open files */
} sd_fat_file_state_t;

typedef struct _sd_fat_dir_entry_t {
    sd_fat_private_t *dev;
    int dirHandle;
} sd_fat_dir_entry_t;

static sd_fat_private_t *sd_fat_get_device_data(const char *path)
{
    const devoptab_t *devoptab = NULL;
    char name[128] = {0};
    int i;

    /* Get the device name from the path */
    strncpy(name, path, 127);
    strtok(name, ":/");

    /* Search the devoptab table for the specified device name */
    /* NOTE: We do this manually due to a 'bug' in GetDeviceOpTab */
    /*       which ignores names with suffixes and causes names */
    /*       like "ntfs" and "ntfs1" to be seen as equals */
    for (i = 3; i < STD_MAX; i++) {
        devoptab = devoptab_list[i];
        if (devoptab && devoptab->name) {
            if (strcmp(name, devoptab->name) == 0) {
                return (sd_fat_private_t *)devoptab->deviceData;
            }
        }
    }

    return NULL;
}

static char *sd_fat_real_path (const char *path, sd_fat_private_t *dev)
{
    /* Sanity check */
    if (!path)
        return NULL;

    /* Move the path pointer to the start of the actual path */
    if (strchr(path, ':') != NULL) {
        path = strchr(path, ':') + 1;
    }

    int mount_len = strlen(dev->mount_path);

    char *new_name = (char*)malloc(mount_len + strlen(path) + 1);
    if(new_name) {
        strcpy(new_name, dev->mount_path);
        strcpy(new_name + mount_len, path);
        return new_name;
    }
    return new_name;
}

static int sd_fat_open_r (struct _reent *r, void *fileStruct, const char *path, int flags, int mode)
{
    sd_fat_private_t *dev = sd_fat_get_device_data(path);
    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    sd_fat_file_state_t *file = (sd_fat_file_state_t *)fileStruct;

    file->dev = dev;
    /* Determine which mode the file is opened for */
    file->flags = flags;

    const char *mode_str;

    if ((flags & 0x03) == O_RDONLY) {
        file->read = true;
        file->write = false;
        file->append = false;
        mode_str = "r";
    } else if ((flags & 0x03) == O_WRONLY) {
        file->read = false;
        file->write = true;
        file->append = (flags & O_APPEND);
        mode_str = file->append ? "a" : "w";
    } else if ((flags & 0x03) == O_RDWR) {
        file->read = true;
        file->write = true;
        file->append = (flags & O_APPEND);
        mode_str = file->append ? "a+" : "r+";
    } else {
        r->_errno = EACCES;
        return -1;
    }

    int fd = -1;

    OSLockMutex(dev->pMutex);

    char *real_path = sd_fat_real_path(path, dev);
    if(!path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    int result = FSOpenFile(dev->pClient, dev->pCmd, real_path, mode_str, (FSFileHandle*)&fd, -1);

    free(real_path);

    if(result == 0)
    {
        FSStat stats;
        result = FSGetStatFile(dev->pClient, dev->pCmd, fd, &stats, -1);
        if(result != 0) {
            FSCloseFile(dev->pClient, dev->pCmd, fd, -1);
            r->_errno = result;
            OSUnlockMutex(dev->pMutex);
            return -1;
        }
        file->fd = fd;
        file->pos = 0;
        file->len = stats.size;
        OSUnlockMutex(dev->pMutex);
        return (int)file;
    }

    r->_errno = result;
    OSUnlockMutex(dev->pMutex);
    return -1;
}

static int sd_fat_close_r (struct _reent *r, void* fd)
{
    sd_fat_file_state_t *file = (sd_fat_file_state_t *)fd;
    if(!file->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(file->dev->pMutex);

    int result = FSCloseFile(file->dev->pClient, file->dev->pCmd, file->fd, -1);

    OSUnlockMutex(file->dev->pMutex);

    if(result < 0)
    {
        r->_errno = result;
        return -1;
    }
    return 0;
}

static off_t sd_fat_seek_r (struct _reent *r, void* fd, off_t pos, int dir)
{
    sd_fat_file_state_t *file = (sd_fat_file_state_t *)fd;
    if(!file->dev) {
        r->_errno = ENODEV;
        return 0;
    }

    OSLockMutex(file->dev->pMutex);

    switch(dir)
    {
    case SEEK_SET:
        file->pos = pos;
        break;
    case SEEK_CUR:
        file->pos += pos;
        break;
    case SEEK_END:
        file->pos = file->len + pos;
        break;
    default:
        r->_errno = EINVAL;
        return -1;
    }

    int result = FSSetPosFile(file->dev->pClient, file->dev->pCmd, file->fd, file->pos, -1);

    OSUnlockMutex(file->dev->pMutex);

    if(result == 0)
    {
        return file->pos;
    }

    return result;
}

static ssize_t sd_fat_write_r (struct _reent *r, void* fd, const char *ptr, size_t len)
{
    sd_fat_file_state_t *file = (sd_fat_file_state_t *)fd;
    if(!file->dev) {
        r->_errno = ENODEV;
        return 0;
    }

    if(!file->write)
    {
        r->_errno = EACCES;
        return 0;
    }

    OSLockMutex(file->dev->pMutex);

    size_t len_aligned = FS_ALIGN(len);
    if(len_aligned > 0x4000)
        len_aligned = 0x4000;

    unsigned char *tmpBuf = (unsigned char *)memalign(FS_ALIGNMENT, len_aligned);
    if(!tmpBuf) {
        r->_errno = ENOMEM;
        OSUnlockMutex(file->dev->pMutex);
        return 0;
    }

    size_t done = 0;

    while(done < len)
    {
        size_t write_size = (len_aligned < (len - done)) ? len_aligned : (len - done);
        memcpy(tmpBuf, ptr + done, write_size);

        int result = FSWriteFile(file->dev->pClient, file->dev->pCmd, tmpBuf, 0x01, write_size, file->fd, 0, -1);
#if 0
        FSFlushFile(file->dev->pClient, file->dev->pCmd, file->fd, -1);
#endif
        if(result < 0)
        {
            r->_errno = result;
            break;
        }
        else if(result == 0)
        {
            if(write_size > 0)
                done = 0;
            break;
        }
        else
        {
            done += result;
            file->pos += result;
        }
    }

    free(tmpBuf);
    OSUnlockMutex(file->dev->pMutex);
    return done;
}

static ssize_t sd_fat_read_r (struct _reent *r, void* fd, char *ptr, size_t len)
{
    sd_fat_file_state_t *file = (sd_fat_file_state_t *)fd;
    if(!file->dev) {
        r->_errno = ENODEV;
        return 0;
    }

    if(!file->read)
    {
        r->_errno = EACCES;
        return 0;
    }

    OSLockMutex(file->dev->pMutex);

    size_t len_aligned = FS_ALIGN(len);
    if(len_aligned > 0x4000)
        len_aligned = 0x4000;

    unsigned char *tmpBuf = (unsigned char *)memalign(FS_ALIGNMENT, len_aligned);
    if(!tmpBuf) {
        r->_errno = ENOMEM;
        OSUnlockMutex(file->dev->pMutex);
        return 0;
    }

    size_t done = 0;

    while(done < len)
    {
        size_t read_size = (len_aligned < (len - done)) ? len_aligned : (len - done);

        int result = FSReadFile(file->dev->pClient, file->dev->pCmd, tmpBuf, 0x01, read_size, file->fd, 0, -1);
        if(result < 0)
        {
            r->_errno = result;
            done = 0;
            break;
        }
        else if(result == 0)
        {
            /*! TODO: error on read_size > 0 */
            break;
        }
        else
        {
            memcpy(ptr + done, tmpBuf, read_size);
            done += result;
            file->pos += result;
        }
    }

    free(tmpBuf);
    OSUnlockMutex(file->dev->pMutex);
    return done;
}

static int sd_fat_fstat_r (struct _reent *r, void* fd, struct stat *st)
{
    sd_fat_file_state_t *file = (sd_fat_file_state_t *)fd;
    if(!file->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(file->dev->pMutex);

    /* Zero out the stat buffer */
    memset(st, 0, sizeof(struct stat));

    FSStat__ stats;
    int result = FSGetStatFile(file->dev->pClient, file->dev->pCmd, file->fd, (FSStat*)&stats, -1);
    if(result != 0) {
        r->_errno = result;
        OSUnlockMutex(file->dev->pMutex);
        return -1;
    }

    st->st_mode = S_IFREG;
    st->st_size = stats.size;
    st->st_blocks = (stats.size + 511) >> 9;
    st->st_nlink = 1;

    /* Fill in the generic entry stats */
    st->st_dev = stats.ent_id;
    st->st_uid = stats.owner_id;
    st->st_gid = stats.group_id;
    st->st_ino = stats.ent_id;
    st->st_atime = stats.mtime;
    st->st_ctime = stats.ctime;
    st->st_mtime = stats.mtime;
    OSUnlockMutex(file->dev->pMutex);
    return 0;
}

static int sd_fat_ftruncate_r (struct _reent *r, void* fd, off_t len)
{
    sd_fat_file_state_t *file = (sd_fat_file_state_t *)fd;
    if(!file->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(file->dev->pMutex);

    int result = FSTruncateFile(file->dev->pClient, file->dev->pCmd, file->fd, -1);

    OSUnlockMutex(file->dev->pMutex);

    if(result < 0) {
        r->_errno = result;
        return -1;
    }

    return 0;
}

static int sd_fat_fsync_r (struct _reent *r, void* fd)
{
    sd_fat_file_state_t *file = (sd_fat_file_state_t *)fd;
    if(!file->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(file->dev->pMutex);

    int result = FSFlushFile(file->dev->pClient, file->dev->pCmd, file->fd, -1);

    OSUnlockMutex(file->dev->pMutex);

    if(result < 0) {
        r->_errno = result;
        return -1;
    }

    return 0;
}

static int sd_fat_stat_r (struct _reent *r, const char *path, struct stat *st)
{
    sd_fat_private_t *dev = sd_fat_get_device_data(path);
    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dev->pMutex);

    /* Zero out the stat buffer */
    memset(st, 0, sizeof(struct stat));

    char *real_path = sd_fat_real_path(path, dev);
    if(!real_path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    FSStat__ stats;

    int result = FSGetStat(dev->pClient, dev->pCmd, real_path, (FSStat*)&stats, -1);

    free(real_path);

    if(result < 0) {
        r->_errno = result;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    /* mark root also as directory */
    st->st_mode = ((stats.flag & 0x80000000) || (strlen(dev->mount_path) + 1 == strlen(real_path)))? S_IFDIR : S_IFREG;
    st->st_nlink = 1;
    st->st_size = stats.size;
    st->st_blocks = (stats.size + 511) >> 9;
    /* Fill in the generic entry stats */
    st->st_dev = stats.ent_id;
    st->st_uid = stats.owner_id;
    st->st_gid = stats.group_id;
    st->st_ino = stats.ent_id;
    st->st_atime = stats.mtime;
    st->st_ctime = stats.ctime;
    st->st_mtime = stats.mtime;

    OSUnlockMutex(dev->pMutex);

    return 0;
}

static int sd_fat_link_r (struct _reent *r, const char *existing, const char *newLink)
{
    r->_errno = ENOTSUP;
    return -1;
}

static int sd_fat_unlink_r (struct _reent *r, const char *name)
{
    sd_fat_private_t *dev = sd_fat_get_device_data(name);
    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dev->pMutex);

    char *real_path = sd_fat_real_path(name, dev);
    if(!real_path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    int result = FSRemove(dev->pClient, dev->pCmd, real_path, -1);

    free(real_path);

    OSUnlockMutex(dev->pMutex);

    if(result < 0) {
        r->_errno = result;
        return -1;
    }

    return 0;
}

static int sd_fat_chdir_r (struct _reent *r, const char *name)
{
    sd_fat_private_t *dev = sd_fat_get_device_data(name);
    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dev->pMutex);

    char *real_path = sd_fat_real_path(name, dev);
    if(!real_path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    int result = FSChangeDir(dev->pClient, dev->pCmd, real_path, -1);

    free(real_path);

    OSUnlockMutex(dev->pMutex);

    if(result < 0) {
        r->_errno = result;
        return -1;
    }

    return 0;
}

static int sd_fat_rename_r (struct _reent *r, const char *oldName, const char *newName)
{
    sd_fat_private_t *dev = sd_fat_get_device_data(oldName);
    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dev->pMutex);

    char *real_oldpath = sd_fat_real_path(oldName, dev);
    if(!real_oldpath) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }
    char *real_newpath = sd_fat_real_path(newName, dev);
    if(!real_newpath) {
        r->_errno = ENOMEM;
        free(real_oldpath);
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    int result = FSRename(dev->pClient, dev->pCmd, real_oldpath, real_newpath, -1);

    free(real_oldpath);
    free(real_newpath);

    OSUnlockMutex(dev->pMutex);

    if(result < 0) {
        r->_errno = result;
        return -1;
    }

    return 0;

}

static int sd_fat_mkdir_r (struct _reent *r, const char *path, int mode)
{
    sd_fat_private_t *dev = sd_fat_get_device_data(path);
    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dev->pMutex);

    char *real_path = sd_fat_real_path(path, dev);
    if(!real_path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    int result = FSMakeDir(dev->pClient, dev->pCmd, real_path, -1);

    free(real_path);

    OSUnlockMutex(dev->pMutex);

    if(result < 0) {
        r->_errno = result;
        return -1;
    }

    return 0;
}

static int sd_fat_statvfs_r (struct _reent *r, const char *path, struct statvfs *buf)
{
    sd_fat_private_t *dev = sd_fat_get_device_data(path);
    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dev->pMutex);

    /* Zero out the stat buffer */
    memset(buf, 0, sizeof(struct statvfs));

    char *real_path = sd_fat_real_path(path, dev);
    if(!real_path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    u64 size;

    int result = FSGetFreeSpaceSize(dev->pClient, dev->pCmd, real_path, &size, -1);

    free(real_path);

    if(result < 0) {
        r->_errno = result;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    /* File system block size */
    buf->f_bsize = 512;

    /* Fundamental file system block size */
    buf->f_frsize = 512;

    /* Total number of blocks on file system in units of f_frsize */
    buf->f_blocks = size >> 9; /* this is unknown */

    /* Free blocks available for all and for non-privileged processes */
    buf->f_bfree = buf->f_bavail = size >> 9;

    /* Number of inodes at this point in time */
    buf->f_files = 0xffffffff;

    /* Free inodes available for all and for non-privileged processes */
    buf->f_ffree = 0xffffffff;

    /* File system id */
    buf->f_fsid = (int)dev;

    /* Bit mask of f_flag values. */
    buf->f_flag = 0;

    /* Maximum length of filenames */
    buf->f_namemax = 255;

    OSUnlockMutex(dev->pMutex);

    return 0;
}

static DIR_ITER *sd_fat_diropen_r (struct _reent *r, DIR_ITER *dirState, const char *path)
{
    sd_fat_private_t *dev = sd_fat_get_device_data(path);
    if(!dev) {
        r->_errno = ENODEV;
        return NULL;
    }

    sd_fat_dir_entry_t *dirIter = (sd_fat_dir_entry_t *)dirState->dirStruct;

    OSLockMutex(dev->pMutex);

    char *real_path = sd_fat_real_path(path, dev);
    if(!real_path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return NULL;
    }

    int dirHandle;

    int result = FSOpenDir(dev->pClient, dev->pCmd, real_path, (FSDirectoryHandle*)&dirHandle, -1);

    free(real_path);

    OSUnlockMutex(dev->pMutex);

    if(result < 0)
    {
        r->_errno = result;
        return NULL;
    }

    dirIter->dev = dev;
    dirIter->dirHandle = dirHandle;

    return dirState;
}

static int sd_fat_dirclose_r (struct _reent *r, DIR_ITER *dirState)
{
    sd_fat_dir_entry_t *dirIter = (sd_fat_dir_entry_t *)dirState->dirStruct;
    if(!dirIter->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dirIter->dev->pMutex);

    int result = FSCloseDir(dirIter->dev->pClient, dirIter->dev->pCmd, dirIter->dirHandle, -1);

    OSUnlockMutex(dirIter->dev->pMutex);

    if(result < 0)
    {
        r->_errno = result;
        return -1;
    }
    return 0;
}

static int sd_fat_dirreset_r (struct _reent *r, DIR_ITER *dirState)
{
    sd_fat_dir_entry_t *dirIter = (sd_fat_dir_entry_t *)dirState->dirStruct;
    if(!dirIter->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dirIter->dev->pMutex);

    int result = FSRewindDir(dirIter->dev->pClient, dirIter->dev->pCmd, dirIter->dirHandle, -1);

    OSUnlockMutex(dirIter->dev->pMutex);

    if(result < 0)
    {
        r->_errno = result;
        return -1;
    }
    return 0;
}

static int sd_fat_dirnext_r (struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *st)
{
    sd_fat_dir_entry_t *dirIter = (sd_fat_dir_entry_t *)dirState->dirStruct;
    if(!dirIter->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dirIter->dev->pMutex);

    FSDirEntry * dir_entry = malloc(sizeof(FSDirEntry));

    int result = FSReadDir(dirIter->dev->pClient, dirIter->dev->pCmd, dirIter->dirHandle, (FSDirectoryEntry*)dir_entry, -1);
    if(result < 0)
    {
        free(dir_entry);
        r->_errno = result;
        OSUnlockMutex(dirIter->dev->pMutex);
        return -1;
    }

    /* Fetch the current entry */
    strcpy(filename, dir_entry->name);

    if(st)
    {
        memset(st, 0, sizeof(struct stat));
        st->st_mode = (dir_entry->stat.flag & 0x80000000) ? S_IFDIR : S_IFREG;
        st->st_nlink = 1;
        st->st_size = dir_entry->stat.size;
        st->st_blocks = (dir_entry->stat.size + 511) >> 9;
        st->st_dev = dir_entry->stat.ent_id;
        st->st_uid = dir_entry->stat.owner_id;
        st->st_gid = dir_entry->stat.group_id;
        st->st_ino = dir_entry->stat.ent_id;
        st->st_atime = dir_entry->stat.mtime;
        st->st_ctime = dir_entry->stat.ctime;
        st->st_mtime = dir_entry->stat.mtime;
    }

    free(dir_entry);
    OSUnlockMutex(dirIter->dev->pMutex);
    return 0;
}

/* NTFS device driver devoptab */
static const devoptab_t devops_sd_fat = {
    NULL, /* Device name */
    sizeof (sd_fat_file_state_t),
    sd_fat_open_r,
    sd_fat_close_r,
    sd_fat_write_r,
    sd_fat_read_r,
    sd_fat_seek_r,
    sd_fat_fstat_r,
    sd_fat_stat_r,
    sd_fat_link_r,
    sd_fat_unlink_r,
    sd_fat_chdir_r,
    sd_fat_rename_r,
    sd_fat_mkdir_r,
    sizeof (sd_fat_dir_entry_t),
    sd_fat_diropen_r,
    sd_fat_dirreset_r,
    sd_fat_dirnext_r,
    sd_fat_dirclose_r,
    sd_fat_statvfs_r,
    sd_fat_ftruncate_r,
    sd_fat_fsync_r,
    NULL, /* sd_fat_chmod_r */
    NULL, /* sd_fat_fchmod_r */
    NULL  /* Device data */
};

static int sd_fat_add_device (const char *name, const char *mount_path, void *pClient, void *pCmd)
{
    devoptab_t *dev = NULL;
    char *devname = NULL;
    char *devpath = NULL;
    int i;

    /* Sanity check */
    if (!name) {
        errno = EINVAL;
        return -1;
    }

    /* Allocate a devoptab for this device */
    dev = (devoptab_t *) malloc(sizeof(devoptab_t) + strlen(name) + 1);
    if (!dev) {
        errno = ENOMEM;
        return -1;
    }

    /* Use the space allocated at the end of the devoptab for storing the device name */
    devname = (char*)(dev + 1);
    strcpy(devname, name);

    /* create private data */
    sd_fat_private_t *priv = (sd_fat_private_t *)malloc(sizeof(sd_fat_private_t) + strlen(mount_path) + 1);
    if(!priv) {
        free(dev);
        errno = ENOMEM;
        return -1;
    }

    devpath = (char*)(priv+1);
    strcpy(devpath, mount_path);

    /* setup private data */
    priv->mount_path = devpath;
    priv->pClient = pClient;
    priv->pCmd = pCmd;
    priv->pMutex = malloc(sizeof(OSMutex));

    if(!priv->pMutex) {
        free(dev);
        free(priv);
        errno = ENOMEM;
        return -1;
    }

    OSInitMutex(priv->pMutex);

    /* Setup the devoptab */
    memcpy(dev, &devops_sd_fat, sizeof(devoptab_t));
    dev->name = devname;
    dev->deviceData = priv;

    /* Add the device to the devoptab table (if there is a free slot) */
    for (i = 3; i < STD_MAX; i++) {
        if (devoptab_list[i] == devoptab_list[0]) {
            devoptab_list[i] = dev;
            return 0;
        }
    }

    /* failure, free all memory */
    free(priv);
    free(dev);

    /* If we reach here then there are no free slots in the devoptab table for this device */
    errno = EADDRNOTAVAIL;
    return -1;
}

static int sd_fat_remove_device (const char *path, void **pClient, void **pCmd, char **mountPath)
{
    const devoptab_t *devoptab = NULL;
    char name[128] = {0};
    int i;

    /* Get the device name from the path */
    strncpy(name, path, 127);
    strtok(name, ":/");

    /* Find and remove the specified device from the devoptab table */
    /* NOTE: We do this manually due to a 'bug' in RemoveDevice */
    /*       which ignores names with suffixes and causes names */
    /*       like "ntfs" and "ntfs1" to be seen as equals */
    for (i = 3; i < STD_MAX; i++) {
        devoptab = devoptab_list[i];
        if (devoptab && devoptab->name) {
            if (strcmp(name, devoptab->name) == 0) {
                devoptab_list[i] = devoptab_list[0];

                if(devoptab->deviceData)
                {
                    sd_fat_private_t *priv = (sd_fat_private_t *)devoptab->deviceData;
                    *pClient = priv->pClient;
                    *pCmd = priv->pCmd;
                    *mountPath = (char*) malloc(strlen(priv->mount_path)+1);
                    if(*mountPath)
                        strcpy(*mountPath, priv->mount_path);
                    if(priv->pMutex)
                        free(priv->pMutex);
                    free(devoptab->deviceData);
                }

                free((devoptab_t*)devoptab);
                return 0;
            }
        }
    }

    return -1;
}

int mount_sd_fat(const char *path)
{
    int result = -1;

    /* get command and client */
    void* pClient = malloc(sizeof(FSClient));
    void* pCmd = malloc(sizeof(FSCmdBlock));

    if(!pClient || !pCmd) {
        /* just in case free if not 0 */
        if(pClient)
            free(pClient);
        if(pCmd)
            free(pCmd);
        return -2;
    }

    FSInit();
    FSInitCmdBlock(pCmd);
    FSAddClient(pClient, -1);

    char *mountPath = NULL;

    if(MountFS(pClient, pCmd, &mountPath) == 0) {
        result = sd_fat_add_device(path, mountPath, pClient, pCmd);
        free(mountPath);
    }

    return result;
}

int unmount_sd_fat(const char *path)
{
    void *pClient = 0;
    void *pCmd = 0;
    char *mountPath = 0;

    int result = sd_fat_remove_device(path, &pClient, &pCmd, &mountPath);
    if(result == 0)
    {
        UmountFS(pClient, pCmd, mountPath);
        FSDelClient(pClient, -1);
        free(pClient);
        free(pCmd);
        free(mountPath);
        /* FSShutdown(); */
    }
    return result;
}
