/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Exercise extracted production capture/audio functions with mocked dispatch. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <retro_atomic.h>
#include "../../../gfx/video_driver.h"
#include "../../../record/record_gpu_dispatch_metal.h"
#include "../../../record/record_driver.h"
#include "capture_fragment.c"
static int dispatch_result;
static unsigned offers,cancels,audio_offers;
static video_driver_state_t test_video;
static uint64_t audio_pts;
video_driver_state_t *video_state_get_ptr(void) { return &test_video; }
int record_gpu_dispatch_metal_audio(record_gpu_dispatch_metal_t *job,
      const int16_t *samples,size_t frames,uint64_t timestamp)
{
   (void)job; (void)samples; assert(frames==256);
   audio_offers++; audio_pts=timestamp; return dispatch_result;
}

static uint64_t offered_pts;
int record_gpu_dispatch_metal_video(record_gpu_dispatch_metal_t *job,
      void *commands,void *source,unsigned x,unsigned y,uint64_t timestamp,uint64_t duration)
{
   (void)job; (void)commands; (void)source; (void)x; (void)y;
   assert(duration==1001); offers++; offered_pts=timestamp; return dispatch_result;
}
void record_gpu_dispatch_metal_cancel(record_gpu_dispatch_metal_t *job)
{ (void)job; cancels++; }
static void reset(record_metal_context_t *ctx)
{
   memset(ctx,0,sizeof(*ctx)); ctx->dims=VIDEO_SCALE_PACK(1920,1080);
   ctx->duration=1001; ctx->allow_drop=true;
   retro_atomic_int_init(&ctx->stop,0); retro_atomic_int_init(&ctx->failed,0);
   offers=cancels=audio_offers=0; dispatch_result=RECORD_GPU_QUEUE_OK;
}
static void offer(record_metal_context_t *ctx,uint64_t timestamp)
{ record_metal_gpu_capture(ctx,NULL,NULL,0,ctx->dims,timestamp,ctx->output_mode); }
int main(void)
{
   record_metal_context_t ctx;
   reset(&ctx); offer(&ctx,5005); assert(offers==1 && offered_pts==0);
   offer(&ctx,5005); assert(offers==1 && ctx.captured==1);
   offer(&ctx,6006); assert(offers==2 && offered_pts==1001);
   reset(&ctx); ctx.audio_enabled=true; offer(&ctx,5005); assert(offered_pts==5005);
   reset(&ctx); dispatch_result=RECORD_GPU_QUEUE_BUSY; offer(&ctx,5005);
   offer(&ctx,5005); assert(offers==1 && !ctx.captured && !cancels);
   dispatch_result=RECORD_GPU_QUEUE_OK; offer(&ctx,6006);
   assert(offered_pts==0 && ctx.origin==6006 && ctx.captured==1);
   reset(&ctx); ctx.allow_drop=false; dispatch_result=RECORD_GPU_QUEUE_BUSY;
   offer(&ctx,0); assert(cancels==1 && retro_atomic_load_acquire_int(&ctx.failed));
   offer(&ctx,1001); assert(offers==1);
   reset(&ctx); dispatch_result=RECORD_GPU_QUEUE_ERROR; offer(&ctx,0); assert(cancels==1);
   reset(&ctx); record_metal_gpu_capture(&ctx,NULL,NULL,0,VIDEO_SCALE_PACK(1280,720),0,0);
   assert(!offers && retro_atomic_load_acquire_int(&ctx.stop)==1);
   reset(&ctx); record_metal_gpu_capture(&ctx,NULL,NULL,0,ctx.dims,0,1);
   assert(!offers && retro_atomic_load_acquire_int(&ctx.stop)==4);
   reset(&ctx); retro_atomic_store_release_int(&ctx.stop,2); offer(&ctx,0);
   assert(offers==1); /* Accepted snapshots may retire after normal stop. */
   {
      struct record_audio_data audio;
      memset(&audio,0,sizeof(audio)); audio.frames=256;
      reset(&ctx); ctx.audio_enabled=true; ctx.audio_rate=48000;
      test_video.av_info.timing.sample_rate=48000;
      assert(record_metal_audio(&ctx,&audio) && !audio_offers && !ctx.audio_frames);
      ctx.armed=true; dispatch_result=RECORD_AUDIO_OK;
      assert(record_metal_audio(&ctx,&audio) && audio_pts==0 && ctx.audio_frames==256);
      assert(record_metal_audio(&ctx,&audio) && audio_pts==256 && ctx.audio_frames==512);
      dispatch_result=RECORD_AUDIO_BUSY;
      assert(!record_metal_audio(&ctx,&audio) && ctx.audio_frames==512 && cancels==1);
      assert(!record_metal_audio(&ctx,&audio) && audio_offers==3);
      reset(&ctx); ctx.audio_enabled=ctx.armed=true; ctx.audio_rate=48000;
      test_video.av_info.timing.sample_rate=44100;
      assert(!record_metal_audio(&ctx,&audio) && !audio_offers && cancels==1);
   }
   puts("PASS: Metal frontend source-clock deduplication, BUSY/error policy and output-change refusal");
   return 0;
}
