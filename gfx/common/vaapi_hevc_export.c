/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_hevc_export.h"
#if defined(__linux__) && !defined(__ANDROID__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/vfs.h>
static bool vaapi_hevc_fd_identity(int fd,uint64_t *device,uint64_t *inode)
{
 struct stat st;
 struct statfs fs;
 /* Older anonymous-inode DMA-BUF exports have no unique buffer identity. */
 if(fstatfs(fd,&fs)!=0 || fs.f_type!=0x444d4142 || fstat(fd,&st)!=0 || !st.st_ino)return false;
 *device=(uint64_t)st.st_dev;*inode=(uint64_t)st.st_ino;return true;
}
static bool vaapi_hevc_fd_cloexec(int fd)
{
 int flags=fcntl(fd,F_GETFD);
 return flags>=0 && fcntl(fd,F_SETFD,flags|FD_CLOEXEC)==0;
}
static void vaapi_hevc_fd_close(int fd){close(fd);}
#endif
bool vaapi_hevc_export_bind(vaapi_hevc_export_ops_t *ops,const vaapi_hevc_runtime_t *runtime)
{
#if defined(__linux__) && !defined(__ANDROID__)
 vaapi_hevc_export_ops_t native;
 function_t symbol;
 if(!ops || !runtime || !runtime->ready || runtime->closing || !runtime->va || !runtime->io.symbol)return false;
 symbol=runtime->io.symbol(runtime->va,"vaExportSurfaceHandle");
 if(!symbol || sizeof(symbol)!=sizeof(native.surface))return false;
 memcpy(&native.surface,&symbol,sizeof(symbol));native.cloexec=vaapi_hevc_fd_cloexec;
 native.close=vaapi_hevc_fd_close;native.identity=vaapi_hevc_fd_identity;*ops=native;return true;
#else
 (void)ops;(void)runtime;return false;
#endif
}
static bool vaapi_hevc_export_valid(const vaapi_hevc_export_t *e)
{
 const VADRMPRIMESurfaceDescriptor *d=&e->descriptor;
 const vaapi_hevc_session_t *s=e->pool->session;
 unsigned i,j,k,used=0;
 if((s->depth!=8 && s->depth!=10) || !d->width || !d->height || (d->width&1) || (d->height&1)
       || d->fourcc!=(s->depth==10?VA_FOURCC_P010:VA_FOURCC_NV12)
       || d->width!=s->width || d->height!=s->height || !d->num_objects
       || d->num_objects>4 || d->num_layers!=2)return false;
 for(i=0;i<4;i++)
 {
  if(i>=d->num_objects){if(d->objects[i].fd>=0)return false;continue;}
  if(d->objects[i].fd<0 || !d->objects[i].size
        || d->objects[i].drm_format_modifier==(uint64_t)-1
        || d->objects[i].drm_format_modifier==(((uint64_t)0x00ffffffu<<32)|0xffffffffu))return false;
  for(j=0;j<i;j++)if(d->objects[i].fd==d->objects[j].fd)return false;
 }
 for(i=0;i<2;i++)
 {
  uint32_t format=s->depth==10?(i?VA_FOURCC('G','R','3','2'):VA_FOURCC('R','1','6',' '))
     :(i?VA_FOURCC('G','R','8','8'):VA_FOURCC('R','8',' ',' '));
  if(d->layers[i].drm_format!=format || !d->layers[i].num_planes || d->layers[i].num_planes>4)return false;
  for(j=0;j<d->layers[i].num_planes;j++)
  {
   uint32_t index=d->layers[i].object_index[j],offset=d->layers[i].offset[j],pitch=d->layers[i].pitch[j];
   if(index>=d->num_objects || !pitch || offset>=d->objects[index].size)return false;
   used|=1u<<index;
   for(k=0;k<j;k++)if(index==d->layers[i].object_index[k] && offset==d->layers[i].offset[k])return false;
   if(!d->objects[index].drm_format_modifier)
   {
    uint64_t rows=i?((uint64_t)d->height+1)/2:d->height;
    uint64_t row=(uint64_t)d->width*(s->depth==10?2:1);
    if(d->layers[i].num_planes!=1 || pitch<row || (rows-1)*pitch+row>d->objects[index].size-offset)return false;
   }
  }
 }
 return used==((1u<<d->num_objects)-1);
}
bool vaapi_hevc_export_open(vaapi_hevc_export_t *e,vaapi_hevc_pool_t *pool,
 unsigned slot,const vaapi_hevc_export_ops_t *ops)
{
 vaapi_hevc_export_ops_t native;
 uint64_t ticket;
 unsigned i;
 if(!e || e->active || !ops || !ops->surface || !ops->cloexec || !ops->close || !ops->identity)return false;
 native=*ops;
 if(!vaapi_hevc_pool_acquire(pool,slot,&ticket))return false;
 memset(e,0,sizeof(*e));e->pool=pool;e->slot=slot;e->ticket=ticket;e->ops=native;e->active=true;
 for(i=0;i<4;i++)e->descriptor.objects[i].fd=-1;
 if(e->ops.surface(pool->display,pool->session->surfaces[slot],VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
       VA_EXPORT_SURFACE_READ_ONLY|VA_EXPORT_SURFACE_SEPARATE_LAYERS,&e->descriptor)!=VA_STATUS_SUCCESS
       || !vaapi_hevc_export_valid(e))goto failed;
 for(i=0;i<e->descriptor.num_objects;i++)
  if(!e->ops.cloexec(e->descriptor.objects[i].fd)
        || !e->ops.identity(e->descriptor.objects[i].fd,&e->object_device[i],&e->object_inode[i])
        || !e->object_inode[i])goto failed;
 e->ready=true;return true;
failed:
 vaapi_hevc_export_close(e);return false;
}
int vaapi_hevc_export_take(vaapi_hevc_export_t *e,unsigned object)
{
 int fd;
 if(!e || !e->ready || object>=e->descriptor.num_objects)return -1;
 fd=e->descriptor.objects[object].fd;e->descriptor.objects[object].fd=-1;return fd;
}
bool vaapi_hevc_export_close(vaapi_hevc_export_t *e)
{
 unsigned i,j;
 if(!e || !e->active)return true;
 e->ready=false;
 for(i=0;i<4;i++)if(e->descriptor.objects[i].fd>=0)
 {
  int fd=e->descriptor.objects[i].fd;
  for(j=i;j<4;j++)if(e->descriptor.objects[j].fd==fd)e->descriptor.objects[j].fd=-1;
  e->ops.close(fd);
 }
 if(!vaapi_hevc_pool_release(e->pool,e->slot,e->ticket))return false;
 memset(e,0,sizeof(*e));return true;
}
