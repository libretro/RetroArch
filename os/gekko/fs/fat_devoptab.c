/* FAT volumes as C library devices: "name:/path" reaches fat.c. */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/iosupport.h>
#include <sys/syslimits.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <gekko/disk.h>
#include <gekko/thread.h>

#include "fat.h"
#include "../kernel/kernel.h"

#define MAX_VOLUMES 8
#define PATH_BUF    1024

/* A name stays a C library device once mounted: files open on it when
 * it is unmounted still reach it (and fail), and mounting it again
 * reuses it. */
struct volume
{
   devoptab_t ops;
   char       name[16];
   fat_vol   *vol;            /* NULL while unmounted */
   int        index;          /* in the C library's device table */
};

static struct volume *volumes[MAX_VOLUMES];
/* Held from finding a path's volume to done with it, and to mount or
 * unmount one. */
static gk_mutex_t     mount_lock = GK_MUTEX_INIT;

/* Whatever is still mounted reaches the disk before the program
 * leaves. */
static void sync_all(void)
{
   unsigned i;
   gk_mutex_lock(&mount_lock);
   for (i = 0; i < MAX_VOLUMES; i++)
      if (volumes[i] && volumes[i]->vol)
         fat_sync(volumes[i]->vol);
   gk_mutex_unlock(&mount_lock);
}

static struct gk_exit_hook sync_hook = { sync_all, NULL };
static int                 sync_hooked;

/* ---- what fat.c needs from the platform ---- */

uint32_t fat_now(void)
{
   return (uint32_t)time(NULL);
}

void *fat_lock_create(void)
{
   gk_rmutex_t *m = (gk_rmutex_t*)malloc(sizeof(*m));
   if (m)
   {
      m->m.word = 0;
      m->depth  = 0;
   }
   return m;
}

void fat_lock_destroy(void *lock)     { free(lock); }
void fat_lock_acquire(void *lock)     { gk_rmutex_lock((gk_rmutex_t*)lock); }
void fat_lock_release(void *lock)     { gk_rmutex_unlock((gk_rmutex_t*)lock); }

/* ---- paths ---- */

/* The volume a path names (the default device when it has no "dev:")
 * and the path itself, made absolute from the working directory when
 * it is relative.  Call with mount_lock held. */
static fat_vol *resolve(const char *path, char *buf, size_t size,
      const char **out)
{
   const devoptab_t *ops;
   *out = path;
   if (!strchr(path, ':') && path[0] != '/' && getcwd(buf, size))
   {
      size_t len = strlen(buf);
      if (len + strlen(path) + 2 > size)
         return NULL;
      if (len && buf[len - 1] != '/')
         buf[len++] = '/';
      strcpy(buf + len, path);
      *out = buf;
   }
   if (!(ops = GetDeviceOpTab(*out)) || !ops->deviceData)
      return NULL;
   return ((struct volume*)ops->deviceData)->vol;
}

/* Takes mount_lock; DONE gives it back. */
#define RESOLVE(r, path, vol, p) \
   char buf_[PATH_BUF]; \
   const char *p; \
   fat_vol *vol; \
   gk_mutex_lock(&mount_lock); \
   if (!(vol = resolve(path, buf_, sizeof(buf_), &p))) \
   { \
      gk_mutex_unlock(&mount_lock); \
      (r)->_errno = ENODEV; \
      return -1; \
   }

#define DONE(r, ret) \
   do { \
      int ret_ = (ret); \
      gk_mutex_unlock(&mount_lock); \
      return result(r, ret_); \
   } while (0)

static int result(struct _reent *r, int ret)
{
   if (ret >= 0)
      return 0;
   r->_errno = -ret;
   return -1;
}

static void to_stat(const fat_stat *fs, uint32_t cluster_size,
      struct stat *st)
{
   memset(st, 0, sizeof(*st));
   st->st_mode    = fs->is_dir ? (S_IFDIR | 0777)
      : (S_IFREG | (fs->read_only ? 0444 : 0666));
   st->st_nlink   = 1;
   st->st_ino     = fs->cluster;
   st->st_size    = (off_t)fs->size;
   st->st_blksize = cluster_size ? cluster_size : 512;
   st->st_blocks  = (fs->size + 511) / 512;
   st->st_atime   = st->st_mtime = st->st_ctime = (time_t)fs->mtime;
}

/* ---- files ---- */

static int dev_open(struct _reent *r, void *fs, const char *path,
      int flags, int mode)
{
   fat_file *f;
   int ret;
   RESOLVE(r, path, vol, p);
   (void)mode;
   if (!(ret = fat_open(vol, p, flags, &f)))
      *(fat_file**)fs = f;
   DONE(r, ret);
}

static int dev_close(struct _reent *r, void *fd)
{
   return result(r, fat_close(*(fat_file**)fd));
}

static ssize_t dev_write(struct _reent *r, void *fd, const char *ptr,
      size_t len)
{
   long ret = fat_write(*(fat_file**)fd, ptr, len);
   if (ret < 0)
   {
      r->_errno = (int)-ret;
      return -1;
   }
   return (ssize_t)ret;
}

static ssize_t dev_read(struct _reent *r, void *fd, char *ptr, size_t len)
{
   long ret = fat_read(*(fat_file**)fd, ptr, len);
   if (ret < 0)
   {
      r->_errno = (int)-ret;
      return -1;
   }
   return (ssize_t)ret;
}

static off_t dev_seek(struct _reent *r, void *fd, off_t pos, int dir)
{
   int64_t ret = fat_seek(*(fat_file**)fd, pos, dir);
   if (ret < 0)
   {
      r->_errno = (int)-ret;
      return -1;
   }
   if (ret != (off_t)ret)
   {
      r->_errno = EOVERFLOW;
      return -1;
   }
   return (off_t)ret;
}

static int dev_fstat(struct _reent *r, void *fd, struct stat *st)
{
   fat_stat fs;
   int ret = fat_fstat(*(fat_file**)fd, &fs);
   if (!ret)
      to_stat(&fs, 0, st);
   return result(r, ret);
}

static int dev_ftruncate(struct _reent *r, void *fd, off_t len)
{
   if (len < 0)
      return result(r, -EINVAL);
   return result(r, fat_truncate(*(fat_file**)fd, (uint64_t)len));
}

static int dev_fsync(struct _reent *r, void *fd)
{
   return result(r, fat_fsync(*(fat_file**)fd));
}

/* ---- names ---- */

static int dev_stat(struct _reent *r, const char *path, struct stat *st)
{
   fat_stat fs;
   int ret;
   RESOLVE(r, path, vol, p);
   if (!(ret = fat_stat_path(vol, p, &fs)))
      to_stat(&fs, 0, st);
   DONE(r, ret);
}

static int dev_unlink(struct _reent *r, const char *path)
{
   RESOLVE(r, path, vol, p);
   DONE(r, fat_unlink(vol, p));
}

static int dev_chdir(struct _reent *r, const char *path)
{
   fat_stat fs;
   int ret;
   RESOLVE(r, path, vol, p);
   if (!(ret = fat_stat_path(vol, p, &fs)) && !fs.is_dir)
      ret = -ENOTDIR;
   DONE(r, ret);
}

static int dev_rename(struct _reent *r, const char *from, const char *to)
{
   char buf2[PATH_BUF];
   const char *q;
   RESOLVE(r, from, vol, p);
   if (resolve(to, buf2, sizeof(buf2), &q) != vol)
      DONE(r, -EXDEV);
   DONE(r, fat_rename(vol, p, q));
}

static int dev_mkdir(struct _reent *r, const char *path, int mode)
{
   RESOLVE(r, path, vol, p);
   (void)mode;
   DONE(r, fat_mkdir(vol, p));
}

static int dev_rmdir(struct _reent *r, const char *path)
{
   RESOLVE(r, path, vol, p);
   DONE(r, fat_rmdir(vol, p));
}

static int dev_statvfs(struct _reent *r, const char *path,
      struct statvfs *buf)
{
   fat_space sp;
   int ret;
   RESOLVE(r, path, vol, p);
   (void)p;
   ret = fat_statvfs(vol, &sp);
   gk_mutex_unlock(&mount_lock);
   if (ret)
      return result(r, ret);
   memset(buf, 0, sizeof(*buf));
   buf->f_bsize   = sp.cluster_size;
   buf->f_frsize  = sp.cluster_size;
   buf->f_blocks  = (fsblkcnt_t)sp.clusters;
   buf->f_bfree   = (fsblkcnt_t)sp.free_clusters;
   buf->f_bavail  = (fsblkcnt_t)sp.free_clusters;
   buf->f_flag    = ST_NOSUID;
   buf->f_namemax = 255;
   return 0;
}

/* ---- directories ---- */

static DIR_ITER *dev_diropen(struct _reent *r, DIR_ITER *it,
      const char *path)
{
   char buf_[PATH_BUF];
   const char *p;
   fat_vol *vol;
   int ret = -ENODEV;
   gk_mutex_lock(&mount_lock);
   if ((vol = resolve(path, buf_, sizeof(buf_), &p)))
      ret = fat_opendir(vol, p, (fat_dir**)it->dirStruct);
   gk_mutex_unlock(&mount_lock);
   if (ret)
   {
      r->_errno = -ret;
      return NULL;
   }
   return it;
}

static int dev_dirreset(struct _reent *r, DIR_ITER *it)
{
   (void)r;
   fat_rewinddir(*(fat_dir**)it->dirStruct);
   return 0;
}

static int dev_dirnext(struct _reent *r, DIR_ITER *it, char *name,
      struct stat *st)
{
   fat_stat fs;
   /* libsysbase gives NAME_MAX + 1 bytes; longer UTF-8 is cut. */
   int ret = fat_readdir(*(fat_dir**)it->dirStruct, name, NAME_MAX + 1, &fs);
   if (ret == 1)
   {
      if (st)
         to_stat(&fs, 0, st);
      return 0;
   }
   r->_errno = ret ? -ret : ENOENT;
   return -1;
}

static int dev_dirclose(struct _reent *r, DIR_ITER *it)
{
   (void)r;
   fat_closedir(*(fat_dir**)it->dirStruct);
   return 0;
}

/* ---- mounting ---- */

static struct volume *find(const char *name)
{
   unsigned i;
   for (i = 0; i < MAX_VOLUMES; i++)
      if (volumes[i] && !strcmp(volumes[i]->name, name))
         return volumes[i];
   return NULL;
}

static int add(const char *name, struct volume **out)
{
   struct volume *v;
   unsigned i;
   int index;

   if (FindDevice(name) >= 0)
      return -EBUSY;
   for (i = 0; i < MAX_VOLUMES && volumes[i]; i++)
      ;
   if (i == MAX_VOLUMES)
      return -ENFILE;
   if (!(v = (struct volume*)calloc(1, sizeof(*v))))
      return -ENOMEM;
   strcpy(v->name, name);
   v->ops.name          = v->name;
   v->ops.structSize    = sizeof(fat_file*);
   v->ops.open_r        = dev_open;
   v->ops.close_r       = dev_close;
   v->ops.write_r       = dev_write;
   v->ops.read_r        = dev_read;
   v->ops.seek_r        = dev_seek;
   v->ops.fstat_r       = dev_fstat;
   v->ops.stat_r        = dev_stat;
   v->ops.lstat_r       = dev_stat;
   v->ops.unlink_r      = dev_unlink;
   v->ops.chdir_r       = dev_chdir;
   v->ops.rename_r      = dev_rename;
   v->ops.mkdir_r       = dev_mkdir;
   v->ops.rmdir_r       = dev_rmdir;
   v->ops.dirStateSize  = sizeof(fat_dir*);
   v->ops.diropen_r     = dev_diropen;
   v->ops.dirreset_r    = dev_dirreset;
   v->ops.dirnext_r     = dev_dirnext;
   v->ops.dirclose_r    = dev_dirclose;
   v->ops.statvfs_r     = dev_statvfs;
   v->ops.ftruncate_r   = dev_ftruncate;
   v->ops.fsync_r       = dev_fsync;
   v->ops.deviceData    = v;
   if ((index = AddDevice(&v->ops)) < 0)
   {
      free(v);
      return -ENFILE;
   }
   v->index   = index;
   volumes[i] = v;
   *out       = v;
   return 0;
}

int gk_fat_mount(const char *name, gk_blockdev_t *dev)
{
   static int have_default;
   struct volume *v;
   int ret = 0;

   if (strlen(name) >= sizeof(v->name) || strchr(name, ':'))
      return -EINVAL;
   gk_mutex_lock(&mount_lock);
   if (!(v = find(name)))
      ret = add(name, &v);
   else if (v->vol)
      ret = -EBUSY;
   if (!ret)
      ret = fat_mount(&v->vol, dev);
   if (!ret && !sync_hooked)
   {
      sync_hooked = 1;
      gk_exit_hook_add(&sync_hook);
   }
   gk_mutex_unlock(&mount_lock);
   if (!ret && !have_default)
   {
      /* Paths without a device go to the first volume mounted. */
      char root[24];
      setDefaultDevice(v->index);
      sprintf(root, "%s:/", name);
      chdir(root);
      have_default = 1;
   }
   return ret;
}

int gk_fat_sync(const char *name)
{
   struct volume *v;
   int ret = -ENODEV;
   gk_mutex_lock(&mount_lock);
   if ((v = find(name)) && v->vol)
      ret = fat_sync(v->vol);
   gk_mutex_unlock(&mount_lock);
   return ret;
}

void gk_fat_unmount(const char *name)
{
   struct volume *v;
   gk_mutex_lock(&mount_lock);
   if ((v = find(name)) && v->vol)
   {
      fat_unmount(v->vol);
      v->vol = NULL;
   }
   gk_mutex_unlock(&mount_lock);
}
