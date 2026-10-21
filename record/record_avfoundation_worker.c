/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_avfoundation_worker.h"
#include "record_mux_avfoundation.h"
#include <stdlib.h>
struct record_avfoundation_worker
{
   record_gpu_worker_vt_t *video;
   record_audio_queue_t *audio;
   void *mux;
   record_avfoundation_worker_ops_t ops;
   bool started,video_end,audio_end,failed,ended,audio_busy;
};
record_avfoundation_worker_t *record_avfoundation_worker_new(
      record_gpu_worker_vt_t *video,record_audio_queue_t *audio,void *mux,
      const record_avfoundation_worker_ops_t *ops)
{
   record_avfoundation_worker_t *w;
   if (!video || !mux || !ops || !ops->video || !ops->finish || (audio && !ops->audio)) return NULL;
   w=(record_avfoundation_worker_t*)calloc(1,sizeof(*w));
   if (w) { w->video=video; w->audio=audio; w->mux=mux; w->ops=*ops; w->audio_end=!audio; }
   return w;
}
int record_avfoundation_worker_step(record_avfoundation_worker_t *w)
{
   void *sample=NULL;
   bool progress=false;
   int result;
   if (!w || w->failed) return RECORD_AVF_WORKER_ERROR;
   if (w->ended) return RECORD_AVF_WORKER_END;
   w->started=true;
   if (!w->video_end)
   {
      result=record_gpu_worker_vt_step(w->video,&sample);
      if (result==RECORD_GPU_VT_ERROR) goto failed;
      if (result==RECORD_GPU_VT_END) { w->video_end=true; progress=true; }
      else if (result==RECORD_GPU_VT_PROGRESS) progress=true;
      else if (result==RECORD_GPU_VT_PACKET)
      {
         result=w->ops.video(w->mux,sample);
         if (result==RECORD_AVF_OK)
         { if (!record_gpu_worker_vt_release(w->video)) goto failed; progress=true; }
         else if (result!=RECORD_AVF_BUSY) goto failed;
      }
      else if (result!=RECORD_GPU_VT_BUSY) goto failed;
   }
   w->audio_busy=false;
   if (!w->audio_end)
   {
      const int16_t *samples=NULL;
      size_t frames=0;
      uint64_t timestamp=0;
      result=record_audio_queue_peek(w->audio,&samples,&frames,&timestamp);
      if (result==RECORD_AUDIO_ERROR) goto failed;
      if (result==RECORD_AUDIO_END) { w->audio_end=true; progress=true; }
      else if (result==RECORD_AUDIO_OK)
      {
         result=w->ops.audio(w->mux,samples,frames,timestamp);
         if (result==RECORD_AVF_OK)
         { if (!record_audio_queue_ack(w->audio)) goto failed; progress=true; }
         else if (result==RECORD_AVF_BUSY) w->audio_busy=true;
         else goto failed;
      }
      else if (result!=RECORD_AUDIO_BUSY) goto failed;
   }
   if (w->video_end && w->audio_end)
   {
      result=w->ops.finish(w->mux);
      if (result==RECORD_AVF_OK) { w->ended=true; return RECORD_AVF_WORKER_END; }
      if (result!=RECORD_AVF_BUSY) goto failed;
   }
   return progress?RECORD_AVF_WORKER_PROGRESS:RECORD_AVF_WORKER_BUSY;
failed:
   w->failed=true; return RECORD_AVF_WORKER_ERROR;
}
bool record_avfoundation_worker_free(record_avfoundation_worker_t *w)
{
   if (!w) return true;
   if (w->started && !w->failed && !w->ended) return false;
   free(w); return true;
}
void record_avfoundation_worker_abort(record_avfoundation_worker_t *w)
{ if (w) w->failed=true; }

bool record_avfoundation_worker_retry_needed(const record_avfoundation_worker_t *w)
{
   return w && !w->failed && !w->ended && (w->audio_busy || (w->video_end && w->audio_end)
         || record_gpu_worker_vt_retry_needed(w->video));
}
