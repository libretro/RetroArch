/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_hevc.h"
typedef struct codec_case {
 const char *name; const uint8_t *hvcc; size_t hvcc_size;
 const uint8_t *data; size_t size; unsigned depth,full; uint32_t crc[3];
} codec_case_t;
typedef struct sequence_packet {
 int64_t dts; const uint8_t *data; size_t size; uint32_t crc[3];
 unsigned display_index,slice_type;
} sequence_packet_t;
typedef struct sequence_case {
 codec_case_t picture; const sequence_packet_t *packets; unsigned reordered;
} sequence_case_t;
#include "../videotoolbox_hevc/sequence_data.h"
#include "../videotoolbox_hevc/reorder_data.h"
static rh265_video_params_t params;
static rh265_access_unit_t au;
static rh265_hw_picture_t picture;
static VAPictureParameterBufferHEVC native,saved;
static VAIQMatrixBufferHEVC matrix;
static VASurfaceID surfaces[32];
static unsigned parsed;
static void sequence(const sequence_case_t *c)
{
 rh265_video *source=rh265_video_open();
 rh265_hw_state_t state;
 rh265_nal_t slices[8];
 unsigned frame,i;
 assert(source && !rh265_video_set_extradata(source,c->picture.hvcc,c->picture.hvcc_size));
 assert(rh265_hw_state_init(&state,32));
 for(frame=0;frame<8;frame++) {
  const sequence_packet_t *packet=&c->packets[frame];
  assert(!rh265_video_get_access_unit(source,packet->data,packet->size,4,slices,8,&au));
  assert(au.slice_types==(1u<<packet->slice_type));
  assert(!rh265_video_get_params(source,(unsigned)au.first.pps_id,&params));
  assert(!rh265_hw_prepare(&state,&params,&au,&picture));
  assert(vaapi_hevc_picture(&native,&params,&au,&picture,surfaces,32));
  assert(native.CurrPic.picture_id==surfaces[picture.slot]);
  assert(native.CurrPic.pic_order_cnt==(int32_t)packet->display_index);
  assert(native.bit_depth_luma_minus8==c->picture.depth-8);
  assert(native.pic_fields.bits.NoBiPredFlag==(packet->slice_type!=0));
  assert(native.slice_parsing_fields.bits.IntraPicFlag==(frame==0));
  assert(native.slice_parsing_fields.bits.IdrPicFlag==(frame==0));
  assert(native.st_rps_bits==au.first.short_term_rps_bits);
  for(i=0;i<picture.num_refs;i++) {
   assert(native.ReferenceFrames[i].picture_id==surfaces[picture.refs[i].slot]);
   assert(native.ReferenceFrames[i].pic_order_cnt==picture.refs[i].poc);
  }
  for(i=picture.num_refs;i<15;i++)
   assert(native.ReferenceFrames[i].picture_id==VA_INVALID_SURFACE
      && native.ReferenceFrames[i].flags==VA_PICTURE_HEVC_INVALID);
  assert(rh265_hw_commit(&state,&picture));
  state.outputs=0;
  parsed++;
 }
 memset(&au,0xa5,sizeof(au));
 assert(rh265_video_get_access_unit(source,c->packets[0].data,1,4,slices,8,&au)<0);
 assert(!au.slice_types && !au.num_slices);
 rh265_video_close(source);
}
static void references(void)
{
 unsigned i;
 memset(&au,0,sizeof(au)); memset(&picture,0,sizeof(picture));
 params.max_dec_pic_buffering=5; params.max_num_reorder_pics=2;
 params.long_term_ref_pics_present=1;
 picture.width=(unsigned)params.width; picture.height=(unsigned)params.height;
 picture.bit_depth=(unsigned)params.bit_depth; picture.poc_log2=(unsigned)params.log2_max_poc_lsb;
 picture.poc=8; picture.num_refs=4;
 for(i=0;i<4;i++)picture.refs[i].slot=i+1;
 picture.refs[0].poc=7; picture.refs[1].poc=9;
 picture.refs[2].poc=1; picture.refs[2].long_term=1; picture.refs[3].poc=3;
 au.num_slices=1; au.slice_types=2;
 au.first.nal_type=1; au.first.slice_type=1; au.first.poc_lsb=8;
 au.first.num_negative=au.first.num_positive=1;
 au.first.delta_poc_before[0]=-1; au.first.delta_poc_after[0]=1;
 au.first.used_before[0]=au.first.used_after[0]=1;
 au.first.long_term.count=1; au.first.long_term.used[0]=1;
 au.first.long_term.poc_lsb[0]=1; au.first.long_term.msb_present[0]=1;
 assert(vaapi_hevc_picture(&native,&params,&au,&picture,surfaces,32));
 assert(native.ReferenceFrames[0].flags==VA_PICTURE_HEVC_RPS_ST_CURR_BEFORE);
 assert(native.ReferenceFrames[1].flags==VA_PICTURE_HEVC_RPS_ST_CURR_AFTER);
 assert(native.ReferenceFrames[2].flags==(VA_PICTURE_HEVC_RPS_LT_CURR|VA_PICTURE_HEVC_LONG_TERM_REFERENCE));
 assert(!native.ReferenceFrames[3].flags && native.pic_fields.bits.NoBiPredFlag);
 au.num_slices=2; au.slice_types=3;
 assert(vaapi_hevc_picture(&native,&params,&au,&picture,surfaces,32));
 assert(!native.pic_fields.bits.NoBiPredFlag);
 saved=native;
#define REFUSE(expr,restore) do { expr; assert(!vaapi_hevc_picture(&native,&params,&au,&picture,surfaces,32)); \
 assert(!memcmp(&native,&saved,sizeof(native))); restore; } while(0)
 REFUSE(picture.num_refs=16,picture.num_refs=4);
 REFUSE(picture.refs[0].slot=32,picture.refs[0].slot=1);
 REFUSE(surfaces[1]=surfaces[0],surfaces[1]=101);
 REFUSE(picture.refs[1].poc=7,picture.refs[1].poc=9);
 REFUSE(au.first.long_term.poc_lsb[0]=2,au.first.long_term.poc_lsb[0]=1);
 REFUSE(params.tiles_enabled=1,params.tiles_enabled=0);
 REFUSE(params.pcm_enabled=1,params.pcm_enabled=0);
 REFUSE(params.dependent_slice_segments_enabled=1,params.dependent_slice_segments_enabled=0);
 REFUSE(params.log2_min_cb=31,params.log2_min_cb=3);
 REFUSE(au.slice_types=8,au.slice_types=3);
#undef REFUSE
}
static void matrices(void)
{
 unsigned i,j;
 params.scaling_list_enabled=1;
 for(i=0;i<6;i++) {
  for(j=0;j<16;j++)params.list4[i][j]=(uint8_t)(1+(i*37+j*19)%255);
  for(j=0;j<64;j++) {
   params.list8[i][j]=(uint8_t)(1+(i*41+j*23)%255);
   params.list16[i][j]=(uint8_t)(1+(i*43+j*29)%255);
   if(i<2)params.list32[i][j]=(uint8_t)(1+(i*47+j*31)%255);
  }
 }
 memset(params.dc16,53,sizeof(params.dc16)); memset(params.dc32,61,sizeof(params.dc32));
 assert(vaapi_hevc_matrix(&matrix,&params));
 assert(!memcmp(matrix.ScalingList4x4,params.list4,sizeof(params.list4)));
 assert(!memcmp(matrix.ScalingList8x8,params.list8,sizeof(params.list8)));
 assert(!memcmp(matrix.ScalingList16x16,params.list16,sizeof(params.list16)));
 assert(!memcmp(matrix.ScalingList32x32,params.list32,sizeof(params.list32)));
 assert(!memcmp(matrix.ScalingListDC16x16,params.dc16,sizeof(params.dc16)));
 assert(!memcmp(matrix.ScalingListDC32x32,params.dc32,sizeof(params.dc32)));
 for(i=0;i<VA_PADDING_LOW;i++)assert(!matrix.va_reserved[i]);
 params.scaling_list_enabled=0;
 assert(!vaapi_hevc_matrix(&matrix,&params));
}
int main(void)
{
 unsigned i;
 for(i=0;i<32;i++)surfaces[i]=100+i;
 for(i=0;i<sizeof(sequence_cases)/sizeof(sequence_cases[0]);i++)sequence(&sequence_cases[i]);
 for(i=0;i<sizeof(reorder_cases)/sizeof(reorder_cases[0]);i++)sequence(&reorder_cases[i]);
 references(); matrices();
 printf("PASS: VA-API HEVC native picture/matrix buffers, %u parsed Main/Main10 I/P/B pictures; no device execution\n",parsed);
 return 0;
}
