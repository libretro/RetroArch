/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif
#include "pyrowave_core.h"
#include "../internal_cores.h"
#include "../../gfx/video_driver.h"
#include "../../audio/audio_presentation.h"
#include <string.h>
#include <stdlib.h>

static retro_environment_t pyrowave_core_environment;
static retro_video_refresh_t pyrowave_core_video;
static retro_audio_sample_batch_t pyrowave_core_audio;
static retro_input_poll_t pyrowave_core_input;
static struct retro_system_av_info pyrowave_core_av;

#if defined(_WIN32) && defined(HAVE_D3D12) && !defined(__WINRT__) && !defined(_XBOX)
#define PYROWAVE_CORE_D3D12
#endif
#if defined(HAVE_VULKAN) && !defined(ANDROID)
#define PYROWAVE_CORE_VULKAN
#endif
#if defined(HAVE_THREADS) && defined(HAVE_CONFIGFILE) && ((defined(HAVE_OPENGL_CORE) && !defined(HAVE_OPENGLES)) || defined(PYROWAVE_CORE_D3D12) || defined(PYROWAVE_CORE_VULKAN))
#define PYROWAVE_CORE_GPU
#include <retro_atomic.h>
#include <file/config_file.h>
#include <net/net_compat.h>
#include "../../gfx/video_thread_wrapper.h"
#include "../../record/record_pyrowave_player_gl.h"
#ifdef PYROWAVE_CORE_D3D12
#include "../../record/record_pyrowave_player_d3d12.h"
#endif
#ifdef PYROWAVE_CORE_VULKAN
#include "../../record/record_pyrowave_player_vulkan.h"
#define PYROWAVE_CORE_VK_FENCES (2 * RECORD_GPU_QUEUE_SLOTS)
#endif
#include "../../record/record_pyrowave_dispatch.h"
#include "../../record/record_driver.h"
#include "../../audio/audio_driver.h"
#include "../../audio/audio_thread_wrapper.h"
#include "../../audio/audio_pyrowave.h"
#include "../../configuration.h"
#include "../../verbosity.h"

typedef struct pyrowave_core_client
{
   record_pyrowave_dispatch_t *io;
   record_pyrowave_player_gl_t *player;
   void *player_d3d12;
#ifdef PYROWAVE_CORE_D3D12
   record_pyrowave_d3d12_image_t d3d12_image, d3d12_future;
   ID3D12Fence *display_fence;
   uint64_t display_value;
   bool d3d12_detached;
#endif
#ifdef PYROWAVE_CORE_VULKAN
   record_pyrowave_vulkan_t *player_vulkan;
   record_pyrowave_vulkan_image_t vulkan_image, vulkan_future;
   VkFence vulkan_fences[PYROWAVE_CORE_VK_FENCES];
   VkFence vulkan_recovery_fence;
   bool vulkan_recovery_submitted;
   bool vulkan_busy[PYROWAVE_CORE_VK_FENCES];
   unsigned vulkan_pending, vulkan_display, vulkan_previous;
   bool vulkan_drew;
   PFN_vkCreateFence vulkan_create;
   PFN_vkDestroyFence vulkan_destroy;
   PFN_vkGetFenceStatus vulkan_status;
   PFN_vkResetFences vulkan_reset;
#endif
   enum video_device_api api;
   record_pyrowave_pcm_t *pcm;
   record_pyrowave_audio_clock_t *audio_clock;
   record_pyrowave_pcm_view_t audio;
   record_pyrowave_video_t input;
   record_pyrowave_source_config_t setup;
   record_pyrowave_gl_image_t image, gl_future;
   video_thread_async_load_t control;
   video_thread_async_job_t control_job;
   char directory[1024];
   int16_t *mono;
   size_t audio_offset;
   uint64_t generation;
   uint64_t target_time, tail_time;
   uint64_t audio_last_sample;
   int64_t tail_anchor;
   void *device;
   retro_atomic_int_t ready, stop, failed, render_done, io_done, video_end, recovery;
   retro_atomic_int_t audio_end, audio_done, audio_failed;
   retro_atomic_int_t clock_sequence, clock_low, clock_high;
   unsigned audio_request;
   unsigned control_kind, decoding;
   bool control_pending, configured, constructed, accepted, av_updating;
   bool held, input_closed, audio_held, audio_input_end;
   bool audio_async, audio_bound, audio_unbinding, audio_reported, audio_received;
   bool target_valid, future, video_input_end, tail;
   bool audio_clock_known;
} pyrowave_core_client_t;

static pyrowave_core_client_t *pyrowave_core_client;
static char *pyrowave_core_path;
static bool pyrowave_core_loaded, pyrowave_core_restart;
static bool pyrowave_core_async_audio;
/* One outstanding main->audio request; ack is the old borrow's last access. */
static pyrowave_core_client_t *pyrowave_core_audio_requested;
static pyrowave_core_client_t *pyrowave_core_audio_active;
static retro_atomic_int_t pyrowave_core_audio_sequence = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t pyrowave_core_audio_ack = RETRO_ATOMIC_INT_INITIALIZER(0);

static bool pyrowave_core_audio_request(pyrowave_core_client_t *target, unsigned *request)
{
   unsigned sequence = (unsigned)retro_atomic_load_relaxed_int(&pyrowave_core_audio_sequence);
   if (sequence != (unsigned)retro_atomic_load_acquire_int(&pyrowave_core_audio_ack)) return false;
   pyrowave_core_audio_requested = target;
   *request = sequence + 1;
   retro_atomic_store_release_int(&pyrowave_core_audio_sequence, (int)*request);
   return true;
}

static void pyrowave_core_audio_retire(pyrowave_core_client_t *c, bool failed)
{
   if (!c) return;
   audio_driver_pyrowave_detach(c->audio_clock);
   if (c->audio_held)
   { record_pyrowave_pcm_release(c->pcm, c->audio.slot); c->audio_held = false; }
   if (failed && !retro_atomic_load_acquire_int(&c->stop))
   {
      retro_atomic_store_release_int(&c->audio_failed, 1);
      retro_atomic_store_release_int(&c->failed, 1);
   }
   retro_atomic_store_release_int(&c->audio_done, 1);
}

void libretro_pyrowave_audio_control(bool exiting)
{
   unsigned sequence = (unsigned)retro_atomic_load_acquire_int(&pyrowave_core_audio_sequence);
   unsigned ack = (unsigned)retro_atomic_load_relaxed_int(&pyrowave_core_audio_ack);
   pyrowave_core_client_t *target = sequence == ack ? NULL : pyrowave_core_audio_requested;
   if (exiting)
   {
      pyrowave_core_audio_retire(pyrowave_core_audio_active, true);
      if (target != pyrowave_core_audio_active) pyrowave_core_audio_retire(target, true);
      pyrowave_core_audio_active = NULL;
   }
   else if (sequence != ack)
   {
      if (target != pyrowave_core_audio_active) pyrowave_core_audio_retire(pyrowave_core_audio_active, false);
      pyrowave_core_audio_active = target;
   }
   if (sequence != ack) retro_atomic_store_release_int(&pyrowave_core_audio_ack, (int)sequence);
}

static void pyrowave_core_audio_publish(pyrowave_core_client_t *c,
      const record_pyrowave_clock_position_t *position, bool ended)
{
   uint64_t packed = 0;
   unsigned sequence = (unsigned)retro_atomic_load_relaxed_int(&c->clock_sequence);
   if (position->valid)
   { c->audio_last_sample = position->source_sample; c->audio_clock_known = true; }
   if (record_pyrowave_audio_clock_drained(c->audio_clock))
   { c->audio_last_sample = record_pyrowave_audio_clock_end_sample(c->audio_clock); c->audio_clock_known = true; }
   if (c->audio_clock_known && c->audio_last_sample <= (((uint64_t)-1) >> 3))
      packed = c->audio_last_sample * 4 + (position->valid ? 1 : 3);
   if (ended)
   {
      uint64_t end = record_pyrowave_audio_clock_end_sample(c->audio_clock);
      if (end <= (((uint64_t)-1) >> 3)) packed = end * 4 + 2;
   }
   retro_atomic_store_release_int(&c->clock_sequence, (int)(sequence + 1));
   retro_atomic_thread_fence_release();
   retro_atomic_store_relaxed_int(&c->clock_low, (int)(uint32_t)packed);
   retro_atomic_store_relaxed_int(&c->clock_high, (int)(uint32_t)(packed >> 32));
   retro_atomic_store_release_int(&c->clock_sequence, (int)(sequence + 2));
}

static void pyrowave_core_audio_callback(void)
{
   pyrowave_core_client_t *c = pyrowave_core_audio_active;
   record_pyrowave_clock_position_t position;
   unsigned i;
   bool ended;
   if (!c || !retro_atomic_load_acquire_int(&c->ready) || retro_atomic_load_acquire_int(&c->stop)) return;
   for (i = 0; i < 4 && !c->audio_input_end; i++)
   {
      ssize_t consumed;
      if (!c->audio_held)
      {
         int result = record_pyrowave_pcm_take(c->pcm, &c->audio);
         if (result == RECORD_GPU_QUEUE_END) { c->audio_input_end = true; break; }
         if (result == RECORD_GPU_QUEUE_ERROR) record_pyrowave_audio_clock_fail(c->audio_clock);
         if (result != RECORD_GPU_QUEUE_OK) break;
         c->audio_held = true; c->audio_offset = 0;
      }
      consumed = audio_driver_pyrowave_submit(c->audio_clock,
            (const int16_t*)c->audio.data + c->audio_offset * c->setup.stream.audio_channels,
            c->audio.frames - c->audio_offset, c->setup.stream.audio_channels,
            c->audio.first_sample + c->audio_offset);
      if (consumed <= 0) break;
      c->audio_received = true;
      c->audio_offset += (size_t)consumed;
      if (c->audio_offset == c->audio.frames)
      {
         record_pyrowave_pcm_release(c->pcm, c->audio.slot); c->audio_held = false;
         record_pyrowave_dispatch_notify(c->io);
      }
   }
   memset(&position, 0, sizeof(position));
   audio_driver_pyrowave_poll(c->audio_clock, &position);
   ended = c->audio_input_end && (!record_pyrowave_audio_clock_has_output(c->audio_clock)
         || record_pyrowave_audio_clock_drained(c->audio_clock));
   if (record_pyrowave_audio_clock_failed(c->audio_clock))
   {
      retro_atomic_store_release_int(&c->audio_failed, 1);
      retro_atomic_store_release_int(&c->failed, 1);
   }
   else
   {
      pyrowave_core_audio_publish(c, &position, ended);
      if (ended) retro_atomic_store_release_int(&c->audio_end, 1);
      else if (!c->audio_held) audio_driver_pyrowave_silence(c->audio_clock);
   }
}

static void pyrowave_core_audio_state(bool enabled)
{
   /* The wrapper calls this while parked, before resuming or after stopping. */
   if (!enabled) libretro_pyrowave_audio_control(false);
}
static bool pyrowave_core_render(pyrowave_core_client_t *c);
/* A held clock is a proven presentation ceiling, not extrapolated audio. */
static bool pyrowave_core_due(const pyrowave_core_client_t *c, const record_gpu_frame_t *frame)
{
   return c->target_valid && frame->timestamp <= c->target_time;
}
static bool pyrowave_core_late(const pyrowave_core_client_t *c, const record_gpu_frame_t *frame)
{
   return c->audio_async && c->setup.stream.audio_rate && c->target_valid
      && !retro_atomic_load_acquire_int(&c->audio_end) && frame->timestamp < c->target_time
      && frame->duration <= c->target_time - frame->timestamp;
}
#ifdef PYROWAVE_CORE_VULKAN
static bool pyrowave_core_vulkan_prepare(pyrowave_core_client_t *c);
static bool pyrowave_core_vulkan_render(pyrowave_core_client_t *c,
      VkCommandBuffer commands, VkFence *completion, bool advance);
static bool pyrowave_core_vulkan_free(pyrowave_core_client_t *c);
#endif
#ifdef PYROWAVE_CORE_D3D12
static bool pyrowave_core_render_d3d12(pyrowave_core_client_t *c,
      ID3D12GraphicsCommandList *commands, ID3D12Fence *fence, uint64_t value);
#endif

static uintptr_t pyrowave_core_control_run(void *data, const video_poke_interface_t *poke, void *user)
{
   pyrowave_core_client_t *c = (pyrowave_core_client_t*)user;
   video_device_info_t device;
   uintptr_t result = 1;
   memset(&device, 0, sizeof(device));
   if (!poke || !poke->get_device || !poke->get_device(data, &device)
         || device.api != c->api || device.device != c->device) return 0;
   if (c->api == VIDEO_DEVICE_OPENGL)
   {
#if defined(HAVE_OPENGL_CORE) && !defined(HAVE_OPENGLES)
      if (device.bind_gl_context) device.bind_gl_context(data, true);
      if (!c->control_kind)
      {
         c->player = record_pyrowave_player_gl_new(&c->setup);
         if (c->player) record_pyrowave_player_gl_video(c->player, &c->input);
         else result = 0;
      }
      else if (c->control_kind == 1) pyrowave_core_render(c);
      else if (c->control_kind == 3)
      {
         if (record_pyrowave_player_gl_retire(c->player))
         {
            c->player = NULL; c->held = c->future = false;
            retro_atomic_store_release_int(&c->render_done, 1);
         }
      }
      else
      {
         if (!record_pyrowave_player_gl_free(c->player)) result = 0;
         else c->player = NULL;
      }
      if (device.bind_gl_context) device.bind_gl_context(data, false);
#else
      result = 0;
#endif
   }
#ifdef PYROWAVE_CORE_D3D12
   else if (c->api == VIDEO_DEVICE_D3D12)
   {
      if (!c->control_kind)
      {
         c->player_d3d12 = record_pyrowave_player_d3d12_new(&c->setup);
         if (!c->player_d3d12) result = 0;
         else if (!video_d3d12_pyrowave_attach(data, c->player_d3d12))
         {
            record_pyrowave_d3d12_free(c->player_d3d12);
            c->player_d3d12 = NULL; result = 0;
         }
         else record_pyrowave_d3d12_session_video(c->player_d3d12, &c->input);
      }
      else if (c->control_kind == 1) pyrowave_core_render_d3d12(c, NULL, NULL, 0);
      else if (c->control_kind == 3)
      {
         if (!c->d3d12_detached && video_d3d12_pyrowave_quiesce(data, c->player_d3d12) > 0)
         {
            video_d3d12_pyrowave_detach(data, c->player_d3d12);
            c->d3d12_detached = true;
         }
         if (c->d3d12_detached && record_pyrowave_d3d12_abandon(c->player_d3d12))
         {
            c->player_d3d12 = NULL; c->held = false;
            video_d3d12_pyrowave_resume(data);
            retro_atomic_store_release_int(&c->render_done, 1);
         }
      }
      else
      {
         /* GPU loans and snapshots have retired before this cold job. */
         if (!c->d3d12_detached)
         {
            video_d3d12_pyrowave_detach(data, c->player_d3d12);
            c->d3d12_detached = true;
         }
         if (!record_pyrowave_d3d12_free(c->player_d3d12)) result = 0;
         else
         {
            c->player_d3d12 = NULL;
            video_d3d12_pyrowave_resume(data);
         }
      }
   }
#endif
#ifdef PYROWAVE_CORE_VULKAN
   else if (c->api == VIDEO_DEVICE_VULKAN)
   {
      if (!c->control_kind)
      {
         if (!pyrowave_core_vulkan_prepare(c)) result = 0;
         else record_pyrowave_vulkan_session_video(c->player_vulkan, &c->input);
      }
      else if (c->control_kind == 1)
         pyrowave_core_vulkan_render(c, VK_NULL_HANDLE, NULL, false);
      else if (c->control_kind == 3)
      {
         int retired = video_vulkan_pyrowave_quiesce(data,
               c->vulkan_recovery_fence, &c->vulkan_recovery_submitted);
         if (retired > 0 && record_pyrowave_vulkan_abandon(c->player_vulkan))
         {
            c->player_vulkan = NULL; c->held = false;
            memset(c->vulkan_busy, 0, sizeof(c->vulkan_busy));
            if (pyrowave_core_vulkan_free(c))
            {
               video_vulkan_pyrowave_resume(data);
               retro_atomic_store_release_int(&c->render_done, 1);
            }
         }
      }
      else if (!pyrowave_core_vulkan_free(c)) result = 0;
   }
#endif
   else result = 0;
   return result;
}
static void pyrowave_core_control_done(void *user, uintptr_t result)
{
   pyrowave_core_client_t *c = (pyrowave_core_client_t*)user;
   c->control_pending = false;
   if (!c->control_kind && result) c->constructed = true;
   if (!result)
   {
      if (!c->control_kind) RARCH_ERR("[PyroWave] Receiver GPU setup failed.\n");
      retro_atomic_store_release_int(&c->failed, 1);
   }
}
static bool pyrowave_core_control_post(pyrowave_core_client_t *c, unsigned kind)
{
   video_driver_state_t *video = video_state_get_ptr();
   if (c->control_pending) return false;
   c->control_kind = kind; c->control_pending = true;
   c->control_job.run = pyrowave_core_control_run; c->control_job.user = c;
   c->control.kind = VIDEO_THREAD_ASYNC_JOB; c->control.img = &c->control_job;
   c->control.user = c; c->control.done = pyrowave_core_control_done;
   if (video->thread_wrapper_active)
   {
      if (video_thread_async_post(&c->control)) return true;
      c->control_pending = false; return false;
   }
   pyrowave_core_control_done(c, pyrowave_core_control_run(video->data, video->poke, c));
   return true;
}

static bool pyrowave_core_open(const struct retro_game_info *game)
{
   pyrowave_core_client_t *c;
   config_file_t *file;
   struct addrinfo hints, *address = NULL;
   char url[512], host[256], service[6];
   const char *start, *colon;
   size_t length;
   bool listen, ok = false;
   uint16_t endian = 1;
   if (!game || !game->path || pyrowave_core_client || recording_gpu_pending()
         || recording_state_get_ptr()->enable || *(const uint8_t*)&endian != 1) return false;
   c = (pyrowave_core_client_t*)calloc(1, sizeof(*c));
   if (!c) return false;
   c->audio_async = pyrowave_core_async_audio;
   retro_atomic_int_init(&c->audio_done, 1);
   retro_atomic_int_init(&c->audio_end, 0); retro_atomic_int_init(&c->audio_failed, 0);
   retro_atomic_int_init(&c->clock_sequence, 0); retro_atomic_int_init(&c->clock_low, 0);
   retro_atomic_int_init(&c->clock_high, 0);
   retro_atomic_int_init(&c->ready, 0); retro_atomic_int_init(&c->stop, 0);
   retro_atomic_int_init(&c->failed, 0); retro_atomic_int_init(&c->render_done, 0);
   retro_atomic_int_init(&c->io_done, 0); retro_atomic_int_init(&c->video_end, 0);
   retro_atomic_int_init(&c->recovery, 0);
   file = config_file_new_from_path_to_string(game->path);
   if (!file) goto end;
   ok = config_get_array(file, "pyrowave_url", url, sizeof(url))
      && config_get_array(file, "pyrowave_shader_directory", c->directory, sizeof(c->directory));
   config_file_free(file);
   if (!ok) goto end;
   ok = false;
   listen = !strncmp(url, "rpws-listen://", 14);
   if (!listen && strncmp(url, "rpws://", 7)) goto end;
   start = url + (listen ? 14 : 7); colon = strrchr(start, ':');
   if (!colon || !colon[1] || strlen(colon + 1) > 5) goto end;
   length = (size_t)(colon - start);
   if (*start == '[' && length > 2 && start[length - 1] == ']') { start++; length -= 2; }
   if (!length || length >= sizeof(host)) goto end;
   memcpy(host, start, length); host[length] = '\0'; strcpy(service, colon + 1);
   memset(&hints, 0, sizeof(hints)); hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
   if (!network_init() || getaddrinfo_retro(host, service, &hints, &address) || !address) goto end;
   c->io = record_pyrowave_dispatch_new(address->ai_addr, address->ai_addrlen,
         listen, NULL, 0, NULL, NULL);
   if (!c->io) goto end;
   pyrowave_core_client = c; ok = true;
   RARCH_LOG("[PyroWave] Receiver connecting.\n");
end:
   freeaddrinfo_retro(address);
   if (!ok) free(c);
   return ok;
}

void libretro_pyrowave_stop(void)
{
   pyrowave_core_client_t *c = pyrowave_core_client;
   if (!c || c->av_updating) return;
   retro_atomic_store_release_int(&c->stop, 1);
   record_pyrowave_dispatch_cancel(c->io);
}
bool libretro_pyrowave_pending(void)
{ return pyrowave_core_client && !pyrowave_core_client->av_updating; }

static bool pyrowave_core_setup(pyrowave_core_client_t *c)
{
   video_device_info_t device;
   rpyrowave_stream_config_t *cfg = &c->setup.stream;
   memset(&device, 0, sizeof(device));
   if (!record_pyrowave_dispatch_config(c->io, cfg, &c->generation)) return false;
   if (cfg->audio_channels > 2) return false;
   if (cfg->sequence.transfer_pq || cfg->sequence.primaries_bt2020)
      c->setup.texture_mode = RPYROWAVE_COLOR_SDR709;
   pyrowave_core_av.geometry.base_width = cfg->sequence.width;
   pyrowave_core_av.geometry.base_height = cfg->sequence.height;
   pyrowave_core_av.geometry.max_width = cfg->sequence.width;
   pyrowave_core_av.geometry.max_height = cfg->sequence.height;
   pyrowave_core_av.geometry.aspect_ratio = (float)cfg->sequence.width / cfg->sequence.height;
   pyrowave_core_av.timing.fps = (double)cfg->fps_numerator / cfg->fps_denominator;
   pyrowave_core_av.timing.sample_rate = cfg->audio_rate ? cfg->audio_rate : 48000;
   c->av_updating = true;
   if (!pyrowave_core_environment(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &pyrowave_core_av))
   { c->av_updating = false; return false; }
   c->av_updating = false;
   if (!video_driver_get_device(&device)) return false;
   if (device.api != VIDEO_DEVICE_OPENGL
#ifdef PYROWAVE_CORE_D3D12
         && device.api != VIDEO_DEVICE_D3D12
#endif
#ifdef PYROWAVE_CORE_VULKAN
         && device.api != VIDEO_DEVICE_VULKAN
#endif
         ) return false;
#if !defined(HAVE_OPENGL_CORE) || defined(HAVE_OPENGLES)
   if (device.api == VIDEO_DEVICE_OPENGL) return false;
#endif
   c->api = device.api; c->setup.device = device.device;
   c->setup.instance = device.instance; c->setup.physical = device.physical_device;
   c->setup.queue = device.queue; c->setup.family = device.queue_family;
   c->setup.instance_proc = (record_pyrowave_source_proc_t)device.get_instance_proc_addr;
   c->device = device.device; c->setup.gl_proc = device.get_gl_proc_address;
   c->setup.directory = c->directory; c->setup.slots = 3;
   if (cfg->audio_rate)
   {
      c->pcm = record_pyrowave_pcm_new(cfg->audio_channels, cfg->maximum_audio_frames, 16);
      if (!c->pcm) return false;
      if (c->audio_async)
      {
         c->audio_clock = record_pyrowave_audio_clock_new(c->generation);
         if (!c->audio_clock) return false;
      }
      else if (cfg->audio_channels == 1)
      {
         c->mono = (int16_t*)calloc((size_t)cfg->maximum_audio_frames * 2, sizeof(int16_t));
         if (!c->mono) return false;
      }
   }
   else retro_atomic_store_release_int(&c->audio_end, 1);
   c->configured = true;
   return pyrowave_core_control_post(c, 0);
}

static bool pyrowave_core_render(pyrowave_core_client_t *c)
{
#if defined(HAVE_OPENGL_CORE) && !defined(HAVE_OPENGLES)
   record_pyrowave_gl_image_t next;
   record_gpu_frame_t dropped;
   int result = RECORD_GPU_QUEUE_BUSY;
   unsigned i;
   if (!c->player || retro_atomic_load_acquire_int(&c->render_done)) return false;
   if (retro_atomic_load_acquire_int(&c->recovery)) return false;
   if (!record_pyrowave_player_gl_poll(c->player))
   {
      retro_atomic_store_release_int(&c->failed, 1);
      retro_atomic_store_release_int(&c->recovery, 1);
      return false;
   }
   if (retro_atomic_load_acquire_int(&c->stop))
   {
      if (c->future)
      {
         if (!record_pyrowave_player_gl_drop(c->player, c->gl_future.frame.slot)) return false;
         c->future = false;
      }
      if (c->held)
      {
         if (!record_pyrowave_player_gl_present(c->player, c->image.frame.slot))
         {
            retro_atomic_store_release_int(&c->failed, 1);
            retro_atomic_store_release_int(&c->recovery, 1);
            return false;
         }
         c->held = false;
      }
      if (retro_atomic_load_acquire_int(&c->io_done))
      {
         if (!c->input_closed)
         {
            if (!c->input.finish(c->input.user)) return false;
            c->input_closed = true;
         }
         for (i = 0; i < c->setup.slots; i++)
            if (record_pyrowave_player_gl_discard(c->player, &dropped) != RECORD_GPU_QUEUE_OK) break;
         for (i = 0; i < c->setup.slots; i++)
         {
            if (record_pyrowave_player_gl_image(c->player, &next) != RECORD_GPU_QUEUE_OK) break;
            record_pyrowave_player_gl_drop(c->player, next.frame.slot);
         }
         if (record_pyrowave_player_gl_idle(c->player))
            retro_atomic_store_release_int(&c->render_done, 1);
      }
      return false;
   }
   for (i = 0; i < c->setup.slots; i++)
   {
      if (record_pyrowave_player_gl_peek(c->player, &dropped) != RECORD_GPU_QUEUE_OK
            || !pyrowave_core_late(c, &dropped)) break;
      if (record_pyrowave_player_gl_discard(c->player, &dropped) != RECORD_GPU_QUEUE_OK)
      { retro_atomic_store_release_int(&c->failed, 1); break; }
   }
   if (!c->video_input_end) result = record_pyrowave_player_gl_decode(c->player);
   if (result == RECORD_GPU_QUEUE_OK) c->decoding++;
   else if (result == RECORD_GPU_QUEUE_ERROR)
   {
      retro_atomic_store_release_int(&c->failed, 1);
      retro_atomic_store_release_int(&c->recovery, 1);
   }
   if (!c->future && record_pyrowave_player_gl_image(c->player, &next) == RECORD_GPU_QUEUE_OK)
   {
      c->gl_future = next; c->future = true;
      if (c->decoding) c->decoding--;
   }
   if (c->future && pyrowave_core_due(c, &c->gl_future.frame))
   {
      if (c->held && !record_pyrowave_player_gl_present(c->player, c->image.frame.slot))
      {
         retro_atomic_store_release_int(&c->failed, 1);
         retro_atomic_store_release_int(&c->recovery, 1);
      }
      else
      { c->image = c->gl_future; c->held = true; c->future = false; }
   }
   if (result == RECORD_GPU_QUEUE_END) c->video_input_end = true;
   if (c->video_input_end && !c->decoding && !c->future)
      retro_atomic_store_release_int(&c->video_end, 1);
   record_pyrowave_dispatch_notify(c->io);
   return c->held;
#else
   (void)c; return false;
#endif
}

unsigned libretro_pyrowave_gl_frame(void *data, unsigned *dims, bool advance,
      const struct video_frame_info *timing)
{
   pyrowave_core_client_t *c = (pyrowave_core_client_t*)data;
   if (!c || c->api != VIDEO_DEVICE_OPENGL || !retro_atomic_load_acquire_int(&c->ready)
         || retro_atomic_load_acquire_int(&c->render_done)) return 0;
   c->target_valid = timing && timing->recording_pyrowave_clock_valid;
   c->target_time = timing ? timing->recording_gpu_timestamp : 0;
   if (advance || retro_atomic_load_acquire_int(&c->stop))
   { if (!pyrowave_core_render(c)) return 0; }
   else if (!c->held) return 0;
   *dims = VIDEO_SCALE_PACK(c->setup.stream.sequence.width, c->setup.stream.sequence.height);
   return c->image.texture;
}
#ifdef PYROWAVE_CORE_D3D12
static bool pyrowave_core_render_d3d12(pyrowave_core_client_t *c,
      ID3D12GraphicsCommandList *commands, ID3D12Fence *fence, uint64_t value)
{
   record_pyrowave_d3d12_t *player = c->player_d3d12;
   record_pyrowave_d3d12_image_t next;
   record_gpu_frame_t dropped;
   int result = RECORD_GPU_QUEUE_BUSY;
   unsigned i;
   if (!player || retro_atomic_load_acquire_int(&c->render_done)
         || retro_atomic_load_acquire_int(&c->recovery)) return false;
   if (!record_pyrowave_d3d12_poll(player))
   {
      retro_atomic_store_release_int(&c->recovery, 1);
      retro_atomic_store_release_int(&c->failed, 1);
      return false;
   }
   if (retro_atomic_load_acquire_int(&c->stop))
   {
      if (c->future)
      {
         if (!record_pyrowave_d3d12_image_drop(player, c->d3d12_future.frame.slot)) return false;
         c->future = false;
      }
      if (c->held)
      {
         if (!record_pyrowave_d3d12_present(player, c->d3d12_image.frame.slot,
                  c->display_fence, c->display_value)) return false;
         c->held = false;
      }
      if (retro_atomic_load_acquire_int(&c->io_done))
      {
         if (!c->input_closed)
         {
            if (!c->input.finish(c->input.user)) return false;
            c->input_closed = true;
         }
         for (i = 0; i < c->setup.slots; i++)
            if (record_pyrowave_d3d12_receive_drop(player, &dropped) != RECORD_GPU_QUEUE_OK) break;
         for (i = 0; i < c->setup.slots; i++)
         {
            if (record_pyrowave_d3d12_image(player, &next) != RECORD_GPU_QUEUE_OK) break;
            record_pyrowave_d3d12_image_drop(player, next.frame.slot);
         }
         if (record_pyrowave_d3d12_idle(player))
            retro_atomic_store_release_int(&c->render_done, 1);
      }
      return false;
   }
   if (commands)
   {
      for (i = 0; i < c->setup.slots; i++)
      {
         if (record_pyrowave_d3d12_receive_peek(player, &dropped) != RECORD_GPU_QUEUE_OK
               || !pyrowave_core_late(c, &dropped)) break;
         if (record_pyrowave_d3d12_receive_drop(player, &dropped) != RECORD_GPU_QUEUE_OK)
         { retro_atomic_store_release_int(&c->failed, 1); break; }
      }
      if (!c->video_input_end) result = record_pyrowave_d3d12_decode(player, commands, fence, value);
      if (result == RECORD_GPU_QUEUE_OK) c->decoding++;
      else if (result == RECORD_GPU_QUEUE_ERROR) retro_atomic_store_release_int(&c->failed, 1);
      if (!c->future && record_pyrowave_d3d12_image(player, &next) == RECORD_GPU_QUEUE_OK)
      {
         c->d3d12_future = next; c->future = true;
         if (c->decoding) c->decoding--;
      }
      if (c->future && pyrowave_core_due(c, &c->d3d12_future.frame))
      {
         if (c->held && !record_pyrowave_d3d12_present(player, c->d3d12_image.frame.slot,
                  c->display_fence, c->display_value))
         {
            retro_atomic_store_release_int(&c->recovery, 1);
            retro_atomic_store_release_int(&c->failed, 1);
         }
         else
         { c->d3d12_image = c->d3d12_future; c->held = true; c->future = false; }
      }
      if (result == RECORD_GPU_QUEUE_END) c->video_input_end = true;
      if (c->video_input_end && !c->decoding && !c->future)
         retro_atomic_store_release_int(&c->video_end, 1);
      record_pyrowave_dispatch_notify(c->io);
   }
   return c->held;
}
#endif

void libretro_pyrowave_d3d12_failed(void *context)
{
#ifdef PYROWAVE_CORE_D3D12
   pyrowave_core_client_t *c = (pyrowave_core_client_t*)context;
   if (!c || c->api != VIDEO_DEVICE_D3D12 || !c->player_d3d12) return;
   retro_atomic_store_release_int(&c->recovery, 1);
   retro_atomic_store_release_int(&c->failed, 1);
#else
   (void)context;
#endif
}

void *libretro_pyrowave_d3d12_frame(void *context, void *commands, void *fence,
      uint64_t value, unsigned *dims, bool advance, const struct video_frame_info *timing)
{
#ifdef PYROWAVE_CORE_D3D12
   pyrowave_core_client_t *c = (pyrowave_core_client_t*)context;
   if (!c || c->api != VIDEO_DEVICE_D3D12 || !retro_atomic_load_acquire_int(&c->ready)
         || retro_atomic_load_acquire_int(&c->render_done)) return NULL;
   c->target_valid = timing && timing->recording_pyrowave_clock_valid;
   c->target_time = timing ? timing->recording_gpu_timestamp : 0;
   if (advance || retro_atomic_load_acquire_int(&c->stop))
   {
      if (!pyrowave_core_render_d3d12(c, commands, fence, value)) return NULL;
   }
   else if (!c->held) return NULL;
   /* The renderer submits every returned image use with this durable
    * timeline label, restoring NON_PIXEL_SHADER_RESOURCE before signalling. */
   c->display_fence = (ID3D12Fence*)fence; c->display_value = value;
   *dims = VIDEO_SCALE_PACK(c->setup.stream.sequence.width, c->setup.stream.sequence.height);
   return c->d3d12_image.resource;
#else
   (void)context; (void)commands; (void)fence; (void)value;
   (void)dims; (void)advance; (void)timing; return NULL;
#endif
}

#ifdef PYROWAVE_CORE_VULKAN
static bool pyrowave_core_vulkan_free(pyrowave_core_client_t *c)
{
   unsigned i;
   for (i = 0; i < PYROWAVE_CORE_VK_FENCES; i++)
      if (c->vulkan_busy[i]) return false;
   if (!record_pyrowave_vulkan_free(c->player_vulkan)) return false;
   c->player_vulkan = NULL;
   for (i = 0; i < PYROWAVE_CORE_VK_FENCES; i++)
      if (c->vulkan_fences[i])
      {
         c->vulkan_destroy((VkDevice)c->device, c->vulkan_fences[i], NULL);
         c->vulkan_fences[i] = VK_NULL_HANDLE;
      }
   if (c->vulkan_recovery_fence)
   {
      c->vulkan_destroy((VkDevice)c->device, c->vulkan_recovery_fence, NULL);
      c->vulkan_recovery_fence = VK_NULL_HANDLE;
   }
   return true;
}

static bool pyrowave_core_vulkan_prepare(pyrowave_core_client_t *c)
{
   PFN_vkGetInstanceProcAddr instance = (PFN_vkGetInstanceProcAddr)c->setup.instance_proc;
   PFN_vkGetDeviceProcAddr device;
   VkFenceCreateInfo info;
   unsigned i;
   if (!instance) return false;
   device = (PFN_vkGetDeviceProcAddr)instance((VkInstance)c->setup.instance, "vkGetDeviceProcAddr");
   if (!device) return false;
   c->vulkan_create = (PFN_vkCreateFence)device((VkDevice)c->device, "vkCreateFence");
   c->vulkan_destroy = (PFN_vkDestroyFence)device((VkDevice)c->device, "vkDestroyFence");
   c->vulkan_status = (PFN_vkGetFenceStatus)device((VkDevice)c->device, "vkGetFenceStatus");
   c->vulkan_reset = (PFN_vkResetFences)device((VkDevice)c->device, "vkResetFences");
   if (!c->vulkan_create || !c->vulkan_destroy || !c->vulkan_status || !c->vulkan_reset) return false;
   c->vulkan_pending = c->vulkan_display = c->vulkan_previous = PYROWAVE_CORE_VK_FENCES;
   c->player_vulkan = record_pyrowave_player_vulkan_new(&c->setup);
   if (!c->player_vulkan) return false;
   memset(&info, 0, sizeof(info)); info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   for (i = 0; i < PYROWAVE_CORE_VK_FENCES; i++)
      if (c->vulkan_create((VkDevice)c->device, &info, NULL, &c->vulkan_fences[i]) != VK_SUCCESS)
      {
         c->vulkan_fences[i] = VK_NULL_HANDLE;
         break;
      }
   if (i == PYROWAVE_CORE_VK_FENCES)
   {
      if (c->vulkan_create((VkDevice)c->device, &info, NULL, &c->vulkan_recovery_fence) == VK_SUCCESS)
         return true;
      c->vulkan_recovery_fence = VK_NULL_HANDLE;
   }
   pyrowave_core_vulkan_free(c);
   return false;
}

static void pyrowave_core_vulkan_poll(pyrowave_core_client_t *c)
{
   unsigned i;
   if (!record_pyrowave_vulkan_poll(c->player_vulkan))
   {
      retro_atomic_store_release_int(&c->recovery, 1);
      retro_atomic_store_release_int(&c->failed, 1);
   }
   for (i = 0; i < PYROWAVE_CORE_VK_FENCES; i++)
   {
      VkResult status;
      if (!c->vulkan_busy[i] || i == c->vulkan_pending || i == c->vulkan_display
            || record_pyrowave_vulkan_fence_borrowed(c->player_vulkan, c->vulkan_fences[i])) continue;
      /* Native acknowledgement precedes reset. A later status query alone
       * cannot erase a signal the codec has not yet consumed. */
      status = c->vulkan_status((VkDevice)c->device, c->vulkan_fences[i]);
      if (status == VK_NOT_READY) continue;
      if (status != VK_SUCCESS || c->vulkan_reset((VkDevice)c->device, 1, &c->vulkan_fences[i]) != VK_SUCCESS)
      {
         retro_atomic_store_release_int(&c->recovery, 1);
         retro_atomic_store_release_int(&c->failed, 1);
         continue;
      }
      c->vulkan_busy[i] = false;
   }
}

static bool pyrowave_core_vulkan_render(pyrowave_core_client_t *c,
      VkCommandBuffer commands, VkFence *completion, bool advance)
{
   record_pyrowave_vulkan_image_t next;
   record_gpu_frame_t dropped;
   unsigned i;
   int result = RECORD_GPU_QUEUE_BUSY;
   if (completion) *completion = VK_NULL_HANDLE;
   if (!c->player_vulkan || retro_atomic_load_acquire_int(&c->render_done)
         || retro_atomic_load_acquire_int(&c->recovery)) return false;
   pyrowave_core_vulkan_poll(c);
   if (retro_atomic_load_acquire_int(&c->recovery)) return false;
   if (retro_atomic_load_acquire_int(&c->stop))
   {
      /* An unacknowledged submission retains both its new display pin and
       * the previous valid pin for explicit cold failure recovery. */
      if (c->vulkan_pending != PYROWAVE_CORE_VK_FENCES) return false;
      if (c->future)
      {
         if (!record_pyrowave_vulkan_image_drop(c->player_vulkan, c->vulkan_future.frame.slot)) return false;
         c->future = false;
      }
      if (c->held)
      {
         if (c->vulkan_display == PYROWAVE_CORE_VK_FENCES)
         {
            if (!record_pyrowave_vulkan_image_drop(c->player_vulkan, c->vulkan_image.frame.slot)) return false;
         }
         else if (!record_pyrowave_vulkan_present(c->player_vulkan, c->vulkan_image.frame.slot,
                  c->vulkan_fences[c->vulkan_display])) return false;
         c->held = false; c->vulkan_display = PYROWAVE_CORE_VK_FENCES;
      }
      if (retro_atomic_load_acquire_int(&c->io_done))
      {
         if (!c->input_closed)
         {
            if (!c->input.finish(c->input.user)) return false;
            c->input_closed = true;
         }
         for (i = 0; i < c->setup.slots; i++)
            if (record_pyrowave_vulkan_receive_drop(c->player_vulkan, &dropped) != RECORD_GPU_QUEUE_OK) break;
         for (i = 0; i < c->setup.slots; i++)
         {
            if (record_pyrowave_vulkan_image(c->player_vulkan, &next) != RECORD_GPU_QUEUE_OK) break;
            record_pyrowave_vulkan_image_drop(c->player_vulkan, next.frame.slot);
         }
         pyrowave_core_vulkan_poll(c);
         for (i = 0; i < PYROWAVE_CORE_VK_FENCES; i++) if (c->vulkan_busy[i]) break;
         if (i == PYROWAVE_CORE_VK_FENCES && record_pyrowave_vulkan_idle(c->player_vulkan))
            retro_atomic_store_release_int(&c->render_done, 1);
      }
      return false;
   }
   if (!commands || !completion) return false;
   if (c->vulkan_pending != PYROWAVE_CORE_VK_FENCES)
   {
      retro_atomic_store_release_int(&c->failed, 1);
      return false;
   }
   for (i = 0; i < PYROWAVE_CORE_VK_FENCES; i++) if (!c->vulkan_busy[i]) break;
   if (i == PYROWAVE_CORE_VK_FENCES)
   {
      retro_atomic_store_release_int(&c->failed, 1);
      return false;
   }
   c->vulkan_pending = i; c->vulkan_busy[i] = true;
   *completion = c->vulkan_fences[i];
   if (advance)
   {
      for (i = 0; i < c->setup.slots; i++)
      {
         if (record_pyrowave_vulkan_receive_peek(c->player_vulkan, &dropped) != RECORD_GPU_QUEUE_OK
               || !pyrowave_core_late(c, &dropped)) break;
         if (record_pyrowave_vulkan_receive_drop(c->player_vulkan, &dropped) != RECORD_GPU_QUEUE_OK)
         { retro_atomic_store_release_int(&c->failed, 1); break; }
      }
      if (!c->video_input_end) result = record_pyrowave_vulkan_decode(c->player_vulkan, commands, *completion);
      if (result == RECORD_GPU_QUEUE_OK) c->decoding++;
      else if (result == RECORD_GPU_QUEUE_ERROR) retro_atomic_store_release_int(&c->failed, 1);
      if (!c->future && record_pyrowave_vulkan_image(c->player_vulkan, &next) == RECORD_GPU_QUEUE_OK)
      {
         c->vulkan_future = next; c->future = true;
         if (c->decoding) c->decoding--;
      }
      if (c->future && pyrowave_core_due(c, &c->vulkan_future.frame))
      {
         bool returned = true;
         if (c->held)
            returned = c->vulkan_display != PYROWAVE_CORE_VK_FENCES
               && record_pyrowave_vulkan_present(c->player_vulkan, c->vulkan_image.frame.slot,
                     c->vulkan_fences[c->vulkan_display]);
         if (!returned)
         {
            record_pyrowave_vulkan_image_drop(c->player_vulkan, c->vulkan_future.frame.slot);
            c->future = false;
            retro_atomic_store_release_int(&c->recovery, 1);
            retro_atomic_store_release_int(&c->failed, 1);
         }
         else
         {
            c->vulkan_image = c->vulkan_future; c->held = true; c->future = false;
            c->vulkan_display = PYROWAVE_CORE_VK_FENCES;
         }
      }
      if (result == RECORD_GPU_QUEUE_END) c->video_input_end = true;
      if (c->video_input_end && !c->decoding && !c->future)
         retro_atomic_store_release_int(&c->video_end, 1);
      record_pyrowave_dispatch_notify(c->io);
   }
   c->vulkan_previous = c->vulkan_display;
   c->vulkan_drew = c->held;
   return c->held;
}

bool libretro_pyrowave_vulkan_frame(void *context, VkCommandBuffer commands,
      struct record_pyrowave_vulkan_image *image, VkFence *completion, unsigned *dims, bool advance,
      const struct video_frame_info *timing)
{
   pyrowave_core_client_t *c = (pyrowave_core_client_t*)context;
   if (completion) *completion = VK_NULL_HANDLE;
   if (c)
   {
      c->target_valid = timing && timing->recording_pyrowave_clock_valid;
      c->target_time = timing ? timing->recording_gpu_timestamp : 0;
   }
   if (!c || c->api != VIDEO_DEVICE_VULKAN || !retro_atomic_load_acquire_int(&c->ready)
         || !image || !completion || !dims || !pyrowave_core_vulkan_render(c, commands, completion, advance)) return false;
   *image = c->vulkan_image;
   *dims = VIDEO_SCALE_PACK(c->setup.stream.sequence.width, c->setup.stream.sequence.height);
   return true;
}

void libretro_pyrowave_vulkan_submitted(void *context, VkFence completion,
      VkResult result, bool discarded)
{
   pyrowave_core_client_t *c = (pyrowave_core_client_t*)context;
   unsigned pending;
   if (!c || c->api != VIDEO_DEVICE_VULKAN || !c->player_vulkan) return;
   pending = c->vulkan_pending;
   if (result == VK_SUCCESS && !discarded)
   {
      if (!completion) return;
      if (pending == PYROWAVE_CORE_VK_FENCES || c->vulkan_fences[pending] != completion)
      {
         retro_atomic_store_release_int(&c->failed, 1);
         return;
      }
      if (c->vulkan_drew) c->vulkan_display = pending;
      c->vulkan_pending = PYROWAVE_CORE_VK_FENCES;
      return;
   }
   retro_atomic_store_release_int(&c->failed, 1);
   if (!discarded || !completion || pending == PYROWAVE_CORE_VK_FENCES
         || c->vulkan_fences[pending] != completion)
   {
      retro_atomic_store_release_int(&c->recovery, 1);
      return;
   }
   record_pyrowave_vulkan_discard(c->player_vulkan, completion);
   if (record_pyrowave_vulkan_fence_borrowed(c->player_vulkan, completion)) return;
   if (c->vulkan_drew && c->vulkan_previous == PYROWAVE_CORE_VK_FENCES)
   {
      record_pyrowave_vulkan_image_drop(c->player_vulkan, c->vulkan_image.frame.slot);
      c->held = false;
   }
   c->vulkan_display = c->vulkan_previous;
   c->vulkan_busy[pending] = false;
   c->vulkan_pending = PYROWAVE_CORE_VK_FENCES;
}
#endif

bool libretro_pyrowave_frame_info(struct video_frame_info *info, bool advance)
{
   pyrowave_core_client_t *c = pyrowave_core_client;
   if (!c || !retro_atomic_load_acquire_int(&c->ready)
         || retro_atomic_load_acquire_int(&c->render_done)) return false;
   if (retro_atomic_load_acquire_int(&c->recovery)) return false;
   info->recording_pyrowave = info->recording_pyrowave_receive = true;
   info->recording_gpu = c;
   info->recording_gpu_capture = advance && !video_state_get_ptr()->frame_repeat;
   info->recording_gpu_pending = retro_atomic_load_acquire_int(&c->stop) != 0;
   info->recording_pyrowave_clock_valid = true;
   info->recording_gpu_timestamp = (uint64_t)-1;
   if (c->audio_async && c->setup.stream.audio_rate)
   {
      unsigned sequence = (unsigned)retro_atomic_load_acquire_int(&c->clock_sequence);
      uint64_t packed = (uint32_t)retro_atomic_load_relaxed_int(&c->clock_low);
      packed |= (uint64_t)(uint32_t)retro_atomic_load_relaxed_int(&c->clock_high) << 32;
      retro_atomic_thread_fence_acquire();
      info->recording_pyrowave_clock_valid = !(sequence & 1)
         && sequence == (unsigned)retro_atomic_load_relaxed_int(&c->clock_sequence)
         && (packed & 3) != 0
         && rpyrowave_stream_time(packed >> 2, c->setup.stream.audio_rate, 1,
               &info->recording_gpu_timestamp) == RPYROWAVE_OK;
      if (advance && info->recording_pyrowave_clock_valid && (packed & 3) == 2)
      {
         /* Once real audio ended, pace the video tail with the existing
          * frame clock. The metadata-only build_info call has no frame
          * time yet. Never extend an audio position through silence. */
         if (!c->tail)
         { c->tail = true; c->tail_anchor = info->frame_time; c->tail_time = info->recording_gpu_timestamp; }
         if (info->frame_time >= c->tail_anchor
               && (uint64_t)(info->frame_time - c->tail_anchor) <= (uint64_t)-1 - c->tail_time)
            info->recording_gpu_timestamp = c->tail_time + (uint64_t)(info->frame_time - c->tail_anchor);
      }
   }
   return true;
}

void libretro_pyrowave_poll(void)
{
   pyrowave_core_client_t *c = pyrowave_core_client;
   unsigned state;
   if (!c || c->av_updating) return;
   if (c->audio_async)
   {
      audio_driver_state_t *audio = audio_state_get_ptr();
      if (!audio->context_audio_data) libretro_pyrowave_audio_control(true);
      else if (audio->current_audio && !strcmp(audio->current_audio->ident, "audio-thread")
            && audio_thread_parked(audio->context_audio_data)) libretro_pyrowave_audio_control(false);
   }
   video_thread_async_poll();
   state = record_pyrowave_dispatch_state(c->io, NULL);
   if (!c->configured && !c->control_pending && state == RECORD_PYROWAVE_DISPATCH_CONFIGURING
         && !retro_atomic_load_acquire_int(&c->stop))
   {
      if (!pyrowave_core_setup(c))
      {
         RARCH_ERR("[PyroWave] Receiver setup failed.\n");
         retro_atomic_store_release_int(&c->failed, 1);
      }
   }
   if (c->constructed && !c->accepted && !c->control_pending
         && !retro_atomic_load_acquire_int(&c->stop) && !retro_atomic_load_acquire_int(&c->failed))
   {
      if (!record_pyrowave_dispatch_accept(c->io, &c->input, c->pcm))
         retro_atomic_store_release_int(&c->failed, 1);
      else
      {
         c->accepted = true; retro_atomic_store_release_int(&c->ready, 1);
         RARCH_LOG("[PyroWave] Receiver ready.\n");
      }
   }
   if (c->accepted && c->audio_async && c->pcm && !c->audio_bound
         && !retro_atomic_load_acquire_int(&c->stop) && !retro_atomic_load_acquire_int(&c->failed))
   {
      retro_atomic_store_release_int(&c->audio_done, 0);
      if (pyrowave_core_audio_request(c, &c->audio_request)) c->audio_bound = true;
      else retro_atomic_store_release_int(&c->audio_done, 1);
   }
   if (!c->audio_reported && retro_atomic_load_acquire_int(&c->audio_failed))
   { c->audio_reported = true; RARCH_ERR("[PyroWave] Audio presentation failed or is unsupported by this output.\n"); }
   if (record_pyrowave_dispatch_quiesced(c->io)) retro_atomic_store_release_int(&c->io_done, 1);
   if (state == RECORD_PYROWAVE_DISPATCH_FAILED && !retro_atomic_load_acquire_int(&c->stop))
      retro_atomic_store_release_int(&c->failed, 1);
   if (retro_atomic_load_acquire_int(&c->failed) || state >= RECORD_PYROWAVE_DISPATCH_CANCELLED
         || (retro_atomic_load_acquire_int(&c->io_done) && retro_atomic_load_acquire_int(&c->audio_end)
            && retro_atomic_load_acquire_int(&c->video_end))) libretro_pyrowave_stop();
   if (!retro_atomic_load_acquire_int(&c->stop)) return;
   if (c->audio_bound)
   {
      if (!c->audio_unbinding)
      {
         if (!pyrowave_core_audio_request(NULL, &c->audio_request)) return;
         c->audio_unbinding = true;
      }
      if ((unsigned)retro_atomic_load_acquire_int(&pyrowave_core_audio_ack) != c->audio_request
            || !retro_atomic_load_acquire_int(&c->audio_done)) return;
      c->audio_bound = false;
   }
   if (retro_atomic_load_acquire_int(&c->io_done))
   {
      record_pyrowave_pcm_view_t audio;
      unsigned i;
      if (c->audio_held)
      { record_pyrowave_pcm_release(c->pcm, c->audio.slot); c->audio_held = false; }
      for (i = 0; c->pcm && i < 16; i++)
      {
         if (record_pyrowave_pcm_take(c->pcm, &audio) != RECORD_GPU_QUEUE_OK) break;
         record_pyrowave_pcm_release(c->pcm, audio.slot);
      }
      if (c->pcm) record_pyrowave_pcm_finish(c->pcm);
      if (!c->constructed && !c->control_pending) retro_atomic_store_release_int(&c->render_done, 1);
   }
   if (!c->control_pending && c->constructed && !retro_atomic_load_acquire_int(&c->render_done)
         && (!video_driver_thread_wrapper_active()
            || !video_thread_record_gpu_in_flight(video_state_get_ptr()->data, c)))
   {
      if (retro_atomic_load_acquire_int(&c->recovery))
      {
         if (retro_atomic_load_acquire_int(&c->io_done) && record_pyrowave_dispatch_free(c->io))
         {
            c->io = NULL;
            pyrowave_core_control_post(c, 3);
         }
      }
      else
         pyrowave_core_control_post(c, 1);
   }
   if (c->control_pending || !retro_atomic_load_acquire_int(&c->render_done)
         || (video_driver_thread_wrapper_active()
            && video_thread_record_gpu_in_flight(video_state_get_ptr()->data, c))) return;
   if (!record_pyrowave_dispatch_free(c->io)) return;
   c->io = NULL;
   if (!record_pyrowave_pcm_free(c->pcm)) return;
   c->pcm = NULL;
   record_pyrowave_audio_clock_free(c->audio_clock); c->audio_clock = NULL;
   if (c->player || c->player_d3d12
#ifdef PYROWAVE_CORE_VULKAN
         || c->player_vulkan
#endif
         ) { pyrowave_core_control_post(c, 2); return; }
   {
      bool ended = retro_atomic_load_acquire_int(&c->video_end) != 0;
      bool failed = retro_atomic_load_acquire_int(&c->failed) != 0;
      if (c->audio_received)
         RARCH_LOG("[PyroWave] Device-clock audio %s.\n",
               !failed && retro_atomic_load_acquire_int(&c->audio_end) ? "drained" : "cancelled");
      free(c->mono); free(c); pyrowave_core_client = NULL;
      RARCH_LOG("[PyroWave] Receiver resources retired.\n");
      if (pyrowave_core_loaded && pyrowave_core_restart)
      {
         struct retro_game_info game;
         memset(&game, 0, sizeof(game)); game.path = pyrowave_core_path;
         pyrowave_core_restart = false;
         if (!pyrowave_core_open(&game)) pyrowave_core_environment(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
      }
      else if (pyrowave_core_loaded && (ended || failed))
         pyrowave_core_environment(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
   }
}

static bool pyrowave_core_audio_block(void)
{
   pyrowave_core_client_t *c = pyrowave_core_client;
   size_t frames, consumed, i;
   const int16_t *samples;
   int result;
   if (!c || !c->accepted || !c->pcm || !pyrowave_core_audio
         || retro_atomic_load_acquire_int(&c->stop)) return false;
   if (!c->audio_held)
   {
      result = record_pyrowave_pcm_take(c->pcm, &c->audio);
      if (result == RECORD_GPU_QUEUE_END) { retro_atomic_store_release_int(&c->audio_end, 1); return false; }
      if (result == RECORD_GPU_QUEUE_ERROR) retro_atomic_store_release_int(&c->failed, 1);
      if (result != RECORD_GPU_QUEUE_OK) return false;
      c->audio_held = true; c->audio_offset = 0;
   }
   frames = c->audio.frames - c->audio_offset;
   samples = (const int16_t*)c->audio.data + c->audio_offset * c->setup.stream.audio_channels;
   if (c->mono)
   {
      for (i = 0; i < frames; i++) c->mono[2 * i] = c->mono[2 * i + 1] = samples[i];
      samples = c->mono;
   }
   consumed = pyrowave_core_audio(samples, frames);
   if (consumed > frames) { retro_atomic_store_release_int(&c->failed, 1); return false; }
   c->audio_offset += consumed;
   if (c->audio_offset == c->audio.frames)
   {
      record_pyrowave_pcm_release(c->pcm, c->audio.slot); c->audio_held = false;
      record_pyrowave_dispatch_notify(c->io);
      return true;
   }
   return false;
}
static void pyrowave_core_audio_step(void)
{
   unsigned i;
   if (pyrowave_core_client && pyrowave_core_client->audio_async) return;
   for (i = 0; i < 16; i++) if (!pyrowave_core_audio_block()) break;
}
#else
void libretro_pyrowave_poll(void) { }
void libretro_pyrowave_audio_control(bool exiting) { (void)exiting; }
void libretro_pyrowave_stop(void) { }
void libretro_pyrowave_d3d12_failed(void *context) { (void)context; }
bool libretro_pyrowave_pending(void) { return false; }
bool libretro_pyrowave_frame_info(struct video_frame_info *info, bool advance)
{ (void)info; (void)advance; return false; }
unsigned libretro_pyrowave_gl_frame(void *data, unsigned *dims, bool advance,
      const struct video_frame_info *timing)
{ (void)data; (void)dims; (void)advance; (void)timing; return 0; }
void *libretro_pyrowave_d3d12_frame(void *context, void *commands, void *fence,
      uint64_t value, unsigned *dims, bool advance, const struct video_frame_info *timing)
{
   (void)context; (void)commands; (void)fence; (void)value;
   (void)dims; (void)advance; (void)timing; return NULL;
}
#ifdef HAVE_VULKAN
bool libretro_pyrowave_vulkan_frame(void *context, VkCommandBuffer commands,
      struct record_pyrowave_vulkan_image *image, VkFence *completion, unsigned *dims, bool advance,
      const struct video_frame_info *timing)
{
   (void)context; (void)commands; (void)image; (void)dims; (void)advance; (void)timing;
   if (completion) *completion = VK_NULL_HANDLE;
   return false;
}
void libretro_pyrowave_vulkan_submitted(void *context, VkFence completion, VkResult result, bool discarded)
{
   (void)context; (void)completion; (void)result; (void)discarded;
}
#endif

#endif

void libretro_pyrowave_retro_init(void)
{
   memset(&pyrowave_core_av, 0, sizeof(pyrowave_core_av));
   pyrowave_core_av.geometry.base_width = pyrowave_core_av.geometry.max_width = 320;
   pyrowave_core_av.geometry.base_height = pyrowave_core_av.geometry.max_height = 240;
   pyrowave_core_av.geometry.aspect_ratio = 4.0f / 3.0f;
   pyrowave_core_av.timing.fps = 60; pyrowave_core_av.timing.sample_rate = 48000;
}
void libretro_pyrowave_retro_deinit(void)
{
#ifdef PYROWAVE_CORE_GPU
   pyrowave_core_loaded = pyrowave_core_restart = false;
   free(pyrowave_core_path); pyrowave_core_path = NULL;
#endif
   libretro_pyrowave_stop();
   audio_driver_request_presentation(false);
}
unsigned libretro_pyrowave_retro_api_version(void) { return RETRO_API_VERSION; }
void libretro_pyrowave_retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info)); info->library_name = "PyroWave"; info->library_version = "1";
   info->valid_extensions = "rpws"; info->need_fullpath = true;
}
void libretro_pyrowave_retro_get_system_av_info(struct retro_system_av_info *info) { *info = pyrowave_core_av; }
void libretro_pyrowave_retro_set_environment(retro_environment_t cb) { pyrowave_core_environment = cb; }
void libretro_pyrowave_retro_set_video_refresh(retro_video_refresh_t cb) { pyrowave_core_video = cb; }
void libretro_pyrowave_retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void libretro_pyrowave_retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { pyrowave_core_audio = cb; }
void libretro_pyrowave_retro_set_input_poll(retro_input_poll_t cb) { pyrowave_core_input = cb; }
void libretro_pyrowave_retro_set_input_state(retro_input_state_t cb) { (void)cb; }
void libretro_pyrowave_retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }
void libretro_pyrowave_retro_reset(void)
{
#ifdef PYROWAVE_CORE_GPU
   if (!pyrowave_core_loaded) return;
   pyrowave_core_restart = true;
   if (!pyrowave_core_client)
   {
      struct retro_game_info game;
      memset(&game, 0, sizeof(game)); game.path = pyrowave_core_path;
      pyrowave_core_restart = false;
      if (!pyrowave_core_open(&game)) pyrowave_core_environment(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
      return;
   }
#endif
   libretro_pyrowave_stop();
}
void libretro_pyrowave_retro_run(void)
{
   /* Bootstrap the frontend cache while loading; decoded frames stay native. */
   static const uint32_t blank = 0;
   const void *frame = &blank;
   unsigned width = 1, height = 1, pitch = sizeof(blank);
   if (pyrowave_core_input) pyrowave_core_input();
   libretro_pyrowave_poll();
#ifdef PYROWAVE_CORE_GPU
   pyrowave_core_audio_step();
   if (pyrowave_core_client && retro_atomic_load_acquire_int(&pyrowave_core_client->ready)
         && !retro_atomic_load_acquire_int(&pyrowave_core_client->stop))
   {
      frame = RETRO_HW_FRAME_BUFFER_VALID;
      width = pyrowave_core_av.geometry.base_width; height = pyrowave_core_av.geometry.base_height;
      pitch = 0;
   }
#endif
   if (pyrowave_core_video) pyrowave_core_video(frame, width, height, pitch);
}
bool libretro_pyrowave_retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!pyrowave_core_environment || !pyrowave_core_environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format)) return false;
#ifdef PYROWAVE_CORE_GPU
   {
      char *path;
      size_t bytes;
      struct retro_audio_callback callback;
      settings_t *settings = config_get_ptr();
      if (!game || !game->path) return false;
      pyrowave_core_async_audio = settings->bools.audio_enable && strcmp(settings->arrays.audio_driver, "null") != 0;
      audio_driver_request_presentation(pyrowave_core_async_audio);
      memset(&callback, 0, sizeof(callback));
      if (pyrowave_core_async_audio)
      {
         callback.callback = pyrowave_core_audio_callback; callback.set_state = pyrowave_core_audio_state;
      }
      if (!pyrowave_core_environment(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &callback))
      { audio_driver_request_presentation(false); return false; }
      bytes = strlen(game->path) + 1; path = (char*)malloc(bytes);
      if (!path) { audio_driver_request_presentation(false); return false; }
      memcpy(path, game->path, bytes);
      if (!pyrowave_core_open(game))
      { free(path); audio_driver_request_presentation(false); return false; }
      free(pyrowave_core_path); pyrowave_core_path = path;
      pyrowave_core_loaded = true; pyrowave_core_restart = false;
      return true;
   }
#else
   (void)game; return false;
#endif
}
void libretro_pyrowave_retro_unload_game(void)
{
#ifdef PYROWAVE_CORE_GPU
   pyrowave_core_loaded = pyrowave_core_restart = false;
#endif
   libretro_pyrowave_stop();
   audio_driver_request_presentation(false);
}
unsigned libretro_pyrowave_retro_get_region(void) { return RETRO_REGION_NTSC; }
bool libretro_pyrowave_retro_load_game_special(unsigned type, const struct retro_game_info *game, size_t count)
{ (void)type; (void)game; (void)count; return false; }
size_t libretro_pyrowave_retro_serialize_size(void) { return 0; }
bool libretro_pyrowave_retro_serialize(void *data, size_t bytes) { (void)data; (void)bytes; return false; }
bool libretro_pyrowave_retro_unserialize(const void *data, size_t bytes) { (void)data; (void)bytes; return false; }
void *libretro_pyrowave_retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t libretro_pyrowave_retro_get_memory_size(unsigned id) { (void)id; return 0; }
void libretro_pyrowave_retro_cheat_reset(void) { }
void libretro_pyrowave_retro_cheat_set(unsigned id, bool enabled, const char *code)
{ (void)id; (void)enabled; (void)code; }
