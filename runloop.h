/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RETROARCH_RUNLOOP_H
#define __RETROARCH_RUNLOOP_H

#include <file/nbio.h>
#include <formats/image.h>
#include <formats/rpng.h>
#include <queues/message_queue.h>
#include "libretro.h"

#ifndef AUDIO_BUFFER_FREE_SAMPLES_COUNT
#define AUDIO_BUFFER_FREE_SAMPLES_COUNT (8 * 1024)
#endif

#ifndef MEASURE_FRAME_TIME_SAMPLES_COUNT
#define MEASURE_FRAME_TIME_SAMPLES_COUNT (2 * 1024)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*transfer_cb_t               )(void *data, size_t len);

typedef struct nbio_image_handle
{
#ifndef IS_SALAMANDER
   struct texture_image ti;
#endif
   bool is_blocking;
   bool is_blocking_on_processing;
   bool is_finished;
   bool is_finished_with_processing;
   transfer_cb_t  cb;
   struct rpng_t *handle;
   unsigned processing_pos_increment;
   unsigned pos_increment;
   uint64_t frame_count;
   uint64_t processing_frame_count;
   int processing_final_state;
   msg_queue_t *msg_queue;
} nbio_image_handle_t;

typedef struct nbio_handle
{
   nbio_image_handle_t image;
   bool is_blocking;
   bool is_finished;
   transfer_cb_t  cb;
   struct nbio_t *handle;
   unsigned pos_increment;
   uint64_t frame_count;
   msg_queue_t *msg_queue;
} nbio_handle_t;

#ifdef HAVE_NETWORKING
typedef struct http_handle
{
   struct
   {
      struct http_connection_t *handle;
      transfer_cb_t  cb;
      char elem1[PATH_MAX_LENGTH];
   } connection;
   msg_queue_t *msg_queue;
   struct http_t *handle;
   transfer_cb_t  cb;
} http_handle_t;
#endif

/* All data runloop-related globals go here. */

struct data_runloop
{
#ifdef HAVE_NETWORKING
   http_handle_t http;
#endif

#ifdef HAVE_LIBRETRODB
   struct
   {
   } db;
#endif

   nbio_handle_t nbio;
};

/* All libretro runloop-related globals go here. */

struct runloop
{
   /* Lifecycle state checks. */
   bool is_paused;
   bool is_idle;
   bool is_menu;
   bool is_slowmotion;

   struct
   {
      struct
      {
         unsigned count;
         unsigned max;
         struct
         {
            struct
            {
               struct
               {
                  bool is_updated;
               } label;

               struct
               {
                  bool is_active;
               } animation;

               struct
               {
                  bool dirty;
               } framebuf;

               struct
               {
                  bool active;
               } action;
            } menu;
         } current;
      } video;

      struct
      {
         retro_time_t minimum_time;
         retro_time_t last_time;
      } limit;
   } frames;

   struct
   {
      unsigned buffer_free_samples[AUDIO_BUFFER_FREE_SAMPLES_COUNT];
      uint64_t buffer_free_samples_count;

      retro_time_t frame_time_samples[MEASURE_FRAME_TIME_SAMPLES_COUNT];
      uint64_t frame_time_samples_count;
   } measure_data;

   msg_queue_t *msg_queue;
};

/* Public data structures. */
extern struct data_runloop g_data_runloop;
extern struct runloop g_runloop;

/**
 * rarch_main_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on successful run, 1 if we have to wait until button input in order
 * to wake up the loop, -1 if we forcibly quit out of the RetroArch iteration loop. 
 **/
int rarch_main_iterate(void);

void rarch_main_msg_queue_push(const char *msg, unsigned prio,
      unsigned duration, bool flush);

const char *rarch_main_msg_queue_pull(void);

void rarch_main_data_iterate(void);

void rarch_main_data_init_queues(void);

#ifdef __cplusplus
}
#endif

#endif
