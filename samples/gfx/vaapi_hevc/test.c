/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_hevc.h"
#include "../../../gfx/common/vaapi_hevc_packet.h"
#include "../../../gfx/common/vaapi_hevc_submit.c"
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
static uint8_t packed[65536],packet_saved[65536];
static VASliceParameterBufferBase short_slices[8],short_saved[8];
static unsigned packed_size,packet_creates,packet_drops,packet_polls;
static vaapi_hevc_submit_t packet_tx;
static vaapi_hevc_session_t packet_session;
static vaapi_hevc_short_work_t packet_work;
static VAStatus packet_create(VADisplay d,VAContextID c,VABufferType type,
 unsigned bytes,unsigned elements,void *data,VABufferID *id)
{
 assert(d==packet_session.display && c==packet_session.context && data);
 if(type==VAPictureParameterBufferType)assert(bytes==sizeof(native) && elements==1 && !memcmp(data,&native,bytes));
 else if(type==VAIQMatrixBufferType)assert(bytes==sizeof(matrix) && elements==1 && !memcmp(data,&matrix,bytes));
 else if(type==VASliceParameterBufferType)assert(bytes==sizeof(short_slices[0]) && elements==au.num_slices && !memcmp(data,short_slices,bytes*elements));
 else assert(type==VASliceDataBufferType && bytes==packed_size && elements==1 && !memcmp(data,packed,bytes));
 *id=1000+packet_creates++;return VA_STATUS_SUCCESS;
}
static VAStatus packet_destroy(VADisplay d,VABufferID id)
{assert(d==packet_session.display && id==1000+packet_drops++ && packet_tx.complete);return VA_STATUS_SUCCESS;}
static VAStatus packet_begin(VADisplay d,VAContextID c,VASurfaceID surface)
{assert(d==packet_session.display && c==packet_session.context && surface==native.CurrPic.picture_id);return VA_STATUS_SUCCESS;}
static VAStatus packet_render(VADisplay d,VAContextID c,VABufferID *ids,int count)
{
 unsigned i;assert(d==packet_session.display && c==packet_session.context && (unsigned)count==packet_creates);
 for(i=0;i<(unsigned)count;i++)assert(ids[i]==1000+i);
 return VA_STATUS_SUCCESS;
}
static VAStatus packet_end(VADisplay d,VAContextID c)
{assert(d==packet_session.display && c==packet_session.context);return VA_STATUS_SUCCESS;}
static VAStatus packet_sync(VADisplay d,VASurfaceID surface,uint64_t timeout)
{assert(d==packet_session.display && surface==native.CurrPic.picture_id && !timeout);return packet_polls++<2?VA_STATUS_ERROR_TIMEDOUT:VA_STATUS_SUCCESS;}
static const vaapi_hevc_submit_ops_t packet_ops={packet_create,packet_destroy,packet_begin,packet_render,packet_end,packet_sync};
static rh265_nal_t many_slices[256];
static VASliceParameterBufferBase many_short[256];
static void packet_limits(void)
{
 uint8_t first[]={38,1,128},next[]={38,1,64};
 rh265_nal_t slices[2];
 unsigned i,size;
 memset(&au,0,sizeof(au));au.num_slices=2;au.coded_bytes=6;au.first.nal_type=19;
 slices[0].data=first;slices[1].data=next;slices[0].size=slices[1].size=3;
 memset(packed,0xa5,sizeof(packed));memset(short_slices,0xa5,sizeof(short_slices));
 memcpy(packet_saved,packed,sizeof(packed));memcpy(short_saved,short_slices,sizeof(short_slices));
#define BAD_PACKET(expr,restore) do {expr;size=99; \
 assert(!vaapi_hevc_short_packet(packed,sizeof(packed),short_slices,8,slices,&au,&size) && !size); \
 assert(!memcmp(packed,packet_saved,sizeof(packed)) && !memcmp(short_slices,short_saved,sizeof(short_slices)));restore;} while(0)
 BAD_PACKET(next[0]|=0x80,next[0]=38);
 BAD_PACKET(next[0]=40,next[0]=38);
 BAD_PACKET(next[1]=0,next[1]=1);
 BAD_PACKET(next[1]=9,next[1]=1);
 BAD_PACKET(next[2]=128,next[2]=64);
 BAD_PACKET(first[2]=0,first[2]=128);
 BAD_PACKET(slices[1].size=2,slices[1].size=3);
 BAD_PACKET(slices[1].size=UINT_MAX,slices[1].size=3);
 BAD_PACKET(slices[1].data=NULL,slices[1].data=next);
 BAD_PACKET(au.coded_bytes=5,au.coded_bytes=6);
 BAD_PACKET(au.num_slices=0,au.num_slices=2);
 BAD_PACKET(au.num_slices=257,au.num_slices=2);
 BAD_PACKET(au.first.nal_type=10,au.first.nal_type=19);
 BAD_PACKET(au.first.temporal_id=7,au.first.temporal_id=0);
#undef BAD_PACKET
 assert(!vaapi_hevc_short_packet(packed,127,short_slices,8,slices,&au,&size));
 assert(!vaapi_hevc_short_packet(packed,sizeof(packed),short_slices,1,slices,&au,&size));
 memcpy(packed,next,3);memcpy(packet_saved,packed,sizeof(packed));
 slices[1].data=packed;
 assert(!vaapi_hevc_short_packet(packed,sizeof(packed),short_slices,8,slices,&au,&size));
 assert(!memcmp(packed,packet_saved,sizeof(packed)) && !memcmp(short_slices,short_saved,sizeof(short_slices)));
 slices[1].data=next;
 assert(!vaapi_hevc_short_packet(packed,sizeof(packed),(VASliceParameterBufferBase*)packed,8,slices,&au,&size));
 assert(!memcmp(packed,packet_saved,sizeof(packed)));
 au.num_slices=256;au.coded_bytes=768;
 for(i=0;i<256;i++){many_slices[i].data=i?next:first;many_slices[i].size=3;}
 assert(vaapi_hevc_short_packet(packed,sizeof(packed),many_short,256,many_slices,&au,&size) && size==1792);
 assert(many_short[255].slice_data_offset==1785 && many_short[255].slice_data_size==7);
}
static void packet_check(rh265_video *source,const rh265_nal_t *slices)
{
 rh265_nal_t roundtrip[8];
 rh265_access_unit_t check;
 unsigned i;
 size_t end=0;
 assert(vaapi_hevc_short_packet(packed,sizeof(packed),short_slices,8,slices,&au,&packed_size));
 assert(packed_size && !(packed_size&127));
 for(i=0;i<au.num_slices;i++) {
  const VASliceParameterBufferBase *s=&short_slices[i];
  assert(s->slice_data_offset==end && s->slice_data_size==slices[i].size+4 && !s->slice_data_flag);
  assert(!packed[end] && !packed[end+1] && !packed[end+2] && packed[end+3]==1);
  assert(!memcmp(packed+end+4,slices[i].data,slices[i].size));end+=s->slice_data_size;
 }
 for(;end<packed_size;end++)assert(!packed[end]);
 assert(!rh265_video_get_access_unit(source,packed,packed_size,0,roundtrip,8,&check));
 assert(check.num_slices==au.num_slices && check.slice_types==au.slice_types && check.coded_bytes==au.coded_bytes);
 for(i=0;i<au.num_slices;i++)assert(roundtrip[i].size==slices[i].size && !memcmp(roundtrip[i].data,slices[i].data,slices[i].size));
 memset(&packet_session,0,sizeof(packet_session));packet_session.display=(VADisplay)(uintptr_t)1;
 packet_session.context=7;packet_session.ready=true;packet_session.slice_mode=VA_DEC_SLICE_MODE_BASE;
 packet_session.width=picture.width;packet_session.height=picture.height;packet_session.depth=picture.bit_depth;
 packet_session.slots=32;memcpy(packet_session.surfaces,surfaces,sizeof(surfaces));
 packet_creates=packet_drops=packet_polls=0;
 if(params.scaling_list_enabled)assert(vaapi_hevc_matrix(&matrix,&params));
 packet_work.data=packed;packet_work.capacity=sizeof(packed);
 packet_session.slice_mode=VA_DEC_SLICE_MODE_NORMAL;
 assert(!vaapi_hevc_short_submit(&packet_tx,&packet_session,&packet_ops,&params,&au,&picture,slices,&packet_work));
 assert(!packet_creates && !packet_tx.active);
 packet_session.slice_mode=VA_DEC_SLICE_MODE_BASE;
 assert(vaapi_hevc_short_submit(&packet_tx,&packet_session,&packet_ops,&params,&au,&picture,slices,&packet_work));
 assert(!vaapi_hevc_submit_retire(&packet_tx));
 assert(vaapi_hevc_submit_poll(&packet_tx)==1 && vaapi_hevc_submit_poll(&packet_tx)==1);
 assert(!vaapi_hevc_submit_poll(&packet_tx) && vaapi_hevc_submit_retire(&packet_tx));
 assert(packet_creates==packet_drops);
}
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
  packet_check(source,slices);
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
 references(); matrices();packet_limits();
 printf("PASS: VA-API HEVC picture/matrix/BASE packet and submission, %u parsed Main/Main10 I/P/B pictures; no device execution\n",parsed);
 return 0;
}
