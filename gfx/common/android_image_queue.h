/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef ANDROID_IMAGE_QUEUE_H
#define ANDROID_IMAGE_QUEUE_H
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct android_image_queue android_image_queue_t;
typedef struct android_image_ticket android_image_ticket_t;
typedef struct android_image_view
{
   android_image_ticket_t *ticket;
   uint64_t generation;
   void *buffer; /* borrowed AHardwareBuffer; valid until accepted return */
   int ready_fd; /* borrowed; renderer duplicates it for GPU-side waiting */
   int64_t timestamp_ns;
   unsigned width, height, left, top, right, bottom;
   unsigned buffer_width, buffer_height; /* actual AHardwareBuffer allocation */
} android_image_view_t;
typedef struct android_image_ops
{
   /* Worker only. 1 image/owned ready-fd, 0 no image, -1 failure. */
   int (*acquire)(void *reader, void **image, int *ready_fd);
   bool (*describe)(void *image, android_image_view_t *view);
   void (*delete_image)(void *image);
   void (*delete_reader)(void *reader);
} android_image_ops_t;
/* Cold constructor adopts reader only on success; maximum 32 slots. */
android_image_queue_t *android_image_queue_new(
      void *reader, unsigned slots, const android_image_ops_t *ops);
/* Worker only. 1 borrowed view, 0 backpressure/empty/closed, -1 sticky failure.
 */
int android_image_queue_acquire(
      android_image_queue_t *q, android_image_view_t *view);
/* One renderer owner per ticket. On true, transfers done_fd (-1 for unused)
 * and consumes the view. A GPU done fence must cover its ready wait and all
 * reads. On false, takes nothing. No waits, native image calls or allocation.
 * Only abandon unused images with -1; ready-fd stays owned by the queue. */
bool android_image_queue_return(
      android_image_ticket_t *ticket, uint64_t generation, int done_fd);
/* Worker only: bounded zero-timeout fence polls, then release images/fds.
 * Reader and AImages survive close until every ticket/fence retires. */
unsigned android_image_queue_poll(android_image_queue_t *q);
/* Retirement owner only: borrowed fence after close, -1 without one. */
int android_image_queue_retire_fd(const android_image_queue_t *q);
void android_image_queue_close(android_image_queue_t *q);
/* Worker only, after close: true when every acquired image/fence retired.
 * Does not delete the reader; stop its codec before freeing the queue. */
bool android_image_queue_idle(const android_image_queue_t *q);
bool android_image_queue_free(android_image_queue_t *q);
RETRO_END_DECLS
#endif
