/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NATIVE_ENCODE_SESSION_H
#define NATIVE_ENCODE_SESSION_H
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
static int session_step_native(struct native_encoder *c,record_gpu_session_vulkan_t *s)
{
   int result=record_gpu_session_vulkan_step(s);
   if (native_session_fence)
   {
      PFN_vkWaitForFences wait_fence=(PFN_vkWaitForFences)c->device_proc(c->device,"vkWaitForFences");
      /* Native fixture orchestration only; session service never waits. */
      assert(wait_fence(c->device,1,&native_session_fence,VK_TRUE,2000000000)==VK_SUCCESS);
      native_session_fence=VK_NULL_HANDLE;
   }
   return result;
}
static int session_pictures(struct native_encoder *c,const char *path,bool direct)
{
   record_gpu_output_vulkan_config_t output;
   record_gpu_session_vulkan_t *s;
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
   native_session_submit=c->ops.submit.submit;
   c->ops.submit.submit=session_submit;
   s=record_gpu_session_vulkan_new(&c->config,&c->ops,&output,path);
   assert(s && record_gpu_session_vulkan_error(s,NULL)==RECORD_VULKAN_SESSION_NONE);
   pool=record_gpu_session_vulkan_pool(s);
   queue=record_gpu_session_vulkan_queue(s);
   assert(pool && queue);
   if (direct && !pool->storage_images)
   {
      record_gpu_session_vulkan_cancel(s);
      assert(record_gpu_session_vulkan_audio_finish(s));
      assert(record_gpu_session_vulkan_video_finish(s));
      for (retry=0;retry<1000;retry++)
         if (session_step_native(c,s)==RECORD_VULKAN_OUTPUT_ERROR) break;
      assert(retry<1000 && record_gpu_session_vulkan_free(s));
      free(pcm);
      puts("UNAVAILABLE direct storage session format");
      return 77;
   }
   if (!direct) assert(!pool->storage_images);
   capture_new(c,pool);
   for (frame=0;frame<12;frame++)
   {
      for (retry=0;retry<10000;retry++)
      {
         result=record_gpu_queue_acquire(queue,&descriptor.slot);
         if (result==RECORD_GPU_QUEUE_OK) break;
         assert(result==RECORD_GPU_QUEUE_BUSY);
         assert(session_step_native(c,s)!=RECORD_VULKAN_OUTPUT_ERROR);
      }
      assert(retry<10000 && descriptor.slot==frame%2);
      capture_frame(c,pool,frame);
      descriptor.timestamp=frame;
      descriptor.duration=1;
      descriptor.ready_value=frame+1;
      descriptor.keyframe=frame%4==0;
      assert(record_gpu_queue_submit(queue,&descriptor));
      for (i=0;i<1600;i++)
      {
         double phase=6.2831853071795864769*(frame*1600+i)/48000.0;
         pcm[i*2]=(int16_t)(6553*sin(phase*1000));
         pcm[i*2+1]=(int16_t)(6553*sin(phase*2300));
      }
      for (retry=0;retry<10000;retry++)
      {
         result=record_gpu_session_vulkan_audio(s,pcm,1600);
         if (result==RECORD_VULKAN_OUTPUT_PROGRESS) break;
         assert(result==RECORD_VULKAN_OUTPUT_BUSY);
         assert(session_step_native(c,s)!=RECORD_VULKAN_OUTPUT_ERROR);
      }
      assert(retry<10000);
   }
   wait_ready=(PFN_vkWaitSemaphores)c->device_proc(c->device,"vkWaitSemaphores");
   assert(wait_ready);
   memset(&wait,0,sizeof(wait));
   wait.sType=VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
   wait.semaphoreCount=1;
   wait.pSemaphores=&pool->ready;
   wait.pValues=&ready;
   assert(wait_ready(c->device,&wait,2000000000)==VK_SUCCESS);
   c->ops.pool.destroy_commands(c->device,c->producer_commands,NULL);
   capture_free(c);
   assert(record_gpu_session_vulkan_audio_finish(s));
   assert(record_gpu_session_vulkan_video_finish(s));
   for (retry=0;retry<10000;retry++)
   {
      result=session_step_native(c,s);
      assert(result!=RECORD_VULKAN_OUTPUT_ERROR);
      if (result==RECORD_VULKAN_OUTPUT_END) break;
   }
   assert(retry<10000 && native_session_submissions==12);
   assert(record_gpu_session_vulkan_free(s));
   c->ops.submit.submit=native_session_submit;
   free(pcm);
   puts("PASS native session A/V pictures=12 PCM=19200 rate=48000 channels=2");
   return 0;
}
#endif
