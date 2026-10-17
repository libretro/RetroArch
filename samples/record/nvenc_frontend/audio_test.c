#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <retro_atomic.h>
#include "../../../record/record_driver.h"
#include "../../../gfx/video_defines.h"
#include "../../../record/record_gpu_dispatch_d3d12.h"
typedef struct record_nvenc_context
{
   uint64_t audio_frames, origin, captured;
   unsigned audio_rate, dims, duration;
   record_gpu_dispatch_d3d12_t *job;
   retro_atomic_int_t stop, failed;
   bool audio_enabled, armed, have_origin, allow_drop;
} record_nvenc_context_t;
static struct fixture_video
{
   struct { struct { double sample_rate; } timing; } av_info;
} fixture_video;
#define video_state_get_ptr() (&fixture_video)
static int result, pushes, cancels, video_pushes;
static uint64_t last_timestamp;
int record_gpu_dispatch_d3d12_audio(record_gpu_dispatch_d3d12_t *job,
      const int16_t *samples, size_t frames)
{
   (void)job;
   assert(samples && frames);
   pushes++;
   return result;
}
void record_gpu_dispatch_d3d12_cancel(record_gpu_dispatch_d3d12_t *job)
{
   (void)job;
   cancels++;
}
int record_gpu_dispatch_d3d12_video(record_gpu_dispatch_d3d12_t *job,
      void *source, unsigned state, unsigned x, unsigned y,
      void *fence, uint64_t value, uint64_t timestamp, uint64_t duration,
      bool keyframe, uint64_t *ready_value)
{
   (void)job; (void)state; (void)x; (void)y; (void)fence;
   (void)value; (void)duration; (void)keyframe; (void)ready_value;
   assert(source);
   video_pushes++;
   last_timestamp = timestamp;
   return result;
}
#include "audio_fragment.c"
#include "video_fragment.c"
static void reset(record_nvenc_context_t *ctx)
{
   memset(ctx, 0, sizeof(*ctx));
   retro_atomic_int_init(&ctx->stop, 0);
   retro_atomic_int_init(&ctx->failed, 0);
   ctx->audio_rate = 48000;
   fixture_video.av_info.timing.sample_rate = 48000;
   result = RECORD_GPU_OUTPUT_PROGRESS;
   pushes = cancels = video_pushes = 0;
   ctx->dims = VIDEO_SCALE_PACK(8, 8);
}
int main(void)
{
   record_nvenc_context_t ctx;
   int16_t samples[8] = {0};
   struct record_audio_data input;
   input.data = samples; input.frames = 4;
   reset(&ctx);
   assert(record_nvenc_audio(&ctx, &input) && !pushes);
   ctx.audio_enabled = true;
   assert(record_nvenc_audio(&ctx, &input) && !pushes);
   ctx.armed = true;
   assert(record_nvenc_audio(&ctx, &input) && pushes == 1 && ctx.audio_frames == 4);
   result = RECORD_GPU_OUTPUT_BUSY;
   assert(!record_nvenc_audio(&ctx, &input));
   assert(pushes == 2 && cancels == 1 && ctx.audio_frames == 4);
   assert(retro_atomic_load_acquire_int(&ctx.failed));
   assert(!record_nvenc_audio(&ctx, &input) && pushes == 2);
   reset(&ctx); ctx.audio_enabled = ctx.armed = true;
   ctx.audio_frames = UINT64_MAX - 2;
   assert(!record_nvenc_audio(&ctx, &input) && !pushes && cancels == 1);
   reset(&ctx); ctx.audio_enabled = ctx.armed = true;
   assert(!record_nvenc_audio(&ctx, NULL) && !pushes && cancels == 1);
   reset(&ctx); ctx.audio_enabled = ctx.armed = true;
   input.frames = 0;
   assert(record_nvenc_audio(&ctx, &input) && !pushes && !cancels);
   input.frames = 4;
   retro_atomic_store_release_int(&ctx.stop, 2);
   assert(!record_nvenc_audio(&ctx, &input) && !pushes);
   reset(&ctx); ctx.audio_enabled = ctx.armed = true;
   fixture_video.av_info.timing.sample_rate = 44100;
   assert(!record_nvenc_audio(&ctx, &input) && !pushes && cancels == 1);
   reset(&ctx);
   ctx.audio_enabled = true;
   retro_atomic_store_release_int(&ctx.stop, 2);
   record_nvenc_gpu_capture(&ctx, samples, 0, 0, ctx.dims, 7007);
   assert(video_pushes == 1 && ctx.captured == 1 && last_timestamp == 7007);
   retro_atomic_store_release_int(&ctx.stop, 1);
   record_nvenc_gpu_capture(&ctx, samples, 0, 0, ctx.dims, 8008);
   assert(video_pushes == 1);
   retro_atomic_store_release_int(&ctx.stop, 3);
   record_nvenc_gpu_capture(&ctx, samples, 0, 0, ctx.dims, 8008);
   assert(video_pushes == 1);
   reset(&ctx); ctx.allow_drop = true;
   result = RECORD_GPU_QUEUE_BUSY;
   record_nvenc_gpu_capture(&ctx, samples, 0, 0, ctx.dims, 0);
   assert(video_pushes == 1 && !ctx.captured && !cancels);
   ctx.allow_drop = false;
   record_nvenc_gpu_capture(&ctx, samples, 0, 0, ctx.dims, 1001);
   assert(video_pushes == 2 && !ctx.captured && cancels == 1);
   puts("PASS: native frontend PCM readiness, whole-batch backpressure and closure");
   return 0;
}
