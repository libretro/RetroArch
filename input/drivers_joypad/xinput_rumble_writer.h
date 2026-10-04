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

#ifndef XINPUT_RUMBLE_WRITER_H__
#define XINPUT_RUMBLE_WRITER_H__

/* The XInput rumble writer, for the two XInput joypad drivers
 * (xinput_joypad.c, and xinput_hybrid_joypad.c, which desktop Windows
 * builds use). Included by each after its g_XInputSetState and
 * g_xinput_rumble_states, which it reads. */

#if defined(HAVE_THREADS) && !defined(_XBOX) && !defined(__WINRT__)
/* Rumble is written off the frontend's thread.
 *
 * XInputSetState() goes down to the controller's driver and returns
 * when the driver has taken it, and it was called from the frame loop
 * each time a core changed a motor's strength. The frontend's thread
 * now notes both motors' strengths in one atomic a controller and a
 * thread of the driver's own makes the call. Nothing is queued: the
 * writer reads what is wanted when it gets to it, so a controller slow
 * to answer is given the latest and not each strength in turn. There
 * is no lock; the writer is woken by an event. */
#define XINPUT_RUMBLE_THREAD
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

static retro_atomic_int_t xinput_rumble_want[4]; /* left << 16 | right */
static retro_atomic_int_t xinput_rumble_quit;
static HANDLE     xinput_rumble_wake   = NULL;
static sthread_t *xinput_rumble_thread = NULL;
/* what each controller was last told: set before the writer starts,
 * and the writer's own from then on */
static uint32_t   xinput_rumble_written[4];

static void xinput_rumble_thread_fn(void *data)
{
   unsigned i;
   uint32_t *written = xinput_rumble_written;

   for (;;)
   {
      WaitForSingleObject(xinput_rumble_wake, INFINITE);
      if (retro_atomic_load_acquire_int(&xinput_rumble_quit))
         break;
      for (i = 0; i < 4; i++)
      {
         XINPUT_VIBRATION v;
         uint32_t want = (uint32_t)retro_atomic_load_acquire_int(
               &xinput_rumble_want[i]);
         if (want == written[i])
            continue;
         v.wLeftMotorSpeed  = (uint16_t)(want >> 16);
         v.wRightMotorSpeed = (uint16_t)(want & 0xffff);
         if (g_XInputSetState)
            g_XInputSetState(i, &v);
         written[i] = want;
      }
   }
}

static void xinput_rumble_start(void)
{
   unsigned i;
   /* what the controllers were last told is where it starts from; the
    * writer is not left to read that for itself, since by the time it
    * runs a new strength may already be wanted */
   for (i = 0; i < 4; i++)
   {
      xinput_rumble_written[i] =
            ((uint32_t)g_xinput_rumble_states[i].wLeftMotorSpeed << 16)
            | g_xinput_rumble_states[i].wRightMotorSpeed;
      retro_atomic_store_release_int(&xinput_rumble_want[i],
            (int)xinput_rumble_written[i]);
   }
   retro_atomic_store_release_int(&xinput_rumble_quit, 0);
   if (!(xinput_rumble_wake = CreateEventA(NULL, FALSE, FALSE, NULL)))
      return;
   if (!(xinput_rumble_thread = sthread_create(xinput_rumble_thread_fn, NULL)))
   {
      CloseHandle(xinput_rumble_wake);
      xinput_rumble_wake = NULL;
   }
}

static void xinput_rumble_stop(void)
{
   if (!xinput_rumble_thread)
      return;
   retro_atomic_store_release_int(&xinput_rumble_quit, 1);
   SetEvent(xinput_rumble_wake);
   sthread_join(xinput_rumble_thread);
   xinput_rumble_thread = NULL;
   CloseHandle(xinput_rumble_wake);
   xinput_rumble_wake   = NULL;
}
#endif

#endif
