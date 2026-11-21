/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif
#include "record_pyrowave.h"
#include "../../gfx/video_driver.h"
#ifdef HAVE_RECORD_PYROWAVE_SOURCE
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <retro_atomic.h>
#include <audio/conversion/float_to_s16.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <net/net_compat.h>
#include <formats/rpyrowave_gpu.h>
#include "../record_pyrowave_source.h"
#include "../record_pyrowave_dispatch.h"
#include "../record_native_buffer.h"
#include "../record_audio_tap.h"
#include "../record_pyrowave_video_link.h"
#include "../../gfx/video_thread_wrapper.h"
#include "../../audio/audio_driver.h"
#include "../../record/record_pyrowave_receiver.h"
#include "../../configuration.h"
#include "../record_pyrowave_lan.h"
#include "../../command.h"
#include "../../verbosity.h"

typedef struct record_pyrowave_context
{
   record_pyrowave_source_t source;
   unsigned api;
   record_pyrowave_dispatch_t *job;
   record_pyrowave_pcm_t *pcm;
   record_audio_tap_t *tap;
   record_pyrowave_video_link_t *link;
   rpyrowave_stream_config_t config;
   record_pyrowave_source_config_t setup;
   char directory[1024];
   struct sockaddr_storage address;
   size_t address_bytes;
   video_thread_async_load_t control;
   video_thread_async_job_t control_job;
   void *device;
   uint64_t frames, samples, last_ticket;
   record_gpu_frame_t drain;
   bool suspending, suspended, drain_held;
   retro_time_t origin;
   unsigned dims, full_dims, pos, format, output_mode;
   retro_atomic_int_t stop, failed, render_done, source_ready, recovery, gpu_idle, resize_pending;
   unsigned control_kind;
   bool frontend, tap_installed;
   bool armed, video_closed, control_pending, source_constructed, listen, detached;
} record_pyrowave_context_t;

static record_pyrowave_context_t *record_pyrowave_active;
static record_pyrowave_context_t *record_pyrowave_retired;
static bool record_pyrowave_start_requested;
static uint64_t record_pyrowave_generation;
static bool record_pyrowave_render_service(record_pyrowave_context_t *c);

enum record_pyrowave_control_kind
{
   RECORD_PYROWAVE_CONTROL_OPEN,
   RECORD_PYROWAVE_CONTROL_SERVICE,
   RECORD_PYROWAVE_CONTROL_SUSPEND,
   RECORD_PYROWAVE_CONTROL_FREE,
   RECORD_PYROWAVE_CONTROL_QUIESCE,
   RECORD_PYROWAVE_CONTROL_ABANDON
};
static uintptr_t record_pyrowave_control_run(void *data,
      const video_poke_interface_t *poke, void *user)
{
   record_pyrowave_context_t *c = (record_pyrowave_context_t*)user;
   video_device_info_t device;
   uintptr_t result = 1;
   memset(&device, 0, sizeof(device));
   if (!poke || !poke->get_device || !poke->get_device(data, &device)
         || (unsigned)device.api != c->api || device.device != c->device) return 0;
   if (device.bind_gl_context) device.bind_gl_context(data, true);
   if (c->control_kind == RECORD_PYROWAVE_CONTROL_OPEN)
   {
      result = 0;
#ifdef HAVE_RECORD_PYROWAVE_GL
      if (c->api == VIDEO_DEVICE_OPENGL)
         result = record_pyrowave_source_gl_new(&c->source, &c->setup) ? 1 : 0;
#endif
#ifdef HAVE_RECORD_PYROWAVE_D3D12
      if (c->api == VIDEO_DEVICE_D3D12)
      {
         result = record_pyrowave_source_d3d12_new(&c->source, &c->setup) ? 1 : 0;
         if (result && !video_d3d12_pyrowave_source_attach(data, c->source.user, c))
         {
            c->source.free(c->source.user); c->source.user = NULL; result = 0;
         }
      }
#endif
#ifdef HAVE_RECORD_PYROWAVE_D3D11
      if (c->api == VIDEO_DEVICE_D3D11)
      {
         result = record_pyrowave_source_d3d11_new(&c->source, &c->setup) ? 1 : 0;
         if (result && !video_d3d11_pyrowave_source_attach(data))
         { c->source.free(c->source.user); c->source.user = NULL; result = 0; }
      }
#endif
#ifdef HAVE_RECORD_PYROWAVE_VULKAN
      if (c->api == VIDEO_DEVICE_VULKAN)
         result = record_pyrowave_source_vulkan_new(&c->source, &c->setup) ? 1 : 0;
#endif
   }
   else if (c->control_kind == RECORD_PYROWAVE_CONTROL_SERVICE)
      record_pyrowave_render_service(c);
   else if (c->control_kind == RECORD_PYROWAVE_CONTROL_SUSPEND)
   {
      unsigned i;
      result = 2;
      if (!c->source.poll(c->source.user))
      {
         retro_atomic_store_release_int(&c->recovery, 1);
         result = 0;
      }
      else if (record_pyrowave_video_link_idle(c->link))
      {
         if (!c->source.video.finish(c->source.video.user)) result = 0;
         for (i = 0; result && i < RECORD_GPU_QUEUE_SLOTS; i++)
         {
            record_pyrowave_packet_t packet;
            int taken = c->drain_held ? RECORD_GPU_QUEUE_OK : c->source.video.take(c->source.video.user, &c->drain);
            if (taken != RECORD_GPU_QUEUE_OK) break;
            c->drain_held = true;
            if (c->source.video.packet(c->source.video.user, c->drain.slot, &packet) == RECORD_GPU_QUEUE_BUSY) break;
            if (!c->source.video.release(c->source.video.user, c->drain.slot)) break;
            c->drain_held = false;
         }
      }
      record_pyrowave_dispatch_notify(c->job);
      if (result && record_pyrowave_video_link_idle(c->link) && !c->drain_held && c->source.retired(c->source.user) >= c->last_ticket
            && c->source.idle(c->source.user))
         retro_atomic_store_release_int(&c->render_done, 1);
   }
   else if (c->control_kind == RECORD_PYROWAVE_CONTROL_QUIESCE)
   {
      if (c->source.quiesce(c->source.user, data))
         retro_atomic_store_release_int(&c->gpu_idle, 1);
   }
   else if (c->control_kind == RECORD_PYROWAVE_CONTROL_ABANDON)
   {
#ifdef HAVE_RECORD_PYROWAVE_D3D11
      if (c->api == VIDEO_DEVICE_D3D11) video_d3d11_pyrowave_source_detach(data);
#endif
#ifdef HAVE_RECORD_PYROWAVE_D3D12
      if (c->api == VIDEO_DEVICE_D3D12 && !c->detached)
      { video_d3d12_pyrowave_detach(data, c->source.user); c->detached = true; }
#endif
      if (!c->source.abandon(c->source.user)) result = 0;
      else
      {
         c->source.user = NULL;
         /* An abandoned primary may have left renderer upload/cache and
          * swapchain-acquire state uncommitted. Keep it paused until the
          * existing frontend device-rebuild path replaces that state. */
         if (c->api == VIDEO_DEVICE_D3D12 || c->api == VIDEO_DEVICE_VULKAN)
            video_driver_modify_disp_flags(VIDEO_FLAG_GPU_DEVICE_LOST, 0);
         retro_atomic_store_release_int(&c->render_done, 1);
      }
   }
   else
   {
#ifdef HAVE_RECORD_PYROWAVE_D3D11
      if (c->api == VIDEO_DEVICE_D3D11) video_d3d11_pyrowave_source_detach(data);
#endif
#ifdef HAVE_RECORD_PYROWAVE_D3D12
      if (c->api == VIDEO_DEVICE_D3D12 && !c->detached)
      { video_d3d12_pyrowave_detach(data, c->source.user); c->detached = true; }
#endif
      if (!c->source.free(c->source.user)) result = 0;
      else
      {
         c->source.user = NULL;
#ifdef HAVE_RECORD_PYROWAVE_D3D12
         if (c->api == VIDEO_DEVICE_D3D12) video_d3d12_pyrowave_resume(data);
#endif
      }
   }
   if (device.bind_gl_context) device.bind_gl_context(data, false);
   return result;
}
static void record_pyrowave_control_done(void *user, uintptr_t result)
{
   record_pyrowave_context_t *c = (record_pyrowave_context_t*)user;
   c->control_pending = false;
   if (result && c->control_kind == RECORD_PYROWAVE_CONTROL_OPEN)
      c->source_constructed = true;
   if (result && c->control_kind == RECORD_PYROWAVE_CONTROL_FREE && c->suspending)
   {
      c->source_constructed = false; c->suspending = false; c->suspended = true;
      c->last_ticket = 0; c->video_closed = false; c->detached = false;
      memset(&c->source, 0, sizeof(c->source));
      retro_atomic_store_release_int(&c->render_done, record_pyrowave_active == c ? 0 : 1);
   }
   if (!result)
   {
      retro_atomic_store_release_int(&c->failed, 1);
      if (c->control_kind == RECORD_PYROWAVE_CONTROL_OPEN)
      {
         RARCH_ERR("[PyroWave] Native compute capture setup failed.\n");
         retro_atomic_store_release_int(&c->render_done, 1);
      }
   }
}
static bool record_pyrowave_control_post(record_pyrowave_context_t *c, unsigned kind)
{
   video_driver_state_t *video = video_state_get_ptr();
   if (c->control_pending) return false;
   c->control_kind = kind; c->control_pending = true;
   c->control_job.run = record_pyrowave_control_run; c->control_job.user = c;
   c->control.kind = VIDEO_THREAD_ASYNC_JOB; c->control.img = &c->control_job;
   c->control.user = c; c->control.done = record_pyrowave_control_done;
   if (video->thread_wrapper_active)
   {
      if (video_thread_async_post(&c->control)) return true;
      c->control_pending = false;
      return false;
   }
   record_pyrowave_control_done(c, record_pyrowave_control_run(video->data, video->poke, c));
   return true;
}

static bool record_pyrowave_rate(double fps, uint32_t *num, uint32_t *den)
{
   uint64_t h0 = 0, h1 = 1, k0 = 1, k1 = 0;
   double original = fps;
   unsigned i;
   if (!(fps >= 1.0 && fps <= 1000.0)) return false;
   for (i = 0; i < 32; i++)
   {
      uint64_t a, h, k;
      double fraction;
      if (fps > 1000000.0) return false;
      a = (uint64_t)fps;
      if ((h1 && a > (1000000u - h0) / h1)
            || (k1 && a > (1000000u - k0) / k1)) return false;
      h = a * h1 + h0; k = a * k1 + k0;
      if (!k || !h) return false;
      fraction = fps - (double)a;
      if (fabs((double)h / k - original) <= original * 0.000000001)
      {
         *num = (uint32_t)h; *den = (uint32_t)k;
         return true;
      }
      h0 = h1; h1 = h; k0 = k1; k1 = k;
      if (fraction <= 0.0) return false;
      fps = 1.0 / fraction;
   }
   return false;
}

static bool record_pyrowave_supported(unsigned api)
{
#ifdef HAVE_RECORD_PYROWAVE_GL
   if (api == VIDEO_DEVICE_OPENGL) return true;
#endif
#ifdef HAVE_RECORD_PYROWAVE_D3D11
   if (api == VIDEO_DEVICE_D3D11) return true;
#endif
#ifdef HAVE_RECORD_PYROWAVE_D3D12
   if (api == VIDEO_DEVICE_D3D12) return true;
#endif
#ifdef HAVE_RECORD_PYROWAVE_VULKAN
   if (api == VIDEO_DEVICE_VULKAN) return true;
#endif
   return false;
}

static bool record_pyrowave_open_codec(const struct record_params *params,
      record_pyrowave_context_t *c, const video_device_info_t *device)
{
   record_pyrowave_source_config_t cfg;
   config_file_t *file;
   char directory[1024] = { 0 };
   bool audio = true, ok = false;
   if (c->frontend && (!config_get_ptr()->bools.audio_enable
            || !strcmp(config_get_ptr()->arrays.audio_driver, "null"))) audio = false;
   memset(&cfg, 0, sizeof(cfg));
   c->config.maximum_frame_bytes = 1048576;
   file = NULL;
   if (params->config && *params->config)
   {
      file = config_file_new_from_path_to_string(params->config);
      if (!file) return false;
      if (config_get_entry(file, "pyrowave_shader_directory")
            && !config_get_array(file, "pyrowave_shader_directory", directory, sizeof(directory))) goto done;
   }
   else if (!record_pyrowave_lan_directory(directory, sizeof(directory))) return false;
   if (file && config_get_entry(file, "pyrowave_maximum_bytes")
         && !config_get_uint(file, "pyrowave_maximum_bytes", &c->config.maximum_frame_bytes)) goto done;
   if (file && config_get_entry(file, "audio_enable") && !config_get_bool(file, "audio_enable", &audio)) goto done;
   /* The composed frontend carries no audio without an audio driver,
    * whatever the record config asks: rate and frames go together */
   if (c->frontend && (!config_get_ptr()->bools.audio_enable
            || !strcmp(config_get_ptr()->arrays.audio_driver, "null"))) audio = false;
   c->config.maximum_audio_frames = audio ? 1024 : 0;
   c->config.audio_rate = audio ? (uint32_t)params->samplerate : 0;
   c->config.audio_channels = audio ? params->channels : 0;
   c->config.audio_channel_mask = audio ? recording_state_get_ptr()->layout : 0;
   if (rpyrowave_stream_config_valid(&c->config)) goto done;
   cfg.stream = c->config; cfg.directory = directory; cfg.scale_source = c->frontend;
   cfg.device = device->device; cfg.physical = device->physical_device;
   cfg.instance = device->instance; cfg.queue = device->queue;
   cfg.instance_proc = device->get_instance_proc_addr; cfg.family = device->queue_family;
   cfg.gl_proc = device->get_gl_proc_address;
   cfg.format = c->format;
   cfg.slots = recording_native_buffer_count("pyrowave", params->video_fifo_frames);
   cfg.texture_mode = c->output_mode == 2 ? RPYROWAVE_COLOR_SCRGB : RPYROWAVE_COLOR_CODE;
   cfg.full_width = VIDEO_SCALE_W(c->full_dims); cfg.full_height = VIDEO_SCALE_H(c->full_dims);
   cfg.x = VIDEO_POS_X(c->pos); cfg.y = VIDEO_POS_Y(c->pos);
   strcpy(c->directory, directory); c->setup = cfg;
   c->setup.directory = c->directory; ok = true;

done:
   if (file) config_file_free(file);
   return ok;
}

static void *record_pyrowave_init(const struct record_params *params)
{
   record_pyrowave_context_t *c;
   video_device_info_t device;
   struct retro_hdr_metadata hdr;
   struct video_viewport vp;
   struct addrinfo hints, *address = NULL;
   const char *url, *colon;
   char host[256], port[6];
   size_t length;
   bool listen;
   if (!params || record_pyrowave_active || record_pyrowave_retired
         || !params->filename || !params->video_gpu_record || (audio_driver_has_callback() && !params->menu_source)
         || (config_get_ptr()->bools.video_record_game_only && !record_pyrowave_lan_sending() && !params->menu_source)
         || !video_driver_get_device(&device) || !record_pyrowave_supported(device.api)
         || !(params->samplerate >= 1.0 && params->samplerate <= 384000.0)
         || params->samplerate != floor(params->samplerate)) return NULL;
   url = params->filename;
   listen = !strncmp(url, "rpws-listen://", 14);
   if (!listen && strncmp(url, "rpws://", 7)) return NULL;
   url += listen ? 14 : 7;
   colon = strrchr(url, ':');
   if (!colon || !colon[1] || strlen(colon + 1) > 5) return NULL;
   length = (size_t)(colon - url);
   if (url[0] == '[' && length > 2 && url[length - 1] == ']') { url++; length -= 2; }
   if (!length || length >= sizeof(host)) return NULL;
   memcpy(host, url, length); host[length] = '\0'; strcpy(port, colon + 1);
   /* Numeric addresses avoid a resolver round trip on the main owner. */
   memset(&hints, 0, sizeof(hints)); hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
   if (!network_init() || getaddrinfo_retro(host, port, &hints, &address) || !address) return NULL;
   c = (record_pyrowave_context_t*)calloc(1, sizeof(*c));
   if (!c) { freeaddrinfo_retro(address); return NULL; }
   c->frontend = params->menu_source;
   retro_atomic_int_init(&c->stop, 0); retro_atomic_int_init(&c->failed, 0);
   retro_atomic_int_init(&c->render_done, 0);
   retro_atomic_int_init(&c->resize_pending, 0);
   retro_atomic_int_init(&c->source_ready, 0);
   retro_atomic_int_init(&c->recovery, 0); retro_atomic_int_init(&c->gpu_idle, 0);
   memset(&hdr, 0, sizeof(hdr)); hdr.version = 1; hdr.size = sizeof(hdr);
   memset(&vp, 0, sizeof(vp));
   if (!video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr)
         || !video_driver_get_viewport_info(&vp) || VIDEO_POS_X(vp.pos) < 0 || VIDEO_POS_Y(vp.pos) < 0
         || (c->frontend ? vp.full_dims : vp.dims) != params->out_dims || !VIDEO_SCALE_W(vp.full_dims) || !VIDEO_SCALE_H(vp.full_dims)
         || hdr.output_mode > 2 || record_pyrowave_generation == UINT64_MAX) goto error;
   if (c->frontend) { vp.pos = 0; vp.dims = vp.full_dims; }
   c->device = device.device; c->dims = vp.dims; c->full_dims = vp.full_dims; c->pos = vp.pos;
   c->output_mode = hdr.output_mode;
   c->format = device.output_format; c->api = device.api;
   c->config.sequence.width = VIDEO_SCALE_W(c->dims); c->config.sequence.height = VIDEO_SCALE_H(c->dims);
   c->config.sequence.chroma_444 = ((c->config.sequence.width | c->config.sequence.height) & 1) != 0;
   c->config.sequence.primaries_bt2020 = c->config.sequence.transfer_pq = c->config.sequence.transform_bt2020 = hdr.output_mode != 0;
   c->config.bit_depth = device.output_bit_depth;
   if (!record_pyrowave_rate(params->fps, &c->config.fps_numerator, &c->config.fps_denominator)) goto error;
   if (!record_pyrowave_open_codec(params, c, &device)) goto error;
   if (c->frontend)
   {
      c->link = record_pyrowave_video_link_new();
      if (!c->link) goto error;
   }
   if (c->config.audio_rate)
   {
      c->pcm = record_pyrowave_pcm_new(c->config.audio_channels, c->config.maximum_audio_frames, RECORD_GPU_QUEUE_SLOTS);
      if (!c->pcm) goto error;
   }
   if (address->ai_addrlen > sizeof(c->address)) goto error;
   memcpy(&c->address, address->ai_addr, address->ai_addrlen);
   c->address_bytes = address->ai_addrlen; c->listen = listen;
   if (!record_pyrowave_control_post(c, RECORD_PYROWAVE_CONTROL_OPEN)) goto error;
   freeaddrinfo_retro(address); record_pyrowave_active = c;
   RARCH_LOG("[PyroWave] Stream negotiating.\n");
   return c;
error:
   freeaddrinfo_retro(address);
   record_pyrowave_pcm_free(c->pcm);
   record_pyrowave_video_link_free(c->link);
   if (c->source.free) c->source.free(c->source.user);
   free(c);
   return NULL;
}

static bool record_pyrowave_finalize(void *data)
{
   record_pyrowave_context_t *c = (record_pyrowave_context_t*)data;
   bool closed = c->tap || !c->pcm || record_pyrowave_pcm_finish(c->pcm);
   RARCH_LOG("[PyroWave] Closing stream.\n");
   retro_atomic_store_release_int(&c->stop, 1);
   if (c->frontend && (c->suspending || c->suspended))
   {
      record_pyrowave_video_link_close(c->link);
      if (!c->source_constructed && !c->control_pending) retro_atomic_store_release_int(&c->render_done, 1);
   }
   if (!closed)
   { record_pyrowave_dispatch_cancel(c->job); return false; }
   if (c->tap) return true;
   if (!c->armed)
   { record_pyrowave_dispatch_cancel(c->job); return true; }
   return !c->job || record_pyrowave_dispatch_finish(c->job);
}

static void record_pyrowave_release_context(void *data)
{
   record_pyrowave_active = NULL;
   record_pyrowave_retired = (record_pyrowave_context_t*)data;
}

static bool record_pyrowave_audio_block(record_pyrowave_context_t *c,
      const struct record_audio_data *audio)
{
   const int16_t *source;
   size_t remaining;
   if (!c->pcm || !c->armed || (audio && !audio->frames)) return true;
   if (retro_atomic_load_acquire_int(&c->stop) || retro_atomic_load_acquire_int(&c->failed)) return false;
   if (!audio || !audio->data || (video_state_get_ptr()->av_info.timing.sample_rate != c->config.audio_rate)
         || c->samples > UINT64_MAX - audio->frames) goto error;
   source = (const int16_t*)audio->data; remaining = audio->frames;
   while (remaining)
   {
      void *span;
      size_t capacity, frames, i, samples;
      int result = record_pyrowave_pcm_acquire(c->pcm, &span, &capacity);
      frames = remaining < c->config.maximum_audio_frames ? remaining : c->config.maximum_audio_frames;
      if (result == RECORD_GPU_QUEUE_ERROR) goto error;
      if (result == RECORD_GPU_QUEUE_OK)
      {
         samples = frames * c->config.audio_channels;
         if (samples * 2 > capacity) { record_pyrowave_pcm_cancel(c->pcm); goto error; }
         /* The callback's source expires on return; lend a converted pool span. */
         for (i = 0; i < samples; i++)
         {
            uint16_t value = (uint16_t)source[i];
            ((uint8_t*)span)[2 * i] = (uint8_t)value;
            ((uint8_t*)span)[2 * i + 1] = (uint8_t)(value >> 8);
         }
         if (!record_pyrowave_pcm_submit(c->pcm, c->samples, frames))
         { record_pyrowave_pcm_cancel(c->pcm); goto error; }
         record_pyrowave_dispatch_notify(c->job);
      }
      c->samples += frames; source += frames * c->config.audio_channels; remaining -= frames;
   }
   return true;
error:
   retro_atomic_store_release_int(&c->failed, 1); record_pyrowave_dispatch_cancel(c->job);
   return false;
}

static bool record_pyrowave_audio(void *data, const struct record_audio_data *audio)
{
   record_pyrowave_context_t *c = (record_pyrowave_context_t*)data;
   return c->frontend || record_pyrowave_audio_block(c, audio);
}
static int record_pyrowave_mixed_audio(void *user, const void *data,
      size_t remaining, bool floating, uint64_t pts)
{
   record_pyrowave_context_t *c = (record_pyrowave_context_t*)user;
   const uint8_t *input = (const uint8_t*)data;
   (void)pts;
   if (retro_atomic_load_acquire_int(&c->stop)) return RECORD_AUDIO_OK;
   if (!remaining) return RECORD_AUDIO_OK;
   if (!data || c->samples > UINT64_MAX - remaining) goto error;
   while (remaining)
   {
      void *span;
      size_t capacity, frames = remaining < c->config.maximum_audio_frames
         ? remaining : c->config.maximum_audio_frames;
      int result = record_pyrowave_pcm_acquire(c->pcm, &span, &capacity);
      if (result == RECORD_GPU_QUEUE_ERROR) goto error;
      if (result == RECORD_GPU_QUEUE_OK)
      {
         if (frames > capacity / (2 * sizeof(int16_t)))
         { record_pyrowave_pcm_cancel(c->pcm); goto error; }
         if (floating) convert_float_to_s16((int16_t*)span, (const float*)input, frames * 2);
         else memcpy(span, input, frames * 2 * sizeof(int16_t));
#ifdef MSB_FIRST
         {
            size_t i;
            for (i = 0; i < frames * 2; i++)
            {
               uint8_t *bytes = (uint8_t*)span + i * 2;
               uint8_t byte = bytes[0]; bytes[0] = bytes[1]; bytes[1] = byte;
            }
         }
#endif
         if (!record_pyrowave_pcm_submit(c->pcm, c->samples, frames))
         { record_pyrowave_pcm_cancel(c->pcm); goto error; }
         record_pyrowave_dispatch_notify(c->job);
      }
      c->samples += frames;
      input += frames * 2 * (floating ? sizeof(float) : sizeof(int16_t));
      remaining -= frames;
   }
   return RECORD_AUDIO_OK;
error:
   retro_atomic_store_release_int(&c->failed, 1);
   record_pyrowave_dispatch_cancel(c->job);
   return RECORD_AUDIO_ERROR;
}
/* Retire the post-mixer producer before closing or freeing its PCM pool. */
static bool record_pyrowave_tap_remove(record_pyrowave_context_t *c)
{
   if (!c->tap) return true;
   if (!audio_driver_record_tap_remove(c->tap) || !record_audio_tap_free(c->tap)) return false;
   c->tap = NULL; c->tap_installed = false;
   return true;
}

const record_driver_t record_pyrowave = {
   record_pyrowave_init, record_pyrowave_release_context, NULL, record_pyrowave_audio,
   record_pyrowave_finalize, "pyrowave"
};

bool record_pyrowave_frame_info(struct video_frame_info *info, bool capture)
{
   record_pyrowave_context_t *c = record_pyrowave_active ? record_pyrowave_active : record_pyrowave_retired;
   uint64_t next;
   /* Stop lending retired render snapshots before collecting the context. */
   if (c == record_pyrowave_retired && c
         && (retro_atomic_load_acquire_int(&c->render_done)
            || retro_atomic_load_acquire_int(&c->recovery))) c = NULL;
   if (c && c->frontend && !retro_atomic_load_acquire_int(&c->source_ready)) c = NULL;
   info->recording_pyrowave = c != NULL;
   info->recording_pyrowave_full_output = c && c->frontend;
   if (!c) return false;
   info->recording_gpu = c; info->recording_gpu_pending = record_pyrowave_retired != NULL;
   info->recording_gpu_capture = false; info->recording_gpu_timestamp = 0;
   info->recording_pyrowave_duration = 0;
   if (record_pyrowave_active != c || !capture || !c->armed || (video_state_get_ptr()->frame_repeat && !c->frontend)
         || retro_atomic_load_acquire_int(&c->failed)) return true;
   if (c->frontend && info->frame_time > 0)
   {
      uint64_t elapsed, tick, scale = (uint64_t)c->config.fps_denominator * 1000000;
      if (!c->origin) c->origin = info->frame_time;
      elapsed = info->frame_time >= c->origin ? (uint64_t)(info->frame_time - c->origin) : 0;
      tick = elapsed / scale * c->config.fps_numerator
         + (elapsed % scale) * c->config.fps_numerator / scale;
      if (tick < c->frames) return true;
      c->frames = tick;
   }
   if (c->frames == UINT64_MAX || rpyrowave_stream_time(c->frames, c->config.fps_numerator, c->config.fps_denominator, &info->recording_gpu_timestamp) != RPYROWAVE_OK
         || rpyrowave_stream_time(c->frames + 1, c->config.fps_numerator, c->config.fps_denominator, &next) != RPYROWAVE_OK)
   { retro_atomic_store_release_int(&c->failed, 1); return true; }
   info->recording_pyrowave_duration = next - info->recording_gpu_timestamp;
   c->frames++; info->recording_gpu_capture = true;
   return true;
}

static bool record_pyrowave_render_service(record_pyrowave_context_t *c)
{
   bool valid;
   if (!c || !retro_atomic_load_acquire_int(&c->source_ready)
         || retro_atomic_load_acquire_int(&c->render_done)
         || retro_atomic_load_acquire_int(&c->recovery)) return false;
   valid = c->source.poll(c->source.user);
   if (!valid)
   {
      if (c->source.quiesce) retro_atomic_store_release_int(&c->recovery, 1);
      retro_atomic_store_release_int(&c->failed, 1);
      record_pyrowave_dispatch_cancel(c->job);
   }
   record_pyrowave_dispatch_notify(c->job);
   if (retro_atomic_load_acquire_int(&c->stop))
   {
      if (!c->video_closed)
      {
         if (!c->source.video.finish(c->source.video.user)) return false;
         c->video_closed = true;
      }
      record_pyrowave_dispatch_notify(c->job);
      if (c->source.retired(c->source.user) >= c->last_ticket && c->source.idle(c->source.user))
         retro_atomic_store_release_int(&c->render_done, 1);
      return false;
   }
   return valid && !retro_atomic_load_acquire_int(&c->failed);
}

static bool record_pyrowave_source_matches(record_pyrowave_context_t *c,
      unsigned format, unsigned full_dims, unsigned pos, unsigned dims, unsigned output_mode)
{
   if (c->frontend)
   {
      if ((format != c->format || full_dims != c->full_dims) && output_mode == c->output_mode)
      {
         retro_atomic_store_release_int(&c->source_ready, 0);
         retro_atomic_store_release_int(&c->resize_pending, 1);
         return false;
      }
      pos = 0; dims = c->dims;
   }
   if (format == c->format && full_dims == c->full_dims && pos == c->pos
         && dims == c->dims && output_mode == c->output_mode) return true;
   retro_atomic_store_release_int(&c->failed, 1);
   record_pyrowave_dispatch_cancel(c->job);
   return false;
}

#ifdef HAVE_RECORD_PYROWAVE_GL
void record_pyrowave_gl_frame(void *data, unsigned format, unsigned full_dims,
      unsigned pos, unsigned dims, unsigned output_mode,
      uint64_t timestamp, uint64_t duration, bool capture)
{
   record_pyrowave_context_t *c = (record_pyrowave_context_t*)data;
   int result;
   if (!record_pyrowave_render_service(c) || !capture || c->api != VIDEO_DEVICE_OPENGL) return;
   if (!record_pyrowave_source_matches(c, format, full_dims, pos, dims, output_mode)) return;
   result = record_pyrowave_source_gl_push(&c->source, timestamp, duration, &c->last_ticket);
   if (result == RECORD_GPU_QUEUE_ERROR)
   {
      if (c->source.quiesce) retro_atomic_store_release_int(&c->recovery, 1);
      retro_atomic_store_release_int(&c->failed, 1); record_pyrowave_dispatch_cancel(c->job);
   }
   record_pyrowave_dispatch_notify(c->job);
}
#endif

#ifdef HAVE_RECORD_PYROWAVE_D3D11
void record_pyrowave_d3d11_frame(void *data, void *source, unsigned format, unsigned full_dims,
      unsigned pos, unsigned dims, unsigned output_mode, uint64_t timestamp, uint64_t duration, bool capture)
{
   record_pyrowave_context_t *c = (record_pyrowave_context_t*)data;
   int result;
   if (!record_pyrowave_render_service(c) || !capture || c->api != VIDEO_DEVICE_D3D11) return;
   if (!record_pyrowave_source_matches(c, format, full_dims, pos, dims, output_mode)) return;
   result = record_pyrowave_source_d3d11_push(&c->source, source,
         timestamp, duration, &c->last_ticket);
   if (result == RECORD_GPU_QUEUE_ERROR)
   {
      if (c->source.quiesce) retro_atomic_store_release_int(&c->recovery, 1);
      retro_atomic_store_release_int(&c->failed, 1); record_pyrowave_dispatch_cancel(c->job);
   }
   record_pyrowave_dispatch_notify(c->job);
}
#endif

void record_pyrowave_d3d12_frame(void *data, void *commands, void *fence,
      uint64_t value, void *source, unsigned format, unsigned full_dims,
      unsigned pos, unsigned dims, unsigned output_mode, uint64_t timestamp, uint64_t duration, bool capture)
{
   record_pyrowave_context_t *c = (record_pyrowave_context_t*)data;
   int result;
   if (!record_pyrowave_render_service(c) || !capture || c->api != VIDEO_DEVICE_D3D12) return;
   if (!record_pyrowave_source_matches(c, format, full_dims, pos, dims, output_mode)) return;
   result = record_pyrowave_source_d3d12_push(&c->source, commands, fence, value, source,
         timestamp, duration, &c->last_ticket);
   if (result == RECORD_GPU_QUEUE_ERROR)
   {
      if (c->source.quiesce) retro_atomic_store_release_int(&c->recovery, 1);
      retro_atomic_store_release_int(&c->failed, 1); record_pyrowave_dispatch_cancel(c->job);
   }
   record_pyrowave_dispatch_notify(c->job);
}

#ifdef HAVE_RECORD_PYROWAVE_VULKAN
VkFence record_pyrowave_vulkan_frame(void *data, VkCommandBuffer commands,
      VkImage image, unsigned format, unsigned full_dims, unsigned pos,
      unsigned dims, unsigned output_mode, uint64_t timestamp, uint64_t duration, bool capture)
{
   record_pyrowave_context_t *c = (record_pyrowave_context_t*)data;
   VkFence completion = VK_NULL_HANDLE;
   int result;
   if (!record_pyrowave_render_service(c) || !capture || c->api != VIDEO_DEVICE_VULKAN) return VK_NULL_HANDLE;
   if (!record_pyrowave_source_matches(c, format, full_dims, pos, dims, output_mode)) return VK_NULL_HANDLE;
   result = record_pyrowave_source_vulkan_push(&c->source, commands, image, timestamp, duration, &c->last_ticket, &completion);
   if (result == RECORD_GPU_QUEUE_ERROR)
   {
      if (c->source.quiesce) retro_atomic_store_release_int(&c->recovery, 1);
      retro_atomic_store_release_int(&c->failed, 1); record_pyrowave_dispatch_cancel(c->job);
   }
   record_pyrowave_dispatch_notify(c->job);
   return completion;
}

void record_pyrowave_vulkan_submitted(void *data, VkFence fence, VkResult result)
{
   record_pyrowave_context_t *c = (record_pyrowave_context_t*)data;
   bool retired;
   if (!c || c->api != VIDEO_DEVICE_VULKAN) return;
   retired = record_pyrowave_source_vulkan_submitted(&c->source, fence, result);
   if (result != VK_SUCCESS || !retired) record_pyrowave_failed(c);
}

void record_pyrowave_vulkan_unsubmitted(void *data, VkFence fence)
{
   record_pyrowave_context_t *c = (record_pyrowave_context_t*)data;
   if (!c || c->api != VIDEO_DEVICE_VULKAN) return;
   if (!record_pyrowave_source_vulkan_discard(&c->source, fence))
      retro_atomic_store_release_int(&c->recovery, 1);
   retro_atomic_store_release_int(&c->failed, 1);
   record_pyrowave_dispatch_cancel(c->job);
}

#endif

void record_pyrowave_failed(void *data)
{
   record_pyrowave_context_t *c = (record_pyrowave_context_t*)data;
   if (!c) return;
   retro_atomic_store_release_int(&c->recovery, 1);
   retro_atomic_store_release_int(&c->failed, 1);
   record_pyrowave_dispatch_cancel(c->job);
}

bool record_pyrowave_frontend_active(void)
{
   return record_pyrowave_active && record_pyrowave_active->frontend
      && !retro_atomic_load_acquire_int(&record_pyrowave_active->failed);
}
bool record_pyrowave_frontend_pause(void)
{
   record_pyrowave_context_t *c = record_pyrowave_active;
   if (!record_pyrowave_frontend_active()) return false;
   if (!c->suspended)
   {
      c->suspending = true;
      retro_atomic_store_release_int(&c->source_ready, 0);
      record_pyrowave_video_link_pause(c->link);
      record_pyrowave_dispatch_notify(c->job);
   }
   return true;
}
bool record_pyrowave_reset_pending(void)
{
   record_pyrowave_context_t *c = record_pyrowave_active;
   return record_pyrowave_retired || record_pyrowave_start_requested
      || (c && (!c->frontend || !c->suspended || c->control_pending));
}
static void record_pyrowave_suspend_poll(record_pyrowave_context_t *c)
{
   if (c->control_pending || (video_driver_thread_wrapper_active()
            && video_thread_record_gpu_in_flight(video_state_get_ptr()->data, c))) return;
   if (!c->source_constructed)
   {
      c->suspending = false; c->suspended = true;
      return;
   }
   if (retro_atomic_load_acquire_int(&c->render_done))
      record_pyrowave_control_post(c, RECORD_PYROWAVE_CONTROL_FREE);
   else record_pyrowave_control_post(c, RECORD_PYROWAVE_CONTROL_SUSPEND);
}
static void record_pyrowave_resume_poll(record_pyrowave_context_t *c)
{
   video_device_info_t device;
   struct video_viewport vp;
   struct retro_hdr_metadata hdr;
   runloop_state_t *loop = runloop_state_get_ptr();
   if (c->control_pending || loop->content_closing || loop->quit_pending
         || runloop_is_content_switching() || recording_gpu_reinit_pending()
         || !video_state_get_ptr()->data) return;
   if (!c->source_constructed)
   {
      memset(&device, 0, sizeof(device)); memset(&vp, 0, sizeof(vp));
      memset(&hdr, 0, sizeof(hdr)); hdr.version = 1; hdr.size = sizeof(hdr);
      if (!video_driver_get_device(&device) || !video_driver_get_viewport_info(&vp)
            || !video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr)) return;
      if (!VIDEO_SCALE_W(vp.full_dims) || !VIDEO_SCALE_H(vp.full_dims) || hdr.output_mode != c->output_mode
            || device.output_bit_depth != c->config.bit_depth || !record_pyrowave_supported(device.api))
      { retro_atomic_store_release_int(&c->failed, 1); return; }
      c->device = device.device; c->format = device.output_format; c->api = device.api;
      c->full_dims = vp.full_dims; c->pos = 0;
      c->setup.full_width = VIDEO_SCALE_W(vp.full_dims); c->setup.full_height = VIDEO_SCALE_H(vp.full_dims);
      c->setup.device = device.device; c->setup.physical = device.physical_device;
      c->setup.instance = device.instance; c->setup.queue = device.queue;
      c->setup.instance_proc = device.get_instance_proc_addr; c->setup.family = device.queue_family;
      c->setup.gl_proc = device.get_gl_proc_address; c->setup.format = c->format;
      record_pyrowave_control_post(c, RECORD_PYROWAVE_CONTROL_OPEN);
   }
   else if (record_pyrowave_video_link_bind(c->link, &c->source.video))
   {
      c->suspended = false;
      retro_atomic_store_release_int(&c->source_ready, 1);
      record_pyrowave_dispatch_notify(c->job);
      RARCH_LOG("[PyroWave] Frontend capture rebound.\n");
   }
}

void record_pyrowave_stop(void)
{
   if (record_pyrowave_active || record_pyrowave_start_requested) command_event(CMD_EVENT_RECORD_DEINIT, NULL);
}
bool record_pyrowave_pending(void)
{
   return record_pyrowave_active || record_pyrowave_retired || record_pyrowave_start_requested;
}
bool record_pyrowave_defer_start(void) { record_pyrowave_start_requested = true; return true; }
bool record_pyrowave_cancel_start(void)
{
   bool requested = record_pyrowave_start_requested; record_pyrowave_start_requested = false; return requested;
}

void record_pyrowave_poll(void)
{
   record_pyrowave_context_t *c = record_pyrowave_active;
   video_thread_async_poll();
   if (record_pyrowave_start_requested)
   {
      struct video_viewport vp;
      memset(&vp, 0, sizeof(vp));
      if (!recording_state_get_ptr()->enable) record_pyrowave_start_requested = false;
      else if (video_driver_get_viewport_info(&vp) && VIDEO_SCALE_W(vp.dims) && VIDEO_SCALE_H(vp.dims))
      { record_pyrowave_start_requested = false; command_event(CMD_EVENT_RECORD_INIT, NULL); }
   }
   if (c)
   {
      unsigned state;
      if (retro_atomic_load_acquire_int(&c->resize_pending))
      {
         retro_atomic_store_release_int(&c->resize_pending, 0);
         record_pyrowave_frontend_pause();
      }
      if (c->suspending) record_pyrowave_suspend_poll(c);
      else if (c->suspended) record_pyrowave_resume_poll(c);
      if (!c->job && !c->suspending && !c->suspended && !c->control_pending
            && c->source_constructed
            && !retro_atomic_load_acquire_int(&c->failed) && !retro_atomic_load_acquire_int(&c->stop))
      {
         record_pyrowave_video_t endpoint;
         const record_pyrowave_video_t *video = &c->source.video;
         if (c->link)
         {
            if (!record_pyrowave_video_link_bind(c->link, video)) return;
            record_pyrowave_video_link_video(c->link, &endpoint); video = &endpoint;
         }
         c->job = record_pyrowave_dispatch_new(&c->address, c->address_bytes, c->listen,
               &c->config, ++record_pyrowave_generation, video, c->pcm);
         if (!c->job) retro_atomic_store_release_int(&c->failed, 1);
         else retro_atomic_store_release_int(&c->source_ready, 1);
      }
      state = c->job ? record_pyrowave_dispatch_state(c->job, NULL) : RECORD_PYROWAVE_DISPATCH_STARTING;
      if (state == RECORD_PYROWAVE_DISPATCH_READY && !c->armed)
      { c->armed = true; RARCH_LOG("[PyroWave] Stream ready.\n"); }
      if (c->frontend && c->armed && c->pcm && !retro_atomic_load_acquire_int(&c->stop))
      {
         if (c->tap && c->tap_installed && record_audio_tap_end(c->tap, NULL))
         {
            record_audio_tap_free(c->tap); c->tap = NULL; c->tap_installed = false;
         }
         if (!c->tap) c->tap = record_audio_tap_new_raw(c->config.audio_rate, record_pyrowave_mixed_audio, c);
         if (!c->tap) retro_atomic_store_release_int(&c->failed, 1);
         else if (!c->tap_installed && !runloop_state_get_ptr()->content_closing
               && !runloop_is_content_switching() && !recording_gpu_reinit_pending())
         {
            int installed = audio_driver_record_tap_install(c->tap);
            if (installed > 0) c->tap_installed = true;
            else if (installed < 0) retro_atomic_store_release_int(&c->failed, 1);
         }
         else if (record_audio_tap_failed(c->tap)) retro_atomic_store_release_int(&c->failed, 1);
      }
      if (retro_atomic_load_acquire_int(&c->failed) || state >= RECORD_PYROWAVE_DISPATCH_DONE) record_pyrowave_stop();
   }
   c = record_pyrowave_retired;
   if (c && c->tap)
   {
      if (!record_pyrowave_tap_remove(c)) return;
      if (c->pcm && !record_pyrowave_pcm_finish(c->pcm)) record_pyrowave_dispatch_cancel(c->job);
      else if (c->job) record_pyrowave_dispatch_finish(c->job);
   }
   /* No frame snapshot names this context: transfer the GPU poll/close
    * owner for retirement even when content no longer produces frames. */
   if (c && c->suspending && !retro_atomic_load_acquire_int(&c->recovery))
      record_pyrowave_suspend_poll(c);
   if (c && (!c->suspending || retro_atomic_load_acquire_int(&c->recovery))
         && (!video_driver_thread_wrapper_active()
            || !video_thread_record_gpu_in_flight(video_state_get_ptr()->data, c)))
   {
      if (!c->control_pending && c->source_constructed)
         retro_atomic_store_release_int(&c->source_ready, 1);
      if (!c->control_pending && retro_atomic_load_acquire_int(&c->source_ready)
            && !retro_atomic_load_acquire_int(&c->render_done))
      {
         if (!retro_atomic_load_acquire_int(&c->recovery))
            record_pyrowave_control_post(c, RECORD_PYROWAVE_CONTROL_SERVICE);
         else if (!retro_atomic_load_acquire_int(&c->gpu_idle))
            record_pyrowave_control_post(c, RECORD_PYROWAVE_CONTROL_QUIESCE);
         else
         {
            record_pyrowave_dispatch_abandon_video(c->job);
            if (record_pyrowave_dispatch_free(c->job))
            {
               c->job = NULL;
               record_pyrowave_control_post(c, RECORD_PYROWAVE_CONTROL_ABANDON);
            }
         }
      }
   }
   if (c && !c->control_pending && retro_atomic_load_acquire_int(&c->render_done)
         && (!video_driver_thread_wrapper_active() || !video_thread_record_gpu_in_flight(video_state_get_ptr()->data, c))
         && record_pyrowave_dispatch_free(c->job))
   {
      record_gpu_frame_t frame;
      record_pyrowave_pcm_view_t pcm;
      c->job = NULL;
      while (c->source.user && c->source.video.take(c->source.video.user, &frame) == RECORD_GPU_QUEUE_OK)
         c->source.video.release(c->source.video.user, frame.slot);
      while (c->pcm && record_pyrowave_pcm_take(c->pcm, &pcm) == RECORD_GPU_QUEUE_OK)
         record_pyrowave_pcm_release(c->pcm, pcm.slot);
      if (c->pcm && !record_pyrowave_pcm_finish(c->pcm))
      {
         record_pyrowave_pcm_cancel(c->pcm);
         if (!record_pyrowave_pcm_finish(c->pcm)) return;
      }
      if (!record_pyrowave_pcm_free(c->pcm)) return;
      c->pcm = NULL;
      if (c->source.user)
      { record_pyrowave_control_post(c, RECORD_PYROWAVE_CONTROL_FREE); return; }
      RARCH_LOG("[PyroWave] Stream resources retired.\n");
      record_pyrowave_video_link_free(c->link);
      record_pyrowave_retired = NULL; free(c);
   }
}
#else
const record_driver_t record_pyrowave = { NULL, NULL, NULL, NULL, NULL, "pyrowave" };
bool record_pyrowave_frontend_active(void) { return false; }
bool record_pyrowave_frontend_pause(void) { return false; }
bool record_pyrowave_reset_pending(void) { return false; }
bool record_pyrowave_frame_info(struct video_frame_info *info, bool capture)
{ (void)capture; info->recording_pyrowave = false; return false; }
void record_pyrowave_poll(void) { }
void record_pyrowave_stop(void) { }
void record_pyrowave_failed(void *data) { (void)data; }
bool record_pyrowave_pending(void) { return false; }
bool record_pyrowave_defer_start(void) { return false; }
bool record_pyrowave_cancel_start(void) { return false; }
void record_pyrowave_d3d11_frame(void *c, void *view, unsigned format, unsigned full_dims,
      unsigned pos, unsigned dims, unsigned mode, uint64_t pts, uint64_t duration, bool capture)
{ (void)c; (void)view; (void)format; (void)full_dims; (void)pos; (void)dims; (void)mode;
  (void)pts; (void)duration; (void)capture; }
void record_pyrowave_d3d12_frame(void *c, void *commands, void *fence, uint64_t value,
      void *source, unsigned format, unsigned full_dims, unsigned pos, unsigned dims,
      unsigned output_mode, uint64_t timestamp, uint64_t duration, bool capture)
{ (void)c; (void)commands; (void)fence; (void)value; (void)source; (void)format;
  (void)full_dims; (void)pos; (void)dims; (void)output_mode; (void)timestamp; (void)duration; (void)capture; }
#endif
