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
#include "record_metal_platform.h"
#include <retro_atomic.h>
#include <defines/cocoa_defines.h>
#include <stdlib.h>
#include <string.h>
#if defined(RECORD_METAL_NATIVE_SDK) && defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)
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
#if defined(HAVE_NETWORKING) && defined(HAVE_RTMP) && defined(HAVE_RFLV)
#include "record_stream_session_vt_native.h"
#ifdef VIDEOTOOLBOX_ENCODE_SLOTS
#define RECORD_METAL_STREAM
#include "record_vt_packet.h"
#include <crypto/kdf.h>
#include <features/features_cpu.h>
#include <net/net_wake.h>
#include <errno.h>
#include <poll.h>
#endif
#endif
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
#ifdef RECORD_METAL_STREAM
   record_gpu_dispatch_metal_stream_config_t stream;
   record_stream_vt_config_t stream_output;
   net_rtmp_client_view_t connect_view;
   net_rtmp_client_t *client, *prebind_client;
   net_wake_t *stream_wake;
   record_audio_queue_t *source;
   record_at_audio_t *aac;
   record_at_resample_t *converter;
   retro_time_t setup_start, drain_start;
   uint8_t random[1528];
   bool streaming, bootstrapping;
#endif
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
      kern_return_t result;
#ifdef RECORD_METAL_STREAM
      if (j->streaming)
      {
         if (!net_wake_notify(j->stream_wake))
            record_metal_dispatch_system(j,errno);
         return;
      }
#endif
      result=semaphore_signal(j->wake);
      if (result!=KERN_SUCCESS) record_metal_dispatch_system(j,(int)result);
   }
}
#ifdef RECORD_METAL_STREAM
struct record_metal_resolver
{
   void (*work)(void*);
   void *job;
   net_wake_t *wake;
};

static void *record_metal_resolve_run(void *opaque)
{
   struct record_metal_resolver *r = (struct record_metal_resolver*)opaque;
   r->work(r->job);
   net_wake_notify(r->wake);
   net_wake_free(r->wake);
   free(r);
   return NULL;
}

static bool record_metal_resolve(void (*work)(void*), void *job, void *userdata)
{
   record_gpu_dispatch_metal_t *j = (record_gpu_dispatch_metal_t*)userdata;
   struct record_metal_resolver *r;
   pthread_attr_t attr;
   pthread_t thread;
   int error;
   r = (struct record_metal_resolver*)calloc(1,sizeof(*r));
   if (!r)
      return false;
   r->work = work;
   r->job = job;
   r->wake = net_wake_dup(j->stream_wake);
   if (!r->wake)
   {
      free(r);
      return false;
   }
   error = pthread_attr_init(&attr);
   if (!error)
   {
      error = pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
      if (!error)
         error = pthread_create(&thread,&attr,record_metal_resolve_run,r);
      pthread_attr_destroy(&attr);
   }
   if (!error)
      return true;
   net_wake_free(r->wake);
   free(r);
   return false;
}

static bool record_metal_stream_unbound_free(record_gpu_dispatch_metal_t *j)
{
   if (j->converter)
   {
      if (!record_at_resample_cancel(j->converter) || !record_at_resample_free(j->converter))
         return false;
      j->converter = NULL;
   }
   if (j->aac)
   {
      if (!record_at_audio_cancel(j->aac) || !record_at_audio_free(j->aac))
         return false;
      j->aac = NULL;
   }
   if (j->client)
   {
      net_rtmp_client_cancel(j->client);
      if (!net_rtmp_client_free(j->client))
         return false;
      j->client = NULL;
   }
   if (!record_audio_queue_free(j->source))
      return false;
   j->source = NULL;
   return true;
}

static int record_metal_stream_bootstrap(record_gpu_dispatch_metal_t *j)
{
   net_rtmp_client_config_t config;
   net_rtmp_codec_capability_t video = {NET_RTMP_CODEC_ENCODE,"avc1"};
   net_rtmp_codec_capability_t audio = {NET_RTMP_CODEC_ENCODE,"mp4a"};
   net_rtmp_codec_caps_t caps;
   enum net_rtmp_client_state state;
   enum record_gpu_session_metal_startup startup = RECORD_METAL_STARTUP_SESSION;
   int32_t status = 0;
   unsigned leading, trailing;
   uint64_t input;
   if (retro_atomic_load_acquire_int(&j->cancelled)
         || (retro_atomic_load_acquire_int(&j->video_closed)
            && retro_atomic_load_acquire_int(&j->audio_closed)))
      goto failed;
   if (!j->client)
   {
      startup = RECORD_METAL_STARTUP_RANDOM;
      if (crypto_random_bytes(j->random,sizeof(j->random)) != 0)
         goto failed;
      memset(&config,0,sizeof(config));
      config.url = j->path;
      config.random = j->random;
      config.dispatch = record_metal_resolve;
      config.userdata = j;
      config.max_reply = 65536;
      config.streams = 8;
      config.legacy_commands = j->stream.legacy_commands;
      if (j->config.hevc)
         memcpy(video.fourcc,"hvc1",5);
      caps.video = &video;
      caps.audio = &audio;
      caps.video_count = 1;
      caps.audio_count = j->stream.channels ? 1 : 0;
      startup = RECORD_METAL_STARTUP_CLIENT;
      j->client = net_rtmp_client_new_caps(&config,&caps);
      if (!j->client)
         goto failed;
      free(j->path);
      j->path = NULL;
   }
   startup = RECORD_METAL_STARTUP_NETWORK;
   state = net_rtmp_client_step(j->client,0,&j->connect_view);
   if (state == NET_RTMP_CLIENT_FAILED)
      goto failed;
   if (state != NET_RTMP_CLIENT_READY)
      return j->connect_view.again ? RECORD_AVF_WORKER_PROGRESS : RECORD_AVF_WORKER_BUSY;
   memset(&j->stream_output,0,sizeof(j->stream_output));
   j->stream_output.max_packet_size = j->stream.max_packet_size;
   j->stream_output.video_timescale = j->config.timescale;
   j->stream_output.bit_depth = j->config.bit_depth;
   j->stream_output.audio_rate = j->stream.audio_rate;
   j->stream_output.channels = j->stream.channels;
   j->stream_output.color = j->stream.color;
   if (j->stream.channels)
   {
      startup = RECORD_METAL_STARTUP_AUDIO;
      j->aac = record_at_audio_new(j->stream.audio_rate,j->stream.channels,j->stream.audio_bitrate);
      j->source = record_audio_queue_new(j->stream.channels,j->stream.max_audio_frames,
            j->stream.audio_capacity_bytes);
      if (!j->aac || !j->source
            || !record_at_audio_timing(j->aac,&leading,&trailing,&input) || input)
         goto failed;
      if (j->stream.source_rate != j->stream.audio_rate)
      {
         j->converter = record_at_resample_new(j->source,j->stream.source_rate,j->stream.audio_rate);
         if (!j->converter)
            goto failed;
      }
      j->stream_output.audio_epoch_frames = leading;
      j->stream_output.video_epoch_ticks = ((uint64_t)leading*j->config.timescale
            + j->stream.audio_rate-1)/j->stream.audio_rate;
   }
   if (__builtin_available(macOS 10.14,*))
      j->session = record_stream_session_vt_new_metal((void*)j->device,&j->config,
            j->source,j->aac,j->converter,j->client,&j->stream_output,
            &j->capture,&j->queue,&startup,&status);
   else
      startup = RECORD_METAL_STARTUP_UNSUPPORTED;
   retro_atomic_store_release_int(&j->startup,(int)startup);
   retro_atomic_store_release_int(&j->native_status,(int)status);
   if (!j->session)
      goto failed;
   j->prebind_client = j->client;
   j->client = NULL;
   j->aac = NULL;
   j->source = NULL;
   j->converter = NULL;
   j->bootstrapping = false;
   retro_atomic_store_release_int(&j->state,retro_atomic_load_acquire_int(&j->cancelled)
         ? RECORD_METAL_DISPATCH_DRAINING : RECORD_METAL_DISPATCH_READY);
   return RECORD_AVF_WORKER_PROGRESS;
failed:
   retro_atomic_store_release_int(&j->startup,(int)startup);
   retro_atomic_store_release_int(&j->native_status,(int)status);
   j->bootstrapping = false;
   record_metal_dispatch_fail(j,RECORD_AVF_SESSION_MUX);
   retro_atomic_store_release_int(&j->state,RECORD_METAL_DISPATCH_DRAINING);
   return RECORD_AVF_WORKER_PROGRESS;
}

static void record_metal_stream_deadline(record_gpu_dispatch_metal_t *j)
{
   bool closed = retro_atomic_load_acquire_int(&j->video_closed)
      && retro_atomic_load_acquire_int(&j->audio_closed);
   retro_time_t now;
   if (!j->bootstrapping && !closed)
      return;
   now = cpu_features_get_time_usec();
   if (closed && !j->drain_start)
      j->drain_start = now;
   if ((j->bootstrapping && now-j->setup_start >= 30000000)
         || (closed && now-j->drain_start >= 10000000))
      record_metal_dispatch_system(j,ETIMEDOUT);
}

static void record_metal_stream_wait(record_gpu_dispatch_metal_t *j,int timeout)
{
   struct pollfd fds[2];
   bool read = false, write = false;
   int fd = -1, result;
   if (j->bootstrapping || j->prebind_client)
   {
      fd = j->connect_view.fd;
      read = j->connect_view.read;
      write = j->connect_view.write;
   }
   else if (j->session)
      record_avfoundation_session_network(j->session,&fd,&read,&write);
   memset(fds,0,sizeof(fds));
   fds[0].fd = net_wake_read_fd(j->stream_wake);
   fds[0].events = POLLIN;
   fds[1].fd = fd;
   fds[1].events = (read ? POLLIN : 0) | (write ? POLLOUT : 0);
   result = poll(fds,fd >= 0 && (read || write) ? 2 : 1,timeout);
   if (result < 0 && errno != EINTR)
      record_metal_dispatch_system(j,errno);
   if (result > 0 && (fds[0].revents&(POLLERR|POLLHUP|POLLNVAL)))
      record_metal_dispatch_system(j,EIO);
   if (result > 0 && (fds[0].revents&POLLIN) && !net_wake_ack(j->stream_wake))
      record_metal_dispatch_system(j,errno ? errno : EIO);
}
#endif
static int record_metal_dispatch_service(record_gpu_dispatch_metal_t *j)
{
   int result;
   enum record_avfoundation_session_error error;
   bool closed=retro_atomic_load_acquire_int(&j->video_closed)
      && retro_atomic_load_acquire_int(&j->audio_closed);
#ifdef RECORD_METAL_STREAM
   if (j->streaming)
   {
      record_metal_stream_deadline(j);
      if (j->bootstrapping)
         return record_metal_stream_bootstrap(j);
      if (!j->session && !record_metal_stream_unbound_free(j))
         return RECORD_AVF_WORKER_BUSY;
      if (j->prebind_client)
      {
         if (!record_avfoundation_session_binding(j->session))
            j->prebind_client = NULL;
         else if (net_rtmp_client_step(j->prebind_client,0,&j->connect_view) == NET_RTMP_CLIENT_FAILED)
            record_metal_dispatch_fail(j,RECORD_AVF_SESSION_MUX);
      }
   }
#endif
   if (!j->session) return closed?RECORD_AVF_WORKER_ERROR:RECORD_AVF_WORKER_BUSY;
   if (retro_atomic_load_acquire_int(&j->video_closed)) record_avfoundation_session_video_finish(j->session);
   if (retro_atomic_load_acquire_int(&j->audio_closed)) record_avfoundation_session_audio_finish(j->session);
   if (retro_atomic_load_acquire_int(&j->cancelled)) record_avfoundation_session_cancel(j->session);
   result=record_avfoundation_session_step(j->session);
#ifdef RECORD_METAL_STREAM
   if (j->prebind_client && !record_avfoundation_session_binding(j->session))
      j->prebind_client = NULL;
   if (result == RECORD_AVF_WORKER_BUSY && j->prebind_client && j->connect_view.again)
      result = RECORD_AVF_WORKER_PROGRESS;
#endif
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
#ifdef RECORD_METAL_STREAM
   if (j->streaming)
   {
      j->setup_start = cpu_features_get_time_usec();
      j->bootstrapping = true;
   }
   else
#endif
   {
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
   }
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
#ifdef RECORD_METAL_STREAM
      if (j->streaming)
      {
         record_metal_stream_wait(j,(int)(timeout.tv_nsec/1000000));
         retro_atomic_fetch_and_int(&j->epoch,~1);
         continue;
      }
#endif
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
static record_gpu_dispatch_metal_t *record_metal_dispatch_new(void *device,
      const record_gpu_session_metal_config_t *config,const record_mux_avfoundation_config_t *output,
      size_t audio_capacity_bytes,const char *path,
      const record_gpu_dispatch_metal_stream_config_t *stream)
{
   record_gpu_dispatch_metal_t *j;
   id<MTLDevice> metal=(__bridge id<MTLDevice>)device;
   size_t length;
   int error;
   if (RECORD_METAL_NATIVE_RUNTIME)
   {
   }
   else
      return NULL;
   if (!metal || !config || (!output && !stream)
         || (output && output->video_format) || !path || !*path) return NULL;
#ifndef RECORD_METAL_STREAM
   if (stream) return NULL;
#else
   if (stream && (!config->timescale || !stream->max_packet_size
            || stream->max_packet_size > RECORD_VT_PACKET_MAX_SIZE
            || (stream->channels && (stream->channels > 2
                  || !(stream->source_rate >= 8000 && stream->source_rate <= 192000)
                  || (stream->audio_rate != 44100 && stream->audio_rate != 48000)
                  || !stream->audio_bitrate || !stream->max_audio_frames
                  || !stream->audio_capacity_bytes))
            || (!stream->channels && stream->audio_rate)))
      return NULL;
#endif
   for (length=0;length<32768 && path[length];length++) {}
   if (length==32768) return NULL;
   j=(record_gpu_dispatch_metal_t*)calloc(1,sizeof(*j)); if (!j) return NULL;
   j->config=*config;
   if (output) j->output=*output;
   j->audio_capacity=audio_capacity_bytes;
   j->device=(CFTypeRef)RARCH_BRIDGE_RETAINED(metal);
   retro_atomic_int_init(&j->state,RECORD_METAL_DISPATCH_STARTING); retro_atomic_int_init(&j->done,0);
   retro_atomic_int_init(&j->epoch,0); retro_atomic_int_init(&j->cancelled,0);
   retro_atomic_int_init(&j->video_closed,0);
   retro_atomic_int_init(&j->audio_closed,!(stream ? stream->channels : output->channels));
   retro_atomic_int_init(&j->startup,0); retro_atomic_int_init(&j->native_status,0);
   retro_atomic_int_init(&j->error,0); retro_atomic_int_init(&j->system_error,0);
   j->path=(char*)malloc(length+1);
   if (j->path)
   {
      memcpy(j->path,path,length+1);
#ifdef RECORD_METAL_STREAM
      if (stream)
      {
         j->stream = *stream;
         j->streaming = true;
         j->stream_wake = net_wake_new();
      }
      if (stream ? j->stream_wake != NULL
            : semaphore_create(mach_task_self(),&j->wake,SYNC_POLICY_FIFO,0)==KERN_SUCCESS)
#else
      if (semaphore_create(mach_task_self(),&j->wake,SYNC_POLICY_FIFO,0)==KERN_SUCCESS)
#endif
      {
         error=pthread_create(&j->thread,NULL,record_metal_dispatch_run,j);
         if (!error)
         {
            error=pthread_detach(j->thread); j->joinable=error!=0;
            if (error) record_metal_dispatch_system(j,error);
            return j;
         }
#ifdef RECORD_METAL_STREAM
         if (stream) net_wake_free(j->stream_wake);
         else
#endif
            semaphore_destroy(mach_task_self(),j->wake);
      }
   }
   CFRelease(j->device); free(j->path); free(j); return NULL;
}
record_gpu_dispatch_metal_t *record_gpu_dispatch_metal_new(void *device,
      const record_gpu_session_metal_config_t *config,const record_mux_avfoundation_config_t *output,
      size_t audio_capacity_bytes,const char *path)
{
   if (!output) return NULL;
   return record_metal_dispatch_new(device,config,output,audio_capacity_bytes,path,NULL);
}
record_gpu_dispatch_metal_t *record_gpu_dispatch_metal_new_stream(void *device,
      const record_gpu_session_metal_config_t *video,
      const record_gpu_dispatch_metal_stream_config_t *stream,const char *url)
{
   if (!stream) return NULL;
   return record_metal_dispatch_new(device,video,NULL,0,url,stream);
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
#ifdef RECORD_METAL_STREAM
   if (j->streaming) net_wake_free(j->stream_wake);
   else
#endif
      if (semaphore_destroy(mach_task_self(),j->wake)!=KERN_SUCCESS) return false;
   CFRelease(j->device); free(j->path); free(j); return true;
}
#else
record_gpu_dispatch_metal_t *record_gpu_dispatch_metal_new_stream(void *device,
      const record_gpu_session_metal_config_t *video,
      const record_gpu_dispatch_metal_stream_config_t *stream,const char *url)
{ (void)device; (void)video; (void)stream; (void)url; return NULL; }
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

bool record_gpu_dispatch_metal_stream_available(void)
{
#ifdef RECORD_METAL_STREAM
   if (__builtin_available(macOS 10.14,*))
      return true;
#endif
   return false;
}
