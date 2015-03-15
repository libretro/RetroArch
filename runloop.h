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

extern struct data_runloop g_data_runloop;

/**
 * rarch_main_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on successful run, 1 if we have to wait until button input in order
 * to wake up the loop, -1 if we forcibly quit out of the RetroArch iteration loop. 
 **/
int rarch_main_iterate(void);

void rarch_main_data_iterate(void);

void rarch_main_data_init_queues(void);

#ifdef __cplusplus
}
#endif

#endif
