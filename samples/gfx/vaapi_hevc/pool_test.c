/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define main vaapi_packet_reference_main
#include "test.c"
#undef main
#include "../../../gfx/common/vaapi_hevc_pool.c"
static vaapi_hevc_pool_t po_pool;
static vaapi_hevc_session_t po_session;
static vaapi_hevc_short_work_t po_work;
static uint8_t po_data[65536];
static unsigned po_live[4096],po_done[32],po_count,po_drops,po_checks,po_target,po_fault,po_drop_fault;
static VAStatus po_create(VADisplay display,VAContextID context,VABufferType type,
 unsigned bytes,unsigned count,void *data,VABufferID *id)
{
 assert(display==po_session.display && context==po_session.context && bytes && count && data && po_count<4096);
 if(type==VAPictureParameterBufferType)po_target=((VAPictureParameterBufferHEVC*)data)->CurrPic.picture_id-100;
 assert(po_target<32);*id=po_count;po_live[po_count++]=1;
 return po_fault==1?VA_STATUS_ERROR_OPERATION_FAILED:VA_STATUS_SUCCESS;
}
static VAStatus po_destroy(VADisplay display,VABufferID id)
{
 assert(display==po_session.display && id<po_count && po_live[id]);
 if(id+1==po_drop_fault)return VA_STATUS_ERROR_OPERATION_FAILED;
 po_live[id]=0;po_drops++;return VA_STATUS_SUCCESS;
}
static VAStatus po_begin(VADisplay display,VAContextID context,VASurfaceID surface)
{
 assert(display==po_session.display && context==po_session.context && surface==100+po_target);
 po_done[po_target]=0;return po_fault==2?VA_STATUS_ERROR_OPERATION_FAILED:VA_STATUS_SUCCESS;
}
static VAStatus po_render(VADisplay display,VAContextID context,VABufferID *ids,int count)
{
 int i;assert(display==po_session.display && context==po_session.context && count>=3 && count<=4);
 for(i=0;i<count;i++)assert(ids[i]<po_count && po_live[ids[i]]);
 return po_fault==3?VA_STATUS_ERROR_OPERATION_FAILED:VA_STATUS_SUCCESS;
}
static VAStatus po_end(VADisplay display,VAContextID context)
{assert(display==po_session.display && context==po_session.context);return po_fault==4?VA_STATUS_ERROR_OPERATION_FAILED:VA_STATUS_SUCCESS;}
static VAStatus po_sync(VADisplay display,VASurfaceID surface,uint64_t timeout)
{
 assert(display==po_session.display && surface>=100 && surface<132 && !timeout);po_checks++;
 if(po_fault==5)return VA_STATUS_ERROR_INVALID_DISPLAY;
 return po_done[surface-100]?VA_STATUS_SUCCESS:VA_STATUS_ERROR_TIMEDOUT;
}
static const vaapi_hevc_submit_ops_t po_ops={po_create,po_destroy,po_begin,po_render,po_end,po_sync};
static rh265_video_params_t po_params,po_first_params;
static rh265_access_unit_t po_au,po_first_au;
static rh265_hw_picture_t po_picture,po_first_picture;
static rh265_nal_t po_slices[8],po_first_slices[8];
static void po_setup(unsigned depth)
{
 unsigned i;
 assert(!po_pool.active);
 memset(&po_session,0,sizeof(po_session));memset(po_live,0,sizeof(po_live));memset(po_done,0,sizeof(po_done));
 po_count=po_drops=po_checks=po_fault=po_drop_fault=0;
 po_session.display=(VADisplay)(uintptr_t)1;po_session.context=7;po_session.ready=true;
 po_session.width=128;po_session.height=96;po_session.depth=depth;po_session.slots=32;po_session.slice_mode=VA_DEC_SLICE_MODE_BASE;
 for(i=0;i<32;i++)po_session.surfaces[i]=100+i;
 po_work.data=po_data;po_work.capacity=sizeof(po_data);
 assert(vaapi_hevc_pool_init(&po_pool,&po_session,&po_ops,&po_work));
}
static void po_sequence(const sequence_case_t *fixture)
{
 rh265_video *source=rh265_video_open();
 rh265_hw_state_t state;
 unsigned i,slots[8],before,frames=8;
 uint64_t tickets[8],old,new_ticket;
 uint32_t reads=0;
 assert(source && !rh265_video_set_extradata(source,fixture->picture.hvcc,fixture->picture.hvcc_size));
 assert(rh265_hw_state_init(&state,32));po_setup(fixture->picture.depth);
 for(i=0;i<frames;i++) {
  const sequence_packet_t *packet=&fixture->packets[i];
  assert(!rh265_video_get_access_unit(source,packet->data,packet->size,4,po_slices,8,&po_au));
  assert(!rh265_video_get_params(source,(unsigned)po_au.first.pps_id,&po_params));
  if(!i && (unsigned)po_params.max_dec_pic_buffering<frames)frames=(unsigned)po_params.max_dec_pic_buffering;
  po_session.width=(unsigned)po_params.width;po_session.height=(unsigned)po_params.height;
  assert(!rh265_hw_prepare(&state,&po_params,&po_au,&po_picture));slots[i]=po_picture.slot;
  if(!i){po_first_params=po_params;po_first_au=po_au;po_first_picture=po_picture;memcpy(po_first_slices,po_slices,sizeof(po_slices));}
  assert(!vaapi_hevc_pool_decode(&po_pool,&po_params,&po_au,&po_picture,po_slices));
  assert(rh265_hw_commit(&state,&po_picture));
  assert(vaapi_hevc_pool_poll(&po_pool,slots[i])==1);
  assert(!vaapi_hevc_pool_acquire(&po_pool,slots[i],&tickets[i]));
  reads|=po_pool.reads[slots[i]];
 }
 assert(reads && !vaapi_hevc_pool_close(&po_pool));
 po_first_picture.serial=frames+1;before=po_count;
 assert(vaapi_hevc_pool_decode(&po_pool,&po_first_params,&po_first_au,&po_first_picture,po_first_slices)==1 && po_count==before);
 for(i=0;i<frames;i++)po_done[slots[i]]=1;
 /* Complete an earlier picture while later GPU readers still retain it. */
 for(i=0;i<frames;i++)if(reads&(UINT32_C(1)<<slots[i])) {
  po_first_picture.slot=slots[i];assert(!vaapi_hevc_pool_poll(&po_pool,slots[i]));
  if(vaapi_hevc_pool_held(&po_pool)&(UINT32_C(1)<<slots[i])) {
   assert(vaapi_hevc_pool_decode(&po_pool,&po_first_params,&po_first_au,&po_first_picture,po_first_slices)==1);break;
  }
 }
 assert(i<frames);
 for(i=frames;i>0;i--) {
  assert(!vaapi_hevc_pool_poll(&po_pool,slots[i-1]));
  assert(vaapi_hevc_pool_acquire(&po_pool,slots[i-1],&tickets[i-1]));
  assert(!vaapi_hevc_pool_acquire(&po_pool,slots[i-1],&new_ticket));
 }
 assert(po_count==po_drops && !vaapi_hevc_pool_close(&po_pool));
 po_first_picture.slot=slots[0];old=tickets[0];
 assert(vaapi_hevc_pool_decode(&po_pool,&po_first_params,&po_first_au,&po_first_picture,po_first_slices)==1);
 for(i=0;i<frames;i++)assert(vaapi_hevc_pool_release(&po_pool,slots[i],tickets[i]));
 assert(!vaapi_hevc_pool_release(&po_pool,slots[0],old));
 assert(!vaapi_hevc_pool_held(&po_pool));
 assert(!vaapi_hevc_pool_decode(&po_pool,&po_first_params,&po_first_au,&po_first_picture,po_first_slices));
 assert(!vaapi_hevc_pool_release(&po_pool,slots[0],old));
 po_done[slots[0]]=1;po_drop_fault=po_count;
 assert(!vaapi_hevc_pool_poll(&po_pool,slots[0]) && po_pool.transactions[slots[0]].active);
 before=po_checks;po_drop_fault=0;
 assert(!vaapi_hevc_pool_poll(&po_pool,slots[0]) && po_checks==before);
 assert(vaapi_hevc_pool_acquire(&po_pool,slots[0],&new_ticket) && new_ticket!=old);
 assert(!vaapi_hevc_pool_release(&po_pool,slots[0],old));
 assert(vaapi_hevc_pool_release(&po_pool,slots[0],new_ticket));
 assert(vaapi_hevc_pool_close(&po_pool) && po_count==po_drops);
 /* Cold reset must not recycle an exported incarnation number. */
 assert(vaapi_hevc_pool_init(&po_pool,&po_session,&po_ops,&po_work));po_first_picture.serial=1;
 assert(!vaapi_hevc_pool_decode(&po_pool,&po_first_params,&po_first_au,&po_first_picture,po_first_slices));
 po_done[slots[0]]=1;assert(!vaapi_hevc_pool_poll(&po_pool,slots[0]));
 assert(vaapi_hevc_pool_acquire(&po_pool,slots[0],&old) && old!=new_ticket);
 assert(!vaapi_hevc_pool_release(&po_pool,slots[0],new_ticket));
 assert(vaapi_hevc_pool_release(&po_pool,slots[0],old) && vaapi_hevc_pool_close(&po_pool));
 rh265_video_close(source);
}
static void po_failures(void)
{
 unsigned fault;
 uint64_t ticket;
 VADisplay display;
 for(fault=1;fault<=5;fault++) {
  po_setup(po_first_params.bit_depth);po_session.width=(unsigned)po_first_params.width;po_session.height=(unsigned)po_first_params.height;
  po_first_picture.serial=1;po_fault=fault;
  if(fault==5){assert(!vaapi_hevc_pool_decode(&po_pool,&po_first_params,&po_first_au,&po_first_picture,po_first_slices));assert(vaapi_hevc_pool_poll(&po_pool,po_first_picture.slot)<0);}
  else assert(vaapi_hevc_pool_decode(&po_pool,&po_first_params,&po_first_au,&po_first_picture,po_first_slices)<0);
  assert(po_pool.broken && !vaapi_hevc_pool_close(&po_pool));
  assert(!vaapi_hevc_pool_terminated(&po_pool,(VADisplay)(uintptr_t)2));
  assert(vaapi_hevc_pool_terminated(&po_pool,po_session.display));
 }
 po_setup(po_first_params.bit_depth);po_session.width=(unsigned)po_first_params.width;po_session.height=(unsigned)po_first_params.height;
 po_first_picture.serial=1;po_first_picture.slot=0;
 assert(!vaapi_hevc_pool_decode(&po_pool,&po_first_params,&po_first_au,&po_first_picture,po_first_slices));
 po_done[0]=1;assert(!vaapi_hevc_pool_poll(&po_pool,0));
 assert(vaapi_hevc_pool_acquire(&po_pool,0,&ticket));
 po_first_picture.serial=2;po_first_picture.slot=1;po_fault=2;
 assert(vaapi_hevc_pool_decode(&po_pool,&po_first_params,&po_first_au,&po_first_picture,po_first_slices)<0);
 assert(!vaapi_hevc_pool_terminated(&po_pool,po_session.display));
 assert(vaapi_hevc_pool_release(&po_pool,0,ticket));
 display=po_session.display;memset(&po_session,0,sizeof(po_session));
 assert(vaapi_hevc_pool_terminated(&po_pool,display));
}
static void po_bounds(void)
{
 uint64_t ticket,generation;
 unsigned before;
 po_setup(po_first_params.bit_depth);po_session.width=(unsigned)po_first_params.width;po_session.height=(unsigned)po_first_params.height;
 po_first_picture.serial=1;po_first_picture.slot=31;po_first_picture.num_refs=0;
 assert(!vaapi_hevc_pool_decode(&po_pool,&po_first_params,&po_first_au,&po_first_picture,po_first_slices));
 assert(vaapi_hevc_pool_held(&po_pool)==(UINT32_C(1)<<31));
 before=po_count;
 assert(vaapi_hevc_pool_decode(&po_pool,&po_first_params,&po_first_au,&po_first_picture,po_first_slices)<0 && po_count==before);
 po_first_picture.serial=2;po_first_picture.slot=0;po_first_picture.num_refs=1;
 po_first_picture.refs[0].slot=31;po_first_picture.refs[0].poc=po_first_picture.poc+1;
 assert(vaapi_hevc_pool_decode(&po_pool,&po_first_params,&po_first_au,&po_first_picture,po_first_slices)<0 && po_count==before);
 po_first_picture.num_refs=0;generation=po_pool.next_generation;po_pool.next_generation=(uint64_t)-1;
 assert(vaapi_hevc_pool_decode(&po_pool,&po_first_params,&po_first_au,&po_first_picture,po_first_slices)<0 && po_count==before);
 po_pool.next_generation=generation;po_done[31]=1;
 assert(!vaapi_hevc_pool_poll(&po_pool,31) && vaapi_hevc_pool_acquire(&po_pool,31,&ticket));
 assert(!vaapi_hevc_pool_acquire(&po_pool,32,&generation));
 assert(vaapi_hevc_pool_release(&po_pool,31,ticket) && vaapi_hevc_pool_close(&po_pool));
}
int main(void)
{
 unsigned i;
 for(i=0;i<sizeof(sequence_cases)/sizeof(sequence_cases[0]);i++)po_sequence(&sequence_cases[i]);
 for(i=0;i<sizeof(reorder_cases)/sizeof(reorder_cases[0]);i++)po_sequence(&reorder_cases[i]);
 po_failures();
 po_bounds();
 puts("PASS: VA in-flight reads/writes, DPB picture identity, exported incarnations, backpressure and cold recovery; no device execution");
 return 0;
}
