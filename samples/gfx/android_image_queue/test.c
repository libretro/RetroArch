/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "../../../gfx/common/android_image_queue.h"
static int ready=-1,deleted,reader_deleted,malformed;
static int image;
static int acquire(void *reader,void **out,int *fd)
{ assert(reader==&image);*out=&image;*fd=ready;ready=-1;return 1; }
static bool describe(void *im,android_image_view_t *v)
{
 assert(im==&image);v->buffer=&image;v->width=v->right=320;v->height=v->bottom=192;
 v->timestamp_ns=1000000;return !malformed;
}
static void release_image(void *im){assert(im==&image);deleted++;}
static void release_reader(void *r){assert(r==&image);reader_deleted++;}
int main(void)
{
 android_image_ops_t ops;android_image_queue_t *q;android_image_view_t v,second;
 int fence[2],done[2];uint64_t generation;
 memset(&ops,0,sizeof(ops));ops.acquire=acquire;ops.describe=describe;
 ops.delete_image=release_image;ops.delete_reader=release_reader;
 assert(pipe(fence)==0);ready=fence[0];
 q=android_image_queue_new(&image,1,&ops);assert(q);assert(!android_image_queue_idle(q));
 assert(android_image_queue_acquire(q,&v)==1 && v.buffer==&image && v.ready_fd==fence[0]);
 assert(android_image_queue_acquire(q,&second)==0);
 generation=v.generation;
 android_image_queue_close(q);assert(!android_image_queue_idle(q));assert(!android_image_queue_free(q));
 assert(android_image_queue_return(v.ticket,v.generation,-1));
 assert(!android_image_queue_return(v.ticket,v.generation,-1));
 assert(!android_image_queue_poll(q) && !deleted && !android_image_queue_idle(q) && !android_image_queue_free(q));
 assert(write(fence[1],"x",1)==1);assert(android_image_queue_poll(q)==1 && deleted==1);
 assert(fcntl(fence[0],F_GETFD)==-1);close(fence[1]);
 assert(android_image_queue_idle(q) && !reader_deleted);
 assert(android_image_queue_free(q) && reader_deleted==1);
 q=android_image_queue_new(&image,1,&ops);assert(q);
 assert(pipe(fence)==0 && pipe(done)==0);ready=fence[0];
 assert(android_image_queue_acquire(q,&v)==1);
 assert(!android_image_queue_return(v.ticket,generation,v.ready_fd));
 assert(android_image_queue_return(v.ticket,v.generation,done[0]));
 assert(!android_image_queue_poll(q));
 assert(write(fence[1],"x",1)==1);assert(!android_image_queue_poll(q));
 assert(write(done[1],"x",1)==1);assert(android_image_queue_poll(q)==1);
 assert(fcntl(fence[0],F_GETFD)==-1 && fcntl(done[0],F_GETFD)==-1);
 close(fence[1]);close(done[1]);
 assert(android_image_queue_acquire(q,&second)==1 && second.generation>v.generation);
 assert(!android_image_queue_return(v.ticket,v.generation,-1));
 assert(android_image_queue_return(second.ticket,second.generation,-1));
 assert(android_image_queue_poll(q)==1);
 android_image_queue_close(q);assert(android_image_queue_free(q));
 q=android_image_queue_new(&image,1,&ops);assert(q);malformed=1;
 assert(android_image_queue_acquire(q,&v)==-1 && !v.ticket && v.ready_fd==-1);
 assert(android_image_queue_poll(q)==1);android_image_queue_close(q);assert(android_image_queue_free(q));
 assert(deleted==4 && reader_deleted==3);
 puts("PASS: Android image owner delayed-ready/done fences, close, stale tickets and malformed image drain");
 return 0;
}
