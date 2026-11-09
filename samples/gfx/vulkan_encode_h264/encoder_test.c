/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/vulkan_encode_h264.h"
static unsigned allocations, fail_allocation;
static void *test_calloc(size_t count, size_t bytes)
{
   return ++allocations == fail_allocation ? NULL : calloc(count,bytes);
}
#define calloc test_calloc
#include "../../../gfx/common/vulkan_encode_h264.c"
#undef calloc
#define HANDLE(type,id) ((type)(uintptr_t)(id))
static vulkan_encode_h264_ops_t ops;
static vulkan_encode_h264_open_t config;
static vulkan_encode_h264_session_t session;
static vulkan_encode_pool_t pool;
static uint8_t payload[16][64];
static unsigned creates, pool_creates, destroys, pool_destroys, starts, polls, retires;
static unsigned session_failure, pool_failure, start_failure, uncertain, completed[16], feedback_failure;
static uint64_t native_ready;
static const vulkan_encode_h264_picture_t *last_picture;
static void unused(void) { abort(); }
static void setup(void)
{
   unsigned i;
   allocations=fail_allocation=creates=pool_creates=destroys=pool_destroys=starts=polls=retires=0;
   session_failure=pool_failure=start_failure=uncertain=feedback_failure=0;
   native_ready=0; last_picture=NULL;
   memset(&config,0,sizeof(config)); memset(&ops,0,sizeof(ops));
   memset(&session,0,sizeof(session)); memset(&pool,0,sizeof(pool));
   memset(completed,0,sizeof(completed));
   config.queue=HANDLE(VkQueue,7); config.session.physical=HANDLE(VkPhysicalDevice,8);
   config.session.device=HANDLE(VkDevice,9); config.session.family=4;
   config.session.video.width=192; config.session.video.height=128;
   config.packet_size=1024; config.slots=16; config.gop=32; config.qp=12;
   config.producer_family=0; config.storage_image_extended_formats=true;
   session.device=config.session.device; session.params.coded_width=192; session.params.coded_height=128;
   session.codec.minQp=0; session.codec.maxQp=51;
   pool.count=16; pool.ready=HANDLE(VkSemaphore,10); pool.done=HANDLE(VkSemaphore,11);
   pool.queries=HANDLE(VkQueryPool,12);
   for (i=0;i<2;i++)
   {
      pool.dpb.image[i]=HANDLE(VkImage,20+i); pool.dpb.view[i]=HANDLE(VkImageView,30+i);
   }
   for (i=0;i<16;i++)
   {
      pool.slot[i].image=HANDLE(VkImage,100+i); pool.slot[i].view=HANDLE(VkImageView,200+i);
      pool.slot[i].command=HANDLE(VkCommandBuffer,300+i); pool.slot[i].fence=HANDLE(VkFence,400+i);
      pool.slot[i].output.buffer=HANDLE(VkBuffer,500+i); pool.slot[i].output.mapped=payload[i];
      pool.slot[i].output.size=64; memset(payload[i],i,64);
   }
#define STUB(member,type) ops.submit.member=(type)unused
   STUB(reset_command,PFN_vkResetCommandBuffer); STUB(begin,PFN_vkBeginCommandBuffer);
   STUB(end,PFN_vkEndCommandBuffer); STUB(barrier,PFN_vkCmdPipelineBarrier2);
   STUB(reset_query,PFN_vkCmdResetQueryPool); STUB(begin_query,PFN_vkCmdBeginQuery);
   STUB(end_query,PFN_vkCmdEndQuery); STUB(begin_coding,PFN_vkCmdBeginVideoCodingKHR);
   STUB(control,PFN_vkCmdControlVideoCodingKHR); STUB(encode,PFN_vkCmdEncodeVideoKHR);
   STUB(end_coding,PFN_vkCmdEndVideoCodingKHR); STUB(reset_fence,PFN_vkResetFences);
   STUB(submit,PFN_vkQueueSubmit); STUB(status,PFN_vkGetFenceStatus); STUB(feedback,PFN_vkGetQueryPoolResults);
#undef STUB
}
vulkan_encode_h264_session_t *vulkan_encode_h264_session_new(
      const vulkan_encode_h264_session_config_t *c,const vulkan_encode_h264_session_ops_t *dispatch)
{
   assert(c==&config.session && dispatch==&ops.session); creates++;
   return session_failure ? NULL : &session;
}
void vulkan_encode_h264_session_free(vulkan_encode_h264_session_t *s)
{
   if (s)
   {
      assert(s==&session); destroys++;
   }
}
bool vulkan_encode_h264_session_headers(vulkan_encode_h264_session_t *s,void *data,size_t capacity,size_t *size)
{
   assert(s==&session && data==payload[0] && capacity==64 && size); *size=4; return true;
}
vulkan_encode_pool_t *vulkan_encode_pool_new(const vulkan_encode_pool_config_t *c,const vulkan_encode_pool_ops_t *dispatch)
{
   assert(dispatch==&ops.pool && c->profile==&session.profile && c->capabilities==&session.capabilities);
   assert(c->device==config.session.device && c->physical==config.session.physical);
   assert(c->extent.width==192 && c->extent.height==128 && c->slots==config.slots);
   assert(c->encode_family==4 && c->producer_family==0 && c->storage_image_extended_formats);
   assert(c->format==VK_FORMAT_G8_B8R8_2PLANE_420_UNORM && c->packet_size==1024);
   pool_creates++; pool.count=c->slots; return pool_failure ? NULL : &pool;
}
void vulkan_encode_pool_free(vulkan_encode_pool_t *p)
{
   if (p)
   {
      assert(p==&pool); pool_destroys++;
   }
}
int vulkan_encode_h264_submit_start(vulkan_encode_h264_submit_t *t,
      const vulkan_encode_h264_session_t *s,const vulkan_encode_h264_submit_ops_t *dispatch,
      const vulkan_encode_h264_resources_t *r,const vulkan_encode_h264_picture_t *p,
      const vulkan_encode_h264_picture_t *previous,unsigned mask,bool reset)
{
   unsigned slot=r->query;
   assert(s==&session && dispatch==&ops.submit && slot<pool.count && !t->active);
   assert(r->input==pool.slot[slot].image && r->input_view==pool.slot[slot].view);
   assert(r->mapped==payload[slot] && r->capacity==64 && r->queue==config.queue);
   assert(r->bitstream==pool.slot[slot].output.buffer && r->command==pool.slot[slot].command);
   assert(r->fence==pool.slot[slot].fence && r->queries==pool.queries);
   assert(r->ready==pool.ready && r->done==pool.done && r->ready_value==r->done_value);
   assert(r->ready_value>native_ready && previous==last_picture && reset==!previous);
   assert(r->dpb->image[0]==pool.dpb.image[0] && r->dpb->view[1]==pool.dpb.view[1]);
   assert(p->header.pRefLists==&p->lists && p->nalu.constantQp==12);
   if (previous) assert((mask&(1u<<previous->slot)) && p->slot!=previous->slot);
   starts++; t->active=true; t->resources=r; t->gpu_complete=false;
   t->error=start_failure ? VK_ERROR_UNKNOWN : VK_SUCCESS;
   t->poisoned=start_failure && uncertain;
   if (start_failure)
   {
      t->gpu_complete=!uncertain; return VULKAN_ENCODE_H264_ERROR;
   }
   native_ready=r->ready_value; last_picture=p; return VULKAN_ENCODE_H264_OK;
}
int vulkan_encode_h264_submit_poll(vulkan_encode_h264_submit_t *t,const void **data,size_t *bytes)
{
   unsigned slot=t->resources->query;
   polls++; *data=NULL; *bytes=0;
   if (!completed[slot]) return VULKAN_ENCODE_H264_BUSY;
   t->gpu_complete=true; t->poisoned=false;
   if (t->error!=VK_SUCCESS || feedback_failure) return VULKAN_ENCODE_H264_ERROR;
   *data=payload[slot]; *bytes=32; return VULKAN_ENCODE_H264_OK;
}
bool vulkan_encode_h264_submit_retire(vulkan_encode_h264_submit_t *t)
{
   retires++;
   if (!t->gpu_complete || t->poisoned) return false;
   t->active=false; return true;
}
static void startup(void)
{
   vulkan_encode_h264_t *e;
   size_t bytes;
   setup(); e=vulkan_encode_h264_open(&config,&ops); assert(e && creates==1 && pool_creates==1);
   assert(vulkan_encode_h264_pool(e)==&pool && !vulkan_encode_h264_pool(NULL));
   assert(vulkan_encode_h264_headers(e,payload[0],64,&bytes) && bytes==4);
   assert(vulkan_encode_h264_free(e) && destroys==1 && pool_destroys==1);
   setup(); fail_allocation=1; assert(!vulkan_encode_h264_open(&config,&ops) && !creates);
   setup(); session_failure=1; assert(!vulkan_encode_h264_open(&config,&ops) && !destroys && !pool_creates);
   setup(); pool_failure=1; assert(!vulkan_encode_h264_open(&config,&ops) && destroys==1 && !pool_destroys);
   setup(); session.codec.minQp=13; assert(!vulkan_encode_h264_open(&config,&ops) && destroys==1 && !pool_creates);
   setup(); assert(!vulkan_encode_h264_open(NULL,&ops) && !vulkan_encode_h264_open(&config,NULL));
   config.gop=0; assert(!vulkan_encode_h264_open(&config,&ops) && !allocations);
   assert(vulkan_encode_h264_free(NULL) && vulkan_encode_h264_drain(NULL));
}
static void admission(void)
{
   vulkan_encode_h264_t *e;
   vulkan_encode_packet_t packet;
   unsigned i;
   for (i=0;i<15;i++)
   {
      setup();
      if (!i) ops.submit.reset_command=NULL;
      if (i==1) ops.submit.begin=NULL;
      if (i==2) ops.submit.end=NULL;
      if (i==3) ops.submit.barrier=NULL;
      if (i==4) ops.submit.reset_query=NULL;
      if (i==5) ops.submit.begin_query=NULL;
      if (i==6) ops.submit.end_query=NULL;
      if (i==7) ops.submit.begin_coding=NULL;
      if (i==8) ops.submit.control=NULL;
      if (i==9) ops.submit.encode=NULL;
      if (i==10) ops.submit.end_coding=NULL;
      if (i==11) ops.submit.reset_fence=NULL;
      if (i==12) ops.submit.submit=NULL;
      if (i==13) ops.submit.status=NULL;
      if (i==14) ops.submit.feedback=NULL;
      assert(!vulkan_encode_h264_open(&config,&ops) && !allocations && !creates);
   }
   for (i=0;i<6;i++)
   {
      setup();
      if (!i) config.slots=0;
      if (i==1) config.slots=17;
      if (i==2) config.gop=0;
      if (i==3) config.gop=65537;
      if (i==4) config.qp=52;
      if (i==5) config.queue=VK_NULL_HANDLE;
      assert(!vulkan_encode_h264_open(&config,&ops) && !allocations);
   }
   setup(); e=vulkan_encode_h264_open(&config,&ops); assert(e);
   assert(vulkan_encode_h264_submit(e,16,0,1,1,false)==VULKAN_ENCODE_ERROR);
   assert(vulkan_encode_h264_submit(e,0,0,0,1,false)==VULKAN_ENCODE_ERROR);
   assert(vulkan_encode_h264_submit(e,0,UINT64_MAX,1,1,false)==VULKAN_ENCODE_ERROR);
   assert(vulkan_encode_h264_submit(e,0,0,1,0,false)==VULKAN_ENCODE_ERROR);
   assert(!starts && vulkan_encode_h264_submit(e,0,10,5,10,false)==VULKAN_ENCODE_OK);
   assert(!vulkan_encode_h264_headers(e,payload[0],64,&packet.size));
   assert(vulkan_encode_h264_submit(e,1,14,5,11,false)==VULKAN_ENCODE_ERROR);
   assert(vulkan_encode_h264_submit(e,1,15,5,10,false)==VULKAN_ENCODE_ERROR);
   assert(starts==1 && vulkan_encode_h264_finish(e)==VULKAN_ENCODE_OK);
   assert(vulkan_encode_h264_poll(e,&packet)==VULKAN_ENCODE_BUSY && !vulkan_encode_h264_free(e));
   completed[0]=1; assert(vulkan_encode_h264_poll(e,&packet)==VULKAN_ENCODE_OK);
   assert(vulkan_encode_h264_release(e) && vulkan_encode_h264_poll(e,&packet)==VULKAN_ENCODE_END);
   assert(vulkan_encode_h264_free(e));
}
static void frames(void)
{
   vulkan_encode_h264_t *e;
   vulkan_encode_packet_t packet,again;
   unsigned i,round;
   const void *held;
   setup(); e=vulkan_encode_h264_open(&config,&ops); assert(e);
   assert(vulkan_encode_h264_poll(e,&packet)==VULKAN_ENCODE_BUSY && !polls);
   for (round=0;round<4;round++)
   {
      for (i=0;i<16;i++)
      {
         unsigned ordinal=round*16+i;
         assert(vulkan_encode_h264_submit(e,i,ordinal*100,100,ordinal+1,ordinal==13)==VULKAN_ENCODE_OK);
         assert(vulkan_encode_h264_submit(e,i,ordinal*100,100,ordinal+1,false)==VULKAN_ENCODE_BUSY);
      }
      assert(!vulkan_encode_h264_free(e) && allocations==1);
      for (i=0;i<16;i++)
      {
         unsigned ordinal=round*16+i;
         assert(vulkan_encode_h264_poll(e,&packet)==VULKAN_ENCODE_BUSY);
         completed[i]=1; assert(vulkan_encode_h264_poll(e,&packet)==VULKAN_ENCODE_OK);
         assert(packet.slot==i && packet.data==payload[i] && packet.size==32);
         assert(packet.timestamp==ordinal*100 && packet.duration==100);
         assert(packet.keyframe==(ordinal==0 || ordinal==13 || ordinal==45));
         held=packet.data; assert(vulkan_encode_h264_poll(e,&again)==VULKAN_ENCODE_OK);
         assert(held==again.data && again.timestamp==packet.timestamp);
         assert(vulkan_encode_h264_release(e) && !vulkan_encode_h264_release(e));
         completed[i]=0;
      }
   }
   assert(starts==64 && retires==64 && allocations==1);
   assert(vulkan_encode_h264_finish(e)==VULKAN_ENCODE_OK && vulkan_encode_h264_finish(e)==VULKAN_ENCODE_OK);
   assert(vulkan_encode_h264_poll(e,&packet)==VULKAN_ENCODE_END);
   assert(vulkan_encode_h264_submit(e,0,6400,100,65,false)==VULKAN_ENCODE_ERROR && starts==64);
   assert(vulkan_encode_h264_free(e));
}
static void failures(void)
{
   vulkan_encode_h264_t *e;
   vulkan_encode_packet_t packet;
   unsigned mode;
   for (mode=0;mode<3;mode++)
   {
      setup(); e=vulkan_encode_h264_open(&config,&ops); assert(e);
      start_failure=1; uncertain=mode!=0;
      assert(vulkan_encode_h264_submit(e,0,0,100,1,false)==VULKAN_ENCODE_ERROR);
      assert(!vulkan_encode_h264_free(e));
      assert(vulkan_encode_h264_submit(e,1,100,100,2,false)==VULKAN_ENCODE_ERROR && starts==1);
      if (mode) assert(!vulkan_encode_h264_drain(e) && !vulkan_encode_h264_free(e));
      completed[0]=1; assert(vulkan_encode_h264_drain(e) && vulkan_encode_h264_free(e));
   }
   setup(); e=vulkan_encode_h264_open(&config,&ops); assert(e);
   assert(vulkan_encode_h264_submit(e,0,0,100,1,false)==VULKAN_ENCODE_OK);
   completed[0]=1; assert(vulkan_encode_h264_poll(e,&packet)==VULKAN_ENCODE_OK);
   assert(!vulkan_encode_h264_drain(e) && !vulkan_encode_h264_free(e));
   assert(vulkan_encode_h264_release(e) && vulkan_encode_h264_drain(e) && vulkan_encode_h264_free(e));
   setup(); e=vulkan_encode_h264_open(&config,&ops); assert(e);
   assert(vulkan_encode_h264_submit(e,0,0,100,1,false)==VULKAN_ENCODE_OK);
   completed[0]=feedback_failure=1;
   assert(vulkan_encode_h264_poll(e,&packet)==VULKAN_ENCODE_ERROR);
   assert(!vulkan_encode_h264_free(e) && vulkan_encode_h264_drain(e) && vulkan_encode_h264_free(e));
}
static void cancel_many(void)
{
   vulkan_encode_h264_t *e;
   unsigned i,before;
   setup(); e=vulkan_encode_h264_open(&config,&ops); assert(e);
   for (i=0;i<16;i++)
      assert(vulkan_encode_h264_submit(e,i,i,1,i+1,false)==VULKAN_ENCODE_OK);
   for (i=0;i<16;i+=2) completed[i]=1;
   before=polls; assert(!vulkan_encode_h264_drain(e) && polls-before==16);
   assert(!vulkan_encode_h264_free(e));
   for (i=1;i<16;i+=2) completed[i]=1;
   before=polls; assert(vulkan_encode_h264_drain(e) && polls-before==8);
   assert(vulkan_encode_h264_free(e) && destroys==1 && pool_destroys==1);
}
static void single_slot(void)
{
   vulkan_encode_h264_t *e;
   vulkan_encode_packet_t packet;
   unsigned i;
   setup(); config.slots=config.gop=1; e=vulkan_encode_h264_open(&config,&ops); assert(e);
   for (i=0;i<3;i++)
   {
      assert(vulkan_encode_h264_submit(e,0,i,1,i+1,false)==VULKAN_ENCODE_OK);
      completed[0]=1; assert(vulkan_encode_h264_poll(e,&packet)==VULKAN_ENCODE_OK && packet.keyframe);
      assert(vulkan_encode_h264_release(e));
   }
   assert(vulkan_encode_h264_finish(e)==VULKAN_ENCODE_OK && vulkan_encode_h264_poll(e,&packet)==VULKAN_ENCODE_END);
   assert(vulkan_encode_h264_free(e));
}
int main(void)
{
   startup(); admission(); frames(); failures(); cancel_many(); single_slot();
   puts("Vulkan H264 adapter FIFO/borrow/EOS/error retirement PASS"); return 0;
}
