/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_avfoundation_session.h"
#include <retro_atomic.h>
#include <stdlib.h>
enum record_avf_session_stage
{ AVFS_BIND, AVFS_RUN, AVFS_CLOSE, AVFS_MUX, AVFS_ENCODER, AVFS_VIDEO, AVFS_AUDIO, AVFS_QUEUES, AVFS_DONE };
struct record_avfoundation_session
{
   record_gpu_queue_t *queue;
   record_gpu_worker_vt_t *video;
   record_audio_queue_t *audio;
   record_avfoundation_worker_t *pump;
   void *context,*mux;
   record_avfoundation_session_ops_t ops;
   retro_atomic_int_t video_closed,audio_closed,cancelled,error;
   enum record_avf_session_stage stage;
};
static void record_avf_session_fail(record_avfoundation_session_t *s,int error)
{
   if (!retro_atomic_load_acquire_int(&s->error))
      retro_atomic_store_release_int(&s->error,error);
   s->stage=AVFS_CLOSE;
}
record_avfoundation_session_t *record_avfoundation_session_new(
      record_gpu_queue_t *queue,record_gpu_worker_vt_t *video,
      record_audio_queue_t *audio,void *context,const record_avfoundation_session_ops_t *ops)
{
   record_avfoundation_session_t *s;
   if (!queue || !video || !context || !ops || !ops->open || !ops->mux.video
         || !ops->mux.finish || (audio && !ops->mux.audio) || !ops->cancel
         || !ops->free_mux || !ops->destroy_encoder || !ops->destroy_context) return NULL;
   s=(record_avfoundation_session_t*)calloc(1,sizeof(*s)); if (!s) return NULL;
   s->queue=queue; s->video=video; s->audio=audio; s->context=context; s->ops=*ops;
   retro_atomic_int_init(&s->video_closed,0); retro_atomic_int_init(&s->audio_closed,!audio);
   retro_atomic_int_init(&s->cancelled,0); retro_atomic_int_init(&s->error,0);
   return s;
}
int record_avfoundation_session_audio(record_avfoundation_session_t *s,
      const int16_t *samples,size_t frames,uint64_t timestamp)
{
   if (!s || retro_atomic_load_acquire_int(&s->audio_closed)
         || retro_atomic_load_acquire_int(&s->cancelled)
         || retro_atomic_load_acquire_int(&s->error)) return RECORD_AUDIO_ERROR;
   return record_audio_queue_push(s->audio,samples,frames,timestamp);
}
bool record_avfoundation_session_video_finish(record_avfoundation_session_t *s)
{
   if (!s) return false;
   if (retro_atomic_load_acquire_int(&s->video_closed)) return true;
   if (!record_gpu_queue_finish(s->queue)) return false;
   retro_atomic_store_release_int(&s->video_closed,1); return true;
}
bool record_avfoundation_session_audio_finish(record_avfoundation_session_t *s)
{
   if (!s) return false;
   if (retro_atomic_load_acquire_int(&s->audio_closed)) return true;
   if (!record_audio_queue_finish(s->audio)) return false;
   retro_atomic_store_release_int(&s->audio_closed,1); return true;
}
void record_avfoundation_session_cancel(record_avfoundation_session_t *s)
{ if (s) retro_atomic_store_release_int(&s->cancelled,1); }
int record_avfoundation_session_step(record_avfoundation_session_t *s)
{
   int result;
   void *sample=NULL;
   if (!s) return RECORD_AVF_WORKER_ERROR;
   if (s->stage==AVFS_DONE) return retro_atomic_load_acquire_int(&s->error)
      ?RECORD_AVF_WORKER_ERROR:RECORD_AVF_WORKER_END;
   if (retro_atomic_load_acquire_int(&s->cancelled)
         && !retro_atomic_load_acquire_int(&s->error))
   {
      retro_atomic_store_release_int(&s->error,RECORD_AVF_SESSION_CANCELLED);
      if (s->stage<AVFS_CLOSE) s->stage=AVFS_CLOSE;
   }
   switch (s->stage)
   {
      case AVFS_BIND:
         result=record_gpu_worker_vt_step(s->video,&sample);
         if (result==RECORD_GPU_VT_PACKET)
         {
            s->mux=s->ops.open(s->context,sample);
            if (s->mux) s->pump=record_avfoundation_worker_new(s->video,s->audio,s->mux,&s->ops.mux);
            if (!s->pump) record_avf_session_fail(s,RECORD_AVF_SESSION_MUX);
            else s->stage=AVFS_RUN;
            return RECORD_AVF_WORKER_PROGRESS;
         }
         if (result==RECORD_GPU_VT_ERROR || result==RECORD_GPU_VT_END)
         {
            record_avf_session_fail(s,result==RECORD_GPU_VT_END?RECORD_AVF_SESSION_EMPTY:RECORD_AVF_SESSION_VIDEO);
            return RECORD_AVF_WORKER_PROGRESS;
         }
         return result==RECORD_GPU_VT_PROGRESS?RECORD_AVF_WORKER_PROGRESS:RECORD_AVF_WORKER_BUSY;
      case AVFS_RUN:
         result=record_avfoundation_worker_step(s->pump);
         if (result==RECORD_AVF_WORKER_ERROR) record_avf_session_fail(s,RECORD_AVF_SESSION_MUX);
         else if (result==RECORD_AVF_WORKER_END) s->stage=AVFS_CLOSE;
         else return result;
         return RECORD_AVF_WORKER_PROGRESS;
      case AVFS_CLOSE:
         if (!retro_atomic_load_acquire_int(&s->video_closed)
               || !retro_atomic_load_acquire_int(&s->audio_closed)) return RECORD_AVF_WORKER_BUSY;
         record_avfoundation_worker_abort(s->pump);
         if (!record_avfoundation_worker_free(s->pump)) return RECORD_AVF_WORKER_BUSY;
         s->pump=NULL; s->stage=AVFS_MUX; return RECORD_AVF_WORKER_PROGRESS;
      case AVFS_MUX:
         if (s->mux)
         {
            /* A pending finish callback progresses independently. Repeated
             * cancel/free polls must never restart finalization or wait. */
            if (retro_atomic_load_acquire_int(&s->error) && !s->ops.cancel(s->mux)) return RECORD_AVF_WORKER_BUSY;
            if (!s->ops.free_mux(s->mux)) return RECORD_AVF_WORKER_BUSY;
            s->mux=NULL;
         }
         s->stage=AVFS_ENCODER; return RECORD_AVF_WORKER_PROGRESS;
      case AVFS_ENCODER:
         s->ops.destroy_encoder(s->context); s->stage=AVFS_VIDEO;
         return RECORD_AVF_WORKER_PROGRESS;
      case AVFS_VIDEO:
         if (!record_gpu_worker_vt_abandon(s->video)) return RECORD_AVF_WORKER_BUSY;
         s->video=NULL; s->stage=AVFS_AUDIO; return RECORD_AVF_WORKER_PROGRESS;
      case AVFS_AUDIO:
         if (s->audio)
         {
            const int16_t *samples;
            size_t frames;
            uint64_t timestamp;
            result=record_audio_queue_peek(s->audio,&samples,&frames,&timestamp);
            if (result==RECORD_AUDIO_OK)
               return record_audio_queue_ack(s->audio)?RECORD_AVF_WORKER_PROGRESS:RECORD_AVF_WORKER_BUSY;
            if (result!=RECORD_AUDIO_END) return RECORD_AVF_WORKER_BUSY;
         }
         s->stage=AVFS_QUEUES; return RECORD_AVF_WORKER_PROGRESS;
      case AVFS_QUEUES:
         if (!record_gpu_queue_free(s->queue)) return RECORD_AVF_WORKER_BUSY;
         s->queue=NULL;
         if (!record_audio_queue_free(s->audio)) return RECORD_AVF_WORKER_BUSY;
         s->audio=NULL;
         if (!s->ops.destroy_context(s->context)) return RECORD_AVF_WORKER_BUSY;
         s->context=NULL; s->stage=AVFS_DONE;
         return retro_atomic_load_acquire_int(&s->error)?RECORD_AVF_WORKER_ERROR:RECORD_AVF_WORKER_END;
      default: break;
   }
   return RECORD_AVF_WORKER_ERROR;
}
enum record_avfoundation_session_error record_avfoundation_session_error(const record_avfoundation_session_t *s)
{ return s?(enum record_avfoundation_session_error)retro_atomic_load_acquire_int(&s->error):RECORD_AVF_SESSION_MUX; }
bool record_avfoundation_session_free(record_avfoundation_session_t *s)
{
   if (!s) return true;
   if (s->stage!=AVFS_DONE) return false;
   free(s); return true;
}

bool record_avfoundation_session_retry_needed(const record_avfoundation_session_t *s)
{
   if (!s || s->stage==AVFS_DONE) return false;
   if (s->stage==AVFS_BIND) return record_gpu_worker_vt_retry_needed(s->video);
   if (s->stage==AVFS_RUN) return record_avfoundation_worker_retry_needed(s->pump);
   if (s->stage==AVFS_CLOSE) return retro_atomic_load_acquire_int(&s->video_closed)
      && retro_atomic_load_acquire_int(&s->audio_closed);
   return true;
}
