/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include "record_pyrowave_receiver.h"
#include "../gfx/video_driver.h"
#include "../audio/audio_presentation.h"
#include <string.h>
#include <stdlib.h>
#include <retro_atomic.h>

#include "../runloop.h"
#include "../command.h"
#include "../driver.h"
#include "../audio/audio_driver.h"
static bool pyrowave_receiver_owned;
static bool pyrowave_receiver_start_pending;
static retro_atomic_int_t pyrowave_receiver_audio_enabled = RETRO_ATOMIC_INT_INITIALIZER(0);

#if defined(_WIN32) && defined(HAVE_D3D11) && !defined(__WINRT__) && !defined(_XBOX)
#define PYROWAVE_RECEIVER_D3D11
#endif
#if defined(_WIN32) && defined(HAVE_D3D12) && !defined(__WINRT__) && !defined(_XBOX)
#define PYROWAVE_RECEIVER_D3D12
#endif
#if defined(HAVE_VULKAN) && !defined(ANDROID)
#define PYROWAVE_RECEIVER_VULKAN
#endif
#if defined(HAVE_THREADS) && defined(HAVE_CONFIGFILE) && ((defined(HAVE_OPENGL_CORE) && !defined(HAVE_OPENGLES)) || defined(PYROWAVE_RECEIVER_D3D11) || defined(PYROWAVE_RECEIVER_D3D12) || defined(PYROWAVE_RECEIVER_VULKAN))
#define PYROWAVE_RECEIVER_GPU
static struct retro_system_av_info pyrowave_receiver_av;
static struct retro_system_av_info pyrowave_receiver_saved_av;
static char *pyrowave_receiver_path;
static bool pyrowave_receiver_restart;
#include <retro_atomic.h>
#include <file/config_file.h>
#include <net/net_compat.h>
#include "../gfx/video_thread_wrapper.h"
#include "record_pyrowave_player_gl.h"
#ifdef PYROWAVE_RECEIVER_D3D11
#include "record_pyrowave_player_d3d11.h"
#endif
#ifdef PYROWAVE_RECEIVER_D3D12
#include "record_pyrowave_player_d3d12.h"
#endif
#ifdef PYROWAVE_RECEIVER_VULKAN
#include "record_pyrowave_player_vulkan.h"
#define PYROWAVE_RECEIVER_VK_FENCES (2 * RECORD_GPU_QUEUE_SLOTS)
#endif
#include "record_pyrowave_dispatch.h"
#include "record_driver.h"
#include "../audio/audio_driver.h"
#include "../audio/audio_thread_wrapper.h"
#include "../audio/audio_pyrowave.h"
#include "../configuration.h"
#include "../verbosity.h"

typedef struct pyrowave_receiver_client
{
   record_pyrowave_dispatch_t *io;
   record_pyrowave_player_gl_t *player;
   void *player_d3d12;
#ifdef PYROWAVE_RECEIVER_D3D11
   record_pyrowave_d3d11_t *player_d3d11;
   record_pyrowave_d3d11_image_t d3d11_image, d3d11_future;
#endif
#ifdef PYROWAVE_RECEIVER_D3D12
   record_pyrowave_d3d12_image_t d3d12_image, d3d12_future;
   ID3D12Fence *display_fence;
   uint64_t display_value;
   bool d3d12_detached;
#endif
#ifdef PYROWAVE_RECEIVER_VULKAN
   record_pyrowave_vulkan_t *player_vulkan;
   record_pyrowave_vulkan_image_t vulkan_image, vulkan_future;
   VkFence vulkan_fences[PYROWAVE_RECEIVER_VK_FENCES];
   VkFence vulkan_recovery_fence;
   bool vulkan_recovery_submitted;
   bool vulkan_busy[PYROWAVE_RECEIVER_VK_FENCES];
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
   bool consent_required, approved;
} pyrowave_receiver_client_t;

static pyrowave_receiver_client_t *pyrowave_receiver_client;
static bool pyrowave_receiver_async_audio;
/* One outstanding main->audio request; ack is the old borrow's last access. */
static pyrowave_receiver_client_t *pyrowave_receiver_audio_requested;
static pyrowave_receiver_client_t *pyrowave_receiver_audio_active;
static retro_atomic_int_t pyrowave_receiver_audio_sequence = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t pyrowave_receiver_audio_ack = RETRO_ATOMIC_INT_INITIALIZER(0);

static bool pyrowave_receiver_audio_request(pyrowave_receiver_client_t *target, unsigned *request)
{
   unsigned sequence = (unsigned)retro_atomic_load_relaxed_int(&pyrowave_receiver_audio_sequence);
   if (sequence != (unsigned)retro_atomic_load_acquire_int(&pyrowave_receiver_audio_ack)) return false;
   pyrowave_receiver_audio_requested = target;
   *request = sequence + 1;
   retro_atomic_store_release_int(&pyrowave_receiver_audio_sequence, (int)*request);
   return true;
}

static void pyrowave_receiver_audio_retire(pyrowave_receiver_client_t *c, bool failed)
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

void record_pyrowave_receiver_audio_control(bool exiting)
{
   unsigned sequence = (unsigned)retro_atomic_load_acquire_int(&pyrowave_receiver_audio_sequence);
   unsigned ack = (unsigned)retro_atomic_load_relaxed_int(&pyrowave_receiver_audio_ack);
   pyrowave_receiver_client_t *target = sequence == ack ? NULL : pyrowave_receiver_audio_requested;
   if (exiting)
   {
      pyrowave_receiver_audio_retire(pyrowave_receiver_audio_active, true);
      if (target != pyrowave_receiver_audio_active) pyrowave_receiver_audio_retire(target, true);
      pyrowave_receiver_audio_active = NULL;
   }
   else if (sequence != ack)
   {
      if (target != pyrowave_receiver_audio_active) pyrowave_receiver_audio_retire(pyrowave_receiver_audio_active, false);
      pyrowave_receiver_audio_active = target;
   }
   if (sequence != ack) retro_atomic_store_release_int(&pyrowave_receiver_audio_ack, (int)sequence);
}

static void pyrowave_receiver_audio_publish(pyrowave_receiver_client_t *c,
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

static void pyrowave_receiver_audio_callback(void)
{
   pyrowave_receiver_client_t *c = pyrowave_receiver_audio_active;
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
      pyrowave_receiver_audio_publish(c, &position, ended);
      if (ended) retro_atomic_store_release_int(&c->audio_end, 1);
      else if (!c->audio_held) audio_driver_pyrowave_silence(c->audio_clock);
   }
}

static bool pyrowave_receiver_render(pyrowave_receiver_client_t *c);
/* A held clock is a proven presentation ceiling, not extrapolated audio. */
static bool pyrowave_receiver_due(const pyrowave_receiver_client_t *c, const record_gpu_frame_t *frame)
{
   return c->target_valid && frame->timestamp <= c->target_time;
}
static bool pyrowave_receiver_late(const pyrowave_receiver_client_t *c, const record_gpu_frame_t *frame)
{
   return c->audio_async && c->setup.stream.audio_rate && c->target_valid
      && !retro_atomic_load_acquire_int(&c->audio_end) && frame->timestamp < c->target_time
      && frame->duration <= c->target_time - frame->timestamp;
}
#ifdef PYROWAVE_RECEIVER_VULKAN
static bool pyrowave_receiver_vulkan_prepare(pyrowave_receiver_client_t *c);
static bool pyrowave_receiver_vulkan_render(pyrowave_receiver_client_t *c,
      VkCommandBuffer commands, VkFence *completion, bool advance);
static bool pyrowave_receiver_vulkan_free(pyrowave_receiver_client_t *c);
#endif
#ifdef PYROWAVE_RECEIVER_D3D12
static bool pyrowave_receiver_render_d3d12(pyrowave_receiver_client_t *c,
      ID3D12GraphicsCommandList *commands, ID3D12Fence *fence, uint64_t value);
#endif

#ifdef PYROWAVE_RECEIVER_D3D11
static bool pyrowave_receiver_render_d3d11(pyrowave_receiver_client_t *c);
#endif

static uintptr_t pyrowave_receiver_control_run(void *data, const video_poke_interface_t *poke, void *user)
{
   pyrowave_receiver_client_t *c = (pyrowave_receiver_client_t*)user;
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
      else if (c->control_kind == 1) pyrowave_receiver_render(c);
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
#ifdef PYROWAVE_RECEIVER_D3D11
   else if (c->api == VIDEO_DEVICE_D3D11)
   {
      if (!c->control_kind)
      {
         c->player_d3d11 = record_pyrowave_player_d3d11_new(&c->setup);
         if (!c->player_d3d11) result = 0;
         else record_pyrowave_d3d11_session_video(c->player_d3d11, &c->input);
      }
      else if (c->control_kind == 1)
      {
         pyrowave_receiver_render_d3d11(c);
         /* Paused teardown has no Present to submit the final-use query. */
         if (retro_atomic_load_acquire_int(&c->stop))
         {
            ID3D11DeviceContext *context = (ID3D11DeviceContext*)c->setup.queue;
            context->lpVtbl->Flush(context);
         }
      }
      else if (c->control_kind == 3)
      {
         if (record_pyrowave_d3d11_quiesce(c->player_d3d11)
               && record_pyrowave_d3d11_abandon(c->player_d3d11))
         {
            c->player_d3d11 = NULL; c->held = c->future = false;
            retro_atomic_store_release_int(&c->render_done, 1);
         }
      }
      else if (!record_pyrowave_d3d11_free(c->player_d3d11)) result = 0;
      else c->player_d3d11 = NULL;
   }
#endif
#ifdef PYROWAVE_RECEIVER_D3D12
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
      else if (c->control_kind == 1) pyrowave_receiver_render_d3d12(c, NULL, NULL, 0);
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
#ifdef PYROWAVE_RECEIVER_VULKAN
   else if (c->api == VIDEO_DEVICE_VULKAN)
   {
      if (!c->control_kind)
      {
         if (!pyrowave_receiver_vulkan_prepare(c)) result = 0;
         else record_pyrowave_vulkan_session_video(c->player_vulkan, &c->input);
      }
      else if (c->control_kind == 1)
         pyrowave_receiver_vulkan_render(c, VK_NULL_HANDLE, NULL, false);
      else if (c->control_kind == 3)
      {
         int retired = video_vulkan_pyrowave_quiesce(data,
               c->vulkan_recovery_fence, &c->vulkan_recovery_submitted);
         if (retired > 0 && record_pyrowave_vulkan_abandon(c->player_vulkan))
         {
            c->player_vulkan = NULL; c->held = false;
            memset(c->vulkan_busy, 0, sizeof(c->vulkan_busy));
            if (pyrowave_receiver_vulkan_free(c))
            {
               video_vulkan_pyrowave_resume(data);
               retro_atomic_store_release_int(&c->render_done, 1);
            }
         }
      }
      else if (!pyrowave_receiver_vulkan_free(c)) result = 0;
   }
#endif
   else result = 0;
   return result;
}
static void pyrowave_receiver_control_done(void *user, uintptr_t result)
{
   pyrowave_receiver_client_t *c = (pyrowave_receiver_client_t*)user;
   c->control_pending = false;
   if (!c->control_kind && result) c->constructed = true;
   if (!result)
   {
      if (!c->control_kind) RARCH_ERR("[PyroWave] Receiver GPU setup failed.\n");
      retro_atomic_store_release_int(&c->failed, 1);
   }
}
static bool pyrowave_receiver_control_post(pyrowave_receiver_client_t *c, unsigned kind)
{
   video_driver_state_t *video = video_state_get_ptr();
   if (c->control_pending) return false;
   c->control_kind = kind; c->control_pending = true;
   c->control_job.run = pyrowave_receiver_control_run; c->control_job.user = c;
   c->control.kind = VIDEO_THREAD_ASYNC_JOB; c->control.img = &c->control_job;
   c->control.user = c; c->control.done = pyrowave_receiver_control_done;
   if (video->thread_wrapper_active)
   {
      if (video_thread_async_post(&c->control)) return true;
      c->control_pending = false; return false;
   }
   pyrowave_receiver_control_done(c, pyrowave_receiver_control_run(video->data, video->poke, c));
   return true;
}

static bool pyrowave_receiver_open(const char *path, const char *directory)
{
   pyrowave_receiver_client_t *c;
   config_file_t *file;
   struct addrinfo hints, *address = NULL;
   char url[512], host[256], service[6];
   const char *start, *colon;
   size_t length;
   bool listen, ok = false;
   uint16_t endian = 1;
   if ((!path && !directory) || pyrowave_receiver_client || recording_gpu_pending()
         || recording_state_get_ptr()->enable || *(const uint8_t*)&endian != 1) return false;
   c = (pyrowave_receiver_client_t*)calloc(1, sizeof(*c));
   if (!c) return false;
   c->audio_async = path ? pyrowave_receiver_async_audio : false;
   c->consent_required = directory != NULL;
   retro_atomic_int_init(&c->audio_done, 1);
   retro_atomic_int_init(&c->audio_end, 0); retro_atomic_int_init(&c->audio_failed, 0);
   retro_atomic_int_init(&c->clock_sequence, 0); retro_atomic_int_init(&c->clock_low, 0);
   retro_atomic_int_init(&c->clock_high, 0);
   retro_atomic_int_init(&c->ready, 0); retro_atomic_int_init(&c->stop, 0);
   retro_atomic_int_init(&c->failed, 0); retro_atomic_int_init(&c->render_done, 0);
   retro_atomic_int_init(&c->io_done, 0); retro_atomic_int_init(&c->video_end, 0);
   retro_atomic_int_init(&c->recovery, 0);
   if (directory)
   {
      if (strlen(directory) >= sizeof(c->directory)) goto end;
      strcpy(c->directory, directory);
      strcpy(url, "rpws-listen://0.0.0.0:0");
   }
   else
   {
      file = config_file_new_from_path_to_string(path);
      if (!file) goto end;
      ok = config_get_array(file, "pyrowave_url", url, sizeof(url));
      if (ok && config_get_entry(file, "pyrowave_shader_directory"))
         ok = config_get_array(file, "pyrowave_shader_directory", c->directory, sizeof(c->directory));
      config_file_free(file);
      if (!ok) goto end;
   }
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
   pyrowave_receiver_client = c; ok = true;
   RARCH_LOG("[PyroWave] Receiver connecting.\n");
end:
   freeaddrinfo_retro(address);
   if (!ok) free(c);
   return ok;
}

void record_pyrowave_receiver_stop(void)
{
   pyrowave_receiver_client_t *c = pyrowave_receiver_client;
   if (c && c->av_updating) return;
   pyrowave_receiver_restart = false;
   pyrowave_receiver_start_pending = false;
   if (!c)
   {
      free(pyrowave_receiver_path); pyrowave_receiver_path = NULL;
      pyrowave_receiver_owned = false;
      retro_atomic_store_release_int(&pyrowave_receiver_audio_enabled, 0);
      audio_driver_request_presentation(false);
      return;
   }
   retro_atomic_store_release_int(&c->stop, 1);
   record_pyrowave_dispatch_cancel(c->io);
}
bool record_pyrowave_receiver_pending(void)
{ return pyrowave_receiver_client && !pyrowave_receiver_client->av_updating; }

static bool pyrowave_receiver_setup(pyrowave_receiver_client_t *c)
{
   video_device_info_t device;
   rpyrowave_stream_config_t *cfg = &c->setup.stream;
   memset(&device, 0, sizeof(device));
   if (!record_pyrowave_dispatch_config(c->io, cfg, &c->generation)) return false;
   if (cfg->audio_channels > 2) return false;
   if (cfg->sequence.transfer_pq || cfg->sequence.primaries_bt2020)
      c->setup.texture_mode = RPYROWAVE_COLOR_SDR709;
   pyrowave_receiver_av.geometry.base_width = cfg->sequence.width;
   pyrowave_receiver_av.geometry.base_height = cfg->sequence.height;
   pyrowave_receiver_av.geometry.max_width = cfg->sequence.width;
   pyrowave_receiver_av.geometry.max_height = cfg->sequence.height;
   pyrowave_receiver_av.geometry.aspect_ratio = (float)cfg->sequence.width / cfg->sequence.height;
   pyrowave_receiver_av.timing.fps = (double)cfg->fps_numerator / cfg->fps_denominator;
   pyrowave_receiver_av.timing.sample_rate = cfg->audio_rate ? cfg->audio_rate : 48000;
   c->av_updating = true;
   if (!runloop_environment_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &pyrowave_receiver_av))
   { c->av_updating = false; return false; }
   c->av_updating = false;
   if (!video_driver_get_device(&device)) return false;
   if (device.api != VIDEO_DEVICE_OPENGL
#ifdef PYROWAVE_RECEIVER_D3D11
         && device.api != VIDEO_DEVICE_D3D11
#endif
#ifdef PYROWAVE_RECEIVER_D3D12
         && device.api != VIDEO_DEVICE_D3D12
#endif
#ifdef PYROWAVE_RECEIVER_VULKAN
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
   return pyrowave_receiver_control_post(c, 0);
}

static bool pyrowave_receiver_render(pyrowave_receiver_client_t *c)
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
            || !pyrowave_receiver_late(c, &dropped)) break;
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
   if (c->future && pyrowave_receiver_due(c, &c->gl_future.frame))
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

#ifdef PYROWAVE_RECEIVER_D3D11
static bool pyrowave_receiver_render_d3d11(pyrowave_receiver_client_t *c)
{
#ifdef PYROWAVE_RECEIVER_D3D11
   record_pyrowave_d3d11_image_t next;
   record_gpu_frame_t dropped;
   int result = RECORD_GPU_QUEUE_BUSY;
   unsigned i;
   if (!c->player_d3d11 || retro_atomic_load_acquire_int(&c->render_done)) return false;
   if (retro_atomic_load_acquire_int(&c->recovery)) return false;
   if (!record_pyrowave_d3d11_poll(c->player_d3d11))
   {
      retro_atomic_store_release_int(&c->failed, 1);
      retro_atomic_store_release_int(&c->recovery, 1);
      return false;
   }
   if (retro_atomic_load_acquire_int(&c->stop))
   {
      if (c->future)
      {
         if (!record_pyrowave_d3d11_image_drop(c->player_d3d11, c->d3d11_future.frame.slot)) return false;
         c->future = false;
      }
      if (c->held)
      {
         if (!record_pyrowave_d3d11_present(c->player_d3d11, c->d3d11_image.frame.slot))
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
            if (record_pyrowave_d3d11_receive_drop(c->player_d3d11, &dropped) != RECORD_GPU_QUEUE_OK) break;
         for (i = 0; i < c->setup.slots; i++)
         {
            if (record_pyrowave_d3d11_image(c->player_d3d11, &next) != RECORD_GPU_QUEUE_OK) break;
            record_pyrowave_d3d11_image_drop(c->player_d3d11, next.frame.slot);
         }
         if (record_pyrowave_d3d11_idle(c->player_d3d11))
            retro_atomic_store_release_int(&c->render_done, 1);
      }
      return false;
   }
   for (i = 0; i < c->setup.slots; i++)
   {
      if (record_pyrowave_d3d11_receive_peek(c->player_d3d11, &dropped) != RECORD_GPU_QUEUE_OK
            || !pyrowave_receiver_late(c, &dropped)) break;
      if (record_pyrowave_d3d11_receive_drop(c->player_d3d11, &dropped) != RECORD_GPU_QUEUE_OK)
      { retro_atomic_store_release_int(&c->failed, 1); break; }
   }
   if (!c->video_input_end) result = record_pyrowave_d3d11_decode(c->player_d3d11);
   if (result == RECORD_GPU_QUEUE_OK) c->decoding++;
   else if (result == RECORD_GPU_QUEUE_ERROR)
   {
      retro_atomic_store_release_int(&c->failed, 1);
      retro_atomic_store_release_int(&c->recovery, 1);
   }
   if (!c->future && record_pyrowave_d3d11_image(c->player_d3d11, &next) == RECORD_GPU_QUEUE_OK)
   {
      c->d3d11_future = next; c->future = true;
      if (c->decoding) c->decoding--;
   }
   if (c->future && pyrowave_receiver_due(c, &c->d3d11_future.frame))
   {
      if (c->held && !record_pyrowave_d3d11_present(c->player_d3d11, c->d3d11_image.frame.slot))
      {
         retro_atomic_store_release_int(&c->failed, 1);
         retro_atomic_store_release_int(&c->recovery, 1);
      }
      else
      { c->d3d11_image = c->d3d11_future; c->held = true; c->future = false; }
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
#endif

unsigned record_pyrowave_receiver_gl_frame(void *data, unsigned *dims, bool advance,
      const struct video_frame_info *timing)
{
   pyrowave_receiver_client_t *c = (pyrowave_receiver_client_t*)data;
   if (!c || c->api != VIDEO_DEVICE_OPENGL || !retro_atomic_load_acquire_int(&c->ready)
         || retro_atomic_load_acquire_int(&c->render_done)) return 0;
   c->target_valid = timing && timing->recording_pyrowave_clock_valid;
   c->target_time = timing ? timing->recording_gpu_timestamp : 0;
   if (advance || retro_atomic_load_acquire_int(&c->stop))
   { if (!pyrowave_receiver_render(c)) return 0; }
   else if (!c->held) return 0;
   *dims = VIDEO_SCALE_PACK(c->setup.stream.sequence.width, c->setup.stream.sequence.height);
   return c->image.texture;
}
void *record_pyrowave_receiver_d3d11_frame(void *data, unsigned *dims, bool advance,
      const struct video_frame_info *timing)
{
#ifdef PYROWAVE_RECEIVER_D3D11
   pyrowave_receiver_client_t *c = (pyrowave_receiver_client_t*)data;
   if (!c || c->api != VIDEO_DEVICE_D3D11 || !retro_atomic_load_acquire_int(&c->ready)
         || retro_atomic_load_acquire_int(&c->render_done)) return NULL;
   c->target_valid = timing && timing->recording_pyrowave_clock_valid;
   c->target_time = timing ? timing->recording_gpu_timestamp : 0;
   if (advance || retro_atomic_load_acquire_int(&c->stop))
   { if (!pyrowave_receiver_render_d3d11(c)) return NULL; }
   else if (!c->held) return NULL;
   *dims = VIDEO_SCALE_PACK(c->setup.stream.sequence.width, c->setup.stream.sequence.height);
   return c->d3d11_image.view;
#else
   (void)data; (void)dims; (void)advance; (void)timing; return NULL;
#endif
}

#ifdef PYROWAVE_RECEIVER_D3D12
static bool pyrowave_receiver_render_d3d12(pyrowave_receiver_client_t *c,
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
               || !pyrowave_receiver_late(c, &dropped)) break;
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
      if (c->future && pyrowave_receiver_due(c, &c->d3d12_future.frame))
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

void record_pyrowave_receiver_d3d12_failed(void *context)
{
#ifdef PYROWAVE_RECEIVER_D3D12
   pyrowave_receiver_client_t *c = (pyrowave_receiver_client_t*)context;
   if (!c || c->api != VIDEO_DEVICE_D3D12 || !c->player_d3d12) return;
   retro_atomic_store_release_int(&c->recovery, 1);
   retro_atomic_store_release_int(&c->failed, 1);
#else
   (void)context;
#endif
}

void *record_pyrowave_receiver_d3d12_frame(void *context, void *commands, void *fence,
      uint64_t value, unsigned *dims, bool advance, const struct video_frame_info *timing)
{
#ifdef PYROWAVE_RECEIVER_D3D12
   pyrowave_receiver_client_t *c = (pyrowave_receiver_client_t*)context;
   if (!c || c->api != VIDEO_DEVICE_D3D12 || !retro_atomic_load_acquire_int(&c->ready)
         || retro_atomic_load_acquire_int(&c->render_done)) return NULL;
   c->target_valid = timing && timing->recording_pyrowave_clock_valid;
   c->target_time = timing ? timing->recording_gpu_timestamp : 0;
   if (advance || retro_atomic_load_acquire_int(&c->stop))
   {
      if (!pyrowave_receiver_render_d3d12(c, commands, fence, value)) return NULL;
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

#ifdef PYROWAVE_RECEIVER_VULKAN
static bool pyrowave_receiver_vulkan_free(pyrowave_receiver_client_t *c)
{
   unsigned i;
   for (i = 0; i < PYROWAVE_RECEIVER_VK_FENCES; i++)
      if (c->vulkan_busy[i]) return false;
   if (!record_pyrowave_vulkan_free(c->player_vulkan)) return false;
   c->player_vulkan = NULL;
   for (i = 0; i < PYROWAVE_RECEIVER_VK_FENCES; i++)
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

static bool pyrowave_receiver_vulkan_prepare(pyrowave_receiver_client_t *c)
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
   c->vulkan_pending = c->vulkan_display = c->vulkan_previous = PYROWAVE_RECEIVER_VK_FENCES;
   c->player_vulkan = record_pyrowave_player_vulkan_new(&c->setup);
   if (!c->player_vulkan) return false;
   memset(&info, 0, sizeof(info)); info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   for (i = 0; i < PYROWAVE_RECEIVER_VK_FENCES; i++)
      if (c->vulkan_create((VkDevice)c->device, &info, NULL, &c->vulkan_fences[i]) != VK_SUCCESS)
      {
         c->vulkan_fences[i] = VK_NULL_HANDLE;
         break;
      }
   if (i == PYROWAVE_RECEIVER_VK_FENCES)
   {
      if (c->vulkan_create((VkDevice)c->device, &info, NULL, &c->vulkan_recovery_fence) == VK_SUCCESS)
         return true;
      c->vulkan_recovery_fence = VK_NULL_HANDLE;
   }
   pyrowave_receiver_vulkan_free(c);
   return false;
}

static void pyrowave_receiver_vulkan_poll(pyrowave_receiver_client_t *c)
{
   unsigned i;
   if (!record_pyrowave_vulkan_poll(c->player_vulkan))
   {
      retro_atomic_store_release_int(&c->recovery, 1);
      retro_atomic_store_release_int(&c->failed, 1);
   }
   for (i = 0; i < PYROWAVE_RECEIVER_VK_FENCES; i++)
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

static bool pyrowave_receiver_vulkan_render(pyrowave_receiver_client_t *c,
      VkCommandBuffer commands, VkFence *completion, bool advance)
{
   record_pyrowave_vulkan_image_t next;
   record_gpu_frame_t dropped;
   unsigned i;
   int result = RECORD_GPU_QUEUE_BUSY;
   if (completion) *completion = VK_NULL_HANDLE;
   if (!c->player_vulkan || retro_atomic_load_acquire_int(&c->render_done)
         || retro_atomic_load_acquire_int(&c->recovery)) return false;
   pyrowave_receiver_vulkan_poll(c);
   if (retro_atomic_load_acquire_int(&c->recovery)) return false;
   if (retro_atomic_load_acquire_int(&c->stop))
   {
      /* An unacknowledged submission retains both its new display pin and
       * the previous valid pin for explicit cold failure recovery. */
      if (c->vulkan_pending != PYROWAVE_RECEIVER_VK_FENCES) return false;
      if (c->future)
      {
         if (!record_pyrowave_vulkan_image_drop(c->player_vulkan, c->vulkan_future.frame.slot)) return false;
         c->future = false;
      }
      if (c->held)
      {
         if (c->vulkan_display == PYROWAVE_RECEIVER_VK_FENCES)
         {
            if (!record_pyrowave_vulkan_image_drop(c->player_vulkan, c->vulkan_image.frame.slot)) return false;
         }
         else if (!record_pyrowave_vulkan_present(c->player_vulkan, c->vulkan_image.frame.slot,
                  c->vulkan_fences[c->vulkan_display])) return false;
         c->held = false; c->vulkan_display = PYROWAVE_RECEIVER_VK_FENCES;
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
         pyrowave_receiver_vulkan_poll(c);
         for (i = 0; i < PYROWAVE_RECEIVER_VK_FENCES; i++) if (c->vulkan_busy[i]) break;
         if (i == PYROWAVE_RECEIVER_VK_FENCES && record_pyrowave_vulkan_idle(c->player_vulkan))
            retro_atomic_store_release_int(&c->render_done, 1);
      }
      return false;
   }
   if (!commands || !completion) return false;
   if (c->vulkan_pending != PYROWAVE_RECEIVER_VK_FENCES)
   {
      retro_atomic_store_release_int(&c->failed, 1);
      return false;
   }
   for (i = 0; i < PYROWAVE_RECEIVER_VK_FENCES; i++) if (!c->vulkan_busy[i]) break;
   if (i == PYROWAVE_RECEIVER_VK_FENCES)
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
               || !pyrowave_receiver_late(c, &dropped)) break;
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
      if (c->future && pyrowave_receiver_due(c, &c->vulkan_future.frame))
      {
         bool returned = true;
         if (c->held)
            returned = c->vulkan_display != PYROWAVE_RECEIVER_VK_FENCES
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
            c->vulkan_display = PYROWAVE_RECEIVER_VK_FENCES;
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

bool record_pyrowave_receiver_vulkan_frame(void *context, VkCommandBuffer commands,
      struct record_pyrowave_vulkan_image *image, VkFence *completion, unsigned *dims, bool advance,
      const struct video_frame_info *timing)
{
   pyrowave_receiver_client_t *c = (pyrowave_receiver_client_t*)context;
   if (completion) *completion = VK_NULL_HANDLE;
   if (c)
   {
      c->target_valid = timing && timing->recording_pyrowave_clock_valid;
      c->target_time = timing ? timing->recording_gpu_timestamp : 0;
   }
   if (!c || c->api != VIDEO_DEVICE_VULKAN || !retro_atomic_load_acquire_int(&c->ready)
         || !image || !completion || !dims || !pyrowave_receiver_vulkan_render(c, commands, completion, advance)) return false;
   *image = c->vulkan_image;
   *dims = VIDEO_SCALE_PACK(c->setup.stream.sequence.width, c->setup.stream.sequence.height);
   return true;
}

void record_pyrowave_receiver_vulkan_submitted(void *context, VkFence completion,
      VkResult result, bool discarded)
{
   pyrowave_receiver_client_t *c = (pyrowave_receiver_client_t*)context;
   unsigned pending;
   if (!c || c->api != VIDEO_DEVICE_VULKAN || !c->player_vulkan) return;
   pending = c->vulkan_pending;
   if (result == VK_SUCCESS && !discarded)
   {
      if (!completion) return;
      if (pending == PYROWAVE_RECEIVER_VK_FENCES || c->vulkan_fences[pending] != completion)
      {
         retro_atomic_store_release_int(&c->failed, 1);
         return;
      }
      if (c->vulkan_drew) c->vulkan_display = pending;
      c->vulkan_pending = PYROWAVE_RECEIVER_VK_FENCES;
      return;
   }
   retro_atomic_store_release_int(&c->failed, 1);
   if (!discarded || !completion || pending == PYROWAVE_RECEIVER_VK_FENCES
         || c->vulkan_fences[pending] != completion)
   {
      retro_atomic_store_release_int(&c->recovery, 1);
      return;
   }
   record_pyrowave_vulkan_discard(c->player_vulkan, completion);
   if (record_pyrowave_vulkan_fence_borrowed(c->player_vulkan, completion)) return;
   if (c->vulkan_drew && c->vulkan_previous == PYROWAVE_RECEIVER_VK_FENCES)
   {
      record_pyrowave_vulkan_image_drop(c->player_vulkan, c->vulkan_image.frame.slot);
      c->held = false;
   }
   c->vulkan_display = c->vulkan_previous;
   c->vulkan_busy[pending] = false;
   c->vulkan_pending = PYROWAVE_RECEIVER_VK_FENCES;
}
#endif

bool record_pyrowave_receiver_frame_info(struct video_frame_info *info, bool advance)
{
   pyrowave_receiver_client_t *c = pyrowave_receiver_client;
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

void record_pyrowave_receiver_poll(void)
{
   pyrowave_receiver_client_t *c = pyrowave_receiver_client;
   unsigned state;
   if (pyrowave_receiver_start_pending && !c && video_state_get_ptr()->data)
   {
      settings_t *settings = config_get_ptr();
      if (!pyrowave_receiver_owned)
         pyrowave_receiver_saved_av = video_state_get_ptr()->av_info;
      pyrowave_receiver_owned = true;
      pyrowave_receiver_start_pending = false;
      pyrowave_receiver_async_audio = settings->bools.audio_enable
         && strcmp(settings->arrays.audio_driver, "null") != 0;
      if (!pyrowave_receiver_open(pyrowave_receiver_path, NULL))
      {
         RARCH_ERR("[PyroWave] Receiver connection failed.\n");
         pyrowave_receiver_owned = false;
         free(pyrowave_receiver_path); pyrowave_receiver_path = NULL;
         retro_atomic_store_release_int(&pyrowave_receiver_audio_enabled, 0);
         audio_driver_request_presentation(false);
         command_event(CMD_EVENT_AUDIO_REINIT, NULL);
      }
      c = pyrowave_receiver_client;
      if (c)
      {
         c->av_updating = true;
         command_event(CMD_EVENT_AUDIO_REINIT, NULL);
         c->av_updating = false;
      }
   }
   if (!c || c->av_updating) return;
   if (c->audio_async)
   {
      audio_driver_state_t *audio = audio_state_get_ptr();
      if (!audio->context_audio_data) record_pyrowave_receiver_audio_control(true);
      else if (audio->current_audio && !strcmp(audio->current_audio->ident, "audio-thread")
            && audio_thread_parked(audio->context_audio_data)) record_pyrowave_receiver_audio_control(false);
   }
   video_thread_async_poll();
   state = record_pyrowave_dispatch_state(c->io, NULL);
   if (c->consent_required && !c->approved && state <= RECORD_PYROWAVE_DISPATCH_CONFIGURING
         && !retro_atomic_load_acquire_int(&c->stop)) return;
   if (!c->configured && !c->control_pending && state == RECORD_PYROWAVE_DISPATCH_CONFIGURING
         && !retro_atomic_load_acquire_int(&c->stop))
   {
      if (!pyrowave_receiver_setup(c))
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
      if (pyrowave_receiver_audio_request(c, &c->audio_request)) c->audio_bound = true;
      else retro_atomic_store_release_int(&c->audio_done, 1);
   }
   if (!c->audio_reported && retro_atomic_load_acquire_int(&c->audio_failed))
   { c->audio_reported = true; RARCH_ERR("[PyroWave] Audio presentation failed or is unsupported by this output.\n"); }
   if (record_pyrowave_dispatch_quiesced(c->io)) retro_atomic_store_release_int(&c->io_done, 1);
   if (state == RECORD_PYROWAVE_DISPATCH_FAILED && !retro_atomic_load_acquire_int(&c->stop))
      retro_atomic_store_release_int(&c->failed, 1);
   if (retro_atomic_load_acquire_int(&c->failed) || state >= RECORD_PYROWAVE_DISPATCH_CANCELLED
         || (retro_atomic_load_acquire_int(&c->io_done) && retro_atomic_load_acquire_int(&c->audio_end)
            && retro_atomic_load_acquire_int(&c->video_end)))
      if (!retro_atomic_load_acquire_int(&c->stop)) record_pyrowave_receiver_stop();
   if (!retro_atomic_load_acquire_int(&c->stop)) return;
   if (c->audio_bound)
   {
      if (!c->audio_unbinding)
      {
         if (!pyrowave_receiver_audio_request(NULL, &c->audio_request)) return;
         c->audio_unbinding = true;
      }
      if ((unsigned)retro_atomic_load_acquire_int(&pyrowave_receiver_audio_ack) != c->audio_request
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
            pyrowave_receiver_control_post(c, 3);
         }
      }
      else
         pyrowave_receiver_control_post(c, 1);
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
#ifdef PYROWAVE_RECEIVER_VULKAN
         || c->player_vulkan
#endif
         ) { pyrowave_receiver_control_post(c, 2); return; }
   {
      bool failed = retro_atomic_load_acquire_int(&c->failed) != 0;
      bool presented = pyrowave_receiver_owned;
      if (c->audio_received)
         RARCH_LOG("[PyroWave] Device-clock audio %s.\n",
               !failed && retro_atomic_load_acquire_int(&c->audio_end) ? "drained" : "cancelled");
      free(c->mono); free(c); pyrowave_receiver_client = NULL;
      RARCH_LOG("[PyroWave] Receiver resources retired.\n");
      if (pyrowave_receiver_restart)
      {
         pyrowave_receiver_restart = false;
         pyrowave_receiver_start_pending = true;
      }
      else
      {
         pyrowave_receiver_owned = false;
         free(pyrowave_receiver_path); pyrowave_receiver_path = NULL;
         retro_atomic_store_release_int(&pyrowave_receiver_audio_enabled, 0);
         audio_driver_request_presentation(false);
         /* Restore the existing frontend session; never unload its core. */
         if (presented && !runloop_state_get_ptr()->quit_pending
               && !runloop_state_get_ptr()->content_closing)
         {
            int flags = DRIVER_VIDEO_AND_INPUT_MASK | DRIVER_AUDIO_MASK;
            /* Discard the receiver's cached hardware-frame sentinel before
             * recreating presentation for the still-loaded game core. */
            video_driver_cached_frame_invalidate();
            runloop_environment_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &pyrowave_receiver_saved_av);
            command_event(CMD_EVENT_REINIT, &flags);
            RARCH_LOG("[PyroWave] Frontend session restored.\n");
         }
      }
   }
}

static bool pyrowave_receiver_audio_block(void)
{
   pyrowave_receiver_client_t *c = pyrowave_receiver_client;
   size_t frames, consumed, i;
   const int16_t *samples;
   int result;
   if (!c || !c->accepted || !c->pcm
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
   consumed = audio_driver_sample_batch(samples, frames);
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
static void pyrowave_receiver_audio_step(void)
{
   unsigned i;
   if (pyrowave_receiver_client && pyrowave_receiver_client->audio_async) return;
   for (i = 0; i < 16; i++) if (!pyrowave_receiver_audio_block()) break;
}
#else
void record_pyrowave_receiver_poll(void) { }
void record_pyrowave_receiver_audio_control(bool exiting) { (void)exiting; }
void record_pyrowave_receiver_stop(void) { }
void record_pyrowave_receiver_d3d12_failed(void *context) { (void)context; }
bool record_pyrowave_receiver_pending(void) { return false; }
bool record_pyrowave_receiver_frame_info(struct video_frame_info *info, bool advance)
{ (void)info; (void)advance; return false; }
unsigned record_pyrowave_receiver_gl_frame(void *data, unsigned *dims, bool advance,
      const struct video_frame_info *timing)
{ (void)data; (void)dims; (void)advance; (void)timing; return 0; }
void *record_pyrowave_receiver_d3d11_frame(void *data, unsigned *dims, bool advance,
      const struct video_frame_info *timing)
{ (void)data; (void)dims; (void)advance; (void)timing; return NULL; }
void *record_pyrowave_receiver_d3d12_frame(void *context, void *commands, void *fence,
      uint64_t value, unsigned *dims, bool advance, const struct video_frame_info *timing)
{
   (void)context; (void)commands; (void)fence; (void)value;
   (void)dims; (void)advance; (void)timing; return NULL;
}
#ifdef HAVE_VULKAN
bool record_pyrowave_receiver_vulkan_frame(void *context, VkCommandBuffer commands,
      struct record_pyrowave_vulkan_image *image, VkFence *completion, unsigned *dims, bool advance,
      const struct video_frame_info *timing)
{
   (void)context; (void)commands; (void)image; (void)dims; (void)advance; (void)timing;
   if (completion) *completion = VK_NULL_HANDLE;
   return false;
}
void record_pyrowave_receiver_vulkan_submitted(void *context, VkFence completion, VkResult result, bool discarded)
{
   (void)context; (void)completion; (void)result; (void)discarded;
}
#endif

#endif

bool record_pyrowave_receiver_start(const char *path)
{
#ifdef PYROWAVE_RECEIVER_GPU
   size_t bytes;
   if (!path || !*path || pyrowave_receiver_path || record_pyrowave_receiver_pending()) return false;
   bytes = strlen(path) + 1;
   pyrowave_receiver_path = (char*)malloc(bytes);
   if (!pyrowave_receiver_path) return false;
   memcpy(pyrowave_receiver_path, path, bytes);
   pyrowave_receiver_start_pending = true;
   retro_atomic_store_release_int(&pyrowave_receiver_audio_enabled, 1);
   audio_driver_request_presentation(true);
   return true;
#else
   (void)path;
   return false;
#endif
}

bool record_pyrowave_receiver_active(void)
{
   return pyrowave_receiver_owned || pyrowave_receiver_start_pending;
}

bool record_pyrowave_receiver_audio_enabled(void)
{
   return retro_atomic_load_acquire_int(&pyrowave_receiver_audio_enabled) != 0;
}

bool record_pyrowave_receiver_reset(void)
{
#ifdef PYROWAVE_RECEIVER_GPU
   if (!record_pyrowave_receiver_active()) return false;
   if (pyrowave_receiver_start_pending) return true;
   record_pyrowave_receiver_stop();
   pyrowave_receiver_restart = true;
   return true;
#else
   return false;
#endif
}

bool record_pyrowave_receiver_frame(void)
{
   /* The frontend owns presentation while receiving. The selected game core
    * stays loaded and is not entered or replaced during stream playback. */
   static const uint32_t blank = 0;
   const void *frame = &blank;
   unsigned width = 1, height = 1, pitch = sizeof(blank);
   if (!record_pyrowave_receiver_active()) return false;
#ifdef PYROWAVE_RECEIVER_GPU
   pyrowave_receiver_audio_step();
   if (pyrowave_receiver_client && retro_atomic_load_acquire_int(&pyrowave_receiver_client->ready)
         && !retro_atomic_load_acquire_int(&pyrowave_receiver_client->stop))
   {
      frame = RETRO_HW_FRAME_BUFFER_VALID;
      width = pyrowave_receiver_av.geometry.base_width; height = pyrowave_receiver_av.geometry.base_height;
      pitch = 0;
   }
#endif
   video_driver_frame(frame, width, height, pitch);
   return true;
}

bool record_pyrowave_receiver_audio_run(void)
{
#ifdef PYROWAVE_RECEIVER_GPU
   audio_driver_state_t *audio = audio_state_get_ptr();
   uint64_t offered = audio->sink_offered_raw;
   uint64_t accepted = audio->sink_accepted;
   pyrowave_receiver_audio_callback();
   return offered != audio->sink_offered_raw || accepted != audio->sink_accepted;
#else
   return false;
#endif
}

bool record_pyrowave_receiver_listen(const char *directory)
{
#ifdef PYROWAVE_RECEIVER_GPU
   if (record_pyrowave_receiver_active()) return false;
   return pyrowave_receiver_open(NULL, directory);
#else
   (void)directory; return false;
#endif
}

unsigned record_pyrowave_receiver_port(void)
{
#ifdef PYROWAVE_RECEIVER_GPU
   pyrowave_receiver_client_t *c = pyrowave_receiver_client;
   struct sockaddr_storage address;
   size_t bytes = sizeof(address);
   if (c && c->consent_required && !retro_atomic_load_acquire_int(&c->stop)
         && record_pyrowave_dispatch_bound_address(c->io, &address, &bytes)
         && address.ss_family == AF_INET)
      return ntohs(((struct sockaddr_in*)&address)->sin_port);
#endif
   return 0;
}

bool record_pyrowave_receiver_incoming(void)
{
#ifdef PYROWAVE_RECEIVER_GPU
   pyrowave_receiver_client_t *c = pyrowave_receiver_client;
   return c && c->consent_required && !c->approved
      && !retro_atomic_load_acquire_int(&c->stop)
      && record_pyrowave_dispatch_state(c->io, NULL) == RECORD_PYROWAVE_DISPATCH_CONFIGURING;
#else
   return false;
#endif
}

bool record_pyrowave_receiver_consent(bool accept)
{
#ifdef PYROWAVE_RECEIVER_GPU
   pyrowave_receiver_client_t *c = pyrowave_receiver_client;
   settings_t *settings = config_get_ptr();
   if (!record_pyrowave_receiver_incoming()) return false;
   if (!accept)
   {
      record_pyrowave_dispatch_reject(c->io, RPYROWAVE_STREAM_REJECTED);
      record_pyrowave_receiver_stop();
      return true;
   }
   pyrowave_receiver_saved_av = video_state_get_ptr()->av_info;
   pyrowave_receiver_owned = true; c->approved = true;
   c->audio_async = settings->bools.audio_enable && strcmp(settings->arrays.audio_driver, "null") != 0;
   retro_atomic_store_release_int(&pyrowave_receiver_audio_enabled, 1);
   audio_driver_request_presentation(true);
   c->av_updating = true;
   command_event(CMD_EVENT_AUDIO_REINIT, NULL);
   c->av_updating = false;
   return true;
#else
   (void)accept; return false;
#endif
}
