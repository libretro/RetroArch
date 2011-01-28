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


#include "driver.h"
#include <stdlib.h>
#include "xaudio-c.h"
#include "general.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct
{
   xaudio2_t *xa;
   bool nonblock;
} xa_t;

static xaudio2_new_t pxanew = NULL;
static xaudio2_write_t pxawrite = NULL;
static xaudio2_write_avail_t pxawrite_avail = NULL;
static xaudio2_free_t pxafree = NULL;
static HMODULE lib = NULL;

#define LIB_NAME "xaudio-c.dll"
#define SYM(X) ((void*)GetProcAddress(lib, "xaudio2_" #X))

static void deinit_lib(void)
{
   FreeModule(lib);
   lib = NULL;
}

static bool init_lib(void)
{
   if (lib)
      return true;

   lib = LoadLibrary(LIB_NAME);
   if (!lib)
      return false;

   pxanew = SYM(new);
   pxawrite = SYM(write);
   pxawrite_avail = SYM(write_avail);
   pxafree = SYM(free);

   if (!pxanew || !pxawrite || !pxawrite_avail || !pxafree)
   {
      deinit_lib();
      return false;
   }
   return true;
}

// Interesting hack from http://www-graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static inline uint32_t next_pow2(uint32_t v)
{
   v--;
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
   v++;
   return v;
}

static void* __xa_init(const char* device, int rate, int latency)
{
   if (!init_lib())
      return NULL;

   xa_t *xa = calloc(1, sizeof(xa_t));
   if (xa == NULL)
      return NULL;

   size_t bufsize = latency * rate / 1000;
   bufsize = next_pow2(bufsize);

   SSNES_LOG("XAudio2: Requesting %d ms latency, using %d ms latency.\n", latency, (int)bufsize * rate / 1000);

   xa->xa = pxanew(rate, 2, 16, bufsize << 2);
   if (!xa->xa)
   {
      SSNES_ERR("Failed to init XAudio2.\n");
      free(xa);
      return NULL;
   }
   return xa;
}

static ssize_t __xa_write(void* data, const void* buf, size_t size)
{
   xa_t *xa = data;
   if (xa->nonblock)
   {
      size_t avail = pxawrite_avail(xa->xa);
      if (avail < size)
         size = avail;
   }
   return pxawrite(xa->xa, buf, size);
}

static bool __xa_stop(void *data)
{
   (void)data;
   return true;
}

static void __xa_set_nonblock_state(void *data, bool state)
{
   xa_t *xa = data;
   xa->nonblock = state;
}

static bool __xa_start(void *data)
{
   (void)data;
   return true;
}

static void __xa_free(void *data)
{
   xa_t *xa = data;
   if (xa && xa->xa)
   {
      if (pxafree)
         pxafree(xa->xa);
      free(xa);
   }
   deinit_lib();
}

const audio_driver_t audio_xa = {
   .init = __xa_init,
   .write = __xa_write,
   .stop = __xa_stop,
   .start = __xa_start,
   .set_nonblock_state = __xa_set_nonblock_state,
   .free = __xa_free,
   .ident = "xaudio"
};

   


   
   
