/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(__linux__) && !defined(__ANDROID__)
#include <sys/stat.h>
#include <sys/sysmacros.h>

static const char *sysfs_expected, *node_expected;
static unsigned stats, fstats;
static int sysfs_missing, node_missing, wrong_mode, wrong_major, wrong_minor,
      fstat_fail, fd_block;

static int test_stat(const char *path, struct stat *out)
{
   memset(out, 0, sizeof(*out));
   stats++;
   if (!strncmp(path, "/sys/", 5))
   {
      assert(!strcmp(path, sysfs_expected));
      return sysfs_missing ? -1 : 0;
   }
   assert(!strcmp(path, node_expected));
   if (node_missing)
      return -1;
   out->st_mode = wrong_mode ? S_IFREG : S_IFCHR;
   out->st_rdev = makedev(wrong_major ? 227u : 226u, wrong_minor ? 130u : 129u);
   return 0;
}

static int test_fstat(int fd, struct stat *out)
{
   assert(fd == 42);
   memset(out, 0, sizeof(*out));
   fstats++;
   if (fstat_fail)
      return -1;
   out->st_mode = fd_block ? S_IFBLK : S_IFCHR;
   out->st_rdev = makedev(226u, 129u);
   return 0;
}

#define VAAPI_DRM_NODE_STAT test_stat
#define VAAPI_DRM_NODE_FSTAT test_fstat
#include "../../../gfx/common/vaapi_drm_node.h"

static void reset(void)
{
   stats = fstats = 0;
   sysfs_missing = node_missing = wrong_mode = wrong_major = wrong_minor = 0;
   fstat_fail = fd_block = 0;
   sysfs_expected = "/sys/dev/char/226:129/device/drm";
   node_expected  = "/dev/dri/renderD129";
}

static void refused(size_t capacity, unsigned expected_stats)
{
   char path[32];
   memset(path, 0x5a, sizeof(path));
   assert(!vaapi_drm_render_path(226, 129, path, capacity));
   assert(path[0] == 0x5a && stats == expected_stats);
}

int main(void)
{
   char path[32];

   reset();
   assert(vaapi_drm_render_path(226, 129, path, sizeof(path)));
   assert(!strcmp(path, "/dev/dri/renderD129") && stats == 2);

   /* Exactly fits, then one byte short of the terminator. */
   reset();
   assert(vaapi_drm_render_path(226, 129, path, 20));
   assert(!strcmp(path, "/dev/dri/renderD129"));
   reset();
   refused(19, 1);

   reset();
   sysfs_missing = 1;
   refused(sizeof(path), 1);
   reset();
   node_missing = 1;
   refused(sizeof(path), 2);
   reset();
   wrong_mode = 1;
   refused(sizeof(path), 2);
   reset();
   wrong_major = 1;
   refused(sizeof(path), 2);
   reset();
   wrong_minor = 1;
   refused(sizeof(path), 2);

   /* The longest numbers must fit the fixed stack buffers. */
   reset();
   sysfs_expected = "/sys/dev/char/4294967295:4294967295/device/drm";
   node_expected  = "/dev/dri/renderD4294967295";
   node_missing   = 1;
   assert(!vaapi_drm_render_path(
         4294967295u, 4294967295u, path, sizeof(path)));
   assert(stats == 2);

   reset();
   assert(vaapi_drm_fd_is_render_node(42) && fstats == 1 && stats == 2);
   reset();
   fstat_fail = 1;
   assert(!vaapi_drm_fd_is_render_node(42) && fstats == 1 && !stats);
   reset();
   fd_block = 1;
   assert(!vaapi_drm_fd_is_render_node(42) && fstats == 1 && !stats);
   reset();
   sysfs_missing = 1;
   assert(!vaapi_drm_fd_is_render_node(42) && stats == 1);
   reset();
   node_missing = 1;
   assert(!vaapi_drm_fd_is_render_node(42) && stats == 2);

   puts("PASS: render-node lookup without libdrm, exact path, capacity and "
        "device-number refusals, descriptor type check");
   return 0;
}
#else
int main(void)
{
   puts("SKIP: Linux render-node lookup fixture");
   return 0;
}
#endif
