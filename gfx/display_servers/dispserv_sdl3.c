/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

/* SDL3 display server: a thin layer over the SDL3 window that switches
 * among the display modes SDL lists. It is not a custom timing path:
 * SDL cannot create a 256x240@60.0988 modeline, so modeline caps are 0
 * and the engine can only pick a listed mode. Which server drives mode
 * switching is decided in video_driver.c from the
 * video_sdl_display_server setting: never, only when the native server
 * cannot switch modes, or always. Metrics, orientation and window
 * decorations stay with the native server.
 *
 * SDL3 differences from the SDL2 server: displays are ids rather than
 * indices, the mode list is an SDL-owned array, refresh rates are
 * floats, and exclusive fullscreen is a fullscreen window with a mode
 * set (NULL mode is desktop fullscreen). */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <SDL3/SDL.h>

#include <compat/strl.h>

#include "../video_display_server.h"
#include "../video_driver.h"
#include "../../verbosity.h"

typedef struct
{
   SDL_DisplayMode desktop;
   SDL_DisplayID display;
   bool opened;
   bool mode_changed;
} dispserv_sdl3_t;

static SDL_Window *sdl3_display_server_window(void)
{
   return (SDL_Window*)video_driver_display_userdata_get();
}

static SDL_DisplayID sdl3_display_server_display(void)
{
   SDL_Window *win  = sdl3_display_server_window();
   SDL_DisplayID id = win ? SDL_GetDisplayForWindow(win) : 0;
   return id ? id : SDL_GetPrimaryDisplay();
}

static int sdl3_display_server_refresh_label(const SDL_DisplayMode *dm)
{
   return (int)floor(dm->refresh_rate + 0.5f);
}

/* Exact where SDL has the ratio: 59.94 is not 60 */
static double sdl3_display_server_mode_hz(const SDL_DisplayMode *dm)
{
   if (dm->refresh_rate_numerator > 0 && dm->refresh_rate_denominator > 0)
      return (double)dm->refresh_rate_numerator
         / (double)dm->refresh_rate_denominator;
   return dm->refresh_rate;
}

/* Modes are sized in points; the frontend's sizes are pixels */
static unsigned sdl3_display_server_mode_dims(const SDL_DisplayMode *dm)
{
   float d = dm->pixel_density > 0.0f ? dm->pixel_density : 1.0f;
   return VIDEO_SCALE_PACK((unsigned)(dm->w * d + 0.5f),
         (unsigned)(dm->h * d + 0.5f));
}

/* SDL lists a rate under every pixel format and density */
static bool sdl3_display_server_same_hz(double a, double b)
{
   return fabs(a - b) < 0.005;
}

static void *sdl3_display_server_init(void)
{
   return calloc(1, sizeof(dispserv_sdl3_t));
}

static void sdl3_display_server_modeline_close(void *data);

static void sdl3_display_server_destroy(void *data)
{
   dispserv_sdl3_t *dispserv = (dispserv_sdl3_t*)data;
   if (!dispserv)
      return;
   sdl3_display_server_modeline_close(dispserv);
   free(dispserv);
}

/* Apply a listed WxH@R, in pixels, to the window: the listed mode of
 * that size nearest the rate as its fullscreen mode, then fullscreen. */
static bool sdl3_display_server_apply(dispserv_sdl3_t *dispserv,
      unsigned dims, double hz)
{
   int i, n;
   SDL_DisplayMode got;
   SDL_DisplayMode **modes;
   const SDL_DisplayMode *best = NULL;
   SDL_Window *win = sdl3_display_server_window();

   if (!win)
      return false;

   if (!(modes = SDL_GetFullscreenDisplayModes(dispserv->display, &n)))
      return false;
   for (i = 0; i < n; i++)
   {
      if (sdl3_display_server_mode_dims(modes[i]) != dims)
         continue;
      if (     !best
            || fabs(sdl3_display_server_mode_hz(modes[i]) - hz)
             < fabs(sdl3_display_server_mode_hz(best) - hz))
         best = modes[i];
   }
   if (best)
      got = *best;
   SDL_free(modes);
   if (!best)
   {
      RARCH_ERR("[SDL3] No listed mode is %ux%u\n",
            VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
      return false;
   }

   if (!SDL_SetWindowFullscreenMode(win, &got))
   {
      RARCH_ERR("[SDL3] SDL_SetWindowFullscreenMode failed: %s\n", SDL_GetError());
      return false;
   }
   if (!SDL_SetWindowFullscreen(win, true))
   {
      RARCH_ERR("[SDL3] SDL_SetWindowFullscreen failed: %s\n", SDL_GetError());
      return false;
   }
   SDL_SyncWindow(win);
   dispserv->mode_changed = true;
   RARCH_LOG("[SDL3] Display mode %dx%d@%.2f set on display %u\n",
         got.w, got.h, got.refresh_rate, (unsigned)dispserv->display);
   return true;
}

static bool sdl3_display_server_set_resolution(void *data,
      unsigned dims, int int_hz, float hz,
      int center, int monitor_index, int xoffset, int padjust)
{
   const SDL_DisplayMode *cur;
   double rate               = hz > 0.0f ? (double)hz : (double)int_hz;
   dispserv_sdl3_t *dispserv = (dispserv_sdl3_t*)data;
   if (!dispserv)
      return false;

   if (!dispserv->opened)
   {
      const SDL_DisplayMode *desk;
      dispserv->display = sdl3_display_server_display();
      desk = SDL_GetDesktopDisplayMode(dispserv->display);
      if (desk)
         dispserv->desktop = *desk;
      dispserv->opened = true;
   }

   cur = SDL_GetCurrentDisplayMode(dispserv->display);
   if (cur)
   {
      unsigned cur_dims = sdl3_display_server_mode_dims(cur);
      if (VIDEO_SCALE_W(dims) == 0)
         VIDEO_SCALE_PUT_W(dims, VIDEO_SCALE_W(cur_dims));
      if (VIDEO_SCALE_H(dims) == 0)
         VIDEO_SCALE_PUT_H(dims, VIDEO_SCALE_H(cur_dims));
      if (rate <= 0.0)
         rate = sdl3_display_server_mode_hz(cur);
   }

   return sdl3_display_server_apply(dispserv, dims, rate);
}

static int sdl3_display_server_resolution_list_qsort(
      const video_display_config_t *a, const video_display_config_t *b)
{
   /* The width sits in the high half, so the word orders by width,
    * then height. */
   if (a->dims != b->dims)
      return a->dims < b->dims ? -1 : 1;
   if (a->refreshrate != b->refreshrate)
      return a->refreshrate < b->refreshrate ? -1 : 1;
   return 0;
}

static void *sdl3_display_server_get_resolution_list(void *data,
      unsigned *len)
{
   int i, n, j;
   SDL_DisplayMode **modes;
   const SDL_DisplayMode *cur;
   video_display_config_t *conf;
   SDL_DisplayID display = sdl3_display_server_display();

   *len  = 0;
   modes = SDL_GetFullscreenDisplayModes(display, &n);
   if (!modes || n <= 0)
   {
      SDL_free(modes);
      return NULL;
   }
   if (!(conf = (video_display_config_t*)calloc(n, sizeof(*conf))))
   {
      SDL_free(modes);
      return NULL;
   }

   cur = SDL_GetCurrentDisplayMode(display);

   for (i = 0, j = 0; i < n; i++)
   {
      int k;
      const SDL_DisplayMode *dm = modes[i];
      unsigned dims             = sdl3_display_server_mode_dims(dm);
      double   hz               = sdl3_display_server_mode_hz(dm);
      bool     current          = cur
         && dims == sdl3_display_server_mode_dims(cur)
         && sdl3_display_server_same_hz(hz, sdl3_display_server_mode_hz(cur));
      for (k = 0; k < j; k++)
         if (     conf[k].dims == dims
               && sdl3_display_server_same_hz(conf[k].refreshrate_float, hz))
            break;
      if (k < j)
      {
         conf[k].current |= current;
         continue;
      }
      conf[j].dims              = dims;
      conf[j].bpp               = SDL_BITSPERPIXEL(dm->format);
      conf[j].refreshrate       = sdl3_display_server_refresh_label(dm);
      conf[j].refreshrate_float = (float)hz;
      conf[j].idx               = j;
      conf[j].current           = current;
      j++;
   }
   SDL_free(modes);
   *len = j;

   qsort(conf, j, sizeof(*conf),
         (int (*)(const void *, const void *))sdl3_display_server_resolution_list_qsort);
   return conf;
}

static float sdl3_display_server_get_refresh_rate(void *data)
{
   const SDL_DisplayMode *cur = SDL_GetCurrentDisplayMode(sdl3_display_server_display());
   return cur ? (float)sdl3_display_server_mode_hz(cur) : 0.0f;
}

static void sdl3_display_server_get_video_output_size(void *data,
      unsigned *dims, char *s, size_t len)
{
   const SDL_DisplayMode *cur = SDL_GetCurrentDisplayMode(sdl3_display_server_display());
   if (!cur)
      return;
   if (dims)
      *dims = sdl3_display_server_mode_dims(cur);
}

static uint32_t sdl3_display_server_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, DISPSERV_CTX_MODELINE);
   return flags;
}

static int sdl3_display_server_modeline_list_outputs(void *data,
      video_output_info_t *out, int max)
{
   int i, n;
   SDL_DisplayID *ids = SDL_GetDisplays(&n);
   SDL_DisplayID window_display = sdl3_display_server_display();
   if (!ids)
      return -1;
   if (n > max)
      n = max;
   for (i = 0; i < n; i++)
   {
      SDL_Rect r;
      const char *name = SDL_GetDisplayName(ids[i]);
      memset(&out[i], 0, sizeof(out[i]));
      out[i].id = (int)ids[i];
      if (SDL_GetDisplayBounds(ids[i], &r))
      {
         out[i].x      = r.x;
         out[i].y      = r.y;
         out[i].dims = VIDEO_SCALE_PACK(r.w, r.h);
      }
      out[i].primary = (ids[i] == window_display);
      strlcpy(out[i].name, name ? name : "", sizeof(out[i].name));
   }
   SDL_free(ids);
   return n;
}

static bool sdl3_display_server_modeline_open(void *data,
      const video_modeline_disp_t *ds)
{
   const SDL_DisplayMode *desk;
   dispserv_sdl3_t *dispserv = (dispserv_sdl3_t*)data;
   if (!dispserv || !sdl3_display_server_window())
      return false;
   if (dispserv->opened)
      return true;

   /* "auto" or "dummy" is the window's display, a digit is an index
    * into the display list */
   dispserv->display = sdl3_display_server_display();
   if (strlen(ds->screen) == 1 && ds->screen[0] >= '0' && ds->screen[0] <= '9')
   {
      int n;
      SDL_DisplayID *ids = SDL_GetDisplays(&n);
      if (ids)
      {
         if (ds->screen[0] - '0' < n)
            dispserv->display = ids[ds->screen[0] - '0'];
         SDL_free(ids);
      }
   }

   memset(&dispserv->desktop, 0, sizeof(dispserv->desktop));
   desk = SDL_GetDesktopDisplayMode(dispserv->display);
   if (desk)
      dispserv->desktop = *desk;
   dispserv->opened       = true;
   dispserv->mode_changed = false;
   return true;
}

static void sdl3_display_server_modeline_close(void *data)
{
   dispserv_sdl3_t *dispserv = (dispserv_sdl3_t*)data;
   SDL_Window *win;
   if (!dispserv || !dispserv->opened)
      return;
   win = sdl3_display_server_window();
   /* Desktop fullscreen is a NULL mode; the display comes back with it */
   if (win && dispserv->mode_changed)
   {
      SDL_SetWindowFullscreenMode(win, NULL);
      SDL_SyncWindow(win);
   }
   dispserv->opened       = false;
   dispserv->mode_changed = false;
}

/* No add, no update: SDL switches among what it lists */
static unsigned sdl3_display_server_modeline_caps(void *data)
{
   return 0;
}

static int sdl3_display_server_modeline_enum(void *data,
      video_modeline_t *modes, int max)
{
   int i, n, j;
   SDL_DisplayMode **list;
   const SDL_DisplayMode *desktop;
   dispserv_sdl3_t *dispserv = (dispserv_sdl3_t*)data;

   if (!dispserv || !dispserv->opened)
      return -1;

   list = SDL_GetFullscreenDisplayModes(dispserv->display, &n);
   if (!list)
      return -1;
   /* The desktop entry; a backend without one reports what is current */
   desktop = SDL_GetDesktopDisplayMode(dispserv->display);
   if (!desktop || desktop->w == 0)
      desktop = SDL_GetCurrentDisplayMode(dispserv->display);

   for (i = 0, j = 0; i < n && j < max; i++)
   {
      const SDL_DisplayMode *dm = list[i];
      unsigned dims = sdl3_display_server_mode_dims(dm);
      double   hz   = sdl3_display_server_mode_hz(dm);
      int k;
      bool dup = false;
      /* One entry per WxH@R */
      for (k = 0; k < j; k++)
      {
         if (modes[k].dims == dims
               && sdl3_display_server_same_hz(modes[k].vfreq, hz))
         {
            dup = true;
            break;
         }
      }
      if (dup)
         continue;
      memset(&modes[j], 0, sizeof(modes[j]));
      modes[j].dims    = dims;
      modes[j].hactive = VIDEO_SCALE_W(dims);
      modes[j].vactive = VIDEO_SCALE_H(dims);
      modes[j].refresh = sdl3_display_server_refresh_label(dm);
      modes[j].vfreq   = hz;
      /* Labels only: SDL does not expose the timing behind a mode */
      modes[j].type    = MODELINE_TIMING_SYSTEM;
      modes[j].platform_data = (uint64_t)dm->format;
      if (     desktop
            && dims == sdl3_display_server_mode_dims(desktop)
            && sdl3_display_server_same_hz(hz,
               sdl3_display_server_mode_hz(desktop)))
         modes[j].type |= MODELINE_DESKTOP;
      j++;
   }
   SDL_free(list);
   return j;
}

static bool sdl3_display_server_modeline_set(void *data,
      video_modeline_t *mode)
{
   dispserv_sdl3_t *dispserv = (dispserv_sdl3_t*)data;
   if (!dispserv || !dispserv->opened || !mode)
      return false;
   return sdl3_display_server_apply(dispserv, mode->dims,
         mode->vfreq > 0.0 ? mode->vfreq : (double)mode->refresh);
}

static bool sdl3_display_server_modeline_flush(void *data)
{
   return true;
}

const video_display_server_t dispserv_sdl3 = {
   sdl3_display_server_init,
   sdl3_display_server_destroy,
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
   sdl3_display_server_set_resolution,
   sdl3_display_server_get_resolution_list,
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   sdl3_display_server_get_refresh_rate,
   sdl3_display_server_get_video_output_size,
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   sdl3_display_server_get_flags,
   NULL, /* get_scanline */
   NULL, /* wait_vblank */
   sdl3_display_server_modeline_list_outputs,
   sdl3_display_server_modeline_open,
   sdl3_display_server_modeline_close,
   sdl3_display_server_modeline_caps,
   sdl3_display_server_modeline_enum,
   NULL, /* modeline_add */
   NULL, /* modeline_update */
   NULL, /* modeline_delete */
   sdl3_display_server_modeline_set,
   sdl3_display_server_modeline_flush,
   NULL, /* get_edid */
   /* idle_wait: video_display_server_idle_wait() asks the native
    * server, and this one is never that - it is reached only through
    * video_display_server_modes(), for mode switching. */
   NULL,
   "sdl3",
   /* get_refresh_rate already reads the window's display */
   sdl3_display_server_get_refresh_rate
};
