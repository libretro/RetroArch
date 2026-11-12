/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <poll.h>
#include <retro_atomic.h>
#include <net/net_compat.h>
#include <retro_timers.h>
static int lifetime_create(pthread_t *,const pthread_attr_t *,void *(*)(void*),void *);
#define pthread_create lifetime_create
#include "../../../record/record_gpu_dispatch_metal.m"
#undef pthread_create

static retro_atomic_int_t pending;
static void *(*completion)(void*);
static void *completion_data;

static int lifetime_create(pthread_t *thread,const pthread_attr_t *attr,
      void *(*start)(void*),void *opaque)
{
   if (start == record_metal_resolve_run)
   {
      completion = start;
      completion_data = opaque;
      retro_atomic_store_release_int(&pending,1);
      return 0;
   }
   return pthread_create(thread,attr,start,opaque);
}

int main(void)
{
   assert(network_init());
   retro_atomic_int_init(&pending,0);
   @autoreleasepool
   {
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();
      record_gpu_session_metal_config_t video;
      record_gpu_dispatch_metal_stream_config_t stream;
      record_gpu_dispatch_metal_t *job;
      net_wake_t *replacement;
      struct pollfd fd;
      retro_time_t deadline;
      int original;
      assert(device);
      memset(&video,0,sizeof(video));
      memset(&stream,0,sizeof(stream));
      video.width = 162;
      video.height = 130;
      video.slots = 2;
      video.bitrate = 16000000;
      video.key_interval = 6;
      video.timescale = 60000;
      video.bit_depth = 8;
      stream.max_packet_size = 4*1024*1024;
      stream.color.primaries = stream.color.transfer = stream.color.matrix = stream.color.range = 1;
      job = record_gpu_dispatch_metal_new_stream((__bridge void*)device,&video,&stream,
            "rtmp://localhost:1/live/cancelled");
      assert(job);
      original = net_wake_read_fd(job->stream_wake);
      deadline = cpu_features_get_time_usec()+5000000;
      while (!retro_atomic_load_acquire_int(&pending))
      {
         assert(cpu_features_get_time_usec() < deadline);
         retro_sleep(1);
      }
      assert(!record_gpu_dispatch_metal_free(job));
      record_gpu_dispatch_metal_cancel(job);
      assert(record_gpu_dispatch_metal_video_finish(job));
      assert(record_gpu_dispatch_metal_audio_finish(job));
      while (!record_gpu_dispatch_metal_free(job))
      {
         assert(cpu_features_get_time_usec() < deadline);
         retro_sleep(1);
      }
      replacement = net_wake_new();
      assert(replacement && net_wake_read_fd(replacement) == original);
      memset(&fd,0,sizeof(fd));
      fd.fd = net_wake_read_fd(replacement);
      fd.events = POLLIN;
      assert(completion && completion_data);
      completion(completion_data);
      assert(poll(&fd,1,0) == 0);
      assert(net_wake_notify(replacement) && poll(&fd,1,0) == 1);
      assert(net_wake_ack(replacement));
      net_wake_free(replacement);
      RARCH_RELEASE(device);
   }
   puts("PASS actual delayed DNS completion after dispatcher free and descriptor reuse");
   return 0;
}
