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

#ifndef __MMDEVICE_ACTIVE_H
#define __MMDEVICE_ACTIVE_H

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_inline.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

/* Ids of the endpoints this process holds open - one per data flow,
 * since a render device and a capture device are open at the same time
 * whenever the microphone driver is in use. The endpoint notifications
 * fire for every audio device in the system, so these are what tell "a
 * device we opened changed" from "some unrelated device changed".
 *
 * The notification callbacks run on an MMDevice thread while init and
 * deinit run on the audio threads. A flag guards the table: what it
 * guards is a compare or a copy, nothing that waits on another thread,
 * and with no lock object to make there is none that can fail to be
 * made, or be made twice by two first callers. */

#define MMDEVICE_FLOW_COUNT 2

typedef struct
{
   wchar_t *id[MMDEVICE_FLOW_COUNT];
   retro_atomic_int_t busy;
} mmdevice_active_t;

static INLINE void mmdevice_active_lock(mmdevice_active_t *a)
{
#ifdef HAVE_THREADS
   while (!retro_atomic_cas_int(&a->busy, 0, 1))
      sthread_yield();
#endif
}

static INLINE void mmdevice_active_unlock(mmdevice_active_t *a)
{
   retro_atomic_store_release_int(&a->busy, 0);
}

/* Records @id (NULL: none) as the open endpoint of @flow. */
static INLINE void mmdevice_active_set(mmdevice_active_t *a,
      unsigned flow, const wchar_t *id)
{
   wchar_t *copy = NULL;
   wchar_t *old;

   if (flow >= MMDEVICE_FLOW_COUNT)
      return;
   if (id)
   {
      size_t _len = wcslen(id) + 1;
      if ((copy = (wchar_t*)malloc(_len * sizeof(wchar_t))))
         memcpy(copy, id, _len * sizeof(wchar_t));
   }

   mmdevice_active_lock(a);
   old          = a->id[flow];
   a->id[flow]  = copy;
   mmdevice_active_unlock(a);

   free(old);
}

/* Whether a notification about @id concerns an endpoint we hold open.
 * With no id, or nothing recorded yet, it cannot tell, so it says yes. */
static INLINE bool mmdevice_active_match(mmdevice_active_t *a,
      const wchar_t *id)
{
   bool match = false;
   unsigned i;

   if (!id)
      return true;

   mmdevice_active_lock(a);
   if (!a->id[0] && !a->id[1])
      match = true;
   else
      for (i = 0; i < MMDEVICE_FLOW_COUNT; i++)
         if (a->id[i] && !wcscmp(a->id[i], id))
         {
            match = true;
            break;
         }
   mmdevice_active_unlock(a);
   return match;
}

#endif
