/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2014-2015 - Jay McCarthy
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

#include "general.h"
#ifdef HAVE_NETWORKING
#include "net_http.h"

int cb_core_updater_download(void *data_, size_t len);
int cb_core_updater_list(void *data_, size_t len);

/**
 * HTTP
 *
 **/

/**
 * rarch_main_iterate_http_transfer:
 *
 * Resumes HTTP transfer update.
 *
 * Returns: 0 when finished, -1 when we should continue
 * with the transfer on the next frame.
 **/
static int rarch_main_iterate_http_transfer(void)
{
   size_t pos = 0, tot = 0;

   if (!net_http_update(g_extern.http.handle, &pos, &tot))
   {
#ifdef _WIN32
		RARCH_LOG("%.9I64u / %.9I64u       \r", (unsigned long long)pos, (unsigned long long)tot);
#else
		RARCH_LOG("%.9llu / %.9llu        \r", (unsigned long long)pos, (unsigned long long)tot);
#endif
      return -1;
   }

   return 0;
}

static int rarch_main_iterate_http_parse(void)
{
   size_t len;
   char *data = (char*)net_http_data(g_extern.http.handle, &len, false);

   if (data && g_extern.http.cb)
      g_extern.http.cb(data, len);

   net_http_delete(g_extern.http.handle);

   g_extern.http.handle = NULL;
   msg_queue_clear(g_extern.http.msg_queue);

   return 0;
}

/**
 * rarch_main_iterate_http_transfer:
 *
 * Polls HTTP message queue to see if any new URLs 
 * are pending.
 *
 * If handle is freed, will set up a new http handle. 
 * The transfer will be started on the next frame.
 *
 * Returns: 0 when an URL has been pulled and we will
 * begin transferring on the next frame. Returns -1 if
 * no HTTP URL has been pulled. Do nothing in that case.
 **/
static int rarch_main_iterate_http_poll(void)
{
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   struct string_list *str_list = NULL;
   const char *url = msg_queue_pull(g_extern.http.msg_queue);

   if (!url)
      return -1;

   /* Can only deal with one HTTP transfer at a time for now */
   if (g_extern.http.handle)
      return -1; 

   str_list         = string_split(url, "|"); 

   if (!str_list)
      return -1;

   if (str_list->size > 0)
      strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));
   if (str_list->size > 1)
      strlcpy(elem1, str_list->elems[1].data, sizeof(elem1));

   g_extern.http.handle = net_http_new(elem0);

   if (!g_extern.http.handle)
   {
      RARCH_ERR("Could not create new HTTP session handle.\n");
      string_list_free(str_list);
      return -1;
   }

   g_extern.http.cb     = NULL;

   if (elem1[0] != '\0')
   {
      if (!strcmp(elem1, "cb_core_updater_download"))
         g_extern.http.cb = &cb_core_updater_download;
      if (!strcmp(elem1, "cb_core_updater_list"))
         g_extern.http.cb = &cb_core_updater_list;
   }

   string_list_free(str_list);
   
   return 0;
}
#endif

#ifdef HAVE_OVERLAY
static int cb_image_overlay(void *data, size_t len)
{
   g_extern.images.is_finished   = true;
   g_extern.images.is_blocking   = true;
   g_extern.images.is_processing = true;

   return 0;
}

static int cb_nbio_image_overlay(void *data, size_t len)
{
   (void)len;

   g_extern.nbio.is_finished = true;

   g_extern.images.handle = (struct rpng_t*)calloc(1, sizeof(struct rpng_t));

   if (!g_extern.images.handle)
      return -1;

   g_extern.images.handle->buff_data = (uint8_t*)data;

   if (!rpng_nbio_load_image_argb_start(g_extern.images.handle))
   {
      rpng_nbio_load_image_free(g_extern.images.handle);
      return -1;
   }

   g_extern.images.cb        = &cb_image_overlay;
   g_extern.nbio.is_blocking = true;

   return 0;
}
#endif

/**
 * IMAGES
 *
 **/

static int cb_image_default(void *data, size_t len)
{
   (void)data;
   (void)len;

   g_extern.images.is_blocking = false;
   g_extern.images.is_finished = true;

   return 0;
}

static int rarch_main_iterate_image_poll(void)
{
   char newstring[PATH_MAX_LENGTH];
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   struct string_list *str_list = NULL;
   const char *path = msg_queue_pull(g_extern.nbio.msg_queue);

   if (!path)
      return -1;

   /* Can only deal with one image transfer at a time for now */
   if (g_extern.images.handle)
      return -1; 

   str_list         = string_split(path, "|"); 

   if (!str_list)
      goto error;

   if (str_list->size > 0)
      strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));
   if (str_list->size > 1)
      strlcpy(elem1, str_list->elems[1].data, sizeof(elem1));

   if (elem1[0] != '\0')
   {
#ifdef HAVE_OVERLAY
      if (!strcmp(elem1, "cb_image_overlay_load"))
         g_extern.nbio.cb = &cb_nbio_image_overlay;
      else
#endif
         g_extern.nbio.cb = &cb_image_default;
   }

   strlcpy(newstring, elem0, sizeof(newstring));
   strlcat(newstring, "|",   sizeof(newstring));
   strlcat(newstring, elem1,   sizeof(newstring));

   /* We need to load the image file first. */
   msg_queue_clear(g_extern.nbio.msg_queue);
   msg_queue_push(g_extern.nbio.msg_queue, newstring, 1, 180);

   string_list_free(str_list);

   return 0;

error:
   if (str_list)
      string_list_free(str_list);

   return -1;
}

static int rarch_main_iterate_image_transfer(void)
{
   if (rpng_nbio_load_image_argb_iterate(
            g_extern.images.handle->buff_data,
            g_extern.images.handle))
   {
      g_extern.images.handle->buff_data += 
         4 + 4 + g_extern.images.handle->chunk.size + 4;
      return 0;
   }

   return -1;
}

static int rarch_main_iterate_image_parse_free(void)
{
   if (!g_extern.images.is_finished)
      return -1;

   rpng_nbio_load_image_free(g_extern.images.handle);
   g_extern.images.handle      = NULL;
   g_extern.images.is_blocking = false;
   g_extern.images.is_finished = false;

   msg_queue_clear(g_extern.images.msg_queue);

   return 0;
}

static int rarch_main_iterate_image_parse(void)
{
   size_t len = 0;

   if (g_extern.images.handle && g_extern.images.cb)
      g_extern.images.cb(g_extern.images.handle, len);

   return 0;
}

/**
 * NBIO
 *
 **/

static int cb_nbio_default(void *data, size_t len)
{
   (void)data;
   (void)len;

   g_extern.nbio.is_blocking = false;
   g_extern.nbio.is_finished = true;

   return 0;
}


static int rarch_main_iterate_nbio_poll(void)
{
   struct nbio_t* handle;
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   struct string_list *str_list = NULL;
   const char *path = msg_queue_pull(g_extern.nbio.msg_queue);

   if (!path)
      return -1;

   /* Can only deal with one NBIO transfer at a time for now */
   if (g_extern.nbio.handle)
      return -1; 

   str_list         = string_split(path, "|"); 

   if (!str_list)
      goto error;

   if (str_list->size > 0)
      strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));
   if (str_list->size > 1)
      strlcpy(elem1, str_list->elems[1].data, sizeof(elem1));

   handle = nbio_open(elem0, NBIO_READ);

   if (!handle)
   {
      RARCH_ERR("Could not create new file loading handle.\n");
      goto error;
   }

   g_extern.nbio.handle      = handle;
   g_extern.nbio.cb          = NULL;
   g_extern.nbio.is_blocking = false;
   g_extern.nbio.is_finished = false;

   if (elem1[0] != '\0')
   {
#ifdef HAVE_OVERLAY
      if (!strcmp(elem1, "cb_image_overlay_load"))
         g_extern.nbio.cb = &cb_nbio_image_overlay;
      else
#endif
         g_extern.nbio.cb = &cb_nbio_default;
   }

   nbio_begin_read(handle);

   string_list_free(str_list);

   return 0;

error:
   if (str_list)
      string_list_free(str_list);

   return -1;
}

static int rarch_main_iterate_nbio_transfer(void)
{
   if (!nbio_iterate(g_extern.nbio.handle))
      return -1;

   return 0;
}

static int rarch_main_iterate_nbio_parse_free(void)
{
   if (!g_extern.nbio.is_finished)
      return -1;

   nbio_free(g_extern.nbio.handle);
   g_extern.nbio.handle      = NULL;
   g_extern.nbio.is_blocking = false;
   g_extern.nbio.is_finished = false;

   msg_queue_clear(g_extern.nbio.msg_queue);

   return 0;
}

static int rarch_main_iterate_nbio_parse(void)
{
   size_t len = 0;
   char *data = (char*)nbio_get_ptr(g_extern.nbio.handle, &len);

   if (data && g_extern.nbio.cb)
      g_extern.nbio.cb(data, len);

   return 0;
}

/**
 *
 * RDB
 *
 **/

#ifdef HAVE_MENU
static void rarch_main_iterate_rdl(void)
{
   if (!driver.menu->rdl)
      return;

   if (driver.menu->rdl->blocking)
   {
      /* Do nonblocking I/O transfers here. */
      return;
   }

   if (!driver.menu->rdl->iterating)
   {
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, "Scanning of directory finished.\n", 1, 180);

      database_info_write_rdl_free(driver.menu->rdl);
      driver.menu->rdl = NULL;
      return;
   }

   database_info_write_rdl_iterate(driver.menu->rdl);

}
#endif


void do_data_state_checks(void)
{
   if (g_extern.nbio.handle)
   {
      if (!g_extern.nbio.is_blocking)
      {
         if (!rarch_main_iterate_nbio_transfer())
            rarch_main_iterate_nbio_parse();
      }
      else if (g_extern.nbio.is_finished)
         rarch_main_iterate_nbio_parse_free();
   }
   else
      rarch_main_iterate_nbio_poll();

   if (g_extern.images.handle)
   {
      if (!g_extern.images.is_blocking)
      {
         if (!rarch_main_iterate_image_transfer())
            rarch_main_iterate_image_parse();
      }
      else if (g_extern.images.is_finished)
         rarch_main_iterate_image_parse_free();
   }
   else
      rarch_main_iterate_image_poll();


#ifdef HAVE_NETWORKING
   if (g_extern.http.handle)
   {
      if (!rarch_main_iterate_http_transfer())
         rarch_main_iterate_http_parse();
   }
   else
      rarch_main_iterate_http_poll();
#endif

#ifdef HAVE_MENU
   if (driver.menu && driver.menu->rdl)
      rarch_main_iterate_rdl();
#endif
}
