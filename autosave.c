/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "autosave.h"
#include "thread.h"
#include <stdlib.h>
#include "boolean.h"
#include <string.h>
#include <stdio.h>
#include "general.h"

struct autosave
{
   volatile bool quit;
   slock_t *lock;

   slock_t *cond_lock;
   scond_t *cond;
   sthread_t *thread;

   void *buffer;
   const void *retro_buffer;
   const char *path;
   size_t bufsize;
   unsigned interval;
};

static void autosave_thread(void *data)
{
   autosave_t *save = (autosave_t*)data;

   bool first_log = true;

   while (!save->quit)
   {
      autosave_lock(save);
      bool differ = memcmp(save->buffer, save->retro_buffer, save->bufsize) != 0;
      if (differ)
         memcpy(save->buffer, save->retro_buffer, save->bufsize);
      autosave_unlock(save);

      if (differ)
      {
         // Should probably deal with this more elegantly.
         FILE *file = fopen(save->path, "wb");
         if (file)
         {
            // Avoid spamming down stderr ... :)
            if (first_log)
            {
               RARCH_LOG("Autosaving SRAM to \"%s\", will continue to check every %u seconds ...\n", save->path, save->interval);
               first_log = false;
            }
            else
               RARCH_LOG("SRAM changed ... autosaving ...\n");

            bool failed = false;
            failed |= fwrite(save->buffer, 1, save->bufsize, file) != save->bufsize;
            failed |= fflush(file) != 0;
            failed |= fclose(file) != 0;
            if (failed)
               RARCH_WARN("Failed to autosave SRAM. Disk might be full.\n");
         }
      }

      slock_lock(save->cond_lock);
      if (!save->quit)
         scond_wait_timeout(save->cond, save->cond_lock, save->interval * 1000000LL);
      slock_unlock(save->cond_lock);
   }
}

autosave_t *autosave_new(const char *path, const void *data, size_t size, unsigned interval)
{
   autosave_t *handle = (autosave_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   handle->bufsize = size;
   handle->interval = interval;
   handle->path = path;
   handle->buffer = malloc(size);
   handle->retro_buffer = data;

   if (!handle->buffer)
   {
      free(handle);
      return NULL;
   }
   memcpy(handle->buffer, handle->retro_buffer, handle->bufsize);

   handle->lock = slock_new();
   handle->cond_lock = slock_new();
   handle->cond = scond_new();

   handle->thread = sthread_create(autosave_thread, handle);

   return handle;
}

void autosave_lock(autosave_t *handle)
{
   slock_lock(handle->lock);
}

void autosave_unlock(autosave_t *handle)
{
   slock_unlock(handle->lock);
}

void autosave_free(autosave_t *handle)
{
   slock_lock(handle->cond_lock);
   handle->quit = true;
   slock_unlock(handle->cond_lock);
   scond_signal(handle->cond);
   sthread_join(handle->thread);

   slock_free(handle->lock);
   slock_free(handle->cond_lock);
   scond_free(handle->cond);

   free(handle->buffer);
   free(handle);
}

void lock_autosave(void)
{
   for (unsigned i = 0; i < sizeof(g_extern.autosave)/sizeof(g_extern.autosave[0]); i++)
   {
      if (g_extern.autosave[i])
         autosave_lock(g_extern.autosave[i]);
   }
}

void unlock_autosave(void)
{
   for (unsigned i = 0; i < sizeof(g_extern.autosave)/sizeof(g_extern.autosave[0]); i++)
   {
      if (g_extern.autosave[i])
         autosave_unlock(g_extern.autosave[i]);
   }
}


