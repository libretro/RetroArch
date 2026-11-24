/*  RetroArch - A frontend for libretro.
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

/* Where the AVFoundation camera's authorization and session stand,
 * without waiting for either: access is asked when the camera is made
 * and answered later on the main queue, and a start that comes first
 * is kept until the session is set up. One word holds the phase and
 * the start, so a start and the answer each see the other. */

#ifndef __CAMERA_AVFOUNDATION_STATE_H
#define __CAMERA_AVFOUNDATION_STATE_H

#include <boolean.h>
#include <retro_inline.h>
#include <retro_atomic.h>

enum avf_phase
{
   AVF_PENDING = 0, /* access asked, not answered */
   AVF_READY   = 1, /* the session is set up */
   AVF_FAILED  = 2  /* refused, or the session could not be set up */
};

#define AVF_PHASE_MASK 3
#define AVF_START_WANTED 4 /* started and not stopped */

enum avf_start
{
   AVF_START_FAIL = 0, /* no session will come */
   AVF_START_NOW,      /* start the session */
   AVF_START_LATER     /* the session starts once set up */
};

typedef struct avf_state
{
   /* Bumped as each camera is made and freed: an answer for a camera
    * that has gone since is dropped */
   retro_atomic_int_t generation;
   retro_atomic_int_t word; /* enum avf_phase | AVF_START_WANTED */
} avf_state_t;

/* A new camera, pending and not started: its generation */
static INLINE int avf_state_begin(avf_state_t *s)
{
   retro_atomic_exchange_int(&s->word, AVF_PENDING);
   return retro_atomic_fetch_add_int(&s->generation, 1) + 1;
}

/* The camera has gone: its answer, when it comes, is dropped */
static INLINE void avf_state_end(avf_state_t *s)
{
   retro_atomic_fetch_and_int(&s->word, ~AVF_START_WANTED);
   retro_atomic_fetch_add_int(&s->generation, 1);
}

/* Whether an answer for camera @gen is still wanted */
static INLINE bool avf_state_current(avf_state_t *s, int gen)
{
   return retro_atomic_load_acquire_int(&s->generation) == gen;
}

/* The answer is in: the session set up or not. True when a start came
 * before it, so the session starts now. */
static INLINE bool avf_state_settle(avf_state_t *s, bool ready)
{
   int old = retro_atomic_fetch_or_int(&s->word,
         ready ? AVF_READY : AVF_FAILED);
   return ready && (old & AVF_START_WANTED);
}

static INLINE enum avf_phase avf_state_phase(avf_state_t *s)
{
   return (enum avf_phase)(retro_atomic_load_acquire_int(&s->word)
         & AVF_PHASE_MASK);
}

static INLINE enum avf_start avf_state_start(avf_state_t *s)
{
   int old = retro_atomic_fetch_or_int(&s->word, AVF_START_WANTED);
   switch (old & AVF_PHASE_MASK)
   {
      case AVF_READY:
         return AVF_START_NOW;
      case AVF_PENDING:
         return AVF_START_LATER;
      default:
         break;
   }
   retro_atomic_fetch_and_int(&s->word, ~AVF_START_WANTED);
   return AVF_START_FAIL;
}

static INLINE void avf_state_stop(avf_state_t *s)
{
   retro_atomic_fetch_and_int(&s->word, ~AVF_START_WANTED);
}

#endif
