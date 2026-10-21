/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../gfx/common/mediacodec_hevc_backend.h"
#include <rthreads/rthreads.h>
#include <assert.h>
#include <formats/image.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
struct mediacodec_hevc_decoder {mediacodec_hevc_import_t importer;};
static retro_atomic_int_t retire_gate,polls,retired;
static uintptr_t caller_thread;
static int native_fail,env_fail,native_opens,image_token,deleted,thread_fail,detach_fail;
static int backend_test_pthread_create(pthread_t *thread,const pthread_attr_t *attr,void *(*run)(void*),void *user)
{return thread_fail?EAGAIN:pthread_create(thread,attr,run,user);}
static int backend_test_pthread_detach(pthread_t thread)
{return detach_fail?EINVAL:pthread_detach(thread);}
static int image_acquire(void *reader,void **image,int *fd)
{assert(reader==&image_token);*image=&image_token;*fd=-1;return 1;}
static bool image_describe(void *image,android_image_view_t *view)
{assert(image==&image_token);view->buffer=image;view->width=view->right=view->buffer_width=64;view->height=view->bottom=view->buffer_height=32;return true;}
static void image_delete(void *image){assert(image==&image_token);deleted++;}
static void reader_delete(void *reader){assert(reader==&image_token);}
static JNIEnv *environment(void *user){assert(user==&native_opens);return env_fail?NULL:(JNIEnv*)&native_opens;}
bool mediacodec_hevc_decoder_open(JNIEnv *env,const rh265_video *source,
      const rh265_video_params_t *params,unsigned slots,unsigned max_input,
      const mediacodec_hevc_import_t *importer,rh265_hw_ops_t *ops)
{
 struct mediacodec_hevc_decoder *decoder;
 assert(env && source && params && slots==2 && max_input==1048576 && importer);
 native_opens++;if(native_fail)return false;
 decoder=(struct mediacodec_hevc_decoder*)calloc(1,sizeof(*decoder));assert(decoder);
 decoder->importer=*importer;ops->decoder=decoder;return true;
}
void mediacodec_hevc_decoder_close(rh265_hw_ops_t *ops)
{
 struct mediacodec_hevc_decoder *decoder=(struct mediacodec_hevc_decoder*)ops->decoder;
 if(!decoder)return;
 memset(ops,0,sizeof(*ops));decoder->importer.enqueue(decoder->importer.context,decoder);
}
bool mediacodec_hevc_decoder_retire_poll(mediacodec_hevc_decoder_t *decoder)
{
 assert(sthread_get_current_thread_id()!=caller_thread);
 retro_atomic_fetch_add_int(&polls,1);
 if(!retro_atomic_load_acquire_int(&retire_gate))return false;
 if(!decoder->importer.release(decoder->importer.context))return false;
 free(decoder);retro_atomic_fetch_add_int(&retired,1);return true;
}
#define pthread_create backend_test_pthread_create
#define pthread_detach backend_test_pthread_detach
#include "../../../gfx/common/mediacodec_hevc_backend.c"
#undef pthread_create
#undef pthread_detach
static void wait_idle(mediacodec_hevc_backend_t *backend)
{
 unsigned i;
 for(i=0;i<2000 && mediacodec_hevc_backend_active(backend);i++)usleep(1000);
 assert(!mediacodec_hevc_backend_active(backend));
}
int main(void)
{
 static mediacodec_hevc_backend_t backend;
 static rh265_video_params_t params;
 rh265_hw_ops_t a,b,rejected;
 const rh265_video *source=(const rh265_video*)&native_opens;
 unsigned i;int opens;
 android_image_ops_t image_ops;android_image_queue_t *images;
 android_image_view_t image;mediacodec_output_format_t format;
 rh265_video_color_t color;video_hw_frame_t frame;
 struct mediacodec_hevc_decoder *decoder;
 caller_thread=sthread_get_current_thread_id();
 retro_atomic_int_init(&retire_gate,0);retro_atomic_int_init(&polls,0);retro_atomic_int_init(&retired,0);
 assert(!mediacodec_hevc_backend_init(&backend,environment,&native_opens,1048576,9));
 assert(mediacodec_hevc_backend_init(&backend,environment,&native_opens,1048576,2));
 memset(&params,0,sizeof(params));params.chroma_format_idc=1;params.bit_depth=8;
 env_fail=1;assert(!backend.offer.open(backend.offer.user,source,&params,2,&a) && !mediacodec_hevc_backend_active(&backend));env_fail=0;
 thread_fail=1;assert(!backend.offer.open(backend.offer.user,source,&params,2,&a) && !mediacodec_hevc_backend_active(&backend));thread_fail=0;
 retro_atomic_store_release_int(&retire_gate,1);detach_fail=1;
 assert(!backend.offer.open(backend.offer.user,source,&params,2,&a) && !a.decoder && !mediacodec_hevc_backend_active(&backend));
 detach_fail=0;retro_atomic_store_release_int(&retire_gate,0);retro_atomic_store_release_int(&retired,0);
 native_fail=1;assert(!backend.offer.open(backend.offer.user,source,&params,2,&a) && !mediacodec_hevc_backend_active(&backend));native_fail=0;
 assert(backend.offer.open(backend.offer.user,source,&params,2,&a));params.bit_depth=10;
 assert(backend.offer.open(backend.offer.user,source,&params,2,&b));assert(mediacodec_hevc_backend_active(&backend)==2);
 opens=native_opens;assert(!backend.offer.open(backend.offer.user,source,&params,2,&rejected) && native_opens==opens);
 backend.offer.close(&a);backend.offer.close(&b);assert(!a.decoder && !b.decoder);
 for(i=0;i<1000 && retro_atomic_load_acquire_int(&polls)<2;i++)usleep(1000);
 assert(retro_atomic_load_acquire_int(&polls)>=2 && !retro_atomic_load_acquire_int(&retired));
 assert(!backend.offer.open(backend.offer.user,source,&params,2,&rejected) && native_opens==opens);
 retro_atomic_store_release_int(&retire_gate,1);wait_idle(&backend);
 assert(retro_atomic_load_acquire_int(&retired)==2);
 /* Actual exported frame-pool lease holds a closed epoch alive. No native
  * decoder mock or exporter release is permitted to consume a live image. */
 assert(backend.offer.open(backend.offer.user,source,&params,2,&a));
 decoder=(struct mediacodec_hevc_decoder*)a.decoder;
 memset(&image_ops,0,sizeof(image_ops));image_ops.acquire=image_acquire;image_ops.describe=image_describe;
 image_ops.delete_image=image_delete;image_ops.delete_reader=reader_delete;
 images=android_image_queue_new(&image_token,1,&image_ops);assert(images);
 assert(android_image_queue_acquire(images,&image)==1);
 memset(&format,0,sizeof(format));format.serial=1;memset(&color,0,sizeof(color));
 color.description_present=color.video_signal_present=1;color.primaries=color.transfer=color.matrix=1;
 assert(!decoder->importer.export_image(decoder->importer.context,&image,&format,&color,&frame));
 assert(frame.pixfmt==IMAGE_PIXFMT_P010);
 backend.offer.close(&a);usleep(30000);
 assert(mediacodec_hevc_backend_active(&backend)==1 && retro_atomic_load_acquire_int(&retired)==2);
 video_hw_frame_abandon(&frame);wait_idle(&backend);
 assert(android_image_queue_poll(images)==1 && deleted==1);
 android_image_queue_close(images);assert(android_image_queue_free(images));
 for(i=0;i<40;i++) {
  assert(backend.offer.open(backend.offer.user,source,&params,2,&a));backend.offer.close(&a);wait_idle(&backend);
 }
 assert(retro_atomic_load_acquire_int(&retired)==43);
 puts("PASS: HEVC backend startup rollback, bounded session admission, detached cold retirement and repeated seek epochs");
 return 0;
}
