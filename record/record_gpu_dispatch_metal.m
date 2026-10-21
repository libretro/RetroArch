/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#if defined(__APPLE__) && !defined(__cplusplus) \
      && (!defined(__STDC_VERSION__) || __STDC_VERSION__<199901L) && !defined(inline)
#define inline __inline__
#define RECORD_METAL_DISPATCH_UNDEF_INLINE
#endif
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <TargetConditionals.h>
#ifdef RECORD_METAL_DISPATCH_UNDEF_INLINE
#undef inline
#undef RECORD_METAL_DISPATCH_UNDEF_INLINE
#endif
#include "record_gpu_dispatch_metal.h"
#include <retro_atomic.h>
#include <defines/cocoa_defines.h>
#include <stdlib.h>
#include <string.h>
#if TARGET_OS_OSX && defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)
#include <pthread.h>
#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__<199901L) && !defined(inline)
#define inline __inline__
#define RECORD_MACH_UNDEF_INLINE
#endif
#include <mach/mach.h>
#include <mach/semaphore.h>
#include <mach/sync_policy.h>
#ifdef RECORD_MACH_UNDEF_INLINE
#undef inline
#undef RECORD_MACH_UNDEF_INLINE
#endif
#include <unistd.h>
struct record_gpu_dispatch_metal
{
   record_gpu_session_metal_config_t config;
   record_mux_avfoundation_config_t output;
   size_t audio_capacity;
   CFTypeRef device;
   char *path;
   record_avfoundation_session_t *session;
   record_gpu_capture_metal_t *capture;
   record_gpu_queue_t *queue;
   pthread_t thread;
   semaphore_t wake;
   retro_atomic_int_t state,done,epoch,cancelled,video_closed,audio_closed;
   retro_atomic_int_t startup,native_status,error,system_error;
   uint64_t video_end;
   unsigned orphan_slot;
   bool have_video,orphan,orphan_released,joinable;
};
static void record_metal_dispatch_fail(record_gpu_dispatch_metal_t *j,int error)
{
   retro_atomic_cas_int(&j->error,0,error);
   retro_atomic_store_release_int(&j->cancelled,1);
}
static void record_metal_dispatch_system(record_gpu_dispatch_metal_t *j,int error)
{
   retro_atomic_cas_int(&j->system_error,0,error);
   retro_atomic_store_release_int(&j->cancelled,1);
}
static void record_metal_dispatch_notify(record_gpu_dispatch_metal_t *j)
{
   if (retro_atomic_fetch_add_seq_cst_int(&j->epoch,2)&1)
   {
      kern_return_t result=semaphore_signal(j->wake);
      if (result!=KERN_SUCCESS) record_metal_dispatch_system(j,(int)result);
   }
}
static int record_metal_dispatch_service(record_gpu_dispatch_metal_t *j)
{
   int result;
   enum record_avfoundation_session_error error;
   bool closed=retro_atomic_load_acquire_int(&j->video_closed)
      && retro_atomic_load_acquire_int(&j->audio_closed);
   if (!j->session) return closed?RECORD_AVF_WORKER_ERROR:RECORD_AVF_WORKER_BUSY;
   if (retro_atomic_load_acquire_int(&j->video_closed)) record_avfoundation_session_video_finish(j->session);
   if (retro_atomic_load_acquire_int(&j->audio_closed)) record_avfoundation_session_audio_finish(j->session);
   if (retro_atomic_load_acquire_int(&j->cancelled)) record_avfoundation_session_cancel(j->session);
   result=record_avfoundation_session_step(j->session);
   error=record_avfoundation_session_error(j->session);
   if (error) retro_atomic_cas_int(&j->error,0,(int)error);
   if (error || retro_atomic_load_acquire_int(&j->cancelled))
      retro_atomic_store_release_int(&j->state,RECORD_METAL_DISPATCH_DRAINING);
   if (result==RECORD_AVF_WORKER_ERROR || result==RECORD_AVF_WORKER_END)
   {
      if (!record_avfoundation_session_free(j->session)) return RECORD_AVF_WORKER_BUSY;
      j->session=NULL; j->capture=NULL; j->queue=NULL;
   }
   return result;
}
static void *record_metal_dispatch_run(void *opaque)
{
   record_gpu_dispatch_metal_t *j=(record_gpu_dispatch_metal_t*)opaque;
   enum record_gpu_session_metal_startup startup;
   int32_t native_status;
   int result;
   @autoreleasepool
   {
      if (__builtin_available(macOS 10.14,*))
         j->session=record_gpu_session_metal_new((void*)j->device,&j->config,&j->output,
               j->audio_capacity,j->path,&j->capture,&j->queue,&startup,&native_status);
      else { startup=RECORD_METAL_STARTUP_UNSUPPORTED; native_status=0; }
   }
   retro_atomic_store_release_int(&j->startup,(int)startup);
   retro_atomic_store_release_int(&j->native_status,(int)native_status);
   if (!j->session) record_metal_dispatch_fail(j,RECORD_AVF_SESSION_MUX);
   retro_atomic_store_release_int(&j->state,j->session && !retro_atomic_load_acquire_int(&j->cancelled)
         ?RECORD_METAL_DISPATCH_READY:RECORD_METAL_DISPATCH_DRAINING);
   for (;;)
   {
      kern_return_t waited;
      mach_timespec_t timeout;
      @autoreleasepool { result=record_metal_dispatch_service(j); }
      if (result==RECORD_AVF_WORKER_ERROR || result==RECORD_AVF_WORKER_END) break;
      if (result!=RECORD_AVF_WORKER_BUSY) continue;
      /* Register before rechecking so producer publication cannot be lost. */
      retro_atomic_fetch_or_int(&j->epoch,1);
      retro_atomic_thread_fence_seq_cst();
      @autoreleasepool { result=record_metal_dispatch_service(j); }
      if (result!=RECORD_AVF_WORKER_BUSY)
      {
         retro_atomic_fetch_and_int(&j->epoch,~1);
         if (result==RECORD_AVF_WORKER_ERROR || result==RECORD_AVF_WORKER_END) break;
         continue;
      }
      timeout.tv_sec=0;
      timeout.tv_nsec=j->session && record_avfoundation_session_retry_needed(j->session)?1000000:100000000;
      if (retro_atomic_load_acquire_int(&j->system_error))
      { usleep(1000); waited=KERN_SUCCESS; } /* Worker-only failure recovery. */
      else waited=semaphore_timedwait(j->wake,timeout);
      retro_atomic_fetch_and_int(&j->epoch,~1);
      if (waited!=KERN_SUCCESS && waited!=KERN_OPERATION_TIMED_OUT && waited!=KERN_ABORTED)
         record_metal_dispatch_system(j,(int)waited);
   }
   retro_atomic_store_release_int(&j->state,result==RECORD_AVF_WORKER_END
         && !retro_atomic_load_acquire_int(&j->system_error)
         ?RECORD_METAL_DISPATCH_DONE:RECORD_METAL_DISPATCH_FAILED);
   /* Last access: a detached worker can now be collected without a join. */
   retro_atomic_store_release_int(&j->done,1);
   return NULL;
}
record_gpu_dispatch_metal_t *record_gpu_dispatch_metal_new(void *device,
      const record_gpu_session_metal_config_t *config,const record_mux_avfoundation_config_t *output,
      size_t audio_capacity_bytes,const char *path)
{
   record_gpu_dispatch_metal_t *j;
   id<MTLDevice> metal=(__bridge id<MTLDevice>)device;
   size_t length;
   int error;
   if (!metal || !config || !output || output->video_format || !path || !*path) return NULL;
   for (length=0;length<32768 && path[length];length++) {}
   if (length==32768) return NULL;
   j=(record_gpu_dispatch_metal_t*)calloc(1,sizeof(*j)); if (!j) return NULL;
   j->config=*config; j->output=*output; j->audio_capacity=audio_capacity_bytes;
   j->device=(CFTypeRef)RARCH_BRIDGE_RETAINED(metal);
   retro_atomic_int_init(&j->state,RECORD_METAL_DISPATCH_STARTING); retro_atomic_int_init(&j->done,0);
   retro_atomic_int_init(&j->epoch,0); retro_atomic_int_init(&j->cancelled,0);
   retro_atomic_int_init(&j->video_closed,0); retro_atomic_int_init(&j->audio_closed,!output->channels);
   retro_atomic_int_init(&j->startup,0); retro_atomic_int_init(&j->native_status,0);
   retro_atomic_int_init(&j->error,0); retro_atomic_int_init(&j->system_error,0);
   j->path=(char*)malloc(length+1);
   if (j->path)
   {
      memcpy(j->path,path,length+1);
      if (semaphore_create(mach_task_self(),&j->wake,SYNC_POLICY_FIFO,0)==KERN_SUCCESS)
      {
         error=pthread_create(&j->thread,NULL,record_metal_dispatch_run,j);
         if (!error)
         {
            error=pthread_detach(j->thread); j->joinable=error!=0;
            if (error) record_metal_dispatch_system(j,error);
            return j;
         }
         semaphore_destroy(mach_task_self(),j->wake);
      }
   }
   CFRelease(j->device); free(j->path); free(j); return NULL;
}
int record_gpu_dispatch_metal_video(record_gpu_dispatch_metal_t *j,void *commands,void *source,
      unsigned x,unsigned y,uint64_t timestamp,uint64_t duration)
{
   record_gpu_frame_t frame;
   int state,result;
   if (!j || retro_atomic_load_acquire_int(&j->video_closed)
         || retro_atomic_load_acquire_int(&j->cancelled)) return RECORD_GPU_QUEUE_ERROR;
   state=retro_atomic_load_acquire_int(&j->state);
   if (state==RECORD_METAL_DISPATCH_STARTING) return RECORD_GPU_QUEUE_BUSY;
   if (state!=RECORD_METAL_DISPATCH_READY) return RECORD_GPU_QUEUE_ERROR;
   if (!duration || timestamp>((uint64_t)1<<63)-1 || duration>((uint64_t)1<<63)-1-timestamp
         || (j->have_video && timestamp<j->video_end))
   { record_metal_dispatch_fail(j,RECORD_AVF_SESSION_VIDEO); record_metal_dispatch_notify(j); return RECORD_GPU_QUEUE_ERROR; }
   memset(&frame,0,sizeof(frame));
   result=record_gpu_queue_acquire(j->queue,&frame.slot);
   if (result!=RECORD_GPU_QUEUE_OK) return result;
   result=record_gpu_capture_metal_encode(j->capture,frame.slot,commands,source,x,y);
   if (result!=1)
   {
      record_gpu_queue_cancel(j->queue);
      if (result<0) { record_metal_dispatch_fail(j,RECORD_AVF_SESSION_VIDEO); record_metal_dispatch_notify(j); }
      return result<0?RECORD_GPU_QUEUE_ERROR:RECORD_GPU_QUEUE_BUSY;
   }
   frame.timestamp=timestamp; frame.duration=duration;
   if (!record_gpu_queue_submit(j->queue,&frame))
   {
      /* Unreachable with a matching fresh queue and one producer. Preserve
       * the GPU lease even on contract failure; renderer still commits. */
      j->orphan=true; j->orphan_slot=frame.slot;
      record_metal_dispatch_fail(j,RECORD_AVF_SESSION_VIDEO); record_metal_dispatch_notify(j);
      return RECORD_GPU_QUEUE_ERROR;
   }
   j->video_end=timestamp+duration; j->have_video=true;
   record_metal_dispatch_notify(j); return RECORD_GPU_QUEUE_OK;
}
int record_gpu_dispatch_metal_audio(record_gpu_dispatch_metal_t *j,const int16_t *samples,size_t frames,uint64_t timestamp)
{
   int state,result;
   if (!j || retro_atomic_load_acquire_int(&j->audio_closed)
         || retro_atomic_load_acquire_int(&j->cancelled)) return RECORD_AUDIO_ERROR;
   state=retro_atomic_load_acquire_int(&j->state);
   if (state==RECORD_METAL_DISPATCH_STARTING) return RECORD_AUDIO_BUSY;
   if (state!=RECORD_METAL_DISPATCH_READY) return RECORD_AUDIO_ERROR;
   result=record_avfoundation_session_audio(j->session,samples,frames,timestamp);
   if (result==RECORD_AUDIO_OK) record_metal_dispatch_notify(j);
   return result;
}
bool record_gpu_dispatch_metal_video_finish(record_gpu_dispatch_metal_t *j)
{
   if (!j) return false;
   if (retro_atomic_load_acquire_int(&j->video_closed)) return true;
   if (j->orphan)
   {
      if (!j->orphan_released)
      {
         if (!record_gpu_capture_metal_ready(j->capture,j->orphan_slot)
               || !record_gpu_capture_metal_release(j->capture,j->orphan_slot)) return false;
         j->orphan_released=true;
      }
      if (!record_gpu_queue_cancel(j->queue)) return false;
      j->orphan=false;
   }
   retro_atomic_store_release_int(&j->video_closed,1); record_metal_dispatch_notify(j); return true;
}
bool record_gpu_dispatch_metal_audio_finish(record_gpu_dispatch_metal_t *j)
{
   if (!j) return false;
   retro_atomic_store_release_int(&j->audio_closed,1); record_metal_dispatch_notify(j); return true;
}
void record_gpu_dispatch_metal_cancel(record_gpu_dispatch_metal_t *j)
{ if (j) { retro_atomic_store_release_int(&j->cancelled,1); record_metal_dispatch_notify(j); } }
enum record_gpu_dispatch_metal_state record_gpu_dispatch_metal_status(const record_gpu_dispatch_metal_t *j,
      enum record_gpu_session_metal_startup *startup,int32_t *native_status,
      enum record_avfoundation_session_error *error,int *system_error)
{
   enum record_gpu_dispatch_metal_state state=j?(enum record_gpu_dispatch_metal_state)retro_atomic_load_acquire_int(&j->state):RECORD_METAL_DISPATCH_FAILED;
   if (startup) *startup=j?(enum record_gpu_session_metal_startup)retro_atomic_load_acquire_int(&j->startup):RECORD_METAL_STARTUP_CONFIG;
   if (native_status) *native_status=j?(int32_t)retro_atomic_load_acquire_int(&j->native_status):0;
   if (error) *error=j?(enum record_avfoundation_session_error)retro_atomic_load_acquire_int(&j->error):RECORD_AVF_SESSION_MUX;
   if (system_error) *system_error=j?retro_atomic_load_acquire_int(&j->system_error):0;
   return state;
}
bool record_gpu_dispatch_metal_free(record_gpu_dispatch_metal_t *j)
{
   if (!j) return true;
   if (!retro_atomic_load_acquire_int(&j->done)) return false;
   if (j->joinable && pthread_join(j->thread,NULL)) return false;
   j->joinable=false;
   if (semaphore_destroy(mach_task_self(),j->wake)!=KERN_SUCCESS) return false;
   CFRelease(j->device); free(j->path); free(j); return true;
}
#else
record_gpu_dispatch_metal_t *record_gpu_dispatch_metal_new(void *device,
      const record_gpu_session_metal_config_t *config,const record_mux_avfoundation_config_t *output,
      size_t capacity,const char *path)
{ (void)device; (void)config; (void)output; (void)capacity; (void)path; return NULL; }
int record_gpu_dispatch_metal_video(record_gpu_dispatch_metal_t *j,void *commands,void *source,
      unsigned x,unsigned y,uint64_t pts,uint64_t duration)
{ (void)j; (void)commands; (void)source; (void)x; (void)y; (void)pts; (void)duration; return RECORD_GPU_QUEUE_ERROR; }
int record_gpu_dispatch_metal_audio(record_gpu_dispatch_metal_t *j,const int16_t *samples,size_t frames,uint64_t pts)
{ (void)j; (void)samples; (void)frames; (void)pts; return RECORD_AUDIO_ERROR; }
bool record_gpu_dispatch_metal_video_finish(record_gpu_dispatch_metal_t *j) { (void)j; return false; }
bool record_gpu_dispatch_metal_audio_finish(record_gpu_dispatch_metal_t *j) { (void)j; return false; }
void record_gpu_dispatch_metal_cancel(record_gpu_dispatch_metal_t *j) { (void)j; }
enum record_gpu_dispatch_metal_state record_gpu_dispatch_metal_status(const record_gpu_dispatch_metal_t *j,
      enum record_gpu_session_metal_startup *startup,int32_t *native_status,
      enum record_avfoundation_session_error *error,int *system_error)
{
   (void)j;
   if (startup) *startup=RECORD_METAL_STARTUP_UNSUPPORTED;
   if (native_status) *native_status=0;
   if (error) *error=RECORD_AVF_SESSION_MUX;
   if (system_error) *system_error=0;
   return RECORD_METAL_DISPATCH_FAILED;
}
bool record_gpu_dispatch_metal_free(record_gpu_dispatch_metal_t *j) { return !j; }
#endif
