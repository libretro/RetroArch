/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "autosave.h"
#include "SDL.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "general.h"

struct autosave
{
   volatile bool quit;
   SDL_mutex *lock;

   SDL_mutex *cond_lock;
   SDL_cond *cond;
   SDL_Thread *thread;

   void *buffer;
   const void *snes_buffer;
   const char *path;
   size_t bufsize;
   unsigned interval;
};

static int autosave_thread(void *data)
{
   autosave_t *save = data;
   while (!save->quit)
   {
      autosave_lock(save);
      memcpy(save->buffer, save->snes_buffer, save->bufsize);
      autosave_unlock(save);

      // Should probably deal with this more elegantly.
      FILE *file = fopen(save->path, "wb");
      if (file)
      {
         SSNES_LOG("Autosaving SRAM to \"%s\"\n", save->path);
         bool failed = false;
         failed |= fwrite(save->buffer, 1, save->bufsize, file) != save->bufsize;
         failed |= fflush(file) != 0;
         failed |= fclose(file) != 0;
         if (failed)
            SSNES_WARN("Failed to autosave SRAM! Disk might be full.\n");
      }

      SDL_mutexP(save->cond_lock);
      if (!save->quit)
         SDL_CondWaitTimeout(save->cond, save->cond_lock, save->interval * 1000);
      SDL_mutexV(save->cond_lock);
   }

   return 0;
}

autosave_t *autosave_new(const char *path, const void *data, size_t size, unsigned interval)
{
   autosave_t *handle = calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   handle->bufsize = size;
   handle->interval = interval;
   handle->path = path;
   handle->buffer = malloc(size);
   handle->snes_buffer = data;

   if (!handle->buffer)
   {
      free(handle);
      return NULL;
   }

   handle->lock = SDL_CreateMutex();
   handle->cond_lock = SDL_CreateMutex();
   handle->cond = SDL_CreateCond();

   handle->thread = SDL_CreateThread(autosave_thread, handle);

   return handle;
}

void autosave_lock(autosave_t *handle)
{
   SDL_mutexP(handle->lock);
}

void autosave_unlock(autosave_t *handle)
{
   SDL_mutexV(handle->lock);
}

void autosave_free(autosave_t *handle)
{
   SDL_mutexP(handle->cond_lock);
   handle->quit = true;
   SDL_mutexV(handle->cond_lock);
   SDL_CondSignal(handle->cond);
   SDL_WaitThread(handle->thread, NULL);

   SDL_DestroyMutex(handle->lock);
   SDL_DestroyMutex(handle->cond_lock);
   SDL_DestroyCond(handle->cond);

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


