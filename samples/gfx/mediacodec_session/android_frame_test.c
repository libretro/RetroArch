/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <retro_atomic.h>
#include <formats/image.h>
#include "../../../gfx/common/android_frame.h"
static int native,deleted;
static int acquire(void *reader,void **image,int *fd)
{assert(reader==&native);*image=&native;*fd=-1;return 1;}
static bool describe(void *image,android_image_view_t *view)
{
 assert(image==&native);view->buffer=&native;view->width=view->right=view->buffer_width=64;
 view->height=view->bottom=view->buffer_height=32;return true;
}
static void delete_image(void *image){assert(image==&native);deleted++;}
static void delete_reader(void *reader){assert(reader==&native);}
static android_image_queue_t *queue;
static android_frame_pool_t *pool;
static video_hw_frame_t shared_frame;
static retro_atomic_int_t ready;
static void *render(void *unused)
{
 unsigned i;android_frame_view_t view;(void)unused;
 for(i=0;i<10000;i++) {
  while(!retro_atomic_load_acquire_int(&ready))sched_yield();
  assert(android_frame_claim(shared_frame.owner,shared_frame.done,shared_frame.done_value,shared_frame.image,&view));
  assert(view.image.buffer==&native);
  assert(android_frame_finish(shared_frame.owner,shared_frame.done,shared_frame.done_value,-1));
  retro_atomic_store_release_int(&ready,0);
 }
 return NULL;
}
int main(void)
{
 android_image_ops_t ops;android_frame_view_t view,taken;
 video_hw_frame_t frame,old,untouched;
 int done[2];unsigned i;pthread_t thread;
 memset(&ops,0,sizeof(ops));ops.acquire=acquire;ops.describe=describe;ops.delete_image=delete_image;ops.delete_reader=delete_reader;
 queue=android_image_queue_new(&native,1,&ops);assert(queue);
 pool=android_frame_pool_new(1);assert(pool);
 memset(&view,0,sizeof(view));view.format.serial=1;view.pixfmt=IMAGE_PIXFMT_P010;
 assert(android_image_queue_acquire(queue,&view.image)==1);
 assert(!android_frame_export(pool,&view,&frame));old=frame;
 assert(android_frame_callback(frame.abandon));
 memset(&untouched,0x55,sizeof(untouched));taken=view;
 assert(android_frame_export(pool,&view,&untouched)==1 && untouched.width==0x55555555);
 assert(!android_frame_claim(frame.owner,frame.done,frame.done_value+1,frame.image,&taken));
 assert(android_frame_claim(frame.owner,frame.done,frame.done_value,frame.image,&taken));
 assert(!android_frame_unclaim(frame.owner,frame.done,frame.done_value+1));
 assert(android_frame_unclaim(frame.owner,frame.done,frame.done_value));
 assert(android_frame_claim(frame.owner,frame.done,frame.done_value,frame.image,&taken));
 video_hw_frame_abandon(&old); /* Already claimed: cannot release GPU-held input. */
 assert(!android_image_queue_poll(queue));
 assert(pipe(done)==0);
 assert(android_frame_finish(frame.owner,frame.done,frame.done_value,done[0]));
 assert(!android_image_queue_poll(queue));assert(write(done[1],"x",1)==1);
 assert(android_image_queue_poll(queue)==1);close(done[1]);
 assert(android_image_queue_acquire(queue,&view.image)==1);
 assert(!android_frame_export(pool,&view,&frame));
 video_hw_frame_abandon(&old);assert(!android_image_queue_poll(queue));
 android_frame_pool_close(pool);assert(!android_frame_pool_free(pool));
 video_hw_frame_abandon(&frame);assert(android_image_queue_poll(queue)==1);
 assert(android_frame_pool_free(pool));
 pool=android_frame_pool_new(1);assert(pool);retro_atomic_int_init(&ready,0);
 assert(!pthread_create(&thread,NULL,render,NULL));
 for(i=0;i<10000;i++) {
  assert(android_image_queue_acquire(queue,&view.image)==1);
  assert(!android_frame_export(pool,&view,&shared_frame));
  retro_atomic_store_release_int(&ready,1);
  while(retro_atomic_load_acquire_int(&ready))sched_yield();
  assert(android_image_queue_poll(queue)==1);
 }
 assert(!pthread_join(thread,NULL));
 android_frame_pool_close(pool);assert(android_frame_pool_free(pool));
 android_image_queue_close(queue);assert(android_image_queue_free(queue));
 assert(deleted==10002);
 puts("PASS: opaque Android frame leases, stale/claimed abandonment, delayed GPU fence and 10000 threaded handoffs");
 return 0;
}
