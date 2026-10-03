/* FAT12/16/32 with long names, over a gk_blockdev_t.  Portable C: the
 * console build adds locking and the C library glue (fat_devoptab.c);
 * the host test drives it over an image file.
 *
 * Paths are UTF-8, '/'-separated, relative to the volume root (a
 * leading '/' is optional).  Functions return 0 or a byte count on
 * success and a negative errno on failure. */

#ifndef GEKKO_FS_FAT_H
#define GEKKO_FS_FAT_H

#include <stddef.h>
#include <stdint.h>

#include <gekko/disk.h>

typedef struct fat_vol  fat_vol;
typedef struct fat_file fat_file;
typedef struct fat_dir  fat_dir;

typedef struct fat_stat
{
   uint64_t size;
   uint32_t cluster;      /* first cluster, an inode number of sorts */
   uint32_t mtime;        /* seconds since 1970, local time */
   uint8_t  is_dir;
   uint8_t  read_only;
} fat_stat;

typedef struct fat_space
{
   uint64_t clusters;
   uint64_t free_clusters;
   uint32_t cluster_size;
} fat_space;

int  fat_mount(fat_vol **out, gk_blockdev_t *dev);
/* The device is not touched again: files and directories still open
 * fail with -EIO until closed. */
int  fat_unmount(fat_vol *v);
int  fat_sync(fat_vol *v);

/* flags: O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_TRUNC, O_APPEND,
 * O_EXCL. */
int     fat_open(fat_vol *v, const char *path, int flags, fat_file **out);
int     fat_close(fat_file *f);
long    fat_read(fat_file *f, void *buf, size_t len);
long    fat_write(fat_file *f, const void *buf, size_t len);
int64_t fat_seek(fat_file *f, int64_t off, int whence);
int     fat_truncate(fat_file *f, uint64_t len);
int     fat_fsync(fat_file *f);
int     fat_fstat(fat_file *f, fat_stat *st);

int fat_stat_path(fat_vol *v, const char *path, fat_stat *st);
int fat_unlink(fat_vol *v, const char *path);
int fat_rename(fat_vol *v, const char *from, const char *to);
int fat_mkdir(fat_vol *v, const char *path);
int fat_rmdir(fat_vol *v, const char *path);
int fat_statvfs(fat_vol *v, fat_space *st);

int  fat_opendir(fat_vol *v, const char *path, fat_dir **out);
/* name: UTF-8, at least 256 * 3 + 1 bytes.  0 at the end. */
int  fat_readdir(fat_dir *d, char *name, size_t name_size, fat_stat *st);
void fat_rewinddir(fat_dir *d);
void fat_closedir(fat_dir *d);

/* Provided by the platform: the clock (seconds since 1970, local
 * time) and a recursive lock per volume. */
uint32_t fat_now(void);
void    *fat_lock_create(void);
void     fat_lock_destroy(void *lock);
void     fat_lock_acquire(void *lock);
void     fat_lock_release(void *lock);

#endif
