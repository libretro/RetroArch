/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_hevc_submit.c"
#include "../../../gfx/common/vaapi_hevc_pool.c"
#include "../../../gfx/common/vaapi_hevc_export.c"
static vaapi_hevc_session_t ex_session;
static vaapi_hevc_pool_t ex_pool;
static vaapi_hevc_export_t ex_frame,ex_second;
static unsigned ex_depth,ex_mode,ex_fault,ex_live[4],ex_closes,ex_calls,ex_seal_fail,ex_seals;
static void ex_close(int fd)
{assert(fd>=0 && fd<4 && ex_live[fd]);ex_live[fd]=0;ex_closes++;}
static bool ex_seal(int fd)
{assert(fd>=0 && fd<4 && ex_live[fd]);ex_seals++;return (unsigned)fd+1!=ex_seal_fail;}
static VAStatus ex_surface(VADisplay display,VASurfaceID surface,uint32_t memory,uint32_t flags,void *out)
{
 VADRMPRIMESurfaceDescriptor *d=(VADRMPRIMESurfaceDescriptor*)out;
 unsigned i,j,objects=ex_mode==2?4:ex_mode==1?2:1;
 unsigned pitch=64*(ex_depth==10?2:1);
 assert(display==ex_session.display && surface==9 && ex_pool.exports==1);
 assert(memory==VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2
       && flags==(VA_EXPORT_SURFACE_READ_ONLY|VA_EXPORT_SURFACE_SEPARATE_LAYERS));ex_calls++;
 for(i=0;i<4;i++)assert(d->objects[i].fd==-1);
 d->fourcc=ex_depth==10?VA_FOURCC_P010:VA_FOURCC_NV12;d->width=d->height=64;
 d->num_objects=objects;d->num_layers=2;
 for(i=0;i<objects;i++) {
  d->objects[i].fd=(int)i;d->objects[i].size=pitch*96;
  if(ex_mode==2)d->objects[i].drm_format_modifier=((uint64_t)1<<56)|7;
 }
 for(i=0;i<2;i++) {
  d->layers[i].drm_format=ex_depth==10?(i?VA_FOURCC('G','R','3','2'):VA_FOURCC('R','1','6',' '))
      :(i?VA_FOURCC('G','R','8','8'):VA_FOURCC('R','8',' ',' '));
  d->layers[i].num_planes=ex_mode==2?2:1;
  for(j=0;j<d->layers[i].num_planes;j++) {
   d->layers[i].object_index[j]=ex_mode==2?2*i+j:ex_mode==1?i:0;
   d->layers[i].offset[j]=ex_mode==0?i*pitch*64:0;d->layers[i].pitch[j]=pitch;
  }
 }
 switch(ex_fault) {
  case 1:d->num_objects=0;break;case 2:d->num_objects=5;break;
  case 3:d->num_layers=3;break;case 4:d->layers[0].num_planes=5;break;
  case 5:d->layers[1].object_index[0]=4;break;case 6:d->layers[1].pitch[0]=0;break;
  case 7:d->layers[0].offset[0]=d->objects[0].size;break;
  case 8:d->width++;break;case 9:d->height++;break;case 10:d->fourcc=0;break;
  case 11:d->layers[0].drm_format=0;break;case 12:d->objects[0].size=1;break;
  case 13:d->objects[0].drm_format_modifier=(uint64_t)-1;break;
  case 14:d->layers[0].num_planes=0;break;
  case 15:d->objects[0].fd=-1;break;
  case 16:if(objects>1)d->objects[1].fd=0;else d->objects[3].fd=0;break;
  case 17:d->num_objects=0;break;
  case 18:d->layers[0].pitch[0]=1;break;
  case 19:d->objects[0].drm_format_modifier=((uint64_t)0x00ffffffu<<32)|0xffffffffu;break;
 }
 for(i=0;i<4;i++)if(d->objects[i].fd>=0)ex_live[d->objects[i].fd]=1;
 return ex_fault==17?VA_STATUS_ERROR_OPERATION_FAILED:VA_STATUS_SUCCESS;
}
static const vaapi_hevc_export_ops_t ex_ops={ex_surface,ex_seal,ex_close};
static void ex_reset(void)
{
 unsigned i;
 assert(!ex_frame.active && !ex_second.active);
 for(i=0;i<4;i++)assert(!ex_live[i]);
 memset(&ex_pool,0,sizeof(ex_pool));memset(&ex_session,0,sizeof(ex_session));
 ex_pool.active=true;ex_pool.ready=ex_pool.valid=1;ex_pool.generation[0]=7;
 ex_pool.display=ex_session.display=(VADisplay)(uintptr_t)1;ex_pool.session=&ex_session;
 ex_session.slots=1;ex_session.surfaces[0]=9;ex_session.width=ex_session.height=64;ex_session.depth=ex_depth;
 ex_closes=ex_calls=ex_fault=ex_seal_fail=ex_seals=0;
}
static void ex_cases(void)
{
 unsigned i,objects=ex_mode==2?4:ex_mode==1?2:1;
 ex_reset();assert(vaapi_hevc_export_open(&ex_frame,&ex_pool,0,&ex_ops));
 assert(ex_pool.exports==1 && vaapi_hevc_pool_held(&ex_pool)==1 && !vaapi_hevc_pool_close(&ex_pool));
 assert(!vaapi_hevc_export_open(&ex_second,&ex_pool,0,&ex_ops) && ex_calls==1);
 for(i=0;i<objects;i++) {
  int fd=vaapi_hevc_export_take(&ex_frame,i);assert(fd==(int)i);
  assert(vaapi_hevc_export_take(&ex_frame,i)==-1);ex_close(fd);
 }
 assert(vaapi_hevc_export_close(&ex_frame) && !ex_pool.exports && ex_closes==objects);
 assert(vaapi_hevc_export_close(&ex_frame) && vaapi_hevc_pool_close(&ex_pool));
 for(i=1;i<=19;i++) {
  if(ex_mode==2 && (i==12 || i==18))continue;
  ex_reset();ex_fault=i;
  assert(!vaapi_hevc_export_open(&ex_frame,&ex_pool,0,&ex_ops));
  assert(!ex_frame.active && !ex_pool.exports && vaapi_hevc_export_close(&ex_frame));
 }
 for(i=1;i<=objects;i++) {
  ex_reset();ex_seal_fail=i;assert(!vaapi_hevc_export_open(&ex_frame,&ex_pool,0,&ex_ops));
  assert(ex_closes==objects && !ex_pool.exports && !ex_frame.active);
 }
 ex_reset();ex_pool.ready=0;assert(!vaapi_hevc_export_open(&ex_frame,&ex_pool,0,&ex_ops) && !ex_calls);
 ex_reset();ex_frame.ops=ex_ops;
 assert(vaapi_hevc_export_open(&ex_frame,&ex_pool,0,&ex_frame.ops));
 assert(vaapi_hevc_export_close(&ex_frame) && ex_closes==objects);
}
int main(void)
{
 for(ex_depth=8;ex_depth<=10;ex_depth+=2)for(ex_mode=0;ex_mode<3;ex_mode++)ex_cases();
 puts("PASS: VA PRIME descriptor bounds, modifiers/auxiliary planes, FD transfer/cleanup and retained surface tickets; no device execution");
 return 0;
}
