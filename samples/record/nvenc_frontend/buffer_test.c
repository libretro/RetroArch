/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual frontend initializer; GPU allocation is intercepted at its boundary. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include "../../../configuration.h"
#include "../../../gfx/video_driver.h"
#include "../../../record/record_driver.h"
#include "../../../record/record_native_buffer.h"
#include "../../../record/record_gpu_dispatch_d3d12.h"

static settings_t settings;
static recording_state_t recording;
static unsigned captured_slots, capture_calls;
static char device_token;
settings_t *config_get_ptr(void)
{
   return &settings;
}
recording_state_t *recording_state_get_ptr(void)
{
   return &recording;
}
bool audio_driver_has_callback(void)
{
   return false;
}
bool video_driver_get_device(video_device_info_t *device)
{
   memset(device, 0, sizeof(*device));
   device->api = VIDEO_DEVICE_D3D12;
   device->device = device->queue = &device_token;
   return true;
}
bool video_driver_get_display_metadata(unsigned command, void *data)
{
   (void)command;
   (void)data;
   return true;
}
static bool record_nvenc_rate(double fps, unsigned *num, unsigned *den)
{
   (void)fps;
   *num = 60;
   *den = 1;
   return true;
}
static bool record_nvenc_config(const char *path, nvenc_d3d12_config_t *encoder,
      bool *audio, bool streaming, bool *legacy)
{
   (void)path;
   (void)encoder;
   (void)streaming;
   *audio = false;
   *legacy = false;
   return true;
}
static bool record_nvenc_color(nvenc_d3d12_config_t *encoder,
      const struct retro_hdr_metadata *hdr)
{
   (void)encoder;
   (void)hdr;
   return true;
}
static bool record_nvenc_layout(uint32_t layout, unsigned channels)
{
   (void)layout;
   (void)channels;
   return true;
}
record_gpu_capture_d3d12_t *record_gpu_capture_d3d12_new(
      const nvenc_d3d12_config_t *encoder, void *queue)
{
   assert(queue == &device_token);
   captured_slots = encoder->slots;
   capture_calls++;
   /* Failure deliberately stops before worker ownership can transfer. */
   return NULL;
}
bool record_gpu_capture_d3d12_free(record_gpu_capture_d3d12_t *capture)
{
   (void)capture;
   assert(0);
   return false;
}
record_gpu_dispatch_d3d12_t *record_gpu_dispatch_d3d12_new_owned(
      record_gpu_capture_d3d12_t *capture, const nvenc_d3d12_config_t *encoder,
      const record_gpu_output_config_t *output, const char *path)
{
   (void)capture;
   (void)encoder;
   (void)output;
   (void)path;
   assert(0);
   return NULL;
}
record_gpu_dispatch_d3d12_t *record_gpu_dispatch_d3d12_new_stream_owned(
      record_gpu_capture_d3d12_t *capture, const nvenc_d3d12_config_t *encoder,
      const record_gpu_output_config_t *output, const char *path, bool legacy)
{
   (void)legacy;
   return record_gpu_dispatch_d3d12_new_owned(capture, encoder, output, path);
}
#define RARCH_LOG(...) ((void)0)
#define RARCH_ERR(...) ((void)0)
#include "buffer_fragment.c"

int main(void)
{
   struct record_params params;
   unsigned slots;
   memset(&params, 0, sizeof(params));
   params.filename = "buffer.mp4";
   params.video_gpu_record = true;
   params.out_dims = VIDEO_SCALE_PACK(320, 240);
   assert(!record_nvenc_start_requested);
   assert(RECORD_GPU_QUEUE_SLOTS == NVENC_D3D12_SLOTS);
   for (slots = 0; slots <= NVENC_D3D12_SLOTS + 1; slots++)
   {
      params.video_fifo_frames = slots;
      assert(!record_nvenc_init(&params));
      assert(capture_calls == slots + 1);
      assert(captured_slots == (slots >= 4 && slots <= 16 ? slots : 4));
   }
   puts("PASS actual NVENC initializer forwards bounded requested capture slots");
   return 0;
}
