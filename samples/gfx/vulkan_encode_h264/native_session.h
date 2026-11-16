/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NATIVE_ENCODE_SESSION_H
#define NATIVE_ENCODE_SESSION_H
#include "../../../record/record_gpu_dispatch_vulkan.h"
#ifdef _WIN32
#include <windows.h>
static void native_record_pause(void) { Sleep(1); }
#else
#include <time.h>
static void native_record_pause(void)
{
   struct timespec delay;
   delay.tv_sec=0;
   delay.tv_nsec=1000000;
   nanosleep(&delay,NULL);
}
#endif
struct native_record_owner
{
   record_gpu_session_vulkan_t *session;
   record_gpu_dispatch_vulkan_t *job;
};
static PFN_vkQueueSubmit native_session_submit;
static VkFence native_session_fence;
static unsigned native_session_submissions;

static VKAPI_ATTR VkResult VKAPI_CALL session_submit(VkQueue queue,
      uint32_t count,const VkSubmitInfo *submits,VkFence fence)
{
   VkResult result=native_session_submit(queue,count,submits,fence);
   if (result==VK_SUCCESS && fence)
   {
      assert(!native_session_fence);
      native_session_fence=fence;
      native_session_submissions++;
   }
   return result;
}
static int session_step_native(struct native_encoder *c,struct native_record_owner *owner)
{
   int result;
   if (owner->job)
   {
      enum record_gpu_dispatch_vulkan_state state=record_gpu_dispatch_vulkan_status(owner->job,NULL,NULL);
      if (state==RECORD_VKD_DONE) return RECORD_VULKAN_OUTPUT_END;
      if (state==RECORD_VKD_FAILED) return RECORD_VULKAN_OUTPUT_ERROR;
      native_record_pause();
      return RECORD_VULKAN_OUTPUT_BUSY;
   }
   result=record_gpu_session_vulkan_step(owner->session);
   if (native_session_fence)
   {
      PFN_vkWaitForFences wait_fence=(PFN_vkWaitForFences)c->device_proc(c->device,"vkWaitForFences");
      /* Native fixture orchestration only; session service never waits. */
      assert(wait_fence(c->device,1,&native_session_fence,VK_TRUE,2000000000)==VK_SUCCESS);
      native_session_fence=VK_NULL_HANDLE;
   }
   return result;
}
static bool session_finish_native(struct native_record_owner *owner)
{
   return owner->job ? record_gpu_dispatch_vulkan_audio_finish(owner->job)
      && record_gpu_dispatch_vulkan_video_finish(owner->job)
      : record_gpu_session_vulkan_audio_finish(owner->session)
      && record_gpu_session_vulkan_video_finish(owner->session);
}
static void session_free_native(struct native_record_owner *owner)
{
   unsigned retry;
   for (retry=0;retry<10000;retry++)
   {
      if (owner->job ? record_gpu_dispatch_vulkan_free(owner->job)
            : record_gpu_session_vulkan_free(owner->session)) return;
      native_record_pause();
   }
   assert(!"native owner did not retire");
}
static int session_pictures(struct native_encoder *c,const char *path,bool direct,bool dispatch)
{
   record_gpu_output_vulkan_config_t output;
   struct native_record_owner owner;
   const vulkan_encode_pool_t *pool;
   record_gpu_queue_t *queue;
   record_gpu_frame_t descriptor;
   VkSemaphoreWaitInfo wait;
   PFN_vkWaitSemaphores wait_ready;
   int16_t *pcm=(int16_t*)malloc(1600*2*sizeof(*pcm));
   uint64_t ready=12;
   unsigned frame,i,retry;
   int result;
   assert(pcm);
   if (dispatch && c->producer_queue==c->queue)
   {
      free(pcm);
      puts("UNAVAILABLE independent encode and producer queues");
      return 77;
   }
   memset(&output,0,sizeof(output));
   output.video.max_packet_size=c->config.packet_size;
   output.audio_rate=48000;
   output.audio_layout=3;
   output.audio_kbps=192;
   output.audio_fifo_frames=4800;
   c->config.slots=2;
   c->config.gop=4;
   c->config.storage_image_extended_formats=direct
      && c->features.features.shaderStorageImageExtendedFormats;
   memset(&owner,0,sizeof(owner));
   if (dispatch)
   {
      owner.job=record_gpu_dispatch_vulkan_new_capture(&c->config,&c->ops,&output,path,&c->capture_ops);
      assert(owner.job);
      for (retry=0;retry<10000;retry++)
      {
         enum record_gpu_dispatch_vulkan_state state=record_gpu_dispatch_vulkan_status(owner.job,NULL,NULL);
         if (state==RECORD_VKD_READY) break;
         assert(state==RECORD_VKD_STARTING);
         native_record_pause();
      }
      assert(retry<10000);
      pool=record_gpu_dispatch_vulkan_pool(owner.job);
      c->capture=record_gpu_dispatch_vulkan_capture(owner.job);
      c->capture_borrowed=true;
      assert(c->capture);
      c->renderer=record_gpu_renderer_vulkan_new(owner.job,1,4,
            (PFN_vkGetSemaphoreCounterValue)c->device_proc(c->device,
               c->api_version<VK_API_VERSION_1_2?"vkGetSemaphoreCounterValueKHR":"vkGetSemaphoreCounterValue"));
      assert(c->renderer);
      queue=NULL;
   }
   else
   {
      native_session_submit=c->ops.submit.submit;
      c->ops.submit.submit=session_submit;
      owner.session=record_gpu_session_vulkan_new(&c->config,&c->ops,&output,path);
      assert(owner.session && record_gpu_session_vulkan_error(owner.session,NULL)==RECORD_VULKAN_SESSION_NONE);
      pool=record_gpu_session_vulkan_pool(owner.session);
      queue=record_gpu_session_vulkan_queue(owner.session);
      assert(queue);
   }
   assert(pool);
   if (direct && !pool->storage_images)
   {
      if (c->renderer)
      {
         assert(record_gpu_renderer_vulkan_finish(c->renderer,false)==RECORD_GPU_QUEUE_OK);
         assert(record_gpu_renderer_vulkan_free(c->renderer));
         c->renderer=NULL;
      }
      if (owner.job) record_gpu_dispatch_vulkan_cancel(owner.job);
      else record_gpu_session_vulkan_cancel(owner.session);
      assert(session_finish_native(&owner));
      for (retry=0;retry<1000;retry++)
         if (session_step_native(c,&owner)==RECORD_VULKAN_OUTPUT_ERROR) break;
      assert(retry<1000);
      session_free_native(&owner);
      free(pcm);
      puts("UNAVAILABLE direct storage session format");
      return 77;
   }
   if (!direct) assert(!pool->storage_images);
   capture_new(c,pool);
   if (c->renderer)
      capture_discard_native(c,pool);
   for (frame=0;frame<12;frame++)
   {
      if (!owner.job)
      {
         for (retry=0;retry<10000;retry++)
         {
            result=record_gpu_queue_acquire(queue,&descriptor.slot);
            if (result==RECORD_GPU_QUEUE_OK) break;
            assert(result==RECORD_GPU_QUEUE_BUSY);
            assert(session_step_native(c,&owner)!=RECORD_VULKAN_OUTPUT_ERROR);
         }
         assert(retry<10000 && descriptor.slot==frame%2);
      }
      capture_frame(c,pool,frame);
      if (!owner.job)
      {
         descriptor.timestamp=frame;
         descriptor.duration=1;
         descriptor.ready_value=frame+1;
         descriptor.keyframe=frame%4==0;
         assert(record_gpu_queue_submit(queue,&descriptor));
      }
      for (i=0;i<1600;i++)
      {
         double phase=6.2831853071795864769*(frame*1600+i)/48000.0;
         pcm[i*2]=(int16_t)(6553*sin(phase*1000));
         pcm[i*2+1]=(int16_t)(6553*sin(phase*2300));
      }
      for (retry=0;retry<10000;retry++)
      {
         result=owner.job ? record_gpu_dispatch_vulkan_audio(owner.job,pcm,1600)
            : record_gpu_session_vulkan_audio(owner.session,pcm,1600);
         if (result==RECORD_VULKAN_OUTPUT_PROGRESS) break;
         assert(result==RECORD_VULKAN_OUTPUT_BUSY);
         assert(session_step_native(c,&owner)!=RECORD_VULKAN_OUTPUT_ERROR);
      }
      assert(retry<10000);
   }
   if (c->renderer)
   {
      for (retry=0;retry<10000;retry++)
      {
         result=record_gpu_renderer_vulkan_finish(c->renderer,false);
         if (result==RECORD_GPU_QUEUE_OK) break;
         assert(result==RECORD_GPU_QUEUE_BUSY);
         native_record_pause();
      }
      assert(retry<10000 && c->producer_completion[1]==13);
      puts("PASS native renderer reset/discard and ready-value gap");
   }
   else
   {
      wait_ready=(PFN_vkWaitSemaphores)c->device_proc(c->device,
            c->api_version<VK_API_VERSION_1_2?"vkWaitSemaphoresKHR":"vkWaitSemaphores");
      assert(wait_ready);
      memset(&wait,0,sizeof(wait));
      wait.sType=VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
      wait.semaphoreCount=1;
      wait.pSemaphores=&pool->ready;
      wait.pValues=&ready;
      assert(wait_ready(c->device,&wait,2000000000)==VK_SUCCESS);
   }
   c->ops.pool.destroy_commands(c->device,c->producer_commands,NULL);
   capture_free(c);
   if (c->renderer)
   {
      assert(record_gpu_renderer_vulkan_finish(c->renderer,false)==RECORD_GPU_QUEUE_OK);
      assert(record_gpu_renderer_vulkan_free(c->renderer));
      c->renderer=NULL;
   }
   assert(session_finish_native(&owner));
   for (retry=0;retry<10000;retry++)
   {
      result=session_step_native(c,&owner);
      assert(result!=RECORD_VULKAN_OUTPUT_ERROR);
      if (result==RECORD_VULKAN_OUTPUT_END) break;
   }
   assert(retry<10000 && (dispatch || native_session_submissions==12));
   session_free_native(&owner);
   if (!dispatch) c->ops.submit.submit=native_session_submit;
   free(pcm);
   printf("PASS native %s A/V pictures=12 PCM=19200 rate=48000 channels=2\n",dispatch?"dispatcher":"session");
   return 0;
}
#endif
