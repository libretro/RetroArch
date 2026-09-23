/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018 - Alphanu / Ben Templeman
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

/* Display server for the legacy Broadcom VideoCore firmware stack
 * (Raspberry Pi without vc4-kms, the vc_egl context and the dispmanx
 * video driver). Its only job is modeline application: the engine in
 * gfx/modeline/ generates a timing like it does everywhere else, and
 * set puts it on the wire the way the firmware takes one - an
 * hdmi_timings gencmd, the DMT 87 custom-timing slot through
 * tvservice, fbset for the framebuffer - and then rebuilds the video
 * driver, whose dispmanx surface was sized for the old mode.
 *
 * get_edid reads the display's EDID over HDMI DDC, which gives the
 * edid CRT preset its ranges.
 *
 * The firmware has no mode list to enumerate and no way to hand back
 * the timing it is running, so the engine generates freely; and it
 * cannot restore a desktop mode on close, so, as before this server
 * existed, the last mode stays on the wire at exit. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bcm_host.h>
#include <interface/vmcs_host/vc_vchi_gencmd.h>
#include <interface/vmcs_host/vc_tvservice.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../video_display_server.h"
#include "../video_driver.h"
#include "../../driver.h"
#include "../../verbosity.h"

#ifdef HAVE_MODELINE
#include "../modeline/modeline_core.h"
#endif

typedef struct
{
   int unused;
} dispserv_videocore_t;

/* bcm_host_init() connects the gencmd and tvservice clients this
 * server talks through; it returns at once when the video driver
 * already made that call */
static void *videocore_display_server_init(void)
{
   dispserv_videocore_t *dispserv = (dispserv_videocore_t*)
      calloc(1, sizeof(*dispserv));
   bcm_host_init();
   return dispserv;
}

static void videocore_display_server_destroy(void *data)
{
   free(data);
}

static uint32_t videocore_display_server_get_flags(void *data)
{
   uint32_t flags = 0;
#ifdef HAVE_MODELINE
   BIT32_SET(flags, DISPSERV_CTX_MODELINE);
#endif
   return flags;
}

#ifdef HAVE_MODELINE
static bool videocore_display_server_modeline_open(void *data,
      const video_modeline_disp_t *ds)
{
   return true;
}

static void videocore_display_server_modeline_close(void *data) { }

static unsigned videocore_display_server_modeline_caps(void *data)
{
   return MODELINE_CAPS_ADD;
}

static int videocore_display_server_modeline_enum(void *data,
      video_modeline_t *modes, int max)
{
   return 0;
}

/* Nothing is staged: the firmware holds one custom timing, and set
 * writes it */
static bool videocore_display_server_modeline_add(void *data,
      video_modeline_t *mode)
{
   return true;
}

static bool videocore_display_server_modeline_flush(void *data)
{
   return true;
}

/* Through the gencmd client bcm_host_init() connected. A client of
 * our own, stopped afterwards, would tear down that shared one. */
static bool videocore_display_server_gencmd(const char *cmd)
{
   char reply[1024];
   int ret;

   reply[0] = '\0';
   ret      = vc_gencmd(reply, sizeof(reply), "%s", cmd);

   if (ret != 0 || strncmp(reply, "error=", 6) == 0)
   {
      RARCH_ERR("[VideoCore] \"%s\" failed: %s\n", cmd, reply);
      return false;
   }
   return true;
}

/* The engine's mode is X modeline shaped: sync start/end and totals
 * in pixels and lines, the vertical ones counting a whole frame when
 * interlaced, vfreq the field rate. hdmi_timings wants porches and
 * pulse widths, a polarity flag that inverts the sync (set for the
 * negative sync CRT presets use), and the frame rate. */
static bool videocore_display_server_modeline_set(void *data,
      video_modeline_t *mode)
{
   char cmd[256];
   char fbset[128];
   int hfp, hsp, hbp, vfp, vsp, vbp;
   double frame_rate;

   if (mode->doublescan)
   {
      RARCH_ERR("[VideoCore] hdmi_timings cannot scan a line twice, "
            "%ux%u doublescan rejected.\n",
            VIDEO_SCALE_W(mode->dims), VIDEO_SCALE_H(mode->dims));
      return false;
   }

   hfp        = mode->hbegin - mode->hactive;
   hsp        = mode->hend   - mode->hbegin;
   hbp        = mode->htotal - mode->hend;
   vfp        = mode->vbegin - mode->vactive;
   vsp        = mode->vend   - mode->vbegin;
   vbp        = mode->vtotal - mode->vend;
   frame_rate = mode->interlace ? mode->vfreq / 2.0 : mode->vfreq;

   if (hfp < 0 || hsp <= 0 || hbp < 0 || vfp < 0 || vsp <= 0 || vbp < 0)
   {
      RARCH_ERR("[VideoCore] Mode %dx%d has impossible porches.\n",
            mode->hactive, mode->vactive);
      return false;
   }

   snprintf(cmd, sizeof(cmd),
         "hdmi_timings %d %d %d %d %d %d %d %d %d %d 0 0 0 %f %d %f 1",
         mode->hactive, mode->hsync ? 0 : 1, hfp, hsp, hbp,
         mode->vactive, mode->vsync ? 0 : 1, vfp, vsp, vbp,
         frame_rate, mode->interlace ? 1 : 0, (double)mode->pclock);

   RARCH_LOG("[VideoCore] %s\n", cmd);
   if (!videocore_display_server_gencmd(cmd))
      return false;

   /* DMT 87 is the firmware's slot for the hdmi_timings just set */
   if (system("tvservice -e \"DMT 87\" > /dev/null") != 0)
   {
      RARCH_ERR("[VideoCore] tvservice could not select the custom timing.\n");
      return false;
   }

   snprintf(fbset, sizeof(fbset), "fbset -g %d %d %d %d 24 > /dev/null",
         mode->hactive, mode->vactive, mode->hactive, mode->vactive);
   if (system(fbset) != 0)
      RARCH_WARN("[VideoCore] fbset could not resize the framebuffer.\n");

   /* The dispmanx surface was sized for the old mode. Nothing of this
    * server is touched after the reinit: when the display server is
    * rebuilt with it, data is gone, and the CRT consumer has already
    * been told through crt_switch_display_server_lost(). */
   video_driver_reinit(DRIVER_VIDEO_MASK);
   return true;
}
#endif

/* The EDID over the HDMI DDC line, the way tvservice -d reads it:
 * the base block, then as many extension blocks as it announces and
 * out has room for. Composite and DPI outputs have no DDC, so the
 * first read fails there. */
static int videocore_display_server_get_edid(void *data,
      uint8_t *out, size_t max)
{
   int i;
   int ext;
   size_t len = 128;

   if (!out || max < 128)
      return -1;
   if (vc_tv_hdmi_ddc_read(0, 128, out) != 128)
      return -1;

   ext = out[0x7e];
   for (i = 0; i < ext && len + 128 <= max; i++, len += 128)
      if (vc_tv_hdmi_ddc_read((uint32_t)len, 128, out + len) != 128)
         break;

   return (int)len;
}

const video_display_server_t dispserv_videocore = {
   videocore_display_server_init,
   videocore_display_server_destroy,
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
   NULL, /* set_resolution */
   NULL, /* get_resolution_list */
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   videocore_display_server_get_flags,
   NULL, /* get_scanline */
   NULL, /* wait_vblank */
#ifdef HAVE_MODELINE
   NULL, /* modeline_list_outputs */
   videocore_display_server_modeline_open,
   videocore_display_server_modeline_close,
   videocore_display_server_modeline_caps,
   videocore_display_server_modeline_enum,
   videocore_display_server_modeline_add,
   videocore_display_server_modeline_add, /* update: same no-op */
   videocore_display_server_modeline_add, /* delete: same no-op */
   videocore_display_server_modeline_set,
   videocore_display_server_modeline_flush,
#else
   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
#endif
   videocore_display_server_get_edid,
   NULL, /* idle_wait */
   "videocore"
};
