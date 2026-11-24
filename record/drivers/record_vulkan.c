/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif
#include "record_vulkan.h"
#ifdef HAVE_RECORD_VULKAN
#include "../record_gpu_dispatch_vulkan_hevc.h"
#endif
#ifdef HAVE_RECORD_VULKAN
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <file/config_file.h>
#include <string/stdstring.h>
#include <retro_atomic.h>
#include "../record_native_buffer.h"
#include "../../gfx/video_driver.h"
#include "../../gfx/video_thread_wrapper.h"
#include "../../audio/audio_driver.h"
#include "../../audio/audio_upmix.h"
#include "../../configuration.h"
#include "../../command.h"
#include "../../runloop.h"
#include "../../verbosity.h"
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV) || defined(HAVE_WEBMPLAYER)
#include "../../defaults.h"
#include "../../playlist.h"
#endif
typedef struct record_vk_context
{
   uint64_t timestamp, audio_frames, pictures, offered;
   struct record_vk_context *next;
   record_gpu_dispatch_vulkan_t *job;
   record_gpu_renderer_vulkan_t *renderer;
   vulkan_encode_h264_open_t encoder;
   vulkan_encode_h264_ops_t ops;
   vulkan_encode_capture_ops_t capture_ops;
   PFN_vkGetInstanceProcAddr instance_proc;
   PFN_vkGetDeviceProcAddr device_proc;
   PFN_vkGetSemaphoreCounterValue counter;
   VkInstance instance;
   VkDevice device;
   char *path;
   uint32_t api_version;
   unsigned dims, duration, audio_rate, output_mode;
   retro_atomic_int_t failed, stop, native_error;
   bool armed, audio_enabled, video_closed, allow_drop, have_offered, streaming;
} record_vk_context_t;
static record_vk_context_t *record_vk_active, *record_vk_retired;
static bool record_vk_start_requested;

#define RVKI(member, name) \
   do \
   { \
      c->member = (PFN_vk##name)c->instance_proc(c->instance, "vk" #name); \
      if (!c->member) \
         return false; \
   } while (0)
#define RVKD(member, name) \
   do \
   { \
      c->member = (PFN_vk##name)c->device_proc(c->device, "vk" #name); \
      if (!c->member) \
         return false; \
   } while (0)
#define RVKS(member) \
   do \
   { \
      c->member = (PFN_vkCmdPipelineBarrier2)c->device_proc(c->device, \
            c->api_version < VK_API_VERSION_1_3 ? "vkCmdPipelineBarrier2KHR" : "vkCmdPipelineBarrier2"); \
      if (!c->member) \
         return false; \
   } while (0)
static bool rvk_load_ops(record_vk_context_t *c)
{
   RVKI(ops.session.families, GetPhysicalDeviceQueueFamilyProperties2);
   RVKI(ops.session.memory, GetPhysicalDeviceMemoryProperties);
   RVKI(ops.session.capabilities, GetPhysicalDeviceVideoCapabilitiesKHR);
   RVKI(ops.session.formats, GetPhysicalDeviceVideoFormatPropertiesKHR);
   RVKD(ops.session.create, CreateVideoSessionKHR);
   RVKD(ops.session.destroy, DestroyVideoSessionKHR);
   RVKD(ops.session.requirements, GetVideoSessionMemoryRequirementsKHR);
   RVKD(ops.session.bind, BindVideoSessionMemoryKHR);
   RVKD(ops.session.allocate, AllocateMemory);
   RVKD(ops.session.free_memory, FreeMemory);
   RVKD(ops.session.parameters, CreateVideoSessionParametersKHR);
   RVKD(ops.session.destroy_parameters, DestroyVideoSessionParametersKHR);
   RVKD(ops.session.headers, GetEncodedVideoSessionParametersKHR);
   RVKI(ops.pool.memory_properties, GetPhysicalDeviceMemoryProperties);
   RVKI(ops.pool.video_formats, GetPhysicalDeviceVideoFormatPropertiesKHR);
   RVKI(ops.pool.image_properties, GetPhysicalDeviceImageFormatProperties2);
   RVKI(ops.pool.format_properties, GetPhysicalDeviceFormatProperties2);
   RVKD(ops.pool.create_image, CreateImage);
   RVKD(ops.pool.destroy_image, DestroyImage);
   RVKD(ops.pool.image_memory, GetImageMemoryRequirements);
   RVKD(ops.pool.bind_image, BindImageMemory);
   RVKD(ops.pool.create_view, CreateImageView);
   RVKD(ops.pool.destroy_view, DestroyImageView);
   RVKD(ops.pool.create_buffer, CreateBuffer);
   RVKD(ops.pool.destroy_buffer, DestroyBuffer);
   RVKD(ops.pool.buffer_memory, GetBufferMemoryRequirements);
   RVKD(ops.pool.bind_buffer, BindBufferMemory);
   RVKD(ops.pool.map, MapMemory);
   RVKD(ops.pool.unmap, UnmapMemory);
   RVKD(ops.pool.allocate, AllocateMemory);
   RVKD(ops.pool.free_memory, FreeMemory);
   RVKD(ops.pool.create_commands, CreateCommandPool);
   RVKD(ops.pool.destroy_commands, DestroyCommandPool);
   RVKD(ops.pool.allocate_command, AllocateCommandBuffers);
   RVKD(ops.pool.create_fence, CreateFence);
   RVKD(ops.pool.destroy_fence, DestroyFence);
   RVKD(ops.pool.create_queries, CreateQueryPool);
   RVKD(ops.pool.destroy_queries, DestroyQueryPool);
   RVKD(ops.pool.create_semaphore, CreateSemaphore);
   RVKD(ops.pool.destroy_semaphore, DestroySemaphore);
   RVKD(ops.submit.reset_command, ResetCommandBuffer);
   RVKD(ops.submit.begin, BeginCommandBuffer);
   RVKD(ops.submit.end, EndCommandBuffer);
   RVKS(ops.submit.barrier);
   RVKD(ops.submit.reset_query, CmdResetQueryPool);
   RVKD(ops.submit.begin_query, CmdBeginQuery);
   RVKD(ops.submit.end_query, CmdEndQuery);
   RVKD(ops.submit.begin_coding, CmdBeginVideoCodingKHR);
   RVKD(ops.submit.control, CmdControlVideoCodingKHR);
   RVKD(ops.submit.encode, CmdEncodeVideoKHR);
   RVKD(ops.submit.end_coding, CmdEndVideoCodingKHR);
   RVKD(ops.submit.reset_fence, ResetFences);
   RVKD(ops.submit.submit, QueueSubmit);
   RVKD(ops.submit.status, GetFenceStatus);
   RVKD(ops.submit.feedback, GetQueryPoolResults);
   RVKD(ops.submit.wait, WaitForFences);
   return true;
}
static bool rvk_load_capture(record_vk_context_t *c)
{
   RVKD(capture_ops.create_set_layout, CreateDescriptorSetLayout);
   RVKD(capture_ops.destroy_set_layout, DestroyDescriptorSetLayout);
   RVKD(capture_ops.create_descriptor_pool, CreateDescriptorPool);
   RVKD(capture_ops.destroy_descriptor_pool, DestroyDescriptorPool);
   RVKD(capture_ops.allocate_sets, AllocateDescriptorSets);
   RVKD(capture_ops.update_sets, UpdateDescriptorSets);
   RVKD(capture_ops.create_sampler, CreateSampler);
   RVKD(capture_ops.destroy_sampler, DestroySampler);
   RVKD(capture_ops.create_pipeline_layout, CreatePipelineLayout);
   RVKD(capture_ops.destroy_pipeline_layout, DestroyPipelineLayout);
   RVKD(capture_ops.create_shader, CreateShaderModule);
   RVKD(capture_ops.destroy_shader, DestroyShaderModule);
   RVKD(capture_ops.create_pipelines, CreateComputePipelines);
   RVKD(capture_ops.destroy_pipeline, DestroyPipeline);
   RVKS(capture_ops.barrier);
   RVKD(capture_ops.bind_pipeline, CmdBindPipeline);
   RVKD(capture_ops.bind_sets, CmdBindDescriptorSets);
   RVKD(capture_ops.push, CmdPushConstants);
   RVKD(capture_ops.dispatch, CmdDispatch);
   RVKD(capture_ops.copy, CmdCopyBufferToImage);
   return true;
}
#undef RVKI
#undef RVKD
#undef RVKS


static bool rvk_rate(double fps, unsigned *num, unsigned *den)
{
   uint64_t h0 = 0, h1 = 1, k0 = 1, k1 = 0;
   double value = fps;
   unsigned i;
   if (fps != fps || fps < 1.0 || fps > 1000.0)
      return false;
   for (i = 0; i < 32; i++)
   {
      uint64_t a, h, k;
      double fraction;
      if (value > UINT_MAX)
         return false;
      a = (uint64_t)value;
      if ((h1 && a > (UINT_MAX - h0) / h1) || (k1 && a > (1000000u - k0) / k1))
         return false;
      h = a * h1 + h0;
      k = a * k1 + k0;
      if (!k || k > 1000000u)
         return false;
      if (fabs((double)h / k - fps) <= fps * 0.000000001)
      {
         *num = (unsigned)h;
         *den = (unsigned)k;
         return true;
      }
      h0 = h1;
      h1 = h;
      k0 = k1;
      k1 = k;
      fraction = value - (double)a;
      if (fraction <= 0.0)
         return false;
      value = 1.0 / fraction;
   }
   return false;
}
static bool rvk_config(const char *path, bool *audio, unsigned *qp, bool *hevc, unsigned *depth,
      bool streaming, bool *legacy, bool hdr_output)
{
   config_file_t *config;
   struct config_file_entry entry;
   char text[16];
   bool ok = false;
   *audio = true;
   *qp = 24;
   *hevc = hdr_output;
   *depth = hdr_output ? 10 : 8;
   *legacy = false;
   if (!path || !*path)
      return true;
   config = config_file_new_from_path_to_string(path);
   if (!config)
      return false;
   if ((config_get_entry(config, "audio_enable") && !config_get_bool(config, "audio_enable", audio))
         || (config_get_entry(config, "video_qp") && (!config_get_uint(config, "video_qp", qp) || *qp > 51))
         || (config_get_entry(config, "vcodec") && (!config_get_array(config, "vcodec", text, sizeof(text))
               || (!string_is_equal(text, "h264") && !string_is_equal(text, "hevc"))))
         || (config_get_entry(config, "format") && (!config_get_array(config, "format", text, sizeof(text))
               || !string_is_equal(text, streaming ? "flv" : "mp4"))))
      goto done;
   if (config_get_entry(config, "rtmp_legacy_commands")
         && (!streaming || !config_get_bool(config, "rtmp_legacy_commands", legacy)))
      goto done;
   if (config_get_entry(config, "vcodec"))
   {
      if (!config_get_array(config, "vcodec", text, sizeof(text)))
         goto done;
      *hevc = string_is_equal(text, "hevc");
      *depth = *hevc && hdr_output ? 10 : 8;
   }
   if (config_get_entry(config, "video_bit_depth")
         && (!config_get_uint(config, "video_bit_depth", depth)
            || (*depth != 8 && *depth != 10) || (!*hevc && *depth != 8)))
      goto done;
   if (config_get_entry_list_head(config, &entry))
      do
      {
         if (!string_is_equal(entry.key, "audio_enable") && !string_is_equal(entry.key, "video_qp")
               && !string_is_equal(entry.key, "vcodec") && !string_is_equal(entry.key, "format")
               && !string_is_equal(entry.key, "video_bit_depth")
               && !(streaming && string_is_equal(entry.key, "rtmp_legacy_commands")))
            goto done;
      } while (config_get_entry_list_next(&entry));
   ok = true;
done:
   config_file_free(config);
   return ok;
}
static bool rvk_layout(uint32_t layout, unsigned channels)
{
   if (!channels || channels > 6 || audio_layout_channels(layout) != channels)
      return false;
   switch (layout & ~AUDIO_SPEAKER_LOW_FREQUENCY)
   {
      case 0x004u:
      case 0x003u:
      case 0x007u:
      case 0x103u:
      case 0x107u:
      case 0x603u:
      case 0x607u:
         return true;
   }
   return false;
}
static void rvk_fail(record_vk_context_t *c)
{
   retro_atomic_store_release_int(&c->failed, 1);
   record_gpu_dispatch_vulkan_cancel(c->job);
}
static record_gpu_dispatch_vulkan_t *rvk_hevc_job(record_vk_context_t *c,
      const record_gpu_output_vulkan_config_t *output, unsigned depth, bool legacy)
{
   vulkan_encode_hevc_open_t encoder;
   vulkan_encode_hevc_ops_t ops;
   memset(&encoder, 0, sizeof(encoder));
   encoder.session.physical = c->encoder.session.physical;
   encoder.session.device = c->encoder.session.device;
   encoder.session.family = c->encoder.session.family;
   encoder.session.video.width = c->encoder.session.video.width;
   encoder.session.video.height = c->encoder.session.video.height;
   encoder.session.video.fps_num = c->encoder.session.video.fps_num;
   encoder.session.video.fps_den = c->encoder.session.video.fps_den;
   encoder.session.video.bitrate = c->encoder.session.video.bitrate;
   encoder.session.video.bit_depth = depth;
   encoder.session.video.color = c->output_mode ? VULKAN_ENCODE_HEVC_PQ : VULKAN_ENCODE_HEVC_BT709;
   encoder.session.video.ctb_size = 0;
   encoder.queue = c->encoder.queue;
   encoder.packet_size = c->encoder.packet_size;
   encoder.producer_family = c->encoder.producer_family;
   encoder.slots = c->encoder.slots;
   encoder.gop = c->encoder.gop;
   encoder.qp = c->encoder.qp;
   encoder.storage_image_extended_formats = c->encoder.storage_image_extended_formats;
   ops.session.families = c->ops.session.families;
   ops.session.memory = c->ops.session.memory;
   ops.session.capabilities = c->ops.session.capabilities;
   ops.session.formats = c->ops.session.formats;
   ops.session.create = c->ops.session.create;
   ops.session.destroy = c->ops.session.destroy;
   ops.session.requirements = c->ops.session.requirements;
   ops.session.bind = c->ops.session.bind;
   ops.session.allocate = c->ops.session.allocate;
   ops.session.free_memory = c->ops.session.free_memory;
   ops.session.parameters = c->ops.session.parameters;
   ops.session.destroy_parameters = c->ops.session.destroy_parameters;
   ops.session.headers = c->ops.session.headers;
   ops.submit.reset_command = c->ops.submit.reset_command;
   ops.submit.begin = c->ops.submit.begin;
   ops.submit.end = c->ops.submit.end;
   ops.submit.barrier = c->ops.submit.barrier;
   ops.submit.reset_query = c->ops.submit.reset_query;
   ops.submit.begin_query = c->ops.submit.begin_query;
   ops.submit.end_query = c->ops.submit.end_query;
   ops.submit.begin_coding = c->ops.submit.begin_coding;
   ops.submit.control = c->ops.submit.control;
   ops.submit.encode = c->ops.submit.encode;
   ops.submit.end_coding = c->ops.submit.end_coding;
   ops.submit.reset_fence = c->ops.submit.reset_fence;
   ops.submit.submit = c->ops.submit.submit;
   ops.submit.status = c->ops.submit.status;
   ops.submit.feedback = c->ops.submit.feedback;
   ops.submit.wait = c->ops.submit.wait;
   ops.pool = c->ops.pool;
   return c->streaming
      ? record_gpu_dispatch_vulkan_new_hevc_stream(&encoder, &ops, output, c->path, &c->capture_ops, legacy)
      : record_gpu_dispatch_vulkan_new_hevc(&encoder, &ops, output, c->path, &c->capture_ops);
}
static void *rvk_init(const struct record_params *params)
{
   record_gpu_output_vulkan_config_t output;
   record_vk_context_t *c;
   const record_vulkan_device_t *snapshot;
   video_device_info_t device;
   struct retro_hdr_metadata hdr;
   settings_t *settings = config_get_ptr();
   size_t length;
   unsigned num, den, qp, depth;
   bool audio, hevc, legacy;
   bool streaming = recording_state_get_ptr()->streaming_enable;
   if (!params || !settings || !params->filename || !*params->filename
         || record_vk_active || record_vk_retired)
      return NULL;
   if (streaming && (!record_vulkan_can_stream()
            || (strncmp(params->filename, "rtmp://", 7) && strncmp(params->filename, "rtmps://", 8))))
   {
      RARCH_ERR("[Vulkan recording] Streaming requires a native RTMP build and an RTMP/RTMPS URL.\n");
      return NULL;
   }
   memset(&hdr, 0, sizeof(hdr));
   hdr.version = 1;
   hdr.size = sizeof(hdr);
   if (!video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr))
      return NULL;
   if (!params->video_gpu_record || settings->bools.video_record_game_only || audio_driver_has_callback()
         || !rvk_rate(params->fps, &num, &den) || !rvk_config(params->config, &audio, &qp, &hevc, &depth, streaming, &legacy, hdr.output_mode != 0))
   {
      RARCH_ERR("[Vulkan recording] Requires viewport capture, synchronous audio and a native MP4/FLV profile.\n");
      return NULL;
   }
   if (!video_driver_get_device(&device) || device.api != VIDEO_DEVICE_VULKAN
         || !(device.recording_caps & VIDEO_DEVICE_CAP_VULKAN_ENCODE) || !device.recording)
   {
      RARCH_ERR("[Vulkan recording] Renderer has no dedicated native H.264 encode queue.\n");
      return NULL;
   }
   snapshot = (const record_vulkan_device_t*)device.recording;
   if (hevc && !snapshot->hevc)
   {
      RARCH_ERR("[Vulkan recording] Reserved encode queue has no enabled HEVC support.\n");
      return NULL;
   }
   if (snapshot->device != device.device || !snapshot->queue || !snapshot->get_proc)
      return NULL;
   if (hdr.output_mode > 2
         || (hdr.output_mode && (!hevc || depth != 10))
         || (!hdr.output_mode && settings->uints.video_swapchain_bit_depth == 2))
   {
      RARCH_ERR("[Vulkan recording] %s profile is incompatible with the display output; HDR requires HEVC 10-bit.\n", hevc ? "HEVC" : "H.264");
      return NULL;
   }
   if (!VIDEO_SCALE_W(params->out_dims) || !VIDEO_SCALE_H(params->out_dims)
         || VIDEO_SCALE_W(params->out_dims) > 4096 || VIDEO_SCALE_H(params->out_dims) > 4096
         || ((VIDEO_SCALE_W(params->out_dims) | VIDEO_SCALE_H(params->out_dims)) & 1))
      return NULL;
   memset(&output, 0, sizeof(output));
   output.video.codec = hevc ? RECORD_MP4_HEVC : RECORD_MP4_H264;
   output.video.max_packet_size = streaming ? (hevc ? 0xfffff7u : 0xfffffau) : 16 * 1024 * 1024;
   if (audio)
   {
      if ((params->samplerate != 32000.0 && params->samplerate != 44100.0 && params->samplerate != 48000.0)
            || !rvk_layout(recording_state_get_ptr()->layout, params->channels))
      {
         RARCH_ERR("[Vulkan recording] Audio requires an AC-3 speaker layout at 32, 44.1 or 48 kHz.\n");
         return NULL;
      }
      if (streaming && (params->samplerate == 32000.0 || params->channels > 2))
      {
         RARCH_ERR("[Vulkan recording] AAC streaming requires mono/stereo at 44.1 or 48 kHz.\n");
         return NULL;
      }
      output.audio_rate = (unsigned)params->samplerate;
      output.audio_layout = recording_state_get_ptr()->layout;
      output.audio_kbps = params->channels > 2 ? 448 : 192;
      output.audio_fifo_frames = output.audio_rate;
      output.audio_follow_video = true;
   }
   length = strlen(params->filename);
   if (length >= PATH_MAX_LENGTH)
      return NULL;
   c = (record_vk_context_t*)calloc(1, sizeof(*c) + length + 1);
   if (!c)
      return NULL;
   c->path = (char*)(c + 1);
   memcpy(c->path, params->filename, length + 1);
   c->device = snapshot->device;
   c->instance = snapshot->instance;
   c->instance_proc = snapshot->get_proc;
   c->api_version = snapshot->api_version;
   c->device_proc = (PFN_vkGetDeviceProcAddr)c->instance_proc(c->instance, "vkGetDeviceProcAddr");
   if (!c->device_proc || !rvk_load_ops(c) || !rvk_load_capture(c))
      goto failed;
   c->counter = (PFN_vkGetSemaphoreCounterValue)c->device_proc(c->device,
         c->api_version < VK_API_VERSION_1_2 ? "vkGetSemaphoreCounterValueKHR" : "vkGetSemaphoreCounterValue");
   if (!c->counter)
      goto failed;
   c->encoder.session.physical = snapshot->physical;
   c->encoder.session.device = snapshot->device;
   c->encoder.session.family = snapshot->family;
   c->encoder.session.video.width = VIDEO_SCALE_W(params->out_dims);
   c->encoder.session.video.height = VIDEO_SCALE_H(params->out_dims);
   c->encoder.session.video.fps_num = num;
   c->encoder.session.video.fps_den = den;
   c->encoder.session.video.bitrate = 16000000;
   c->encoder.queue = snapshot->queue;
   c->encoder.packet_size = output.video.max_packet_size;
   c->encoder.producer_family = snapshot->producer_family;
   c->encoder.slots = recording_native_buffer_count("vulkan", params->video_fifo_frames);
   c->encoder.gop = (unsigned)(((uint64_t)num * 2 + den - 1) / den);
   c->encoder.qp = qp;
   c->encoder.storage_image_extended_formats = snapshot->storage_images;
   c->dims = params->out_dims;
   c->output_mode = hdr.output_mode;
   c->duration = den;
   c->audio_enabled = audio;
   c->audio_rate = output.audio_rate;
   c->allow_drop = params->allow_frame_drop;
   c->streaming = streaming;
   retro_atomic_int_init(&c->failed, 0);
   retro_atomic_int_init(&c->stop, 0);
   retro_atomic_int_init(&c->native_error, 0);
   c->job = hevc ? rvk_hevc_job(c, &output, depth, legacy)
      : streaming ? record_gpu_dispatch_vulkan_new_stream(&c->encoder, &c->ops, &output, c->path, &c->capture_ops, legacy)
      : record_gpu_dispatch_vulkan_new_capture(&c->encoder, &c->ops, &output, c->path, &c->capture_ops);
   if (!c->job)
      goto failed;
   c->renderer = record_gpu_renderer_vulkan_new(c->job, den, c->encoder.gop, c->counter);
   if (!c->renderer)
   {
      rvk_fail(c);
      record_gpu_dispatch_vulkan_audio_finish(c->job);
      c->next = record_vk_retired;
      record_vk_retired = c;
      return NULL;
   }
   record_vk_active = c;
   RARCH_LOG("[Vulkan recording] Starting native %s%s at %u/%u fps %s.\n",
         hevc ? "HEVC" : "H.264", audio ? (streaming ? "/AAC" : "/AC-3") : "",
         num, den, streaming ? "streaming" : "recording");
   return c;
failed:
   free(c);
   return NULL;
}
static bool rvk_finalize(void *data)
{
   record_vk_context_t *c = (record_vk_context_t*)data;
   retro_atomic_cas_int(&c->stop, 0, 2);
   record_gpu_dispatch_vulkan_audio_finish(c->job);
   return !retro_atomic_load_acquire_int(&c->failed);
}
static void rvk_release(void *data)
{
   record_vk_context_t *c = (record_vk_context_t*)data;
   record_vk_active = NULL;
   c->next = record_vk_retired;
   record_vk_retired = c;
}
static bool rvk_audio(void *data, const struct record_audio_data *audio)
{
   record_vk_context_t *c = (record_vk_context_t*)data;
   if (!c->audio_enabled || !c->armed || (audio && !audio->frames))
      return true;
   if (retro_atomic_load_acquire_int(&c->stop) || retro_atomic_load_acquire_int(&c->failed))
      return false;
   if (!audio || video_state_get_ptr()->av_info.timing.sample_rate != c->audio_rate
         || c->audio_frames > (uint64_t)-1 - audio->frames
         || record_gpu_dispatch_vulkan_audio(c->job, (const int16_t*)audio->data, audio->frames) != RECORD_VULKAN_OUTPUT_PROGRESS)
   {
      rvk_fail(c);
      return false;
   }
   c->audio_frames += audio->frames;
   return true;
}
const record_driver_t record_vulkan = {
   rvk_init,
   rvk_release,
   NULL,
   rvk_audio,
   rvk_finalize,
   "vulkan"
};
bool record_vulkan_frame_info(struct video_frame_info *info, bool capture)
{
   record_vk_context_t *c = record_vk_active;
   if (!c && !record_vk_retired)
      return false;
   info->recording_gpu = c;
   info->recording_gpu_pending = record_vk_retired != NULL;
   info->recording_gpu_capture = false;
   info->recording_gpu_timestamp = 0;
   if (!c || !capture || !c->armed || recording_state_get_ptr()->data != c || video_state_get_ptr()->frame_repeat)
      return true;
   if (c->timestamp > (uint64_t)-1 - c->duration)
   {
      rvk_fail(c);
      return true;
   }
   info->recording_gpu_timestamp = c->timestamp;
   c->timestamp += c->duration;
   info->recording_gpu_capture = true;
   return true;
}
bool record_vulkan_gpu_capture(void *data, VkCommandBuffer command,
      const vulkan_encode_capture_source_t *source, uint64_t timestamp,
      unsigned dims, unsigned output_mode)
{
   record_vk_context_t *c = (record_vk_context_t*)data;
   int stop, result;
   if (!c)
      return false;
   stop = retro_atomic_load_acquire_int(&c->stop);
   if ((stop && stop != 2) || retro_atomic_load_acquire_int(&c->failed))
      return false;
   if (dims != c->dims || output_mode != c->output_mode)
   {
      retro_atomic_cas_int(&c->stop, 0, 1);
      return false;
   }
   if (!source || source->output_mode != output_mode)
   {
      rvk_fail(c);
      return false;
   }
   if (c->have_offered && timestamp == c->offered)
      return false;
   c->have_offered = true;
   c->offered = timestamp;
   result = record_gpu_renderer_vulkan_record(c->renderer, command, source, timestamp);
   if (result == RECORD_GPU_QUEUE_OK)
      return true;
   if (result == RECORD_GPU_QUEUE_ERROR || !c->allow_drop)
      rvk_fail(c);
   return false;
}
const VkSubmitInfo *record_vulkan_gpu_submission(void *data, const VkSubmitInfo *submit)
{
   record_vk_context_t *c = (record_vk_context_t*)data;
   return c ? record_gpu_renderer_vulkan_submission(c->renderer, submit) : NULL;
}
void record_vulkan_gpu_submitted(void *data, VkResult result)
{
   record_vk_context_t *c = (record_vk_context_t*)data;
   if (!c)
      return;
   if (result != VK_SUCCESS)
      retro_atomic_cas_int(&c->native_error, 0, (int)result);
   if (record_gpu_renderer_vulkan_submitted(c->renderer, result))
      c->pictures++;
   else
      rvk_fail(c);
   if (result == VK_ERROR_DEVICE_LOST)
      record_gpu_renderer_vulkan_abandon_lost(c->renderer, result);
}
void record_vulkan_gpu_discard(void *data)
{
   record_vk_context_t *c = (record_vk_context_t*)data;
   if (c)
   {
      record_gpu_renderer_vulkan_discard(c->renderer);
      rvk_fail(c);
   }
}
void record_vulkan_gpu_reconfigure(void *data)
{
   record_vk_context_t *c = (record_vk_context_t*)data;
   if (c)
      retro_atomic_cas_int(&c->stop, 0, 1);
}
void record_vulkan_stop(void)
{
   if (record_vk_active || record_vk_start_requested)
      command_event(CMD_EVENT_RECORD_DEINIT, NULL);
}
bool record_vulkan_pending(void)
{
   return record_vk_active || record_vk_retired || record_vk_start_requested;
}
bool record_vulkan_defer_start(void)
{
   record_vk_start_requested = true;
   return true;
}
bool record_vulkan_cancel_start(void)
{
   bool requested = record_vk_start_requested;
   record_vk_start_requested = false;
   return requested;
}
void record_vulkan_poll(void)
{
   record_vk_context_t **link = &record_vk_retired;
   video_driver_state_t *video = video_state_get_ptr();
   if (record_vk_start_requested)
   {
      struct video_viewport vp;
      vp.pos = vp.dims = vp.full_dims = 0;
      if (!recording_state_get_ptr()->enable)
         record_vk_start_requested = false;
      else if (video_driver_get_viewport_info(&vp) && VIDEO_SCALE_W(vp.dims) && VIDEO_SCALE_H(vp.dims))
      {
         record_vk_start_requested = false;
         command_event(CMD_EVENT_RECORD_INIT, NULL);
      }
   }
   if (record_vk_active)
   {
      record_vk_context_t *c = record_vk_active;
      video_device_info_t device;
      struct retro_hdr_metadata hdr;
      enum record_gpu_dispatch_vulkan_state state = record_gpu_dispatch_vulkan_status(c->job, NULL, NULL);
      if (state == RECORD_VKD_READY)
         c->armed = true;
      if (!video_driver_get_device(&device) || device.device != c->device)
         retro_atomic_cas_int(&c->stop, 0, 3);
      memset(&hdr, 0, sizeof(hdr));
      hdr.version = 1;
      hdr.size = sizeof(hdr);
      if (!video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_HDR_METADATA, &hdr)
            || hdr.output_mode != c->output_mode)
         retro_atomic_cas_int(&c->stop, 0, 4);
      if (state >= RECORD_VKD_DRAINING || retro_atomic_load_acquire_int(&c->stop)
            || retro_atomic_load_acquire_int(&c->failed))
         record_vulkan_stop();
   }
   while (*link)
   {
      record_vk_context_t *c = *link;
      enum record_gpu_dispatch_vulkan_state state;
      enum record_gpu_session_vulkan_error error;
      int file_error;
      if (!c->video_closed)
      {
         int result;
         if (video_driver_thread_wrapper_active() && video_thread_record_gpu_in_flight(video->data, c))
         {
            link = &c->next;
            continue;
         }
         if (!c->pictures)
            record_gpu_dispatch_vulkan_cancel(c->job);
         result = c->renderer ? record_gpu_renderer_vulkan_finish(c->renderer, false)
            : record_gpu_dispatch_vulkan_video_finish(c->job) ? RECORD_GPU_QUEUE_OK : RECORD_GPU_QUEUE_BUSY;
         if (result == RECORD_VKR_DEVICE_LOST)
         {
            rvk_fail(c);
            retro_atomic_cas_int(&c->native_error, 0, (int)VK_ERROR_DEVICE_LOST);
            if (record_gpu_renderer_vulkan_abandon_lost(c->renderer, VK_ERROR_DEVICE_LOST))
               result = RECORD_GPU_QUEUE_OK;
         }
         if (result != RECORD_GPU_QUEUE_OK)
         {
            link = &c->next;
            continue;
         }
         c->video_closed = true;
         record_gpu_renderer_vulkan_free(c->renderer);
         c->renderer = NULL;
      }
      state = record_gpu_dispatch_vulkan_status(c->job, &error, &file_error);
      if (state == RECORD_VKD_DONE || state == RECORD_VKD_FAILED)
         record_gpu_dispatch_vulkan_audio_frames(c->job, &c->audio_frames);
      if ((state != RECORD_VKD_DONE && state != RECORD_VKD_FAILED) || !record_gpu_dispatch_vulkan_free(c->job))
      {
         link = &c->next;
         continue;
      }
      if (state == RECORD_VKD_DONE && c->pictures && !retro_atomic_load_acquire_int(&c->failed))
      {
         RARCH_LOG("[Vulkan recording] Recording finalized (%llu GPU frames).\n", (unsigned long long)c->pictures);
         if (c->audio_enabled)
            RARCH_LOG("[Vulkan recording] Audio finalized (%llu PCM frames).\n", (unsigned long long)c->audio_frames);
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV) || defined(HAVE_WEBMPLAYER)
         if (!c->streaming && config_get_ptr()->bools.history_list_enable)
         {
            struct playlist_entry entry = {0};
            entry.path = c->path;
            entry.core_path = (char*)"builtin";
            entry.core_name = (char*)"movieplayer";
            command_playlist_push_write(g_defaults.video_history, &entry);
         }
#endif
      }
      else
         RARCH_ERR("[Vulkan recording] Recording failed (session %u, file %d, native %d).\n",
               (unsigned)error, file_error, retro_atomic_load_acquire_int(&c->native_error));
      *link = c->next;
      free(c);
   }
}
#endif

#ifdef HAVE_RECORD_VULKAN
bool record_vulkan_can_stream(void)
{
#if defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX) \
      && defined(HAVE_THREADS) && defined(HAVE_NETWORKING) && defined(HAVE_MF_AUDIO) \
      && defined(HAVE_RTMP) && defined(HAVE_RFLV) && defined(HAVE_CRYPTO)
   return true;
#else
   return false;
#endif
}
#endif
