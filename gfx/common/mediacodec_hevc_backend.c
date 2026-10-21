/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "mediacodec_hevc_backend.h"
#include "mediacodec_frame_export.h"
#include <formats/image.h>
#include <rthreads/rthreads.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
typedef struct mediacodec_hevc_backend_context {
 mediacodec_hevc_backend_t *backend;
 mediacodec_frame_exporter_t *exporter;
 slock_t *lock;
 scond_t *wake;
 mediacodec_hevc_decoder_t *retired;
 int abort;
} mediacodec_hevc_backend_context_t;
static void mediacodec_hevc_backend_context_free(mediacodec_hevc_backend_context_t *c)
{
 mediacodec_hevc_backend_t *backend=c->backend;
 if(c->exporter)mediacodec_frame_exporter_free(c->exporter);
 if(c->wake)scond_free(c->wake);
 if(c->lock)slock_free(c->lock);
 free(c);
 retro_atomic_fetch_sub_int(&backend->active,1);
}
static bool mediacodec_hevc_backend_release(void *opaque)
{
 mediacodec_hevc_backend_context_t *c=(mediacodec_hevc_backend_context_t*)opaque;
 if(c->exporter && !mediacodec_frame_exporter_free(c->exporter))return false;
 c->exporter=NULL;return true;
}
static int mediacodec_hevc_backend_export(void *opaque,const android_image_view_t *image,
      const mediacodec_output_format_t *format,const rh265_video_color_t *color,video_hw_frame_t *frame)
{
 mediacodec_hevc_backend_context_t *c=(mediacodec_hevc_backend_context_t*)opaque;
 return mediacodec_hevc_frame_export(c->exporter,image,format,color,frame);
}
static void mediacodec_hevc_backend_enqueue(void *opaque,mediacodec_hevc_decoder_t *decoder)
{
 mediacodec_hevc_backend_context_t *c=(mediacodec_hevc_backend_context_t*)opaque;
 /* Cold decoder close only. The executor was created before native startup. */
 slock_lock(c->lock);c->retired=decoder;scond_signal(c->wake);slock_unlock(c->lock);
}
static void *mediacodec_hevc_backend_retire(void *opaque)
{
 mediacodec_hevc_backend_context_t *c=(mediacodec_hevc_backend_context_t*)opaque;
 mediacodec_hevc_decoder_t *decoder;
 slock_lock(c->lock);
 while(!c->abort && !c->retired)scond_wait(c->wake,c->lock);
 decoder=c->retired;slock_unlock(c->lock);
 if(decoder)while(!mediacodec_hevc_decoder_retire_poll(decoder)) {
  /* Poll native completion on this exclusive cold worker only. No busy spin,
   * producer wait, task-queue/frame-thread execution or frame-path lock. */
  slock_lock(c->lock);scond_wait_timeout(c->wake,c->lock,10000);slock_unlock(c->lock);
 }
 mediacodec_hevc_backend_context_free(c);
 return NULL;
}
static bool mediacodec_hevc_backend_open(void *opaque,const rh265_video *source,
      const rh265_video_params_t *params,unsigned slots,rh265_hw_ops_t *ops)
{
 mediacodec_hevc_backend_t *backend=(mediacodec_hevc_backend_t*)opaque;
 mediacodec_hevc_backend_context_t *c;
 mediacodec_hevc_import_t importer;
 pthread_t thread;
 JNIEnv *env;
 int active;
 if(!ops)return false;
 memset(ops,0,sizeof(*ops));
 if(!backend || !backend->environment || !source || !params || !slots || slots>32
       || params->chroma_format_idc!=1 || (params->bit_depth!=8 && params->bit_depth!=10))return false;
 for(;;) {
  active=retro_atomic_load_acquire_int(&backend->active);
  if(active<0 || (unsigned)active>=backend->max_sessions)return false;
  if(retro_atomic_cas_int(&backend->active,active,active+1))break;
 }
 env=backend->environment(backend->user);
 if(!env){retro_atomic_fetch_sub_int(&backend->active,1);return false;}
 c=(mediacodec_hevc_backend_context_t*)calloc(1,sizeof(*c));
 if(!c){retro_atomic_fetch_sub_int(&backend->active,1);return false;}
 c->backend=backend;c->lock=slock_new();c->wake=scond_new();
 c->exporter=mediacodec_frame_exporter_new(slots,params->bit_depth==10?IMAGE_PIXFMT_P010:IMAGE_PIXFMT_NV12);
 if(!c->lock || !c->wake || !c->exporter){mediacodec_hevc_backend_context_free(c);return false;}
 if(pthread_create(&thread,NULL,mediacodec_hevc_backend_retire,c)){mediacodec_hevc_backend_context_free(c);return false;}
 memset(&importer,0,sizeof(importer));importer.context=c;
 importer.export_image=mediacodec_hevc_backend_export;importer.release=mediacodec_hevc_backend_release;
 importer.enqueue=mediacodec_hevc_backend_enqueue;
 if(!mediacodec_hevc_decoder_open(env,source,params,slots,backend->max_input,&importer,ops)) {
  slock_lock(c->lock);c->abort=1;scond_signal(c->wake);slock_unlock(c->lock);
  pthread_join(thread,NULL);return false;
 }
 if(pthread_detach(thread)) {
  mediacodec_hevc_decoder_close(ops);pthread_join(thread,NULL);return false;
 }
 return true;
}
bool mediacodec_hevc_backend_init(mediacodec_hevc_backend_t *backend,
      JNIEnv *(*environment)(void*),void *user,unsigned max_input,unsigned max_sessions)
{
#if !defined(HAVE_THREADS) || !defined(RETRO_ATOMIC_LOCK_FREE)
 return false;
#endif
 if(!backend || !environment || !max_input || max_input>64u*1024u*1024u
       || !max_sessions || max_sessions>8)return false;
 memset(backend,0,sizeof(*backend));backend->user=user;backend->environment=environment;
 backend->max_input=max_input;backend->max_sessions=max_sessions;
 retro_atomic_int_init(&backend->active,0);
 backend->offer.user=backend;backend->offer.open=mediacodec_hevc_backend_open;
 backend->offer.close=mediacodec_hevc_decoder_close;return true;
}
unsigned mediacodec_hevc_backend_active(mediacodec_hevc_backend_t *backend)
{
 return backend?(unsigned)retro_atomic_load_acquire_int(&backend->active):0;
}
