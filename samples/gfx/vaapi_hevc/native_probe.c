/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "../../../gfx/common/vaapi_hevc_runtime.h"
static unsigned fd_count(void)
{
 DIR *directory=opendir("/proc/self/fd");
 unsigned count=0;
 assert(directory);while(readdir(directory))count++;closedir(directory);return count;
}
int main(void)
{
 vaapi_hevc_runtime_t runtime={0};
 struct stat status;
 unsigned before,i;
 assert(!stat("/dev/null",&status));
 assert(!vaapi_hevc_runtime_open(&runtime,"/dev/null",major(status.st_rdev),minor(status.st_rdev),64,64,8,4,VA_DEC_SLICE_MODE_NORMAL));
 assert(runtime.fd>=0 && runtime.va && runtime.va_drm && runtime.drm && !runtime.display);
 assert(fcntl(runtime.fd,F_GETFD)&FD_CLOEXEC);
 assert(vaapi_hevc_runtime_close(&runtime));before=fd_count();
 for(i=0;i<32;i++) {
  assert(!vaapi_hevc_runtime_open(&runtime,"/dev/null",major(status.st_rdev),minor(status.st_rdev),64,64,8,4,VA_DEC_SLICE_MODE_NORMAL));
  assert(vaapi_hevc_runtime_close(&runtime));
  assert(!vaapi_hevc_runtime_open(&runtime,"/dev/null",major(status.st_rdev)+1,minor(status.st_rdev),64,64,10,4,VA_DEC_SLICE_MODE_NORMAL));
  assert(runtime.fd<0 && !runtime.va && vaapi_hevc_runtime_close(&runtime));
 }
 assert(fd_count()==before);
 puts("PASS: real VA/DRM libraries and symbols bind; wrong/non-render nodes refuse without FD leaks; no VA device decoded");
 return 0;
}
