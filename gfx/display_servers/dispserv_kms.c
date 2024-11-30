/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <math.h>

#include <compat/strl.h>
#include <string/stdstring.h>

#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif


#include "../video_display_server.h"
#include "../../retroarch.h"
#include "../video_crt_switch.h" /* Needed to set aspect for low resolution in Linux */
#include "../common/drm_common.h"
#include "../../verbosity.h"

typedef struct
{
   int crt_name_id;
   int monitor_index;
   unsigned opacity;
   uint8_t flags;
   char crt_name[16];
   char new_mode[256];
   char old_mode[256];
   char orig_output[256];
} dispserv_kms_t;

static bool kms_display_server_set_resolution(void *data,
      unsigned width, unsigned height, int int_hz, float hz,
      int center, int monitor_index, int xoffset, int padjust)
{
   unsigned curr_width               = 0;
   unsigned curr_height              = 0;
   float curr_refreshrate            = 0;
   bool retval = false;
   int reinit_flags                  = DRIVERS_CMD_ALL;
   dispserv_kms_t *dispserv = (dispserv_kms_t*)data;

   if (!dispserv)
      return false;

   if (g_drm_mode)
   {
      curr_refreshrate = drm_calc_refresh_rate(g_drm_mode);
      curr_width       = g_drm_mode->hdisplay;
      curr_height      = g_drm_mode->vdisplay;
   }
   RARCH_DBG("[DRM]: Display server set resolution - incoming: %d x %d, %f Hz\n",width, height, hz);

   if (width == 0)
      width = curr_width;
   if (height == 0)
      height = curr_height;
   if (hz == 0)
      hz = curr_refreshrate;
   
   /* set core refresh from hz */
   video_monitor_set_refresh_rate(hz);

   RARCH_DBG("[DRM]: Display server set resolution - actual: %d x %d, %f Hz\n",width, height, hz);

   retval = video_driver_set_video_mode(width, height, true);

   /* Reinitialize drivers. */
   command_event(CMD_EVENT_REINIT, &reinit_flags);

   return retval;
}

/* TODO: move to somewhere common as it is reused from dispserv_win32.c */
/* Display resolution list qsort helper function */
static int resolution_list_qsort_func(
      const video_display_config_t *a, const video_display_config_t *b)
{
   char str_a[64];
   char str_b[64];

   if (!a || !b)
      return 0;

   str_a[0] = str_b[0] = '\0';

   snprintf(str_a, sizeof(str_a), "%04dx%04d (%d Hz)",
         a->width,
         a->height,
         a->refreshrate);

   snprintf(str_b, sizeof(str_b), "%04dx%04d (%d Hz)",
         b->width,
         b->height,
         b->refreshrate);

   return strcasecmp(str_a, str_b);
}

static void *kms_display_server_get_resolution_list(
      void *data, unsigned *len)
{
   unsigned i                        = 0;
   unsigned j                        = 0;
   unsigned count                    = 0;
   unsigned curr_width               = 0;
   unsigned curr_height              = 0;
   unsigned curr_bpp                 = 0;
   bool curr_interlaced              = false;
   bool curr_dblscan                 = false;
   float curr_refreshrate            = 0;
   unsigned curr_orientation         = 0;
   struct video_display_config *conf = NULL;


   if (g_drm_mode)
   {
      curr_refreshrate = drm_calc_refresh_rate(g_drm_mode);
      curr_width       = g_drm_mode->hdisplay;
      curr_height      = g_drm_mode->vdisplay;
      curr_bpp         = 32;
      curr_interlaced  = (g_drm_mode->flags & DRM_MODE_FLAG_INTERLACE) ? true : false;
      curr_dblscan     = (g_drm_mode->flags & DRM_MODE_FLAG_DBLSCAN)   ? true : false;
   }

   *len = g_drm_connector->count_modes;
   if (!(conf = (struct video_display_config*)
      calloc(*len, sizeof(struct video_display_config))))
      return NULL;

   for (i = 0, j = 0; (int)i < g_drm_connector->count_modes; i++)
   {
      conf[j].width       = g_drm_connector->modes[i].hdisplay;
      conf[j].height      = g_drm_connector->modes[i].vdisplay;
      conf[j].bpp         = 32;
      conf[j].refreshrate = floor(drm_calc_refresh_rate(&g_drm_connector->modes[i]));
      conf[j].refreshrate_float = drm_calc_refresh_rate(&g_drm_connector->modes[i]);
      conf[j].interlaced  = (g_drm_connector->modes[i].flags & DRM_MODE_FLAG_INTERLACE) ? true : false;
      conf[j].dblscan     = (g_drm_connector->modes[i].flags & DRM_MODE_FLAG_DBLSCAN)   ? true : false;
      conf[j].idx         = j;
      conf[j].current     = false;

      if (     (conf[j].width       == curr_width)
            && (conf[j].height      == curr_height)
            && (conf[j].bpp         == curr_bpp)
            && (conf[j].refreshrate_float == curr_refreshrate)
            && (conf[j].interlaced  == curr_interlaced)
            && (conf[j].dblscan     == curr_dblscan)
         )
         conf[j].current  = true;
      j++;
   }

   qsort(
         conf, count,
         sizeof(video_display_config_t),
         (int (*)(const void *, const void *))
               resolution_list_qsort_func);

   return conf;
}

/* TODO: screen orientation has support in DRM via planes, although not really exposed via xf86drm */
#if 0
static void kms_display_server_set_screen_orientation(void *data,
      enum rotation rotation)
{
}

static enum rotation kms_display_server_get_screen_orientation(void *data)
{
   int i, j;
   enum rotation     rotation     = ORIENTATION_NORMAL;
   dispserv_kms_t *dispserv       = (dispserv_kms_t*)data;
   return rotation;
}
#endif

static void* kms_display_server_init(void)
{
   dispserv_kms_t *dispserv = (dispserv_kms_t*)calloc(1, sizeof(*dispserv));

   if (dispserv)
      return dispserv;
   return NULL;
}

static void kms_display_server_destroy(void *data)
{
   dispserv_kms_t *dispserv       = (dispserv_kms_t*)data;
   if (dispserv)
      free(dispserv);
}

static bool kms_display_server_set_window_opacity(void *data, unsigned opacity)
{
   return true;
}

static uint32_t kms_display_server_get_flags(void *data)
{
   uint32_t             flags   = 0;
   BIT32_SET(flags, DISPSERV_CTX_CRT_SWITCHRES);

   return flags;
}

const video_display_server_t dispserv_kms = {
   kms_display_server_init,
   kms_display_server_destroy,
   kms_display_server_set_window_opacity,
   NULL, /* set_window_progress */
   NULL, /* set window decorations */
   kms_display_server_set_resolution,
   kms_display_server_get_resolution_list,
   NULL, /* get output options */
   NULL, /* kms_display_server_set_screen_orientation */
   NULL, /* kms_display_server_get_screen_orientation */
   kms_display_server_get_flags,
   "kms"
};
