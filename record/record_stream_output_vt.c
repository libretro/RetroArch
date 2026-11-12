/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_stream_output_vt.h"
#include <stdlib.h>
#include <string.h>
enum record_stream_output_vt_stage
{ VT_OUTPUT_RUN, VT_OUTPUT_SRC, VT_OUTPUT_AUDIO, VT_OUTPUT_CLIENT };
struct record_stream_output_vt
{
   record_stream_vt_t *stream;
   record_at_audio_t *audio;
   record_at_resample_t *converter;
   net_rtmp_client_t *client;
   record_stream_vt_wait_t wait;
   enum record_stream_vt_error error;
   enum record_stream_output_vt_stage stage;
   bool ended, cancelled;
};
record_stream_output_vt_t *record_stream_output_vt_new(record_gpu_worker_vt_t *video,
      record_at_audio_t *audio, record_at_resample_t *converter,
      net_rtmp_client_t *client, const record_stream_vt_config_t *config,
      record_audio_queue_t *source)
{
   record_stream_output_vt_t *o;
   record_audio_queue_t *configured;
   unsigned rate, channels;
   if ((audio && !source) || (!audio && (source || converter))) return NULL;
   if (converter && (!record_at_resample_config(converter, &configured, &rate, &channels)
            || configured != source)) return NULL;
   o=(record_stream_output_vt_t*)calloc(1,sizeof(*o));
   if (!o) return NULL;
   o->wait.network.fd=-1;
   o->stream=converter?record_stream_vt_new_resampled(video,audio,client,config,converter)
      :source?record_stream_vt_new_queued(video,audio,client,config,source)
      :record_stream_vt_new(video,NULL,client,config);
   if (!o->stream) { free(o); return NULL; }
   o->audio=audio; o->converter=converter; o->client=client;
   return o;
}
static int record_stream_output_vt_step(void *data)
{
   record_stream_output_vt_t *o=(record_stream_output_vt_t*)data;
   int result;
   if (!o || o->cancelled || o->error) return RECORD_AVF_WORKER_ERROR;
   if (o->ended) return RECORD_AVF_WORKER_END;
   result=record_stream_vt_step(o->stream,0,&o->wait);
   if (result==RECORD_STREAM_VT_ERROR)
   {
      o->error=record_stream_vt_error(o->stream);
      return RECORD_AVF_WORKER_ERROR;
   }
   if (result==RECORD_STREAM_VT_END)
   { o->ended=true; return RECORD_AVF_WORKER_END; }
   return result==RECORD_STREAM_VT_PROGRESS?RECORD_AVF_WORKER_PROGRESS:RECORD_AVF_WORKER_BUSY;
}
static void record_stream_output_vt_retired(record_stream_output_vt_t *o)
{
   o->stream=NULL; o->stage=VT_OUTPUT_SRC;
   memset(&o->wait,0,sizeof(o->wait)); o->wait.network.fd=-1;
}
static bool record_stream_output_vt_cancel(void *data)
{
   record_stream_output_vt_t *o=(record_stream_output_vt_t*)data;
   int result;
   if (!o) return false;
   o->cancelled=true;
   if (!o->error) o->error=RECORD_STREAM_VT_ERROR_ABORTED;
   if (!o->stream) return true;
   record_stream_vt_abort(o->stream);
   result=record_stream_vt_step(o->stream,0,&o->wait);
   if (result==RECORD_STREAM_VT_BUSY) return false;
   if (!record_stream_vt_free(o->stream))
   { o->wait.encoder_retry=true; return false; }
   record_stream_output_vt_retired(o); return true;
}
static bool record_stream_output_vt_free(void *data)
{
   record_stream_output_vt_t *o=(record_stream_output_vt_t*)data;
   if (!o) return true;
   if (o->stream)
   {
      if (!o->ended && !o->cancelled) return false;
      if (o->cancelled)
      {
         if (!record_stream_output_vt_cancel(o)) return false;
      }
      else
      {
         if (!record_stream_vt_free(o->stream)) return false;
         record_stream_output_vt_retired(o);
      }
      return false;
   }
   switch (o->stage)
   {
      case VT_OUTPUT_SRC:
         if (o->converter)
         {
            if (o->cancelled && !record_at_resample_cancel(o->converter)) return false;
            if (!record_at_resample_free(o->converter)) return false;
            o->converter=NULL;
         }
         o->stage=VT_OUTPUT_AUDIO; return false;
      case VT_OUTPUT_AUDIO:
         if (o->audio)
         {
            if (o->cancelled)
            {
               /* Either loan may already have been returned by the stream. */
               record_at_audio_input_release(o->audio);
               record_at_audio_release(o->audio);
               if (!record_at_audio_cancel(o->audio)) return false;
            }
            if (!record_at_audio_free(o->audio)) return false;
            o->audio=NULL;
         }
         o->stage=VT_OUTPUT_CLIENT; return false;
      case VT_OUTPUT_CLIENT:
         if (!net_rtmp_client_free(o->client)) return false;
         free(o); return true;
      default: break;
   }
   return false;
}
static bool record_stream_output_vt_retry(const void *data)
{
   const record_stream_output_vt_t *o=(const record_stream_output_vt_t*)data;
   return o && (o->stage!=VT_OUTPUT_RUN || o->wait.encoder_retry);
}
static bool record_stream_output_vt_network(const void *data,int *fd,bool *read,bool *write)
{
   const record_stream_output_vt_t *o=(const record_stream_output_vt_t*)data;
   if (!o || !o->stream || o->wait.network.fd<0) return false;
   *fd=o->wait.network.fd; *read=o->wait.network.read; *write=o->wait.network.write;
   return *read || *write;
}
bool record_stream_output_vt_set_ops(record_avfoundation_session_output_ops_t *ops)
{
   if (!ops) return false;
   ops->step=record_stream_output_vt_step;
   ops->cancel=record_stream_output_vt_cancel;
   ops->free_output=record_stream_output_vt_free;
   ops->retry_needed=record_stream_output_vt_retry;
   ops->network=record_stream_output_vt_network;
   return true;
}
enum record_stream_vt_error record_stream_output_vt_error(const record_stream_output_vt_t *o)
{ return o?o->error:RECORD_STREAM_VT_ERROR_NETWORK; }
