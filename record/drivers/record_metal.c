/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif
#include "record_metal.h"
#include "../../gfx/video_driver.h"
#ifdef HAVE_RECORD_METAL
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <file/config_file.h>
#include <string/stdstring.h>
#include <retro_atomic.h>
#include "../record_gpu_dispatch_metal.h"
#include "../../gfx/video_thread_wrapper.h"
#include "../../audio/audio_driver.h"
#include "../../audio/audio_upmix.h"
#include "../../configuration.h"
#include "../../command.h"
#include "../../retroarch.h"
#include "../../runloop.h"
#include "../../defaults.h"
#include "../../playlist.h"
#include "../../verbosity.h"
typedef struct record_metal_context
{
   uint64_t next_timestamp,origin,captured,audio_frames,last_offered;
   struct record_metal_context *next;
   record_gpu_dispatch_metal_t *job;
   void *device;
   char *path;
   unsigned dims,duration,audio_rate,output_mode;
   retro_atomic_int_t failed,stop;
   bool allow_drop,have_origin,video_closed,audio_enabled,armed,have_offered;
} record_metal_context_t;
static record_metal_context_t *record_metal_active,*record_metal_retired;
static bool record_metal_start_requested;
static bool record_metal_rate(double fps, unsigned *num, unsigned *den)
{
   uint64_t h0 = 0, h1 = 1, k0 = 1, k1 = 0;
   double value = fps;
   unsigned i;
   if (fps != fps || fps < 1.0 || fps > 1000.0) return false;
   for (i = 0; i < 32; i++)
   {
      uint64_t a, h, k;
      double fraction;
      if (value > UINT_MAX) return false;
      a = (uint64_t)value;
      if ((h1 && a > (UINT_MAX - h0) / h1)
            || (k1 && a > (1000000u - k0) / k1)) return false;
      h = a * h1 + h0; k = a * k1 + k0;
      if (!k || k > 1000000u) return false;
      if (fabs((double)h / k - fps) <= fps * 0.000000001)
      {
         *num = (unsigned)h; *den = (unsigned)k;
         return true;
      }
      h0 = h1; h1 = h; k0 = k1; k1 = k;
      fraction = value - (double)a;
      if (fraction <= 0.0) return false;
      value = 1.0 / fraction;
   }
   return false;
}

static bool record_metal_config(const char *path,record_gpu_session_metal_config_t *video,bool *audio_enabled)
{
   config_file_t *config;
   struct config_file_entry entry;
   char codec[16],format[8];
   bool audio=true,ok=false;
   if (!path || !*path) { *audio_enabled=true; return true; }
   config=config_file_new_from_path_to_string(path); if (!config) return false;
   if (config_get_entry(config,"audio_enable") && !config_get_bool(config,"audio_enable",&audio)) goto done;
   if (config_get_entry(config,"vcodec"))
   {
      if (!config_get_array(config,"vcodec",codec,sizeof(codec))) goto done;
      if (string_is_equal(codec,"h264")) video->hevc=false;
      else if (string_is_equal(codec,"hevc")) video->hevc=true;
      else goto done;
   }
   if (config_get_entry(config,"format") && (!config_get_array(config,"format",format,sizeof(format))
            || !string_is_equal(format,"mp4"))) goto done;
   if (config_get_entry(config,"video_bit_rate") && (!config_get_uint(config,"video_bit_rate",&video->bitrate)
            || !video->bitrate || video->bitrate>INT32_MAX)) goto done;
   if (config_get_entry_list_head(config,&entry)) do
   {
      if (!string_is_equal(entry.key,"audio_enable") && !string_is_equal(entry.key,"vcodec")
            && !string_is_equal(entry.key,"format") && !string_is_equal(entry.key,"video_bit_rate")) goto done;
   } while (config_get_entry_list_next(&entry));
   *audio_enabled=audio; ok=true;
done:
   config_file_free(config); return ok;
}
static bool record_metal_color(record_gpu_session_metal_config_t *video,const struct retro_hdr_metadata *hdr)
{
   if (!video || !hdr || hdr->version!=1 || hdr->size<sizeof(*hdr)) return false;
   video->mode=hdr->output_mode; video->bit_depth=hdr->output_mode?10:8;
   if (!hdr->output_mode) return true;
   return video->hevc && ((hdr->output_mode==1 && hdr->color_primaries==9 && hdr->transfer_function==16)
         || (hdr->output_mode==2 && hdr->color_primaries==1 && hdr->transfer_function==8));
}
static void *record_metal_init(const struct record_params *params)
{
   record_gpu_session_metal_config_t video;
   record_mux_avfoundation_config_t output;
   record_metal_context_t *ctx;
   video_device_info_t device;
   struct retro_hdr_metadata hdr;
   settings_t *settings=config_get_ptr();
   unsigned fps_num,fps_den,rate=0;
   size_t length,audio_capacity=0;
   bool audio_enabled;
   if (!params || !settings || record_metal_active || record_metal_retired || !params->filename || !*params->filename)
      return NULL;
   if (!params->video_gpu_record || recording_state_get_ptr()->streaming_enable
         || settings->bools.video_record_game_only || audio_driver_has_callback())
   {
      RARCH_ERR("[Metal recording] Requires viewport recording without streaming, Game Only or asynchronous core audio callbacks.\n");
      return NULL;
   }
   if (!video_driver_get_device(&device) || device.api!=VIDEO_DEVICE_METAL || !device.device) return NULL;
   memset(&video,0,sizeof(video)); memset(&output,0,sizeof(output)); memset(&hdr,0,sizeof(hdr));
   hdr.version=1; hdr.size=sizeof(hdr);
   if (!video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_HDR_METADATA,&hdr)) return NULL;
   video.width=VIDEO_SCALE_W(params->out_dims); video.height=VIDEO_SCALE_H(params->out_dims);
   video.slots=params->video_fifo_frames>=2 && params->video_fifo_frames<=16?params->video_fifo_frames:4;
   video.bitrate=16000000; video.hevc=hdr.output_mode!=0;
   if (!video.width || !video.height || ((video.width|video.height)&1) || video.width>16384 || video.height>16384
         || !record_metal_rate(params->fps,&fps_num,&fps_den) || fps_num>INT32_MAX
         || !record_metal_config(params->config,&video,&audio_enabled) || !record_metal_color(&video,&hdr)
         || (!hdr.output_mode && settings->uints.video_swapchain_bit_depth==2))
   {
      RARCH_ERR("[Metal recording] Unsupported viewport, frame rate or MP4 configuration; HDR requires HEVC/P010.\n");
      return NULL;
   }
   video.timescale=fps_num; video.key_interval=(unsigned)ceil(params->fps*2.0);
   output.timescale=fps_num;
   if (audio_enabled)
   {
      uint32_t layout=recording_state_get_ptr()->layout;
      double sample_rate=params->samplerate;
      if (sample_rate!=sample_rate || sample_rate<8000.0 || sample_rate>192000.0 || floor(sample_rate)!=sample_rate
            || !params->channels || params->channels>8 || audio_layout_channels(layout)!=params->channels
            || (layout&~0x63fu)) return NULL;
      rate=(unsigned)sample_rate;
      output.audio_rate=rate; output.channels=params->channels; output.audio_layout=layout;
      output.audio_bitrate=params->channels>2?384000:128000; output.max_audio_frames=4096;
      audio_capacity=(size_t)rate*params->channels*2+1024;
   }
   length=strlen(params->filename); if (length>=PATH_MAX_LENGTH) return NULL;
   ctx=(record_metal_context_t*)calloc(1,sizeof(*ctx)+length+1); if (!ctx) return NULL;
   ctx->path=(char*)(ctx+1); memcpy(ctx->path,params->filename,length+1);
   ctx->device=device.device; ctx->dims=params->out_dims; ctx->output_mode=hdr.output_mode;
   ctx->duration=fps_den; ctx->audio_rate=rate; ctx->audio_enabled=audio_enabled; ctx->allow_drop=params->allow_frame_drop;
   retro_atomic_int_init(&ctx->failed,0); retro_atomic_int_init(&ctx->stop,0);
   ctx->job=record_gpu_dispatch_metal_new(device.device,&video,&output,audio_capacity,ctx->path);
   if (!ctx->job) { free(ctx); return NULL; }
   record_metal_active=ctx;
   RARCH_LOG("[Metal recording] Starting native %s %u/%u fps recording.\n",audio_enabled?"AAC/video":"video-only",fps_num,fps_den);
   return ctx;
}
static bool record_metal_finalize(void *data)
{
   record_metal_context_t *ctx=(record_metal_context_t*)data;
   enum record_avfoundation_session_error error;
   retro_atomic_cas_int(&ctx->stop,0,2);
   record_gpu_dispatch_metal_audio_finish(ctx->job);
   record_gpu_dispatch_metal_status(ctx->job,NULL,NULL,&error,NULL);
   return !error && !retro_atomic_load_acquire_int(&ctx->failed);
}
static void record_metal_release(void *data)
{
   record_metal_context_t *ctx=(record_metal_context_t*)data;
   record_metal_active=NULL; ctx->next=record_metal_retired; record_metal_retired=ctx;
}
static bool record_metal_audio(void *data,const struct record_audio_data *audio)
{
   record_metal_context_t *ctx=(record_metal_context_t*)data;
   int result;
   if (!ctx->audio_enabled || !ctx->armed || (audio && !audio->frames)) return true;
   if (retro_atomic_load_acquire_int(&ctx->stop) || retro_atomic_load_acquire_int(&ctx->failed)) return false;
   if (!audio || video_state_get_ptr()->av_info.timing.sample_rate!=ctx->audio_rate
         || ctx->audio_frames>~(uint64_t)0-audio->frames) result=RECORD_AUDIO_ERROR;
   else result=record_gpu_dispatch_metal_audio(ctx->job,(const int16_t*)audio->data,audio->frames,ctx->audio_frames);
   if (result!=RECORD_AUDIO_OK)
   {
      retro_atomic_store_release_int(&ctx->failed,1); record_gpu_dispatch_metal_cancel(ctx->job); return false;
   }
   ctx->audio_frames+=audio->frames; return true;
}
const record_driver_t record_metal={record_metal_init,record_metal_release,NULL,record_metal_audio,record_metal_finalize,"metal"};
void record_metal_frame_info(struct video_frame_info *info,bool capture)
{
   record_metal_context_t *ctx=record_metal_active;
   info->recording_gpu=ctx; info->recording_gpu_pending=record_metal_retired!=NULL;
   info->recording_gpu_capture=false; info->recording_gpu_timestamp=0;
   if (!ctx || !capture || !ctx->armed || recording_state_get_ptr()->data!=ctx || video_state_get_ptr()->frame_repeat) return;
   if (ctx->next_timestamp>~(uint64_t)0-ctx->duration)
   {
      retro_atomic_store_release_int(&ctx->failed,1); record_gpu_dispatch_metal_cancel(ctx->job); return;
   }
   info->recording_gpu_timestamp=ctx->next_timestamp; ctx->next_timestamp+=ctx->duration; info->recording_gpu_capture=true;
}
void record_metal_gpu_capture(void *data,void *commands,void *source,unsigned pos,unsigned dims,uint64_t timestamp,unsigned output_mode)
{
   record_metal_context_t *ctx=(record_metal_context_t*)data;
   int stop,result;
   uint64_t pts;
   if (!ctx) return;
   stop=retro_atomic_load_acquire_int(&ctx->stop);
   if ((stop && stop!=2) || retro_atomic_load_acquire_int(&ctx->failed)) return;
   if (output_mode!=ctx->output_mode) { retro_atomic_cas_int(&ctx->stop,0,4); return; }
   if (dims!=ctx->dims || VIDEO_POS_X(pos)<0 || VIDEO_POS_Y(pos)<0)
   { retro_atomic_cas_int(&ctx->stop,0,1); return; }
   /* Shader subframes/swap-interval repeats share the source timestamp.
    * Admit only the first presentation attempt, including an allowed drop. */
   if (ctx->have_offered && timestamp==ctx->last_offered) return;
   ctx->have_offered=true; ctx->last_offered=timestamp;
   pts=ctx->audio_enabled?timestamp:ctx->have_origin?timestamp-ctx->origin:0;
   result=record_gpu_dispatch_metal_video(ctx->job,commands,source,VIDEO_POS_X(pos),VIDEO_POS_Y(pos),pts,ctx->duration);
   if (result==RECORD_GPU_QUEUE_OK)
   {
      if (!ctx->have_origin) { ctx->have_origin=true; ctx->origin=timestamp; }
      ctx->captured++;
   }
   else if (result==RECORD_GPU_QUEUE_ERROR || !ctx->allow_drop)
   { retro_atomic_store_release_int(&ctx->failed,1); record_gpu_dispatch_metal_cancel(ctx->job); }
}
void record_metal_stop(void)
{ if (record_metal_active || record_metal_start_requested) command_event(CMD_EVENT_RECORD_DEINIT,NULL); }
bool record_metal_pending(void) { return record_metal_start_requested || record_metal_active || record_metal_retired; }
bool record_metal_defer_start(void) { record_metal_start_requested=true; return true; }
bool record_metal_cancel_start(void)
{ bool requested=record_metal_start_requested; record_metal_start_requested=false; return requested; }
void record_metal_poll(void)
{
   record_metal_context_t **link=&record_metal_retired;
   video_driver_state_t *video=video_state_get_ptr();
   if (record_metal_start_requested)
   {
      struct video_viewport vp;
      memset(&vp,0,sizeof(vp));
      if (!recording_state_get_ptr()->enable) record_metal_start_requested=false;
      else if (video_driver_get_viewport_info(&vp) && VIDEO_SCALE_W(vp.dims) && VIDEO_SCALE_H(vp.dims))
      { record_metal_start_requested=false; command_event(CMD_EVENT_RECORD_INIT,NULL); }
   }
   if (record_metal_active)
   {
      record_metal_context_t *ctx=record_metal_active;
      video_device_info_t device;
      struct retro_hdr_metadata hdr;
      enum record_gpu_dispatch_metal_state state=record_gpu_dispatch_metal_status(ctx->job,NULL,NULL,NULL,NULL);
      if (state==RECORD_METAL_DISPATCH_READY) ctx->armed=true;
      if (!video_driver_get_device(&device) || device.device!=ctx->device) retro_atomic_cas_int(&ctx->stop,0,3);
      memset(&hdr,0,sizeof(hdr)); hdr.version=1; hdr.size=sizeof(hdr);
      if (!video_driver_get_display_metadata(RETRO_ENVIRONMENT_GET_HDR_METADATA,&hdr) || hdr.output_mode!=ctx->output_mode)
         retro_atomic_cas_int(&ctx->stop,0,4);
      if (retro_atomic_load_acquire_int(&ctx->stop) || retro_atomic_load_acquire_int(&ctx->failed)
            || state>=RECORD_METAL_DISPATCH_DRAINING) record_metal_stop();
   }
   while (*link)
   {
      record_metal_context_t *ctx=*link;
      enum record_gpu_dispatch_metal_state state;
      enum record_gpu_session_metal_startup startup;
      enum record_avfoundation_session_error error;
      int32_t native_status;
      int system_error;
      if (!ctx->video_closed)
      {
         if (video_driver_thread_wrapper_active() && video_thread_record_gpu_in_flight(video->data,ctx))
         { link=&ctx->next; continue; }
         if (!record_gpu_dispatch_metal_video_finish(ctx->job)) { link=&ctx->next; continue; }
         ctx->video_closed=true;
      }
      state=record_gpu_dispatch_metal_status(ctx->job,&startup,&native_status,&error,&system_error);
      if ((state!=RECORD_METAL_DISPATCH_DONE && state!=RECORD_METAL_DISPATCH_FAILED)
            || !record_gpu_dispatch_metal_free(ctx->job)) { link=&ctx->next; continue; }
      if (state==RECORD_METAL_DISPATCH_DONE && ctx->captured)
      {
         RARCH_LOG("[Metal recording] Finalized %llu GPU frames, %llu PCM frames.\n",
               (unsigned long long)ctx->captured,(unsigned long long)ctx->audio_frames);
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV) || defined(HAVE_WEBMPLAYER)
         if (config_get_ptr()->bools.history_list_enable)
         {
            struct playlist_entry entry={0};
            entry.path=ctx->path; entry.core_path=(char*)"builtin"; entry.core_name=(char*)"movieplayer";
            command_playlist_push_write(g_defaults.video_history,&entry);
         }
#endif
      }
      else RARCH_ERR("[Metal recording] Failed (startup %u, native %d, session %u, system %d).\n",
            (unsigned)startup,(int)native_status,(unsigned)error,system_error);
      *link=ctx->next; free(ctx);
   }
}
#else
const record_driver_t record_metal={NULL,NULL,NULL,NULL,NULL,"metal"};
void record_metal_frame_info(struct video_frame_info *info,bool capture)
{
   (void)capture; info->recording_gpu=NULL; info->recording_gpu_timestamp=0;
   info->recording_gpu_pending=info->recording_gpu_capture=false;
}
void record_metal_poll(void) {}
void record_metal_stop(void) {}
bool record_metal_pending(void) { return false; }
bool record_metal_defer_start(void) { return false; }
bool record_metal_cancel_start(void) { return false; }
void record_metal_gpu_capture(void *data,void *commands,void *source,unsigned pos,unsigned dims,uint64_t timestamp,unsigned output_mode)
{ (void)data; (void)commands; (void)source; (void)pos; (void)dims; (void)timestamp; (void)output_mode; }
#endif
