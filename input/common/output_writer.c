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

#include <stdlib.h>
#include <stddef.h>

#include "output_writer.h"

#if defined(HAVE_THREADS) && !defined(_XBOX) && !defined(__WINRT__)

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#endif

#include <retro_atomic.h>
#include <rthreads/rthreads.h>

struct input_output_writer
{
   input_output_writer_cb cb;
   input_output_writer_timeout_cb timeout_cb;
   void *userdata;
   sthread_t *thread;
   retro_atomic_int_t quit;
#ifdef _WIN32
   HANDLE wake;
#else
   int wake[2];
#endif
};

static void input_output_writer_thread(void *data)
{
   input_output_writer_t *writer = (input_output_writer_t*)data;

   for (;;)
   {
      bool quit = false;
      int timeout = writer->timeout_cb
         ? writer->timeout_cb(writer->userdata) : -1;
#ifdef _WIN32
      if (WaitForSingleObject(writer->wake,
               timeout < 0 ? INFINITE : (DWORD)timeout) == WAIT_FAILED)
         quit = true;
#else
      bool readable = true;
      if (writer->timeout_cb)
      {
         struct pollfd pfd;
         int ready;
         pfd.fd     = writer->wake[0];
         pfd.events = POLLIN;
         ready      = poll(&pfd, 1, timeout);
         if (ready < 0 && errno == EINTR)
            continue;
         if (ready < 0 || (ready > 0 && !(pfd.revents & POLLIN)))
            quit = true;
         readable = ready > 0 && !quit;
      }
      if (readable)
      {
         char buf[64];
         ssize_t n = read(writer->wake[0], buf, sizeof(buf));
         if (n < 0 && errno == EINTR)
            continue;
         if (n <= 0)
            quit = true;
      }
#endif
      if (retro_atomic_load_acquire_int(&writer->quit))
         quit = true;
      writer->cb(writer->userdata, quit);
      if (quit)
         break;
   }
}

input_output_writer_t *input_output_writer_new_timed(
      input_output_writer_cb cb, input_output_writer_timeout_cb timeout_cb,
      void *userdata)
{
   input_output_writer_t *writer = (input_output_writer_t*)
      calloc(1, sizeof(*writer));

   if (!writer)
      return NULL;
   writer->timeout_cb = timeout_cb;
   writer->cb       = cb;
   writer->userdata = userdata;
   retro_atomic_store_release_int(&writer->quit, 0);

#ifdef _WIN32
   if (!(writer->wake = CreateEventA(NULL, FALSE, FALSE, NULL)))
   {
      free(writer);
      return NULL;
   }
#else
   if (pipe(writer->wake) != 0)
   {
      free(writer);
      return NULL;
   }
   /* a full pipe has wakes enough in it: the waker never waits */
   fcntl(writer->wake[1], F_SETFL,
         fcntl(writer->wake[1], F_GETFL) | O_NONBLOCK);
#endif

   if (!(writer->thread = sthread_create(input_output_writer_thread, writer)))
   {
#ifdef _WIN32
      CloseHandle(writer->wake);
#else
      close(writer->wake[0]);
      close(writer->wake[1]);
#endif
      free(writer);
      return NULL;
   }
   return writer;
}

void input_output_writer_wake(input_output_writer_t *writer)
{
   if (!writer)
      return;
#ifdef _WIN32
   SetEvent(writer->wake);
#else
   {
      char c = 1;
      ssize_t written;
      do
      {
         written = write(writer->wake[1], &c, 1);
      } while (written < 0 && errno == EINTR);
   }
#endif
}

void input_output_writer_free(input_output_writer_t *writer)
{
   if (!writer)
      return;
   retro_atomic_store_release_int(&writer->quit, 1);
   input_output_writer_wake(writer);
   sthread_join(writer->thread);
#ifdef _WIN32
   CloseHandle(writer->wake);
#else
   close(writer->wake[0]);
   close(writer->wake[1]);
#endif
   free(writer);
}

#else

input_output_writer_t *input_output_writer_new_timed(
      input_output_writer_cb cb, input_output_writer_timeout_cb timeout_cb,
      void *userdata)
{
   (void)timeout_cb;
   (void)cb;
   (void)userdata;
   return NULL;
}

void input_output_writer_wake(input_output_writer_t *writer) { (void)writer; }
void input_output_writer_free(input_output_writer_t *writer) { (void)writer; }

#endif

input_output_writer_t *input_output_writer_new(
      input_output_writer_cb cb, void *userdata)
{
   return input_output_writer_new_timed(cb, NULL, userdata);
}
