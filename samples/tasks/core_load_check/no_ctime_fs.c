/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Linked with -Wl,--wrap=fstatfs,--wrap=fstatfs64: while no_ctime_fs is
 * set, every file reads as vfat, which doesn't keep ctime, so no
 * identity binds a check's pass to its file */
#define _LARGEFILE64_SOURCE 1
#include <sys/vfs.h>

#define NO_CTIME_FS_MAGIC 0x4d44 /* vfat */

int no_ctime_fs;

int __real_fstatfs(int fd, struct statfs *buf);
int __real_fstatfs64(int fd, struct statfs64 *buf);

int __wrap_fstatfs(int fd, struct statfs *buf)
{
   int ret = __real_fstatfs(fd, buf);
   if (ret == 0 && no_ctime_fs)
      buf->f_type = NO_CTIME_FS_MAGIC;
   return ret;
}

int __wrap_fstatfs64(int fd, struct statfs64 *buf)
{
   int ret = __real_fstatfs64(fd, buf);
   if (ret == 0 && no_ctime_fs)
      buf->f_type = NO_CTIME_FS_MAGIC;
   return ret;
}
