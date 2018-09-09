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
#include <stdint.h>
#include <fcntl.h>
#include <stdio.h>
#include "os_functions.h"
#include "iosuhax.h"

typedef struct _fs_dev_private_t {
    char *mount_path;
    int fsaFd;
    int mounted;
    void *pMutex;
} fs_dev_private_t;

typedef struct _fs_dev_file_state_t {
    fs_dev_private_t *dev;
    int fd;                                     /* File descriptor */
    int flags;                                  /* Opening flags */
    int read;                                   /* True if allowed to read from file */
    int write;                                  /* True if allowed to write to file */
    int append;                                 /* True if allowed to append to file */
    uint32_t pos;                                    /* Current position within the file (in bytes) */
    uint32_t len;                                    /* Total length of the file (in bytes) */
    struct _fs_dev_file_state_t *prevOpenFile;  /* The previous entry in a double-linked FILO list of open files */
    struct _fs_dev_file_state_t *nextOpenFile;  /* The next entry in a double-linked FILO list of open files */
} fs_dev_file_state_t;

typedef struct _fs_dev_dir_entry_t {
    fs_dev_private_t *dev;
    int dirHandle;
} fs_dev_dir_entry_t;

static fs_dev_private_t *fs_dev_get_device_data(const char *path)
{
    const devoptab_t *devoptab = NULL;
    char name[128] = {0};
    int i;

    if(!path)
        return NULL;

    /* Get the device name from the path */
    strncpy(name, path, 127);
    strtok(name, ":/");

    // Search the devoptab table for the specified device name
    // NOTE: We do this manually due to a 'bug' in GetDeviceOpTab
    //       which ignores names with suffixes and causes names
    //       like "ntfs" and "ntfs1" to be seen as equals
    for (i = 3; i < STD_MAX; i++) {
        devoptab = devoptab_list[i];
        if (devoptab && devoptab->name) {
            if (strcmp(name, devoptab->name) == 0) {
                return (fs_dev_private_t *)devoptab->deviceData;
            }
        }
    }

    return NULL;
}

static char *fs_dev_real_path (const char *path, fs_dev_private_t *dev)
{
    /* Sanity check */
    if (!path || !dev)
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

static int fs_dev_open_r (struct _reent *r, void *fileStruct, const char *path, int flags, int mode)
{
    fs_dev_private_t *dev = fs_dev_get_device_data(path);

    if(!r)
        return -1;

    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    fs_dev_file_state_t *file = (fs_dev_file_state_t *)fileStruct;
    if(!file) {
       r->_errno = EINVAL;
       return -1;
    }

    file->dev = dev;
    /* Determine which mode the file is opened for */
    file->flags = flags;

    const char *mode_str;

    if ((flags & 0x03) == O_RDONLY) {
        file->read = 1;
        file->write = 0;
        file->append = 0;
        mode_str = "r";
    } else if ((flags & 0x03) == O_WRONLY) {
        file->read = 0;
        file->write = 1;
        file->append = (flags & O_APPEND);
        mode_str = file->append ? "a" : "w";
    } else if ((flags & 0x03) == O_RDWR) {
        file->read = 1;
        file->write = 1;
        file->append = (flags & O_APPEND);
        mode_str = file->append ? "a+" : "r+";
    } else {
        r->_errno = EACCES;
        return -1;
    }

    int fd = -1;

    OSLockMutex(dev->pMutex);

    char *real_path = fs_dev_real_path(path, dev);
    if(!path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    int result = IOSUHAX_FSA_OpenFile(dev->fsaFd, real_path, mode_str, &fd);

    free(real_path);

    if(result == 0)
    {
        fileStat_s stats;
        result = IOSUHAX_FSA_StatFile(dev->fsaFd, fd, &stats);
        if(result != 0) {
            IOSUHAX_FSA_CloseFile(dev->fsaFd, fd);
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

static int fs_dev_close_r (struct _reent *r, void *fd)
{
    fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;

    if(!r)
        return -1;

    if(!file) {
        r->_errno = EINVAL;
        return -1;
    }

    if(!file->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(file->dev->pMutex);

    int result = IOSUHAX_FSA_CloseFile(file->dev->fsaFd, file->fd);

    OSUnlockMutex(file->dev->pMutex);

    if(result < 0)
    {
        r->_errno = result;
        return -1;
    }
    return 0;
}

static off_t fs_dev_seek_r (struct _reent *r, void* fd, off_t pos, int dir)
{
    fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;

    if(!r)
        return 0;

    if(!file) {
        r->_errno = EINVAL;
        return 0;
    }

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

    int result = IOSUHAX_FSA_SetFilePos(file->dev->fsaFd, file->fd, file->pos);

    OSUnlockMutex(file->dev->pMutex);

    if(result == 0)
    {
        return file->pos;
    }

    return result;
}

static ssize_t fs_dev_write_r (struct _reent *r, void *fd, const char *ptr, size_t len)
{
    fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;

    if(!r)
        return 0;

    if(!file) {
        r->_errno = EINVAL;
        return 0;
    }

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

    size_t done = 0;

    while(done < len)
    {
        size_t write_size = len - done;

        int result = IOSUHAX_FSA_WriteFile(file->dev->fsaFd, ptr + done, 0x01, write_size, file->fd, 0);
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

    OSUnlockMutex(file->dev->pMutex);
    return done;
}

static ssize_t fs_dev_read_r (struct _reent *r, void *fd, char *ptr, size_t len)
{
    if(!r)
        return 0;

    fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;

    if(!file) {
        r->_errno = EINVAL;
        return 0;
    }

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

    size_t done = 0;

    while(done < len)
    {
        size_t read_size = len - done;

        int result = IOSUHAX_FSA_ReadFile(file->dev->fsaFd, ptr + done, 0x01, read_size, file->fd, 0);
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
            done += result;
            file->pos += result;
        }
    }

    OSUnlockMutex(file->dev->pMutex);
    return done;
}


static int fs_dev_fstat_r (struct _reent *r, void *fd, struct stat *st)
{
    fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;

    if(!r)
        return -1;

    if(!file) {
        r->_errno = EINVAL;
        return -1;
    }

    if(!file->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(file->dev->pMutex);

    /* Zero out the stat buffer */
    memset(st, 0, sizeof(struct stat));

    fileStat_s stats;
    int result = IOSUHAX_FSA_StatFile(file->dev->fsaFd, (int)fd, &stats);
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
    st->st_dev = stats.id;
    st->st_uid = stats.owner_id;
    st->st_gid = stats.group_id;
    st->st_ino = stats.id;
    st->st_atime = stats.mtime;
    st->st_ctime = stats.ctime;
    st->st_mtime = stats.mtime;
    OSUnlockMutex(file->dev->pMutex);
    return 0;
}

static int fs_dev_ftruncate_r (struct _reent *r, void *fd, off_t len)
{
    fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;

    if(!r)
        return -1;

    if(!file) {
        r->_errno = EINVAL;
        return -1;
    }

    if(!file->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    r->_errno = ENOTSUP;
    /* TODO */
    return -1;
}

static int fs_dev_fsync_r (struct _reent *r, void *fd)
{
    fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;

    if(!r)
        return -1;

    if(!file) {
        r->_errno = EINVAL;
        return -1;
    }

    if(!file->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    r->_errno = ENOTSUP;
    /* TODO */
    return -1;
}

static int fs_dev_stat_r (struct _reent *r, const char *path, struct stat *st)
{
    fs_dev_private_t *dev = fs_dev_get_device_data(path);

    if(!r)
        return -1;

    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dev->pMutex);

    // Zero out the stat buffer
    memset(st, 0, sizeof(struct stat));

    char *real_path = fs_dev_real_path(path, dev);
    if(!real_path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    fileStat_s stats;

    int result = IOSUHAX_FSA_GetStat(dev->fsaFd, real_path, &stats);

    free(real_path);

    if(result < 0) {
        r->_errno = result;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    // mark root also as directory
    st->st_mode = ((stats.flag & 0x80000000) || (strlen(dev->mount_path) + 1 == strlen(real_path)))? S_IFDIR : S_IFREG;
    st->st_nlink = 1;
    st->st_size = stats.size;
    st->st_blocks = (stats.size + 511) >> 9;
    // Fill in the generic entry stats
    st->st_dev = stats.id;
    st->st_uid = stats.owner_id;
    st->st_gid = stats.group_id;
    st->st_ino = stats.id;
    st->st_atime = stats.mtime;
    st->st_ctime = stats.ctime;
    st->st_mtime = stats.mtime;

    OSUnlockMutex(dev->pMutex);

    return 0;
}

static int fs_dev_link_r (struct _reent *r, const char *existing, const char *newLink)
{
    if(!r)
      return -1;

    r->_errno = ENOTSUP;
    return -1;
}

static int fs_dev_unlink_r (struct _reent *r, const char *name)
{
    fs_dev_private_t *dev = fs_dev_get_device_data(name);

    if(!r)
        return -1;

    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dev->pMutex);

    char *real_path = fs_dev_real_path(name, dev);
    if(!real_path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    int result = IOSUHAX_FSA_Remove(dev->fsaFd, real_path);

    free(real_path);

    OSUnlockMutex(dev->pMutex);

    if(result < 0) {
        r->_errno = result;
        return -1;
    }

    return result;
}

static int fs_dev_chdir_r (struct _reent *r, const char *name)
{
    fs_dev_private_t *dev = fs_dev_get_device_data(name);

    if(!r)
        return -1;

    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dev->pMutex);

    char *real_path = fs_dev_real_path(name, dev);
    if(!real_path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    int result = IOSUHAX_FSA_ChangeDir(dev->fsaFd, real_path);

    free(real_path);

    OSUnlockMutex(dev->pMutex);

    if(result < 0) {
        r->_errno = result;
        return -1;
    }

    return 0;
}

static int fs_dev_rename_r (struct _reent *r, const char *oldName, const char *newName)
{
    fs_dev_private_t *dev = fs_dev_get_device_data(oldName);

    if(!r)
        return -1;

    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    if(!newName) {
       r->_errno = EINVAL;
       return -1;
    }

    OSLockMutex(dev->pMutex);

    char *real_oldpath = fs_dev_real_path(oldName, dev);
    if(!real_oldpath) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }
    char *real_newpath = fs_dev_real_path(newName, dev);
    if(!real_newpath) {
        r->_errno = ENOMEM;
        free(real_oldpath);
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    //! TODO
    int result = -ENOTSUP;

    free(real_oldpath);
    free(real_newpath);

    OSUnlockMutex(dev->pMutex);

    if(result < 0) {
        r->_errno = result;
        return -1;
    }

    return 0;

}

static int fs_dev_mkdir_r (struct _reent *r, const char *path, int mode)
{
    fs_dev_private_t *dev = fs_dev_get_device_data(path);

    if(!r)
        return -1;

    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dev->pMutex);

    char *real_path = fs_dev_real_path(path, dev);
    if(!real_path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    int result = IOSUHAX_FSA_MakeDir(dev->fsaFd, real_path, mode);

    free(real_path);

    OSUnlockMutex(dev->pMutex);

    if(result < 0) {
        r->_errno = result;
        return -1;
    }

    return 0;
}

static int fs_dev_chmod_r (struct _reent *r, const char *path, int mode)
{
    fs_dev_private_t *dev = fs_dev_get_device_data(path);

    if(!r)
        return -1;

    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dev->pMutex);

    char *real_path = fs_dev_real_path(path, dev);
    if(!real_path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    int result = IOSUHAX_FSA_ChangeMode(dev->fsaFd, real_path, mode);

    free(real_path);

    OSUnlockMutex(dev->pMutex);

    if(result < 0) {
        r->_errno = result;
        return -1;
    }

    return 0;
}

static int fs_dev_statvfs_r (struct _reent *r, const char *path, struct statvfs *buf)
{
    fs_dev_private_t *dev = fs_dev_get_device_data(path);

    if(!r)
        return -1;

    if(!dev) {
        r->_errno = ENODEV;
        return -1;
    }

    if(!buf) {
        r->_errno = EINVAL;
        return -1;
    }

    OSLockMutex(dev->pMutex);

    // Zero out the stat buffer
    memset(buf, 0, sizeof(struct statvfs));

    char *real_path = fs_dev_real_path(path, dev);
    if(!real_path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    uint64_t size;

    int result = IOSUHAX_FSA_GetDeviceInfo(dev->fsaFd, real_path, 0x00, (uint32_t*)&size);

    free(real_path);

    if(result < 0) {
        r->_errno = result;
        OSUnlockMutex(dev->pMutex);
        return -1;
    }

    // File system block size
    buf->f_bsize = 512;

    // Fundamental file system block size
    buf->f_frsize = 512;

    // Total number of blocks on file system in units of f_frsize
    buf->f_blocks = size >> 9; // this is unknown

    // Free blocks available for all and for non-privileged processes
    buf->f_bfree = buf->f_bavail = size >> 9;

    // Number of inodes at this point in time
    buf->f_files = 0xffffffff;

    // Free inodes available for all and for non-privileged processes
    buf->f_ffree = 0xffffffff;

    // File system id
    buf->f_fsid = (int)dev;

    // Bit mask of f_flag values.
    buf->f_flag = 0;

    // Maximum length of filenames
    buf->f_namemax = 255;

    OSUnlockMutex(dev->pMutex);

    return 0;
}

static DIR_ITER *fs_dev_diropen_r (struct _reent *r, DIR_ITER *dirState, const char *path)
{
    fs_dev_private_t *dev = fs_dev_get_device_data(path);

    if(!r)
        return NULL;

    if(!dev) {
        r->_errno = ENODEV;
        return NULL;
    }

    if(!dirState) {
       r->_errno = EINVAL;
       return NULL;
    }

    fs_dev_dir_entry_t *dirIter = (fs_dev_dir_entry_t *)dirState->dirStruct;

    OSLockMutex(dev->pMutex);

    char *real_path = fs_dev_real_path(path, dev);
    if(!real_path) {
        r->_errno = ENOMEM;
        OSUnlockMutex(dev->pMutex);
        return NULL;
    }

    int dirHandle;

    int result = IOSUHAX_FSA_OpenDir(dev->fsaFd, real_path, &dirHandle);

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

static int fs_dev_dirclose_r (struct _reent *r, DIR_ITER *dirState)
{
    fs_dev_dir_entry_t *dirIter;

    if(!r)
        return -1;

    if(!dirState) {
        r->_errno = EINVAL;
        return -1;
    }

    dirIter = (fs_dev_dir_entry_t *)dirState->dirStruct;

    if(!dirIter->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dirIter->dev->pMutex);

    int result = IOSUHAX_FSA_CloseDir(dirIter->dev->fsaFd, dirIter->dirHandle);

    OSUnlockMutex(dirIter->dev->pMutex);

    if(result < 0)
    {
        r->_errno = result;
        return -1;
    }
    return 0;
}

static int fs_dev_dirreset_r (struct _reent *r, DIR_ITER *dirState)
{
    fs_dev_dir_entry_t *dirIter;

    if(!r)
        return -1;

    if(!dirState) {
        r->_errno = EINVAL;
        return -1;
    }

    dirIter = (fs_dev_dir_entry_t *)dirState->dirStruct;

    if(!dirIter->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dirIter->dev->pMutex);

    int result = IOSUHAX_FSA_RewindDir(dirIter->dev->fsaFd, dirIter->dirHandle);

    OSUnlockMutex(dirIter->dev->pMutex);

    if(result < 0)
    {
        r->_errno = result;
        return -1;
    }
    return 0;
}

static int fs_dev_dirnext_r (struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *st)
{
    fs_dev_dir_entry_t *dirIter;

    if(!r)
        return -1;

    if(!dirState) {
        r->_errno = EINVAL;
        return -1;
    }

    dirIter = (fs_dev_dir_entry_t *)dirState->dirStruct;

    if(!dirIter->dev) {
        r->_errno = ENODEV;
        return -1;
    }

    OSLockMutex(dirIter->dev->pMutex);

    directoryEntry_s * dir_entry = malloc(sizeof(directoryEntry_s));

    int result = IOSUHAX_FSA_ReadDir(dirIter->dev->fsaFd, dirIter->dirHandle, dir_entry);
    if(result < 0)
    {
        free(dir_entry);
        r->_errno = result;
        OSUnlockMutex(dirIter->dev->pMutex);
        return -1;
    }

    // Fetch the current entry
    strcpy(filename, dir_entry->name);

    if(st)
    {
        memset(st, 0, sizeof(struct stat));
        st->st_mode = (dir_entry->stat.flag & 0x80000000) ? S_IFDIR : S_IFREG;
        st->st_nlink = 1;
        st->st_size = dir_entry->stat.size;
        st->st_blocks = (dir_entry->stat.size + 511) >> 9;
        st->st_dev = dir_entry->stat.id;
        st->st_uid = dir_entry->stat.owner_id;
        st->st_gid = dir_entry->stat.group_id;
        st->st_ino = dir_entry->stat.id;
        st->st_atime = dir_entry->stat.mtime;
        st->st_ctime = dir_entry->stat.ctime;
        st->st_mtime = dir_entry->stat.mtime;
    }

    free(dir_entry);
    OSUnlockMutex(dirIter->dev->pMutex);
    return 0;
}

// NTFS device driver devoptab
static const devoptab_t devops_fs = {
    NULL, /* Device name */
    sizeof (fs_dev_file_state_t),
    fs_dev_open_r,
    fs_dev_close_r,
    fs_dev_write_r,
    fs_dev_read_r,
    fs_dev_seek_r,
    fs_dev_fstat_r,
    fs_dev_stat_r,
    fs_dev_link_r,
    fs_dev_unlink_r,
    fs_dev_chdir_r,
    fs_dev_rename_r,
    fs_dev_mkdir_r,
    sizeof (fs_dev_dir_entry_t),
    fs_dev_diropen_r,
    fs_dev_dirreset_r,
    fs_dev_dirnext_r,
    fs_dev_dirclose_r,
    fs_dev_statvfs_r,
    fs_dev_ftruncate_r,
    fs_dev_fsync_r,
    fs_dev_chmod_r,
    NULL, /* fs_dev_fchmod_r */
    NULL  /* Device data */
};

static int fs_dev_add_device (const char *name, const char *mount_path, int fsaFd, int isMounted)
{
    devoptab_t *dev = NULL;
    char *devname = NULL;
    char *devpath = NULL;
    int i;

    // Sanity check
    if (!name || !mount_path) {
        errno = EINVAL;
        return -1;
    }

    // Allocate a devoptab for this device
    dev = (devoptab_t *) malloc(sizeof(devoptab_t) + strlen(name) + 1);
    if (!dev) {
        errno = ENOMEM;
        return -1;
    }

    // Use the space allocated at the end of the devoptab for storing the device name
    devname = (char*)(dev + 1);
    strcpy(devname, name);

    // create private data
    fs_dev_private_t *priv = (fs_dev_private_t *)malloc(sizeof(fs_dev_private_t) + strlen(mount_path) + 1);
    if(!priv) {
        free(dev);
        errno = ENOMEM;
        return -1;
    }

    devpath = (char*)(priv+1);
    strcpy(devpath, mount_path);

    // setup private data
    priv->mount_path = devpath;
    priv->fsaFd = fsaFd;
    priv->mounted = isMounted;
    priv->pMutex = malloc(OS_MUTEX_SIZE);

    if(!priv->pMutex) {
        free(dev);
        free(priv);
        errno = ENOMEM;
        return -1;
    }

    OSInitMutex(priv->pMutex);

    // Setup the devoptab
    memcpy(dev, &devops_fs, sizeof(devoptab_t));
    dev->name = devname;
    dev->deviceData = priv;

    // Add the device to the devoptab table (if there is a free slot)
    for (i = 3; i < STD_MAX; i++) {
        if (devoptab_list[i] == devoptab_list[0]) {
            devoptab_list[i] = dev;
            return 0;
        }
    }

    // failure, free all memory
    free(priv);
    free(dev);

    // If we reach here then there are no free slots in the devoptab table for this device
    errno = EADDRNOTAVAIL;
    return -1;
}

static int fs_dev_remove_device (const char *path)
{
    const devoptab_t *devoptab = NULL;
    char name[128] = {0};
    int i;

    if(!path)
        return -1;

    // Get the device name from the path
    strncpy(name, path, 127);
    strtok(name, ":/");

    // Find and remove the specified device from the devoptab table
    // NOTE: We do this manually due to a 'bug' in RemoveDevice
    //       which ignores names with suffixes and causes names
    //       like "ntfs" and "ntfs1" to be seen as equals
    for (i = 3; i < STD_MAX; i++) {
        devoptab = devoptab_list[i];
        if (devoptab && devoptab->name) {
            if (strcmp(name, devoptab->name) == 0) {
                devoptab_list[i] = devoptab_list[0];

                if(devoptab->deviceData)
                {
                    fs_dev_private_t *priv = (fs_dev_private_t *)devoptab->deviceData;

                    if(priv->mounted)
                        IOSUHAX_FSA_Unmount(priv->fsaFd, priv->mount_path, 2);

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

int mount_fs(const char *virt_name, int fsaFd, const char *dev_path, const char *mount_path)
{
    int isMounted = 0;

    if(dev_path)
    {
        isMounted = 1;

        int res = IOSUHAX_FSA_Mount(fsaFd, dev_path, mount_path, 2, 0, 0);
        if(res != 0)
        {
            return res;
        }
    }

    return fs_dev_add_device(virt_name, mount_path, fsaFd, isMounted);
}

int unmount_fs(const char *virt_name)
{
    return fs_dev_remove_device(virt_name);
}
