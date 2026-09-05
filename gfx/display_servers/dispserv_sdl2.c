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

/* SDL2 display server: a thin layer over the SDL2 window that switches
 * among the display modes SDL already lists. It is not a custom timing
 * path: SDL cannot create a 256x240@60.0988 modeline, so modeline caps
 * are 0 and the engine can only pick a listed mode. Which server drives
 * mode switching is decided in video_driver.c from the
 * video_sdl_display_server setting: never, only when the native server
 * cannot switch modes, or always. Metrics, orientation and window
 * decorations stay with the native server. */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "SDL.h"

#include <compat/strl.h>

#include "../video_display_server.h"
#include "../video_driver.h"
#include "../../verbosity.h"

typedef struct
{
   SDL_DisplayMode desktop;
   int display_index;
   bool opened;
   bool mode_changed;
} dispserv_sdl_t;

static SDL_Window *sdl_display_server_window(void)
{
   return (SDL_Window*)video_driver_display_userdata_get();
}

static int sdl_display_server_display_index(void)
{
   SDL_Window *win = sdl_display_server_window();
   int idx         = win ? SDL_GetWindowDisplayIndex(win) : 0;
   return idx < 0 ? 0 : idx;
}

static void *sdl_display_server_init(void)
{
   dispserv_sdl_t *dispserv = (dispserv_sdl_t*)calloc(1, sizeof(*dispserv));
   return dispserv;
}

static void sdl_display_server_modeline_close(void *data);

static void sdl_display_server_destroy(void *data)
{
   dispserv_sdl_t *dispserv = (dispserv_sdl_t*)data;
   if (!dispserv)
      return;
   sdl_display_server_modeline_close(dispserv);
   free(dispserv);
}

static bool sdl_display_server_mode_to_modeline(const SDL_DisplayMode *dm,
      video_modeline_t *mode)
{
   memset(mode, 0, sizeof(*mode));
   mode->width   = mode->hactive = dm->w;
   mode->height  = mode->vactive = dm->h;
   mode->refresh = dm->refresh_rate;
   mode->vfreq   = dm->refresh_rate;
   /* Labels only: SDL does not expose the timing behind a mode */
   mode->type    = MODELINE_TIMING_SYSTEM;
   mode->platform_data = (uint64_t)dm->format;
   return true;
}

/* Apply a listed WxH@R to the window. Exclusive fullscreen is the
 * only state in which SDL2 honours a display mode, and it excludes
 * fullscreen-desktop, so the window flips to exclusive for the set. */
static bool sdl_display_server_apply(dispserv_sdl_t *dispserv,
      int w, int h, int refresh)
{
   SDL_DisplayMode want, got;
   SDL_Window *win = sdl_display_server_window();
   Uint32 flags;

   if (!win)
      return false;

   memset(&want, 0, sizeof(want));
   want.w            = w;
   want.h            = h;
   want.refresh_rate = refresh;
   want.format       = dispserv->desktop.format;

   if (!SDL_GetClosestDisplayMode(dispserv->display_index, &want, &got))
   {
      RARCH_ERR("[SDL] No listed mode close to %dx%d@%d: %s\n",
            w, h, refresh, SDL_GetError());
      return false;
   }
   if (got.w != w || got.h != h)
   {
      RARCH_ERR("[SDL] Closest listed mode is %dx%d@%d, not %dx%d\n",
            got.w, got.h, got.refresh_rate, w, h);
      return false;
   }

   flags = SDL_GetWindowFlags(win);
   if ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP)
      SDL_SetWindowFullscreen(win, 0);

   if (SDL_SetWindowDisplayMode(win, &got) != 0)
   {
      RARCH_ERR("[SDL] SDL_SetWindowDisplayMode failed: %s\n", SDL_GetError());
      return false;
   }
   if (SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN) != 0)
   {
      RARCH_ERR("[SDL] SDL_SetWindowFullscreen failed: %s\n", SDL_GetError());
      return false;
   }
   dispserv->mode_changed = true;
   RARCH_LOG("[SDL] Display mode %dx%d@%d set on display %d\n",
         got.w, got.h, got.refresh_rate, dispserv->display_index);
   return true;
}

static bool sdl_display_server_set_resolution(void *data,
      unsigned width, unsigned height, int int_hz, float hz,
      int center, int monitor_index, int xoffset, int padjust)
{
   SDL_DisplayMode cur;
   dispserv_sdl_t *dispserv = (dispserv_sdl_t*)data;
   if (!dispserv)
      return false;

   if (!dispserv->opened)
   {
      dispserv->display_index = sdl_display_server_display_index();
      SDL_GetDesktopDisplayMode(dispserv->display_index, &dispserv->desktop);
      dispserv->opened = true;
   }

   memset(&cur, 0, sizeof(cur));
   SDL_GetCurrentDisplayMode(dispserv->display_index, &cur);
   if (width == 0)
      width = cur.w;
   if (height == 0)
      height = cur.h;
   if (int_hz == 0)
      int_hz = cur.refresh_rate;

   return sdl_display_server_apply(dispserv, (int)width, (int)height, int_hz);
}

static int sdl_display_server_resolution_list_qsort(
      const video_display_config_t *a, const video_display_config_t *b)
{
   if (a->width != b->width)
      return a->width < b->width ? -1 : 1;
   if (a->height != b->height)
      return a->height < b->height ? -1 : 1;
   if (a->refreshrate != b->refreshrate)
      return a->refreshrate < b->refreshrate ? -1 : 1;
   return 0;
}

static void *sdl_display_server_get_resolution_list(void *data,
      unsigned *len)
{
   int i, n, j;
   SDL_DisplayMode cur;
   video_display_config_t *conf;
   int display = sdl_display_server_display_index();

   *len = 0;
   n    = SDL_GetNumDisplayModes(display);
   if (n <= 0)
      return NULL;
   if (!(conf = (video_display_config_t*)calloc(n, sizeof(*conf))))
      return NULL;

   memset(&cur, 0, sizeof(cur));
   SDL_GetCurrentDisplayMode(display, &cur);

   for (i = 0, j = 0; i < n; i++)
   {
      SDL_DisplayMode dm;
      if (SDL_GetDisplayMode(display, i, &dm) != 0)
         continue;
      conf[j].width             = dm.w;
      conf[j].height            = dm.h;
      conf[j].bpp               = SDL_BITSPERPIXEL(dm.format);
      conf[j].refreshrate       = dm.refresh_rate;
      conf[j].refreshrate_float = (float)dm.refresh_rate;
      conf[j].idx               = j;
      conf[j].current           = (dm.w == cur.w && dm.h == cur.h
            && dm.refresh_rate == cur.refresh_rate && dm.format == cur.format);
      j++;
   }
   *len = j;

   qsort(conf, j, sizeof(*conf),
         (int (*)(const void *, const void *))sdl_display_server_resolution_list_qsort);
   return conf;
}

static float sdl_display_server_get_refresh_rate(void *data)
{
   SDL_DisplayMode cur;
   memset(&cur, 0, sizeof(cur));
   if (SDL_GetCurrentDisplayMode(sdl_display_server_display_index(), &cur) != 0)
      return 0.0f;
   return (float)cur.refresh_rate;
}

static void sdl_display_server_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *s, size_t len)
{
   SDL_DisplayMode cur;
   memset(&cur, 0, sizeof(cur));
   if (SDL_GetCurrentDisplayMode(sdl_display_server_display_index(), &cur) != 0)
      return;
   if (width)
      *width  = cur.w;
   if (height)
      *height = cur.h;
}

static uint32_t sdl_display_server_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, DISPSERV_CTX_MODELINE);
   return flags;
}

static int sdl_display_server_modeline_list_outputs(void *data,
      video_output_info_t *out, int max)
{
   int i;
   int n = SDL_GetNumVideoDisplays();
   int window_display = sdl_display_server_display_index();
   if (n < 0)
      return -1;
   if (n > max)
      n = max;
   for (i = 0; i < n; i++)
   {
      SDL_Rect r;
      const char *name = SDL_GetDisplayName(i);
      memset(&out[i], 0, sizeof(out[i]));
      out[i].id = i;
      if (SDL_GetDisplayBounds(i, &r) == 0)
      {
         out[i].x      = r.x;
         out[i].y      = r.y;
         out[i].width  = r.w;
         out[i].height = r.h;
      }
      out[i].primary = (i == window_display);
      strlcpy(out[i].name, name ? name : "", sizeof(out[i].name));
   }
   return n;
}

static bool sdl_display_server_modeline_open(void *data,
      const video_modeline_disp_t *ds)
{
   dispserv_sdl_t *dispserv = (dispserv_sdl_t*)data;
   if (!dispserv || !sdl_display_server_window())
      return false;
   if (dispserv->opened)
      return true;

   /* "auto" or "dummy" is the window's display, a digit is an index */
   dispserv->display_index = sdl_display_server_display_index();
   if (strlen(ds->screen) == 1 && ds->screen[0] >= '0' && ds->screen[0] <= '9'
         && ds->screen[0] - '0' < SDL_GetNumVideoDisplays())
      dispserv->display_index = ds->screen[0] - '0';

   memset(&dispserv->desktop, 0, sizeof(dispserv->desktop));
   SDL_GetDesktopDisplayMode(dispserv->display_index, &dispserv->desktop);
   dispserv->opened       = true;
   dispserv->mode_changed = false;
   return true;
}

static void sdl_display_server_modeline_close(void *data)
{
   dispserv_sdl_t *dispserv = (dispserv_sdl_t*)data;
   SDL_Window *win;
   if (!dispserv || !dispserv->opened)
      return;
   win = sdl_display_server_window();
   /* The desktop mode comes back with the window; SDL restores the
    * display when the window leaves exclusive fullscreen */
   if (win && dispserv->mode_changed)
      SDL_SetWindowDisplayMode(win, &dispserv->desktop);
   dispserv->opened       = false;
   dispserv->mode_changed = false;
}

/* No add, no update: SDL switches among what it lists */
static unsigned sdl_display_server_modeline_caps(void *data)
{
   return 0;
}

static int sdl_display_server_modeline_enum(void *data,
      video_modeline_t *modes, int max)
{
   int i, n, j;
   SDL_DisplayMode desktop;
   dispserv_sdl_t *dispserv = (dispserv_sdl_t*)data;

   if (!dispserv || !dispserv->opened)
      return -1;

   n = SDL_GetNumDisplayModes(dispserv->display_index);
   if (n < 0)
      return -1;
   /* The desktop entry; a backend without one reports what is
    * current instead */
   memset(&desktop, 0, sizeof(desktop));
   if (SDL_GetDesktopDisplayMode(dispserv->display_index, &desktop) != 0
         || desktop.w == 0)
      SDL_GetCurrentDisplayMode(dispserv->display_index, &desktop);

   for (i = 0, j = 0; i < n && j < max; i++)
   {
      SDL_DisplayMode dm;
      int k;
      bool dup = false;
      if (SDL_GetDisplayMode(dispserv->display_index, i, &dm) != 0)
         continue;
      /* One entry per WxH@R; SDL lists every pixel format */
      for (k = 0; k < j; k++)
      {
         if (modes[k].width == dm.w && modes[k].height == dm.h
               && modes[k].refresh == dm.refresh_rate)
         {
            dup = true;
            break;
         }
      }
      if (dup)
         continue;
      sdl_display_server_mode_to_modeline(&dm, &modes[j]);
      if (dm.w == desktop.w && dm.h == desktop.h
            && dm.refresh_rate == desktop.refresh_rate)
         modes[j].type |= MODELINE_DESKTOP;
      j++;
   }
   return j;
}

static bool sdl_display_server_modeline_set(void *data,
      video_modeline_t *mode)
{
   dispserv_sdl_t *dispserv = (dispserv_sdl_t*)data;
   if (!dispserv || !dispserv->opened || !mode)
      return false;
   return sdl_display_server_apply(dispserv, mode->width, mode->height,
         mode->refresh);
}

static bool sdl_display_server_modeline_flush(void *data)
{
   return true;
}

const video_display_server_t dispserv_sdl2 = {
   sdl_display_server_init,
   sdl_display_server_destroy,
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
   sdl_display_server_set_resolution,
   sdl_display_server_get_resolution_list,
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   sdl_display_server_get_refresh_rate,
   sdl_display_server_get_video_output_size,
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   sdl_display_server_get_flags,
   NULL, /* get_scanline */
   NULL, /* wait_vblank */
   sdl_display_server_modeline_list_outputs,
   sdl_display_server_modeline_open,
   sdl_display_server_modeline_close,
   sdl_display_server_modeline_caps,
   sdl_display_server_modeline_enum,
   NULL, /* modeline_add */
   NULL, /* modeline_update */
   NULL, /* modeline_delete */
   sdl_display_server_modeline_set,
   sdl_display_server_modeline_flush,
   "sdl2"
};
