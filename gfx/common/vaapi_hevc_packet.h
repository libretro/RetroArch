/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_PACKET_H
#define RARCH_VAAPI_HEVC_PACKET_H
#include "vaapi_hevc.h"
#include "vaapi_hevc_submit.h"
#include <limits.h>
static INLINE bool vaapi_hevc_packet_overlap(const void *a,size_t an,const void *b,size_t bn)
{
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return x<=y?y-x<an:x-y<bn;
}
/* BASE-mode raw slice stream: start-code offsets, escaped NALs, 128-byte tail
 * padding. Not the NORMAL-mode long-slice layout. The AU and NALs must have
 * passed rh265_video_get_access_unit. Separate caller-owned output buffers;
 * used must be separate from all input/output storage. Failure leaves the
 * output buffers unchanged and sets used to zero. No allocation/VA calls. */
static INLINE bool vaapi_hevc_short_packet(uint8_t *data,size_t capacity,
 VASliceParameterBufferBase *out,unsigned out_count,const rh265_nal_t *slices,
 const rh265_access_unit_t *au,unsigned *used)
{
 size_t total=0,padded,offset=0,descriptor_bytes;
 unsigned i,type,temporal;
 if(!used)return false;
 *used=0;
 if(!data || !out || !slices || !au || !au->num_slices || au->num_slices>256
       || au->num_slices>out_count || au->first.nal_type<0
       || au->first.temporal_id<0 || au->first.temporal_id>6)return false;
 descriptor_bytes=au->num_slices*sizeof(*out);
 type=(unsigned)au->first.nal_type;temporal=(unsigned)au->first.temporal_id+1;
 if(!(type<=9 || (type>=16 && type<=21)))return false;
 for(i=0;i<au->num_slices;i++)
 {
  const rh265_nal_t *n=&slices[i];
  if(!n->data || n->size<3 || n->size>UINT_MAX-4u
        || total>UINT_MAX-4u-n->size)return false;
  if((n->data[0]&0x81) || (n->data[0]>>1)!=type
        || n->data[1]!=temporal || !!(n->data[2]&0x80)!=(i==0))return false;
  total+=n->size+4;
 }
 if(total-4u*au->num_slices!=au->coded_bytes || total>UINT_MAX-127u)return false;
 padded=(total+127u)&~(size_t)127u;
 if(padded>capacity
       || vaapi_hevc_packet_overlap(data,padded,out,descriptor_bytes)
       || vaapi_hevc_packet_overlap(data,padded,slices,au->num_slices*sizeof(*slices))
       || vaapi_hevc_packet_overlap(out,descriptor_bytes,slices,au->num_slices*sizeof(*slices))
       || vaapi_hevc_packet_overlap(data,padded,au,sizeof(*au))
       || vaapi_hevc_packet_overlap(out,descriptor_bytes,au,sizeof(*au))
       || vaapi_hevc_packet_overlap(data,padded,used,sizeof(*used))
       || vaapi_hevc_packet_overlap(out,descriptor_bytes,used,sizeof(*used)))return false;
 for(i=0;i<au->num_slices;i++)
  if(vaapi_hevc_packet_overlap(data,padded,slices[i].data,slices[i].size)
        || vaapi_hevc_packet_overlap(out,descriptor_bytes,slices[i].data,slices[i].size))return false;
 for(i=0;i<au->num_slices;i++)
 {
  size_t size=slices[i].size;
  out[i].slice_data_offset=(uint32_t)offset;
  out[i].slice_data_size=(uint32_t)(size+4);
  out[i].slice_data_flag=VA_SLICE_DATA_FLAG_ALL;
  data[offset]=data[offset+1]=data[offset+2]=0;data[offset+3]=1;
  memcpy(data+offset+4,slices[i].data,size);offset+=size+4;
 }
 memset(data+offset,0,padded-offset);*used=(unsigned)padded;return true;
}
typedef struct vaapi_hevc_short_work {
 uint8_t *data;
 size_t capacity;
 VAPictureParameterBufferHEVC picture;
 VAIQMatrixBufferHEVC matrix;
 VASliceParameterBufferBase slices[256];
 unsigned used;
} vaapi_hevc_short_work_t;
/* Preallocate work/data on the cold owner, never on the stack. Does not commit
 * the shared DPB plan: commit only after true. False can retain native buffers
 * in t; retire them before retry. Caller retains surface/reference leases. */
static INLINE bool vaapi_hevc_short_submit(vaapi_hevc_submit_t *t,
 const vaapi_hevc_session_t *session,const vaapi_hevc_submit_ops_t *ops,
 const rh265_video_params_t *params,const rh265_access_unit_t *au,
 const rh265_hw_picture_t *picture,const rh265_nal_t *slices,vaapi_hevc_short_work_t *work)
{
 if(!t || t->active || !session || !session->ready || session->closing
       || session->slice_mode!=VA_DEC_SLICE_MODE_BASE || !work || !params)return false;
 if(!vaapi_hevc_picture(&work->picture,params,au,picture,session->surfaces,session->slots))return false;
 if(params->scaling_list_enabled && !vaapi_hevc_matrix(&work->matrix,params))return false;
 if(!vaapi_hevc_short_packet(work->data,work->capacity,work->slices,256,slices,au,&work->used))return false;
 return vaapi_hevc_submit_start(t,session,ops,&work->picture,
  params->scaling_list_enabled?&work->matrix:NULL,work->slices,au->num_slices,work->data,work->used);
}
#endif
