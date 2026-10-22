/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../gfx/common/vaapi_hevc_session.h"
static int allocation_fail;
static void *session_alloc(size_t n,size_t size)
{return allocation_fail?NULL:calloc(n,size);}
#define calloc session_alloc
#include "../../../gfx/common/vaapi_hevc_session.c"
#undef calloc
enum fault {
 NONE,PROFILE_MAX,PROFILE_QUERY,PROFILE_COUNT,NO_PROFILE,ENTRY_MAX,ENTRY_QUERY,
 NO_ENTRY,ATTRIBUTES,CONFIG,ATTR_COUNT,ATTR_QUERY,NO_FORMAT,SURFACES,PARTIAL,
 DUPLICATE,CONTEXT,CONTEXT_EMPTY,CONFIG_EMPTY_SUCCESS,CONTEXT_EMPTY_SUCCESS,
 ATTR_COUNT_GROW,FAULT_END
};
static enum fault fault;
static unsigned depth,mode,rt_mask,slice_mask,max_width,usage,expected_slots;
static unsigned calls,configs,contexts,freed_surfaces,context_drops,config_drops;
static int live_config,live_context,live_surfaces[32];
static int fail_context_drop,fail_surface_drop,fail_config_drop;
static vaapi_hevc_session_t session;
static void check_display(VADisplay d)
{assert(d==(VADisplay)(uintptr_t)1);calls++;}
static int max_profiles(VADisplay d)
{check_display(d);return fault==PROFILE_MAX?257:2;}
static VAStatus profiles(VADisplay d,VAProfile *p,int *n)
{
 check_display(d);assert(*n==2);
 p[0]=fault==NO_PROFILE?VAProfileH264High:VAProfileHEVCMain;
 p[1]=fault==NO_PROFILE?VAProfileH264High:VAProfileHEVCMain10;
 *n=fault==PROFILE_COUNT?257:2;
 return fault==PROFILE_QUERY?VA_STATUS_ERROR_OPERATION_FAILED:VA_STATUS_SUCCESS;
}
static int max_entrypoints(VADisplay d)
{check_display(d);return fault==ENTRY_MAX?-1:2;}
static VAStatus entrypoints(VADisplay d,VAProfile p,VAEntrypoint *v,int *n)
{
 check_display(d);assert(p==(depth==10?VAProfileHEVCMain10:VAProfileHEVCMain) && *n==2);
 v[0]=fault==NO_ENTRY?VAEntrypointEncSlice:VAEntrypointVLD;v[1]=VAEntrypointEncSlice;*n=2;
 return fault==ENTRY_QUERY?VA_STATUS_ERROR_OPERATION_FAILED:VA_STATUS_SUCCESS;
}
static VAStatus attributes(VADisplay d,VAProfile p,VAEntrypoint e,VAConfigAttrib *a,int n)
{
 check_display(d);assert(p==(depth==10?VAProfileHEVCMain10:VAProfileHEVCMain) && e==VAEntrypointVLD && n==4);
 assert(a[0].type==VAConfigAttribRTFormat && a[1].type==VAConfigAttribDecSliceMode);
 assert(a[2].type==VAConfigAttribMaxPictureWidth && a[3].type==VAConfigAttribMaxPictureHeight);
 a[0].value=rt_mask;a[1].value=slice_mask;a[2].value=max_width;a[3].value=2160;
 return fault==ATTRIBUTES?VA_STATUS_ERROR_OPERATION_FAILED:VA_STATUS_SUCCESS;
}
static VAStatus create_config(VADisplay d,VAProfile p,VAEntrypoint e,VAConfigAttrib *a,int n,VAConfigID *out)
{
 check_display(d);assert(p==(depth==10?VAProfileHEVCMain10:VAProfileHEVCMain) && e==VAEntrypointVLD);
 assert(n==(slice_mask==VA_ATTRIB_NOT_SUPPORTED?1:2));
 assert(a[0].value==(depth==10?VA_RT_FORMAT_YUV420_10:VA_RT_FORMAT_YUV420));
 if(n==2)assert(a[1].value==mode);
 assert(!live_config);configs++;
 if(fault==CONFIG_EMPTY_SUCCESS)return VA_STATUS_SUCCESS;
 live_config=1;*out=11;
 return fault==CONFIG?VA_STATUS_ERROR_OPERATION_FAILED:VA_STATUS_SUCCESS;
}
static VAStatus surface_attributes(VADisplay d,VAConfigID config,VASurfaceAttrib *a,unsigned *n)
{
 check_display(d);assert(config==11 && live_config);
 if(!a){*n=fault==ATTR_COUNT?65:3;return VA_STATUS_SUCCESS;}
 assert(*n==3);memset(a,0,3*sizeof(*a));
 a[0].type=a[1].type=VASurfaceAttribPixelFormat;
 a[0].flags=a[1].flags=VA_SURFACE_ATTRIB_SETTABLE;
 a[0].value.type=a[1].value.type=VAGenericValueTypeInteger;
 a[0].value.value.i=VA_FOURCC_NV12;a[1].value.value.i=VA_FOURCC_P010;
 if(fault==NO_FORMAT)a[depth==10?1:0].flags=VA_SURFACE_ATTRIB_GETTABLE;
 a[2].type=VASurfaceAttribUsageHint;a[2].flags=VA_SURFACE_ATTRIB_SETTABLE;
 a[2].value.type=VAGenericValueTypeInteger;a[2].value.value.i=(int)usage;
 if(fault==ATTR_COUNT_GROW)*n=65;
 return fault==ATTR_QUERY?VA_STATUS_ERROR_OPERATION_FAILED:VA_STATUS_SUCCESS;
}
static VAStatus create_surfaces(VADisplay d,unsigned format,unsigned width,unsigned height,
      VASurfaceID *out,unsigned slots,VASurfaceAttrib *a,unsigned n)
{
 unsigned i,created=slots;
 check_display(d);assert(format==(depth==10?VA_RT_FORMAT_YUV420_10:VA_RT_FORMAT_YUV420));
 assert(width==64 && height==64 && slots==expected_slots && live_config && !live_context);
 assert(n==(usage&VA_SURFACE_ATTRIB_USAGE_HINT_DECODER?2u:1u));
 assert(a[0].type==VASurfaceAttribPixelFormat && a[0].value.type==VAGenericValueTypeInteger);
 assert((unsigned)a[0].value.value.i==(depth==10?VA_FOURCC_P010:VA_FOURCC_NV12));
 if(n==2)assert(a[1].value.value.i==VA_SURFACE_ATTRIB_USAGE_HINT_DECODER);
 if(fault==SURFACES)created=2;
 if(fault==PARTIAL)created=3;
 for(i=0;i<created;i++)
 {
  if(fault==DUPLICATE && i==1){out[i]=100;continue;}
  assert(!live_surfaces[i]);out[i]=100+i;live_surfaces[i]=1;
 }
 return fault==SURFACES?VA_STATUS_ERROR_OPERATION_FAILED:VA_STATUS_SUCCESS;
}
static VAStatus create_context(VADisplay d,VAConfigID c,int w,int h,int flags,
      VASurfaceID *surfaces,int n,VAContextID *out)
{
 int i;
 check_display(d);assert(c==11 && live_config && !live_context && w==64 && h==64 && flags==VA_PROGRESSIVE && n==(int)expected_slots);
 for(i=0;i<n;i++)assert(surfaces[i]==100u+(unsigned)i && live_surfaces[i]);
 contexts++;
 if(fault==CONTEXT_EMPTY)return VA_STATUS_ERROR_OPERATION_FAILED;
 if(fault==CONTEXT_EMPTY_SUCCESS)return VA_STATUS_SUCCESS;
 live_context=1;*out=22;
 return fault==CONTEXT?VA_STATUS_ERROR_OPERATION_FAILED:VA_STATUS_SUCCESS;
}
static VAStatus destroy_context(VADisplay d,VAContextID c)
{
 check_display(d);assert(c==22 && live_context && live_config);context_drops++;
 if(fail_context_drop){fail_context_drop=0;return VA_STATUS_ERROR_OPERATION_FAILED;}
 live_context=0;return VA_STATUS_SUCCESS;
}
static VAStatus destroy_surfaces(VADisplay d,VASurfaceID *v,int n)
{
 unsigned i;
 check_display(d);assert(!live_context && live_config && n==1 && *v>=100 && *v<132);
 i=*v-100;assert(live_surfaces[i]);
 if(fail_surface_drop && i==1)
 {fail_surface_drop=0;*v=VA_INVALID_SURFACE;return VA_STATUS_ERROR_OPERATION_FAILED;}
 live_surfaces[i]=0;freed_surfaces++;return VA_STATUS_SUCCESS;
}
static VAStatus destroy_config(VADisplay d,VAConfigID c)
{
 unsigned i;
 check_display(d);assert(c==11 && live_config && !live_context);config_drops++;
 for(i=0;i<32;i++)assert(!live_surfaces[i]);
 if(fail_config_drop){fail_config_drop=0;return VA_STATUS_ERROR_OPERATION_FAILED;}
 live_config=0;return VA_STATUS_SUCCESS;
}
static const vaapi_hevc_session_ops_t ops={max_profiles,profiles,max_entrypoints,
 entrypoints,attributes,create_config,destroy_config,surface_attributes,
 create_surfaces,destroy_surfaces,create_context,destroy_context};
static bool open_session(void)
{return vaapi_hevc_session_open(&session,(VADisplay)(uintptr_t)1,&ops,64,64,depth,expected_slots,mode);}
static void reset(void)
{
 unsigned i;
 assert(!session.initialized && !live_config && !live_context);
 for(i=0;i<32;i++)assert(!live_surfaces[i]);
 memset(&session,0,sizeof(session));fault=NONE;allocation_fail=0;
 fail_context_drop=fail_surface_drop=fail_config_drop=0;
 calls=configs=contexts=freed_surfaces=context_drops=config_drops=0;
 rt_mask=VA_RT_FORMAT_YUV420|VA_RT_FORMAT_YUV420_10;
 slice_mask=VA_DEC_SLICE_MODE_NORMAL|VA_DEC_SLICE_MODE_BASE;
 max_width=4096;usage=VA_SURFACE_ATTRIB_USAGE_HINT_DECODER;expected_slots=4;
}
static void cases(void)
{
 int f;
 unsigned before,i;
 vaapi_hevc_session_ops_t incomplete;
 for(f=NONE;f<FAULT_END;f++)
 {
  reset();fault=(enum fault)f;
  assert(open_session()==(f==NONE));
  assert(session.initialized && session.ready==(f==NONE));
  assert(!vaapi_hevc_session_open(&session,(VADisplay)(uintptr_t)1,&ops,64,64,depth,4,mode));
  assert(vaapi_hevc_session_close(&session));
  assert(vaapi_hevc_session_close(&session));
  if(f==SURFACES)assert(freed_surfaces==2 && !contexts);
  if(f==DUPLICATE || f==PARTIAL)assert(freed_surfaces==3 && !contexts);
 }
 reset();allocation_fail=1;assert(!open_session() && !calls);
 assert(vaapi_hevc_session_close(&session));
 reset();rt_mask=VA_ATTRIB_NOT_SUPPORTED;assert(!open_session() && !configs);
 assert(vaapi_hevc_session_close(&session));
 reset();slice_mask=mode==VA_DEC_SLICE_MODE_BASE?VA_DEC_SLICE_MODE_NORMAL:VA_DEC_SLICE_MODE_BASE;
 assert(!open_session() && !configs);assert(vaapi_hevc_session_close(&session));
 reset();slice_mask=VA_ATTRIB_NOT_SUPPORTED;
 assert(open_session()==(mode==VA_DEC_SLICE_MODE_NORMAL));assert(vaapi_hevc_session_close(&session));
 reset();max_width=32;assert(!open_session() && !configs);assert(vaapi_hevc_session_close(&session));
 reset();max_width=VA_ATTRIB_NOT_SUPPORTED;usage=VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER;
 assert(open_session() && session.ready);assert(vaapi_hevc_session_close(&session));
 reset();assert(open_session());fail_context_drop=1;
 assert(!vaapi_hevc_session_close(&session) && !session.ready && session.closing && !freed_surfaces && !config_drops);
 assert(vaapi_hevc_session_close(&session) && context_drops==2 && freed_surfaces==4 && config_drops==1);
 reset();assert(open_session());fail_surface_drop=1;
 assert(!vaapi_hevc_session_close(&session) && session.surfaces[1]==101 && freed_surfaces==3 && !config_drops);
 assert(vaapi_hevc_session_close(&session) && context_drops==1 && freed_surfaces==4 && config_drops==1);
 reset();assert(open_session());fail_config_drop=1;
 assert(!vaapi_hevc_session_close(&session) && session.config==11);
 assert(vaapi_hevc_session_close(&session) && context_drops==1 && freed_surfaces==4 && config_drops==2);
 reset();session.ops=ops;
 assert(vaapi_hevc_session_open(&session,(VADisplay)(uintptr_t)1,&session.ops,64,64,depth,4,mode));
 assert(vaapi_hevc_session_close(&session));
 reset();assert(!vaapi_hevc_session_open(&session,(VADisplay)(uintptr_t)1,&ops,65,64,depth,4,mode));
 assert(!session.initialized && !calls);
 reset();expected_slots=2;assert(open_session());assert(vaapi_hevc_session_close(&session) && freed_surfaces==2);
 reset();expected_slots=32;assert(open_session());assert(vaapi_hevc_session_close(&session) && freed_surfaces==32);
 reset();incomplete=ops;incomplete.destroy_context=NULL;
 assert(!vaapi_hevc_session_open(&session,(VADisplay)(uintptr_t)1,&incomplete,64,64,depth,4,mode));
 assert(!vaapi_hevc_session_open(&session,NULL,&ops,64,64,depth,4,mode));
 assert(!vaapi_hevc_session_open(&session,(VADisplay)(uintptr_t)1,&ops,64,64,12,4,mode));
 assert(!vaapi_hevc_session_open(&session,(VADisplay)(uintptr_t)1,&ops,64,64,depth,33,mode));
 assert(!vaapi_hevc_session_open(&session,(VADisplay)(uintptr_t)1,&ops,64,64,depth,4,3));
 assert(!session.initialized && !calls && vaapi_hevc_session_close(&session));
 reset();assert(open_session());
 assert(!vaapi_hevc_session_terminated(&session,(VADisplay)(uintptr_t)1));
 fail_context_drop=1;assert(!vaapi_hevc_session_close(&session));before=calls;
 assert(!vaapi_hevc_session_terminated(&session,(VADisplay)(uintptr_t)2) && session.initialized);
 /* Successful native display termination invalidates all its IDs. */
 live_config=live_context=0;for(i=0;i<32;i++)live_surfaces[i]=0;
 assert(vaapi_hevc_session_terminated(&session,(VADisplay)(uintptr_t)1));
 assert(!session.initialized && calls==before && vaapi_hevc_session_close(&session));
}

static void auto_cases(void)
{
   unsigned i;
   const unsigned masks[5] = {VA_DEC_SLICE_MODE_NORMAL | VA_DEC_SLICE_MODE_BASE,
         VA_DEC_SLICE_MODE_BASE, VA_DEC_SLICE_MODE_NORMAL,
         VA_ATTRIB_NOT_SUPPORTED, 0};
   for (i = 0; i < 5; i++)
   {
      reset();
      slice_mask = masks[i];
      mode       = i == 1 ? VA_DEC_SLICE_MODE_BASE : VA_DEC_SLICE_MODE_NORMAL;
      assert(vaapi_hevc_session_open(&session, (VADisplay)(uintptr_t)1, &ops,
                   64, 64, depth, expected_slots, VAAPI_HEVC_SLICE_AUTO)
             == (i != 4));
      if (i != 4)
         assert(session.slice_mode == mode && session.ready);
      else
         assert(!configs);
      assert(vaapi_hevc_session_close(&session));
   }
}

int main(void)
{
   for (depth = 8; depth <= 10; depth += 2)
      for (mode = VA_DEC_SLICE_MODE_NORMAL; mode <= VA_DEC_SLICE_MODE_BASE;
            mode++)
         cases();
   for (depth = 8; depth <= 10; depth += 2)
      auto_cases();
   puts("PASS: VA-API HEVC profile/mode admission, partial creation, ordered retirement and retry; no device execution");
   return 0;
}
