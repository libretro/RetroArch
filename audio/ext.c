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

#include "ext/ssnes_audio.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include "driver.h"
#include "dynamic.h"
#include "general.h"
#include <sys/types.h>

typedef struct audio_ext
{
   dylib_t lib;
   const ssnes_audio_driver_t *driver;
   void *handle;
   bool is_float;
} audio_ext_t;

static void audio_ext_free(void *data)
{
   audio_ext_t *ext = data;
   if (ext)
   {
      if (ext->driver && ext->handle)
         ext->driver->free(ext->handle);
      if (ext->lib)
         dylib_close(ext->lib);
      free(ext);
   }
}

static void* audio_ext_init(const char *device, int rate, int latency)
{
   if (!(*g_settings.audio.external_driver))
   {
      SSNES_ERR("Please define an external audio driver.\n");
      return NULL;
   }

   audio_ext_t *ext = calloc(1, sizeof(*ext));
   if (!ext)
      return NULL;

   ext->lib = dylib_load(g_settings.audio.external_driver);
   if (!ext->lib)
   {
      SSNES_ERR("Failed to load external library \"%s\"\n", g_settings.audio.external_driver);
      goto error;
   }

   const ssnes_audio_driver_t* (*plugin_load)(void) = dylib_proc(ext->lib, "ssnes_audio_driver_init");

   if (!plugin_load)
   {
      SSNES_ERR("Failed to find symbol \"ssnes_audio_driver_init\" in plugin.\n");
      goto error;
   }

   ext->driver = plugin_load();
   if (!ext->driver)
   {
      SSNES_ERR("Received invalid driver from plugin.\n");
      goto error;
   }

   SSNES_LOG("Loaded external audio driver: \"%s\"\n", ext->driver->ident ? ext->driver->ident : "Unknown");

   if (ext->driver->api_version != SSNES_API_VERSION)
   {
      SSNES_ERR("API mismatch in external video plugin. SSNES: %d, Plugin: %d ...\n", SSNES_API_VERSION, ext->driver->api_version);
      goto error;
   }

   ssnes_audio_driver_info_t info = {
      .device = device,
      .sample_rate = rate,
      .latency = latency
   };

   ext->handle = ext->driver->init(&info);
   if (!ext->handle)
   {
      SSNES_ERR("Failed to init audio driver.\n");
      goto error;
   }

   return ext;

error:
   audio_ext_free(ext);
   return NULL;
}

static ssize_t audio_ext_write(void *data, const void *buf, size_t size)
{
   audio_ext_t *ext = data;
   unsigned frame_size = ext->is_float ? (2 * sizeof(float)) : (2 * sizeof(int16_t));
   size /= frame_size;

   int ret = ext->driver->write(ext->handle, buf, size);
   if (ret < 0)
      return -1;
   return ret * frame_size;
}

static bool audio_ext_start(void *data)
{
   audio_ext_t *ext = data;
   return ext->driver->start(ext->handle);
}

static bool audio_ext_stop(void *data)
{
   audio_ext_t *ext = data;
   return ext->driver->stop(ext->handle);
}

static void audio_ext_set_nonblock_state(void *data, bool toggle)
{
   audio_ext_t *ext = data;
   ext->driver->set_nonblock_state(ext->handle, toggle);
}

static bool audio_ext_use_float(void *data)
{
   audio_ext_t *ext = data;
   ext->is_float = ext->driver->use_float(ext->handle);
   return ext->is_float;
}


const audio_driver_t audio_ext = {
   .init = audio_ext_init,
   .write = audio_ext_write,
   .stop = audio_ext_stop,
   .start = audio_ext_start,
   .set_nonblock_state = audio_ext_set_nonblock_state,
   .use_float = audio_ext_use_float,
   .free = audio_ext_free,
   .ident = "ext"
};
