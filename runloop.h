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

enum runloop_data_type
{
   DATA_TYPE_NONE = 0,
   DATA_TYPE_FILE,
   DATA_TYPE_IMAGE,
   DATA_TYPE_HTTP,
#ifdef HAVE_OVERLAY
   DATA_TYPE_OVERLAY,
#endif
};

/* All libretro runloop-related globals go here. */

typedef struct runloop
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
} runloop_t;

runloop_t *rarch_main_get_ptr(void);

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

void rarch_main_msg_queue_free(void);

void rarch_main_msg_queue_init(void);

void rarch_main_data_msg_queue_push(unsigned type,
      const char *msg, const char *msg2,
      unsigned prio, unsigned duration, bool flush);

void rarch_main_clear_state(void);

void rarch_main_data_clear_state(void);

void rarch_main_data_iterate(void);

void rarch_main_data_init_queues(void);

#ifdef __cplusplus
}
#endif

#endif
