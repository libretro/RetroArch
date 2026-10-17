/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define main session_fixture_main
#define nvenc_d3d12_wait_handle fixture_wait_handle
#include "../gpu_av_output/session_test.c"
#undef main
#undef nvenc_d3d12_wait_handle
#include "../../../record/record_gpu_dispatch_d3d12.h"
/* The simulated encoder completes through service, not a kernel handle. */
void *nvenc_d3d12_wait_handle(const nvenc_d3d12_t *e) { (void)e; return NULL; }
static unsigned captures_freed;
static int integration(unsigned mode, bool owned)
{
   record_gpu_capture_d3d12_t capture;
   record_gpu_capture_d3d12_t *input = &capture;
   record_gpu_output_config_t output;
   nvenc_d3d12_config_t encoder;
   record_gpu_dispatch_d3d12_t *job;
   enum record_gpu_dispatch_state state;
   enum record_gpu_session_error error;
   uint32_t file_error, system_error;
   int16_t pcm[200] = {0};
   unsigned i, attempts;
   setup(&output, &encoder);
   captures_freed = 0;
   if (owned) { input = (record_gpu_capture_d3d12_t*)calloc(1, sizeof(*input)); CHECK(input); }
   input->queue = record_gpu_queue_new(2); CHECK(input->queue);
   headers_fault = mode == 2;
   file_step_fail = mode == 3;
   job = owned ? record_gpu_dispatch_d3d12_new_owned(input, &encoder, &output, "unused")
      : record_gpu_dispatch_d3d12_new(input, &encoder, &output, "unused"); CHECK(job);
   for (attempts = 0; attempts < 10000; attempts++)
   {
      state = record_gpu_dispatch_d3d12_status(job, NULL, NULL, NULL);
      if (state != RECORD_GPU_DISPATCH_STARTING) break;
      SESSION_YIELD();
   }
   CHECK(attempts < 10000);
   if (mode < 2)
      for (i = 0; i < 12; i++)
      {
         int result = RECORD_GPU_QUEUE_BUSY;
         for (attempts = 0; attempts < 10000 && result == RECORD_GPU_QUEUE_BUSY; attempts++)
         {
            result = record_gpu_dispatch_d3d12_video(job, NULL, 0, 0, 0, NULL, 0, i, 1, true, NULL);
            if (result == RECORD_GPU_QUEUE_BUSY) SESSION_YIELD();
         }
         CHECK(result == RECORD_GPU_QUEUE_OK);
         result = RECORD_GPU_OUTPUT_BUSY;
         for (attempts = 0; attempts < 10000 && result == RECORD_GPU_OUTPUT_BUSY; attempts++)
         {
            result = record_gpu_dispatch_d3d12_audio(job, pcm, 100);
            if (result == RECORD_GPU_OUTPUT_BUSY) SESSION_YIELD();
         }
         CHECK(result == RECORD_GPU_OUTPUT_PROGRESS);
         if (mode == 1 && i == 5) { record_gpu_dispatch_d3d12_cancel(job); break; }
      }
   CHECK(record_gpu_dispatch_d3d12_video_finish(job));
   CHECK(record_gpu_dispatch_d3d12_audio_finish(job));
   for (attempts = 0; attempts < 10000; attempts++)
   {
      state = record_gpu_dispatch_d3d12_status(job, &error, &file_error, &system_error);
      if (state == RECORD_GPU_DISPATCH_DONE || state == RECORD_GPU_DISPATCH_FAILED) break;
      SESSION_YIELD();
   }
   CHECK(attempts < 10000 && !system_error);
   CHECK(state == (mode ? RECORD_GPU_DISPATCH_FAILED : RECORD_GPU_DISPATCH_DONE));
   CHECK(error == (mode == 0 ? RECORD_GPU_SESSION_ERROR_NONE : mode == 1
            ? RECORD_GPU_SESSION_ERROR_CANCELLED : mode == 2
            ? RECORD_GPU_SESSION_ERROR_STARTUP : RECORD_GPU_SESSION_ERROR_OUTPUT));
   if (mode == 3) CHECK(file_error);
   for (attempts = 0; attempts < 10000 && !record_gpu_dispatch_d3d12_free(job); attempts++) SESSION_YIELD();
   CHECK(attempts < 10000 && opened == freed);
   if (owned) CHECK(captures_freed == 1);
   else { CHECK(!captures_freed); CHECK(record_gpu_queue_free(capture.queue)); }
   return 0;
}
int record_gpu_capture_d3d12_push(record_gpu_capture_d3d12_t *c, void *source,
      unsigned state, unsigned x, unsigned y, void *fence, uint64_t value,
      uint64_t timestamp, uint64_t duration, bool keyframe, uint64_t *ready)
{
   record_gpu_frame_t frame;
   int result = record_gpu_queue_acquire(c->queue, &frame.slot);
   (void)source; (void)state; (void)x; (void)y; (void)fence; (void)value;
   if (result != RECORD_GPU_QUEUE_OK) return result;
   frame.timestamp = timestamp; frame.duration = duration; frame.ready_value = 1; frame.keyframe = keyframe;
   if (ready) *ready = 1;
   return record_gpu_queue_submit(c->queue, &frame) ? RECORD_GPU_QUEUE_OK : RECORD_GPU_QUEUE_ERROR;
}
bool record_gpu_capture_d3d12_reap(record_gpu_capture_d3d12_t *c) { return c != NULL; }
bool record_gpu_capture_d3d12_free(record_gpu_capture_d3d12_t *c)
{
   if (!record_gpu_queue_free(c->queue)) return false;
   free(c); captures_freed++; return true;
}
int main(void)
{
   unsigned i;
   for (i = 0; i < 4; i++) if (integration(i, false) || integration(i, true)) return 1;
   puts("PASS: dispatcher with production session, worker, queue, AC-3 and MP4 ownership");
   return 0;
}
