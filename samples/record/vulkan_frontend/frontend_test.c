/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <file/config_file.h>
#include "../../../record/drivers/record_vulkan.h"
#include "../../../record/record_gpu_dispatch_vulkan_hevc.h"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/video_thread_wrapper.h"
#include "../../../audio/audio_driver.h"
#include "../../../audio/audio_upmix.h"
#include "../../../configuration.h"
#include "../../../command.h"
struct record_gpu_dispatch_vulkan
{
   enum record_gpu_dispatch_vulkan_state state;
   bool video_closed, audio_closed, cancelled;
};
struct record_gpu_renderer_vulkan
{
   bool closed;
};
static struct record_gpu_dispatch_vulkan job;
static struct record_gpu_renderer_vulkan renderer;
static settings_t settings;
static recording_state_t recording;
static video_driver_state_t video;
static struct retro_hdr_metadata hdr;
static record_vulkan_device_t device;
static vulkan_encode_h264_open_t offered;
static vulkan_encode_hevc_open_t hevc_offered;
static record_gpu_output_vulkan_config_t output;
static const char *profile = "", *missing;
static unsigned starts, finishes, frees, records, commits, audio_calls, deinit_calls, init_calls;
static int finish_result, frame_result, audio_result;
static bool asynchronous, wrapper, in_flight, renderer_fail;
static unsigned stream_requests;
static bool stream_legacy;
settings_t *config_get_ptr(void)
{
   return &settings;
}
recording_state_t *recording_state_get_ptr(void)
{
   return &recording;
}
video_driver_state_t *video_state_get_ptr(void)
{
   return &video;
}
bool audio_driver_has_callback(void)
{
   return asynchronous;
}
unsigned audio_layout_channels(uint32_t layout)
{
   unsigned count = 0;
   for (; layout; layout >>= 1)
      count += layout & 1;
   return count;
}
void RARCH_LOG(const char *fmt, ...)
{
   (void)fmt;
}
void RARCH_ERR(const char *fmt, ...)
{
   (void)fmt;
}
bool video_driver_get_device(video_device_info_t *info)
{
   memset(info, 0, sizeof(*info));
   info->api = VIDEO_DEVICE_VULKAN;
   info->device = device.device;
   info->recording = &device;
   info->recording_caps = VIDEO_DEVICE_CAP_VULKAN_ENCODE;
   return true;
}
bool video_driver_get_display_metadata(unsigned command, void *data)
{
   assert(command == RETRO_ENVIRONMENT_GET_HDR_METADATA);
   *(struct retro_hdr_metadata*)data = hdr;
   return true;
}
bool video_driver_get_viewport_info(struct video_viewport *vp)
{
   vp->pos = 0;
   vp->dims = vp->full_dims = VIDEO_SCALE_PACK(320, 192);
   return true;
}
bool video_driver_thread_wrapper_active(void)
{
   return wrapper;
}
bool video_thread_record_gpu_in_flight(void *data, const void *recorder)
{
   (void)data;
   assert(recorder);
   return in_flight;
}
static VKAPI_ATTR void VKAPI_CALL unused_proc(void) { }
static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get_device_proc(VkDevice d, const char *name)
{
   assert(d == device.device);
   return missing && !strcmp(name, missing) ? NULL : unused_proc;
}
static VKAPI_ATTR VkResult VKAPI_CALL hevc_capabilities(VkPhysicalDevice physical,
      const VkVideoProfileInfoKHR *profile, VkVideoCapabilitiesKHR *caps)
{
   (void)physical;
   (void)profile;
   (void)caps;
   assert(!"HEVC capabilities must be queried on the I/O worker");
   return VK_ERROR_FORMAT_NOT_SUPPORTED;
}
static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL get_instance_proc(VkInstance i, const char *name)
{
   assert(i == device.instance);
   if (!strcmp(name, "vkGetDeviceProcAddr"))
      return (PFN_vkVoidFunction)get_device_proc;
   if (!strcmp(name, "vkGetPhysicalDeviceVideoCapabilitiesKHR"))
      return (PFN_vkVoidFunction)hevc_capabilities;
   return unused_proc;
}
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_capture(
      const vulkan_encode_h264_open_t *encoder, const vulkan_encode_h264_ops_t *ops,
      const record_gpu_output_vulkan_config_t *mux, const char *path,
      const vulkan_encode_capture_ops_t *capture)
{
   assert(ops && ops->submit.submit && capture && capture->dispatch && path && *path);
   starts++;
   offered = *encoder;
   output = *mux;
   job.state = RECORD_VKD_STARTING;
   return &job;
}
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_hevc(
      const vulkan_encode_hevc_open_t *encoder, const vulkan_encode_hevc_ops_t *ops,
      const record_gpu_output_vulkan_config_t *mux, const char *path,
      const vulkan_encode_capture_ops_t *capture)
{
   assert(ops && ops->submit.submit && capture && capture->dispatch && path && *path);
   hevc_offered = *encoder;
   starts++;
   output = *mux;
   job.state = RECORD_VKD_STARTING;
   return &job;
}
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_stream(
      const vulkan_encode_h264_open_t *e, const vulkan_encode_h264_ops_t *ops,
      const record_gpu_output_vulkan_config_t *mux, const char *url,
      const vulkan_encode_capture_ops_t *capture, bool legacy)
{
   assert(mux->video.codec == RECORD_MP4_H264 && mux->video.max_packet_size == 0xfffffa);
   assert(!strncmp(url, "rtmp", 4));
   stream_requests++;
   stream_legacy = legacy;
   return record_gpu_dispatch_vulkan_new_capture(e, ops, mux, url, capture);
}
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_hevc_stream(
      const vulkan_encode_hevc_open_t *e, const vulkan_encode_hevc_ops_t *ops,
      const record_gpu_output_vulkan_config_t *mux, const char *url,
      const vulkan_encode_capture_ops_t *capture, bool legacy)
{
   assert(mux->video.codec == RECORD_MP4_HEVC && mux->video.max_packet_size == 0xfffff7);
   assert(!strncmp(url, "rtmp", 4));
   stream_requests++;
   stream_legacy = legacy;
   return record_gpu_dispatch_vulkan_new_hevc(e, ops, mux, url, capture);
}

record_gpu_renderer_vulkan_t *record_gpu_renderer_vulkan_new(record_gpu_dispatch_vulkan_t *j,
      unsigned duration, unsigned gop, PFN_vkGetSemaphoreCounterValue counter)
{
   assert(j == &job && duration == 1001 && gop == 120 && counter);
   return renderer_fail ? NULL : &renderer;
}
int record_gpu_renderer_vulkan_record(record_gpu_renderer_vulkan_t *r,
      VkCommandBuffer command, const vulkan_encode_capture_source_t *source, uint64_t timestamp)
{
   assert(r == &renderer && command && source);
   (void)timestamp;
   records++;
   return frame_result;
}
const VkSubmitInfo *record_gpu_renderer_vulkan_submission(record_gpu_renderer_vulkan_t *r, const VkSubmitInfo *submit)
{
   assert(r == &renderer);
   return submit;
}
bool record_gpu_renderer_vulkan_submitted(record_gpu_renderer_vulkan_t *r, VkResult result)
{
   assert(r == &renderer);
   commits++;
   return result == VK_SUCCESS;
}
bool record_gpu_renderer_vulkan_discard(record_gpu_renderer_vulkan_t *r)
{
   assert(r == &renderer);
   return true;
}
int record_gpu_renderer_vulkan_finish(record_gpu_renderer_vulkan_t *r, bool quiesced)
{
   assert(r == &renderer && !quiesced);
   finishes++;
   if (finish_result == RECORD_GPU_QUEUE_OK)
      job.video_closed = renderer.closed = true;
   return finish_result;
}
bool record_gpu_renderer_vulkan_abandon_lost(record_gpu_renderer_vulkan_t *r, VkResult result)
{
   assert(r == &renderer && result == VK_ERROR_DEVICE_LOST);
   job.cancelled = job.video_closed = renderer.closed = true;
   return true;
}
bool record_gpu_renderer_vulkan_free(record_gpu_renderer_vulkan_t *r)
{
   assert(!r || (r == &renderer && r->closed));
   return true;
}
int record_gpu_dispatch_vulkan_audio(record_gpu_dispatch_vulkan_t *j, const int16_t *samples, size_t frames)
{
   assert(j == &job && samples && frames);
   audio_calls++;
   return audio_result;
}
bool record_gpu_dispatch_vulkan_audio_finish(record_gpu_dispatch_vulkan_t *j)
{
   assert(j == &job);
   j->audio_closed = true;
   return true;
}
bool record_gpu_dispatch_vulkan_video_finish(record_gpu_dispatch_vulkan_t *j)
{
   assert(j == &job);
   j->video_closed = true;
   return true;
}
void record_gpu_dispatch_vulkan_cancel(record_gpu_dispatch_vulkan_t *j)
{
   assert(j == &job);
   j->cancelled = true;
}
enum record_gpu_dispatch_vulkan_state record_gpu_dispatch_vulkan_status(
      const record_gpu_dispatch_vulkan_t *j, enum record_gpu_session_vulkan_error *error, int *file_error)
{
   assert(j == &job);
   if (error) *error = j->cancelled ? RECORD_VULKAN_SESSION_CANCELLED : RECORD_VULKAN_SESSION_NONE;
   if (file_error) *file_error = 0;
   return j->video_closed && j->audio_closed ? j->cancelled ? RECORD_VKD_FAILED : RECORD_VKD_DONE : j->state;
}
bool record_gpu_dispatch_vulkan_audio_frames(const record_gpu_dispatch_vulkan_t *j, uint64_t *frames)
{
   (void)j;
   *frames = 8;
   return true;
}
bool record_gpu_dispatch_vulkan_free(record_gpu_dispatch_vulkan_t *j)
{
   assert(j == &job && j->video_closed && j->audio_closed);
   frees++;
   return true;
}
static config_file_t *profile_new(const char *path)
{
   size_t length = strlen(profile);
   char *owned = (char*)malloc(length + 1);
   assert(path && owned);
   memcpy(owned, profile, length + 1);
   return config_file_new_take_string(owned, length, NULL);
}
#define config_file_new_from_path_to_string profile_new
#include "../../../record/drivers/record_vulkan.c"
#undef config_file_new_from_path_to_string
bool command_event(enum event_command action, void *data)
{
   (void)data;
   if (action == CMD_EVENT_RECORD_INIT)
      init_calls++;
   else
   {
      assert(action == CMD_EVENT_RECORD_DEINIT);
      deinit_calls++;
      if (record_vk_active)
      {
         rvk_finalize(record_vk_active);
         rvk_release(record_vk_active);
         recording.data = NULL;
      }
   }
   return true;
}
static void reset(struct record_params *params)
{
   assert(!record_vk_active && !record_vk_retired && !record_vk_start_requested);
   memset(params, 0, sizeof(*params));
   memset(&settings, 0, sizeof(settings));
   memset(&recording, 0, sizeof(recording));
   memset(&video, 0, sizeof(video));
   memset(&device, 0, sizeof(device));
   memset(&hdr, 0, sizeof(hdr));
   memset(&job, 0, sizeof(job));
   memset(&renderer, 0, sizeof(renderer));
   device.instance = (VkInstance)(uintptr_t)1;
   device.device = (VkDevice)(uintptr_t)2;
   device.physical = (VkPhysicalDevice)(uintptr_t)3;
   device.queue = (VkQueue)(uintptr_t)4;
   device.get_proc = get_instance_proc;
   device.api_version = VK_API_VERSION_1_1;
   params->filename = "test.mp4";
   params->config = "profile";
   params->video_gpu_record = true;
   params->out_dims = VIDEO_SCALE_PACK(320, 192);
   params->fps = 60000.0 / 1001.0;
   params->samplerate = video.av_info.timing.sample_rate = 48000;
   params->channels = 2;
   params->allow_frame_drop = true;
   recording.layout = AUDIO_LAYOUT_STEREO;
   recording.enable = true;
   hdr.version = 1;
   hdr.size = sizeof(hdr);
   profile = "audio_enable = true\nvcodec = h264\nformat = mp4\nvideo_qp = 24\n";
   missing = NULL;
   starts = finishes = frees = records = commits = audio_calls = deinit_calls = init_calls = 0;
   asynchronous = wrapper = in_flight = renderer_fail = stream_legacy = false;
   stream_requests = 0;
   finish_result = frame_result = RECORD_GPU_QUEUE_OK;
   audio_result = RECORD_VULKAN_OUTPUT_PROGRESS;
}
static record_vk_context_t *start(struct record_params *params)
{
   record_vk_context_t *c = (record_vk_context_t*)record_vulkan.init(params);
   assert(c && record_vk_active == c && record_vulkan_pending());
   recording.data = c;
   recording.driver = &record_vulkan;
   assert(offered.session.video.fps_num == 60000 && offered.session.video.fps_den == 1001);
   assert(offered.slots == 4 && offered.qp == 24 && output.audio_rate == 48000);
   return c;
}
static void close_recording(void)
{
   record_vulkan_stop();
   record_vulkan_poll();
   assert(!record_vulkan_pending() && frees == 1);
}
static void admission(void)
{
   struct record_params params;
   record_vk_context_t *c;
   static const char *bad[] = {"vcodec = hevc\n", "format = mkv\n", "video_qp = 52\n", "unknown = true\n"};
   unsigned i;
   for (i = 0; i < sizeof(bad) / sizeof(*bad); i++)
   {
      reset(&params); profile = bad[i];
      assert(!record_vulkan.init(&params) && !starts);
   }
   reset(&params); asynchronous = true;
   assert(!record_vulkan.init(&params) && !starts);
   reset(&params); hdr.output_mode = 1;
   assert(!record_vulkan.init(&params) && !starts);
   reset(&params); params.samplerate = 22050;
   assert(!record_vulkan.init(&params) && !starts);
   reset(&params); recording.layout = 0x33; params.channels = 4;
   assert(!record_vulkan.init(&params) && !starts);
   reset(&params); missing = "vkCreateSampler";
   assert(!record_vulkan.init(&params) && !starts);
   reset(&params); recording.streaming_enable = true;
   assert(!record_vulkan.init(&params) && !starts);
   reset(&params); renderer_fail = true;
   assert(!record_vulkan.init(&params) && starts == 1 && record_vulkan_pending());
   record_vulkan_poll();
   assert(!record_vulkan_pending() && frees == 1 && job.cancelled);
   reset(&params); c = start(&params); (void)c;
   close_recording();
}
static void lifecycle(void)
{
   struct record_params params;
   record_vk_context_t *c;
   video_frame_info_t *info = (video_frame_info_t*)calloc(1, sizeof(*info));
   vulkan_encode_capture_source_t source;
   struct record_audio_data audio;
   static const int16_t pcm[16] = {0};
   unsigned before;
   assert(info);
   memset(&source, 0, sizeof(source));
   memset(&audio, 0, sizeof(audio));
   audio.data = pcm;
   audio.frames = 8;
   reset(&params); c = start(&params);
   assert(record_vulkan_frame_info(info, true) && !info->recording_gpu_capture);
   assert(rvk_audio(c, &audio) && !audio_calls);
   job.state = RECORD_VKD_READY;
   record_vulkan_poll();
   assert(record_vulkan_frame_info(info, true) && info->recording_gpu == c && info->recording_gpu_capture);
   assert(info->recording_gpu_timestamp == 0 && c->timestamp == 1001);
   assert(record_vulkan_gpu_capture(c, (VkCommandBuffer)(uintptr_t)5, &source, 0, c->dims, 0));
   assert(!record_vulkan_gpu_capture(c, (VkCommandBuffer)(uintptr_t)5, &source, 0, c->dims, 0) && records == 1);
   record_vulkan_gpu_submitted(c, VK_SUCCESS);
   assert(c->pictures == 1 && commits == 1);
   assert(rvk_audio(c, &audio) && audio_calls == 1 && c->audio_frames == 8);
   wrapper = in_flight = true;
   record_vulkan_gpu_reconfigure(c);
   assert(!rvk_audio(c, &audio) && audio_calls == 1 && c->audio_frames == 8);
   assert(!record_vulkan_gpu_capture(c, (VkCommandBuffer)(uintptr_t)5, &source, 1, c->dims, 0));
   record_vulkan_stop();
   record_vulkan_poll();
   assert(record_vulkan_pending() && !finishes && !frees);
   in_flight = false;
   finish_result = RECORD_GPU_QUEUE_BUSY;
   record_vulkan_poll();
   assert(record_vulkan_pending() && !frees);
   before = finishes;
   finish_result = RECORD_VKR_DEVICE_LOST;
   record_vulkan_poll();
   assert(!record_vulkan_pending() && finishes == before + 1 && frees == 1 && job.cancelled);
   reset(&params);
   assert(record_vulkan_defer_start());
   record_vulkan_poll();
   assert(init_calls == 1 && !record_vk_start_requested);
   free(info);
}
static void hevc_admission(void)
{
   struct record_params params;
   record_vk_context_t *c;
   unsigned depth;
   for (depth = 8; depth <= 10; depth += 2)
   {
      reset(&params);
      device.hevc = true;
      profile = depth == 8 ? "vcodec = hevc\nvideo_bit_depth = 8\n"
         : "vcodec = hevc\nvideo_bit_depth = 10\n";
      c = (record_vk_context_t*)record_vulkan.init(&params);
      assert(c && starts == 1 && hevc_offered.session.video.bit_depth == depth);
      assert(hevc_offered.session.video.color == VULKAN_ENCODE_HEVC_BT709);
      assert(!hevc_offered.session.video.ctb_size && hevc_offered.gop == 120);
      assert(hevc_offered.session.video.fps_num == 60000 && hevc_offered.session.video.fps_den == 1001);
      recording.data = c;
      recording.driver = &record_vulkan;
      close_recording();
   }
   reset(&params); device.hevc = true; profile = "vcodec = hevc\nvideo_bit_depth = 9\n";
   assert(!record_vulkan.init(&params) && !starts);
   reset(&params); device.hevc = true; profile = "vcodec = h264\nvideo_bit_depth = 10\n";
   assert(!record_vulkan.init(&params) && !starts);
   reset(&params); device.hevc = true; profile = "vcodec = hevc\n"; hdr.output_mode = 1;
   assert(!record_vulkan.init(&params) && !starts);
}
static void hdr_lifecycle(void)
{
   struct record_params params;
   record_vk_context_t *c;
   vulkan_encode_capture_source_t source;
   unsigned mode;
   for (mode = 1; mode <= 2; mode++)
   {
      reset(&params);
      device.hevc = true;
      profile = "vcodec = hevc\nvideo_bit_depth = 10\n";
      hdr.output_mode = mode;
      c = (record_vk_context_t*)record_vulkan.init(&params);
      assert(c && starts == 1 && c->output_mode == mode);
      assert(hevc_offered.session.video.color == VULKAN_ENCODE_HEVC_PQ);
      recording.data = c;
      recording.driver = &record_vulkan;
      job.state = RECORD_VKD_READY;
      record_vulkan_poll();
      assert(record_vk_active == c && !retro_atomic_load_acquire_int(&c->stop));
      memset(&source, 0, sizeof(source));
      source.output_mode = mode;
      source.format = mode == 1 ? VK_FORMAT_A2B10G10R10_UNORM_PACK32 : VK_FORMAT_R16G16B16A16_SFLOAT;
      assert(record_vulkan_gpu_capture(c, (VkCommandBuffer)(uintptr_t)5, &source, 0, c->dims, mode));
      record_vulkan_gpu_submitted(c, VK_SUCCESS);
      assert(!record_vulkan_gpu_capture(c, (VkCommandBuffer)(uintptr_t)5, &source, 1, c->dims, 0));
      assert(retro_atomic_load_acquire_int(&c->stop));
      close_recording();
   }
   reset(&params);
   device.hevc = true;
   profile = "vcodec = hevc\nvideo_bit_depth = 10\n";
   hdr.output_mode = 1;
   c = (record_vk_context_t*)record_vulkan.init(&params);
   assert(c);
   recording.data = c;
   recording.driver = &record_vulkan;
   job.state = RECORD_VKD_READY;
   record_vulkan_poll();
   hdr.output_mode = 2;
   record_vulkan_poll();
   assert(!record_vulkan_pending() && frees == 1 && job.cancelled);
   reset(&params);
   device.hevc = true;
   profile = "vcodec = hevc\nvideo_bit_depth = 10\n";
   hdr.output_mode = 3;
   assert(!record_vulkan.init(&params) && !starts);
}
static void stream_admission(void)
{
   struct record_params params;
   record_vk_context_t *c;
   unsigned codec;
   reset(&params);
   profile = "rtmp_legacy_commands = true\n";
   assert(!record_vulkan.init(&params) && !starts);
   for (codec = 0; codec < 3; codec++)
   {
      reset(&params);
      recording.streaming_enable = true; params.filename = "rtmps://localhost/live/test";
      device.hevc = true;
      profile = codec == 0 ? "audio_enable = true\nvcodec = h264\nformat = flv\n"
         : codec == 1 ? "audio_enable = true\nvcodec = hevc\nformat = flv\nvideo_bit_depth = 10\nrtmp_legacy_commands = true\n"
         : "audio_enable = false\nvcodec = hevc\nformat = flv\n";
      if (!record_vulkan_can_stream())
      {
         assert(!record_vulkan.init(&params) && !starts);
         continue;
      }
      c = (record_vk_context_t*)record_vulkan.init(&params);
      assert(c && stream_requests == 1 && stream_legacy == (codec == 1));
      assert(c->streaming && c->audio_enabled == (codec != 2));
      if (codec != 2) assert(output.audio_follow_video && output.audio_rate == 48000);
      close_recording();
   }
   reset(&params); recording.streaming_enable = true; params.filename = "rtmp://localhost/live/test";
   params.samplerate = 32000; profile = "format = flv\n";
   assert(!record_vulkan.init(&params) && !starts);
   reset(&params); recording.streaming_enable = true; params.filename = "rtmp://localhost/live/test";
   params.channels = 6; recording.layout = 0x607; profile = "format = flv\n";
   assert(!record_vulkan.init(&params) && !starts);
   reset(&params); recording.streaming_enable = true; params.filename = "rtmp://localhost/live/test";
   profile = "format = mp4\n";
   assert(!record_vulkan.init(&params) && !starts);
}

int main(void)
{
   admission();
   stream_admission();
   hevc_admission();
   hdr_lifecycle();
   lifecycle();
   puts("Vulkan frontend admission, PCM, frame snapshots and asynchronous retirement PASS");
   return 0;
}
