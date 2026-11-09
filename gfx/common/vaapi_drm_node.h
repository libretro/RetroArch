/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_DRM_NODE_H
#define RARCH_VAAPI_DRM_NODE_H
#include <boolean.h>
#include <retro_inline.h>
#include <stddef.h>

#if defined(__linux__) && !defined(__ANDROID__)
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#ifndef VAAPI_DRM_NODE_STAT
#define VAAPI_DRM_NODE_STAT stat
#endif
#ifndef VAAPI_DRM_NODE_FSTAT
#define VAAPI_DRM_NODE_FSTAT fstat
#endif

static INLINE char *vaapi_drm_append_text(char *out, const char *text)
{
   size_t length = strlen(text);
   memcpy(out, text, length);
   return out + length;
}

static INLINE char *vaapi_drm_append_uint(char *out, unsigned value)
{
   char digits[10];
   size_t count = 0;
   do
   {
      digits[count++] = (char)('0' + value % 10u);
      value /= 10u;
   } while (value);
   while (count)
      *out++ = digits[--count];
   return out;
}

/* The same two facts libdrm checks on Linux. Open time only. */
static INLINE bool vaapi_drm_render_path(
      unsigned major_id, unsigned minor_id, char *path, size_t capacity)
{
   struct stat status;
   char sysfs[48], node[32], *end;
   size_t length;
   end  = vaapi_drm_append_text(sysfs, "/sys/dev/char/");
   end  = vaapi_drm_append_uint(end, major_id);
   *end++ = ':';
   end  = vaapi_drm_append_uint(end, minor_id);
   end  = vaapi_drm_append_text(end, "/device/drm");
   *end = '\0';
   if (VAAPI_DRM_NODE_STAT(sysfs, &status))
      return false;
   end  = vaapi_drm_append_text(node, "/dev/dri/renderD");
   end  = vaapi_drm_append_uint(end, minor_id);
   *end = '\0';
   length = (size_t)(end - node);
   if (length >= capacity || VAAPI_DRM_NODE_STAT(node, &status)
         || !S_ISCHR(status.st_mode) || major(status.st_rdev) != major_id
         || minor(status.st_rdev) != minor_id)
      return false;
   memcpy(path, node, length + 1);
   return true;
}

static INLINE bool vaapi_drm_fd_is_render_node(int fd)
{
   struct stat status;
   char path[32];
   return !VAAPI_DRM_NODE_FSTAT(fd, &status) && S_ISCHR(status.st_mode)
         && vaapi_drm_render_path((unsigned)major(status.st_rdev),
               (unsigned)minor(status.st_rdev), path, sizeof(path));
}
#else
static INLINE bool vaapi_drm_render_path(
      unsigned major_id, unsigned minor_id, char *path, size_t capacity)
{
   (void)major_id;
   (void)minor_id;
   (void)path;
   (void)capacity;
   return false;
}
#endif
#endif
