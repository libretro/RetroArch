/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_AVFOUNDATION_WORKER_NATIVE_H
#define RECORD_AVFOUNDATION_WORKER_NATIVE_H
#include "record_avfoundation_worker.h"
#include "record_mux_avfoundation.h"
#include <retro_inline.h>
static INLINE int record_avf_native_video(void *mux,void *sample)
{ return record_mux_avfoundation_video((record_mux_avfoundation_t*)mux,sample); }
static INLINE int record_avf_native_audio(void *mux,const int16_t *samples,size_t frames,uint64_t pts)
{ return record_mux_avfoundation_audio((record_mux_avfoundation_t*)mux,samples,frames,pts); }
static INLINE int record_avf_native_finish(void *mux)
{ return record_mux_avfoundation_finish((record_mux_avfoundation_t*)mux); }
static INLINE record_avfoundation_worker_t *record_avfoundation_worker_new_native(
      record_gpu_worker_vt_t *video,record_audio_queue_t *audio,record_mux_avfoundation_t *mux)
{
   record_avfoundation_worker_ops_t ops;
   ops.video=record_avf_native_video; ops.audio=record_avf_native_audio; ops.finish=record_avf_native_finish;
   return record_avfoundation_worker_new(video,audio,mux,&ops);
}
#endif
