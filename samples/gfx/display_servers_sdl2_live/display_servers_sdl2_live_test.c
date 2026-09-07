/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (display_servers_sdl2_live_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* The SDL2 display server on a live display: an SDL2 window on
 * $DISPLAY, the server lists the outputs and the listed modes, picks
 * a listed mode other than the desktop one, switches to it through
 * SDL_SetWindowDisplayMode and exclusive fullscreen, and the display
 * is read back through SDL to confirm the switch. Close puts the
 * desktop mode back. The listed-mode picker for the menu
 * (get_resolution_list / set_resolution) is exercised the same way.
 *
 * A window on an Xorg 'dummy' driver server has several listed modes
 * to choose from; with no DISPLAY the test skips. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"

#include "../../../gfx/video_display_server.h"

/* RetroArch-side symbols dispserv_sdl2.c refers to: the SDL window
 * handle the video driver registered, and logging. */
static SDL_Window *s_window;
static int s_verbose;

uintptr_t video_driver_display_userdata_get(void)
{
   return (uintptr_t)s_window;
}

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
   if (!s_verbose)
      return;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

static int current_mode(SDL_DisplayMode *m)
{
   int idx = SDL_GetWindowDisplayIndex(s_window);
   if (idx < 0)
      idx = 0;
   memset(m, 0, sizeof(*m));
   return SDL_GetCurrentDisplayMode(idx, m);
}

int main(void)
{
   void *data;
   video_modeline_disp_t ds;
   video_output_info_t outputs[8];
   video_modeline_t modes[64];
   video_display_config_t *list;
   SDL_DisplayMode desktop, cur;
   unsigned nlist = 0;
   int nout, nmodes, i, pick = -1, pick2 = -1;

   s_verbose = getenv("LIVE_VERBOSE") != NULL;
   setvbuf(stdout, NULL, _IONBF, 0);

   if (!getenv("DISPLAY"))
   {
      puts("[skip] no display; run against an Xorg dummy-driver server");
      return 0;
   }
   if (SDL_Init(SDL_INIT_VIDEO) != 0)
   {
      printf("[skip] SDL_Init failed: %s\n", SDL_GetError());
      return 0;
   }
   s_window = SDL_CreateWindow("dispserv_sdl2 live", SDL_WINDOWPOS_CENTERED,
         SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
   if (!s_window)
   {
      fprintf(stderr, "FAIL: SDL_CreateWindow: %s\n", SDL_GetError());
      return 1;
   }
   if (SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(s_window), &desktop) != 0)
   {
      fprintf(stderr, "FAIL: SDL_GetDesktopDisplayMode: %s\n", SDL_GetError());
      return 1;
   }
   printf("desktop: %dx%d@%d on %s\n", desktop.w, desktop.h,
         desktop.refresh_rate, SDL_GetCurrentVideoDriver());

   data = dispserv_sdl2.init();

   nout = dispserv_sdl2.modeline_list_outputs(data, outputs, 8);
   if (nout < 1)
   {
      fprintf(stderr, "FAIL: list_outputs returned %d\n", nout);
      return 1;
   }
   printf("[pass] list_outputs: %d, first '%s' %ux%u\n", nout,
         outputs[0].name, outputs[0].width, outputs[0].height);

   memset(&ds, 0, sizeof(ds));
   strcpy(ds.screen, "auto");
   if (!dispserv_sdl2.modeline_open(data, &ds))
   {
      fprintf(stderr, "FAIL: modeline_open\n");
      return 1;
   }
   if (dispserv_sdl2.modeline_caps(data) != 0)
   {
      fprintf(stderr, "FAIL: SDL must report no add/update caps\n");
      return 1;
   }
   nmodes = dispserv_sdl2.modeline_enum(data, modes, 64);
   if (nmodes < 1)
   {
      fprintf(stderr, "FAIL: enum returned %d\n", nmodes);
      return 1;
   }
   for (i = 0; i < nmodes; i++)
   {
      if (!(modes[i].type & MODELINE_TIMING_SYSTEM))
      {
         fprintf(stderr, "FAIL: SDL modes must be labels only (system timing)\n");
         return 1;
      }
      if ((modes[i].type & MODELINE_DESKTOP) && pick2 < 0)
         pick2 = i;
      else if (!(modes[i].type & MODELINE_DESKTOP) && pick < 0
            && modes[i].width != desktop.w)
         pick = i;
   }
   if (pick2 < 0)
   {
      fprintf(stderr, "FAIL: no listed mode flagged as the desktop\n");
      return 1;
   }
   printf("[pass] open + enum: %d listed modes, desktop flagged\n", nmodes);

   if (pick < 0)
   {
      puts("[skip] only the desktop mode is listed; nothing to switch to");
      dispserv_sdl2.modeline_close(data);
      dispserv_sdl2.destroy(data);
      SDL_DestroyWindow(s_window);
      SDL_Quit();
      return 0;
   }

   /* Switch to a listed non-desktop mode and read the display back */
   if (!dispserv_sdl2.modeline_set(data, &modes[pick]))
   {
      fprintf(stderr, "FAIL: modeline_set %dx%d@%d\n", modes[pick].width,
            modes[pick].height, modes[pick].refresh);
      return 1;
   }
   SDL_PumpEvents();
   if (current_mode(&cur) != 0 || cur.w != modes[pick].width || cur.h != modes[pick].height)
   {
      fprintf(stderr, "FAIL: display reads %dx%d@%d after set of %dx%d@%d\n",
            cur.w, cur.h, cur.refresh_rate, modes[pick].width, modes[pick].height,
            modes[pick].refresh);
      return 1;
   }
   if (!(SDL_GetWindowFlags(s_window) & SDL_WINDOW_FULLSCREEN))
   {
      fprintf(stderr, "FAIL: window is not exclusive fullscreen after set\n");
      return 1;
   }
   printf("[pass] modeline_set: display now %dx%d@%d\n", cur.w, cur.h, cur.refresh_rate);

   /* Back to the desktop entry through the same path */
   if (!dispserv_sdl2.modeline_set(data, &modes[pick2]))
   {
      fprintf(stderr, "FAIL: modeline_set back to desktop\n");
      return 1;
   }
   SDL_PumpEvents();
   if (current_mode(&cur) != 0 || cur.w != desktop.w || cur.h != desktop.h)
   {
      fprintf(stderr, "FAIL: display reads %dx%d after set of the desktop entry\n",
            cur.w, cur.h);
      return 1;
   }
   printf("[pass] modeline_set: desktop %dx%d back\n", cur.w, cur.h);

   /* The menu's listed-mode picker: list, then set one of the listed */
   list = (video_display_config_t*)dispserv_sdl2.get_resolution_list(data, &nlist);
   if (!list || nlist < 2)
   {
      fprintf(stderr, "FAIL: get_resolution_list returned %u\n", nlist);
      return 1;
   }
   for (i = 0; i < (int)nlist; i++)
   {
      if (!list[i].current && list[i].width != (unsigned)desktop.w)
      {
         if (!dispserv_sdl2.set_resolution(data, list[i].width, list[i].height,
                  (int)list[i].refreshrate, list[i].refreshrate_float, 0, 0, 0, 0))
         {
            fprintf(stderr, "FAIL: set_resolution %ux%u@%u\n", list[i].width,
                  list[i].height, list[i].refreshrate);
            return 1;
         }
         SDL_PumpEvents();
         if (current_mode(&cur) != 0 || cur.w != (int)list[i].width)
         {
            fprintf(stderr, "FAIL: set_resolution left the display at %dx%d\n",
                  cur.w, cur.h);
            return 1;
         }
         printf("[pass] set_resolution: display now %dx%d@%d, %.1f Hz reported\n",
               cur.w, cur.h, cur.refresh_rate, dispserv_sdl2.get_refresh_rate(data));
         break;
      }
   }
   free(list);

   /* Close restores the desktop mode */
   dispserv_sdl2.modeline_close(data);
   SDL_SetWindowFullscreen(s_window, 0);
   SDL_PumpEvents();
   if (current_mode(&cur) != 0 || cur.w != desktop.w || cur.h != desktop.h)
   {
      fprintf(stderr, "FAIL: after close the display reads %dx%d, desktop is %dx%d\n",
            cur.w, cur.h, desktop.w, desktop.h);
      return 1;
   }
   printf("[pass] close: desktop %dx%d@%d back\n", cur.w, cur.h, cur.refresh_rate);

   dispserv_sdl2.destroy(data);
   SDL_DestroyWindow(s_window);
   SDL_Quit();
   puts("ALL OK");
   return 0;
}
