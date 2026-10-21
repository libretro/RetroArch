/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Compile the real controller, with only the dispatch boundary mocked. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <retro_atomic.h>
#include "../../../gfx/video_driver.h"
#include "../../../record/record_gpu_dispatch_metal.h"
#include "capture_fragment.c"
static unsigned offers,cancels;
static uint64_t offered_pts;
static int dispatch_result;
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
   offers=cancels=0; dispatch_result=RECORD_GPU_QUEUE_OK;
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
   puts("PASS: Metal frontend source-clock deduplication, BUSY/error policy and output-change refusal");
   return 0;
}
