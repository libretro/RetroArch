/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "vaapi_hevc_runtime.h"
#include "vaapi_vp9_runtime.h"
#include "vaapi_av1_runtime.h"
#include <string.h>
#if defined(__linux__) && !defined(__ANDROID__) && defined(NEED_DYNAMIC) && !defined(VAAPI_HEVC_RUNTIME_TEST)
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#define VAAPI_HEVC_NATIVE_RUNTIME 1
static int vaapi_hevc_open_node(const char *path,unsigned major_id,unsigned minor_id)
{
 struct stat status;
 int fd=open(path,O_RDWR|O_CLOEXEC);
 if(fd<0)return -1;
 if(fstat(fd,&status)<0 || !S_ISCHR(status.st_mode)
       || major(status.st_rdev)!=major_id || minor(status.st_rdev)!=minor_id)
 {close(fd);return -1;}
 return fd;
}
static void vaapi_hevc_close_node(int fd){close(fd);}
#endif
static bool vaapi_runtime_open_with_io(vaapi_hevc_runtime_t *r,
      const char *path, unsigned major_id, unsigned minor_id, unsigned width,
      unsigned height, unsigned depth, unsigned slots, unsigned slice_mode,
      const vaapi_hevc_runtime_io_t *io, bool vp9, bool av1);
bool vaapi_hevc_runtime_open(vaapi_hevc_runtime_t *r,const char *path,
      unsigned major_id,unsigned minor_id,unsigned width,unsigned height,
      unsigned depth,unsigned slots,unsigned slice_mode)
{
#ifdef VAAPI_HEVC_NATIVE_RUNTIME
 static const vaapi_hevc_runtime_io_t io={vaapi_hevc_open_node,vaapi_hevc_close_node,
  dylib_load,dylib_proc,dylib_close};
 return vaapi_hevc_runtime_open_with_io(r,path,major_id,minor_id,width,height,depth,slots,slice_mode,&io);
#else
 (void)r;(void)path;(void)major_id;(void)minor_id;(void)width;(void)height;
 (void)depth;(void)slots;(void)slice_mode;return false;
#endif
}

static bool vaapi_runtime_open_with_io(vaapi_hevc_runtime_t *r, const char *path,
      unsigned major_id, unsigned minor_id, unsigned width, unsigned height,
      unsigned depth, unsigned slots, unsigned slice_mode,
      const vaapi_hevc_runtime_io_t *io, bool vp9, bool av1)
{
   vaapi_hevc_runtime_io_t native;
   bool frame = vp9 || av1;
   unsigned minimum_slots = av1 ? 9u : vp9 ? 10u : 2u;
#if !VA_CHECK_VERSION(1, 8, 0)
   if (av1)
      return false;
#endif
   if ((vp9 && av1) || !r || r->active || !path || !*path || !io
         || !io->open_node
         || !io->close_node || !io->load || !io->symbol || !io->unload
         || width < (frame ? 1u : 8u)
         || width > (av1 ? 65536u : 65535u)
         || height < (frame ? 1u : 8u)
         || height > (av1 ? 65536u : 65535u)
         || (!frame && ((width & 1) || (height & 1)))
         || (depth != 8 && depth != 10)
         || slots < minimum_slots || slots > 32
         || (!frame && slice_mode != VAAPI_HEVC_SLICE_AUTO
               && slice_mode != VA_DEC_SLICE_MODE_NORMAL
               && slice_mode != VA_DEC_SLICE_MODE_BASE))
      return false;
   native = *io;
   memset(r, 0, sizeof(*r));
   r->io     = native;
   r->fd     = -1;
   r->active = true;
   r->error  = VA_STATUS_ERROR_INVALID_DISPLAY;
   r->fd     = r->io.open_node(path, major_id, minor_id);
   if (r->fd < 0)
      return false;
   r->error = VA_STATUS_ERROR_UNIMPLEMENTED;
   r->va    = r->io.load("libva.so.2");
   if (!r->va)
      return false;
   r->va_drm = r->io.load("libva-drm.so.2");
   if (!r->va_drm)
      return false;
   r->drm = r->io.load("libdrm.so.2");
   if (!r->drm)
      return false;
#define VAAPI_HEVC_LOAD(member, library, name)                                 \
   do                                                                          \
   {                                                                           \
      function_t symbol = r->io.symbol(r->library, name);                      \
      if (!symbol || sizeof(symbol) != sizeof(r->member))                      \
         return false;                                                         \
      memcpy(&r->member, &symbol, sizeof(symbol));                             \
   } while (0)
   VAAPI_HEVC_LOAD(get_display, va_drm, "vaGetDisplayDRM");
   VAAPI_HEVC_LOAD(initialize, va, "vaInitialize");
   VAAPI_HEVC_LOAD(terminate, va, "vaTerminate");
   VAAPI_HEVC_LOAD(node_type, drm, "drmGetNodeTypeFromFd");
   VAAPI_HEVC_LOAD(session.ops.max_profiles, va, "vaMaxNumProfiles");
   VAAPI_HEVC_LOAD(session.ops.profiles, va, "vaQueryConfigProfiles");
   VAAPI_HEVC_LOAD(session.ops.max_entrypoints, va, "vaMaxNumEntrypoints");
   VAAPI_HEVC_LOAD(session.ops.entrypoints, va, "vaQueryConfigEntrypoints");
   VAAPI_HEVC_LOAD(session.ops.attributes, va, "vaGetConfigAttributes");
   VAAPI_HEVC_LOAD(session.ops.create_config, va, "vaCreateConfig");
   VAAPI_HEVC_LOAD(session.ops.destroy_config, va, "vaDestroyConfig");
   VAAPI_HEVC_LOAD(
         session.ops.surface_attributes, va, "vaQuerySurfaceAttributes");
   VAAPI_HEVC_LOAD(session.ops.create_surfaces, va, "vaCreateSurfaces");
   VAAPI_HEVC_LOAD(session.ops.destroy_surfaces, va, "vaDestroySurfaces");
   VAAPI_HEVC_LOAD(session.ops.create_context, va, "vaCreateContext");
   VAAPI_HEVC_LOAD(session.ops.destroy_context, va, "vaDestroyContext");
   VAAPI_HEVC_LOAD(submit.create, va, "vaCreateBuffer");
   VAAPI_HEVC_LOAD(submit.destroy, va, "vaDestroyBuffer");
   VAAPI_HEVC_LOAD(submit.begin, va, "vaBeginPicture");
   VAAPI_HEVC_LOAD(submit.render, va, "vaRenderPicture");
   VAAPI_HEVC_LOAD(submit.end, va, "vaEndPicture");
   VAAPI_HEVC_LOAD(submit.sync, va, "vaSyncSurface2");
#undef VAAPI_HEVC_LOAD
   r->error = VA_STATUS_ERROR_INVALID_DISPLAY;
   if (r->node_type(r->fd) != VAAPI_HEVC_RENDER_NODE)
      return false;
   r->display = r->get_display(r->fd);
   if (!r->display)
      return false;
   r->error = r->initialize(r->display, &r->major_version, &r->minor_version);
   if (r->error != VA_STATUS_SUCCESS)
      return false;
   if (r->major_version != VA_MAJOR_VERSION || r->minor_version < 0)
   {
      r->error = VA_STATUS_ERROR_UNIMPLEMENTED;
      return false;
   }
   if (!(av1 ? vaapi_av1_session_open(&r->session, r->display,
                      &r->session.ops, width, height, depth, slots)
              : vp9 ? vaapi_vp9_session_open(&r->session, r->display,
                      &r->session.ops, width, height, depth, slots)
              : vaapi_hevc_session_open(&r->session, r->display,
                      &r->session.ops, width, height, depth, slots, slice_mode)))
   {
      r->error = r->session.error;
      return false;
   }
   r->ready = true;
   return true;
}
bool vaapi_hevc_runtime_open_with_io(vaapi_hevc_runtime_t *r,
      const char *path, unsigned major_id, unsigned minor_id, unsigned width,
      unsigned height, unsigned depth, unsigned slots, unsigned slice_mode,
      const vaapi_hevc_runtime_io_t *io)
{
   return vaapi_runtime_open_with_io(r, path, major_id, minor_id, width,
         height, depth, slots, slice_mode, io, false, false);
}

bool vaapi_vp9_runtime_open_with_io(vaapi_vp9_runtime_t *r,
      const char *path, unsigned major_id, unsigned minor_id, unsigned width,
      unsigned height, unsigned depth, unsigned slots,
      const vaapi_vp9_runtime_io_t *io)
{
   return vaapi_runtime_open_with_io(r, path, major_id, minor_id, width,
         height, depth, slots, 0, io, true, false);
}

bool vaapi_vp9_runtime_open(vaapi_vp9_runtime_t *r, const char *path,
      unsigned major_id, unsigned minor_id, unsigned width, unsigned height,
      unsigned depth, unsigned slots)
{
#ifdef VAAPI_HEVC_NATIVE_RUNTIME
   static const vaapi_vp9_runtime_io_t io = {vaapi_hevc_open_node,
         vaapi_hevc_close_node, dylib_load, dylib_proc, dylib_close};
   return vaapi_vp9_runtime_open_with_io(r, path, major_id, minor_id, width,
         height, depth, slots, &io);
#else
   (void)r;
   (void)path;
   (void)major_id;
   (void)minor_id;
   (void)width;
   (void)height;
   (void)depth;
   (void)slots;
   return false;
#endif
}

bool vaapi_av1_runtime_open_with_io(vaapi_av1_runtime_t *r,
      const char *path, unsigned major_id, unsigned minor_id, unsigned width,
      unsigned height, unsigned depth, unsigned slots,
      const vaapi_av1_runtime_io_t *io)
{
   return vaapi_runtime_open_with_io(r, path, major_id, minor_id, width,
         height, depth, slots, 0, io, false, true);
}

bool vaapi_av1_runtime_open(vaapi_av1_runtime_t *r, const char *path,
      unsigned major_id, unsigned minor_id, unsigned width, unsigned height,
      unsigned depth, unsigned slots)
{
#ifdef VAAPI_HEVC_NATIVE_RUNTIME
   static const vaapi_av1_runtime_io_t io = {vaapi_hevc_open_node,
         vaapi_hevc_close_node, dylib_load, dylib_proc, dylib_close};
   return vaapi_av1_runtime_open_with_io(r, path, major_id, minor_id, width,
         height, depth, slots, &io);
#else
   (void)r;
   (void)path;
   (void)major_id;
   (void)minor_id;
   (void)width;
   (void)height;
   (void)depth;
   (void)slots;
   return false;
#endif
}

bool vaapi_hevc_runtime_close(vaapi_hevc_runtime_t *r)
{
 if(!r || !r->active)return true;
 r->ready=false;r->closing=true;
 if(r->display)
 {
  if(!r->terminating)
  {
   vaapi_hevc_session_close(&r->session);
   r->terminating=true;
  }
  if(!r->terminated)
  {
   r->error=r->terminate(r->display);
   if(r->error!=VA_STATUS_SUCCESS)return false;
   r->terminated=true;
  }
  if(r->session.initialized && !vaapi_hevc_session_terminated(&r->session,r->display))
  {r->error=VA_STATUS_ERROR_OPERATION_FAILED;return false;}
  r->display=NULL;
 }
 if(r->drm)r->io.unload(r->drm);
 if(r->va_drm)r->io.unload(r->va_drm);
 if(r->va)r->io.unload(r->va);
 if(r->fd>=0)r->io.close_node(r->fd);
 memset(r,0,sizeof(*r));return true;
}
