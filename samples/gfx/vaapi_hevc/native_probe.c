/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "../../../gfx/common/vaapi_hevc_runtime.h"
#include "../../../gfx/common/vaapi_hevc_export.h"
static unsigned fd_count(void)
{
 DIR *directory=opendir("/proc/self/fd");
 unsigned count=0;
 assert(directory);while(readdir(directory))count++;closedir(directory);return count;
}
int main(void)
{
 vaapi_hevc_runtime_t runtime={0};
 vaapi_hevc_runtime_t binding;
 vaapi_hevc_export_ops_t export_ops;
 VADRMPRIMESurfaceDescriptor descriptor;
 struct stat status;
 unsigned before,i;
 int fd;
 before=fd_count();
 assert(!stat("/dev/null",&status));
 assert(!vaapi_hevc_runtime_open(&runtime,"/dev/null",major(status.st_rdev),minor(status.st_rdev),64,64,8,4,VA_DEC_SLICE_MODE_NORMAL));
 assert(runtime.fd>=0 && runtime.va && runtime.va_drm && runtime.drm && !runtime.display);
 assert(fcntl(runtime.fd,F_GETFD)&FD_CLOEXEC);
 /* Binding-only fixture: no display has been initialized. */
 binding=runtime;binding.ready=true;
 assert(vaapi_hevc_export_bind(&export_ops,&binding));
 fd=open("/dev/null",O_RDONLY);assert(fd>=0 && !fcntl(fd,F_SETFD,0));
 assert(export_ops.cloexec(fd) && (fcntl(fd,F_GETFD)&FD_CLOEXEC));export_ops.close(fd);
 assert(export_ops.surface(NULL,VA_INVALID_SURFACE,VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
       VA_EXPORT_SURFACE_READ_ONLY|VA_EXPORT_SURFACE_SEPARATE_LAYERS,&descriptor)==VA_STATUS_ERROR_INVALID_DISPLAY);
 assert(!vaapi_hevc_export_bind(&export_ops,&runtime));
 assert(vaapi_hevc_runtime_close(&runtime) && fd_count()==before);
 for(i=0;i<32;i++) {
  assert(!vaapi_hevc_runtime_open(&runtime,"/dev/null",major(status.st_rdev),minor(status.st_rdev),64,64,8,4,VA_DEC_SLICE_MODE_NORMAL));
  assert(vaapi_hevc_runtime_close(&runtime));
  assert(!vaapi_hevc_runtime_open(&runtime,"/dev/null",major(status.st_rdev)+1,minor(status.st_rdev),64,64,10,4,VA_DEC_SLICE_MODE_NORMAL));
  assert(runtime.fd<0 && !runtime.va && vaapi_hevc_runtime_close(&runtime));
 }
 assert(fd_count()==before);
 puts("PASS: real VA/DRM/export bindings, CLOEXEC and invalid-display/node refusal without FD leaks; no VA device decoded or exported");
 return 0;
}
