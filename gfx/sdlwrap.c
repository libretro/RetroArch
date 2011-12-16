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

#include "sdlwrap.h"
#include "SDL_syswm.h"
#include "general.h"
#include <assert.h>

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#endif

// SDL 1.2 is portable, sure, but you still need some platform specific workarounds ;)
// Hopefully SDL 1.3 will solve this more cleanly :D
// Welcome to #ifdef HELL! :D
//

#if SDL_MODERN
static SDL_Window* g_window;
static SDL_GLContext g_ctx;
#endif

static bool g_fullscreen;
static unsigned g_interval;

void sdlwrap_set_swap_interval(unsigned interval, bool inited)
{
   g_interval = interval;

#if SDL_MODERN
   if (g_window)
      SDL_GL_SetSwapInterval(g_interval);
#else
   if (inited)
   {
#if defined(_WIN32)
      static BOOL (APIENTRY *wgl_swap_interval)(int) = NULL;
      if (!wgl_swap_interval)
      {
         SDL_SYM_WRAP(wgl_swap_interval, "wglSwapIntervalEXT");
      }
      if (wgl_swap_interval) wgl_swap_interval(g_interval);

#elif defined(__APPLE__) && defined(HAVE_OPENGL)
      GLint val = g_interval;
      CGLSetParameter(CGLGetCurrentContext(), kCGLCPSwapInterval, &val);
#else
      static int (*glx_swap_interval)(int) = NULL;
      if (!glx_swap_interval) 
         SDL_SYM_WRAP(glx_swap_interval, "glXSwapIntervalSGI");
      if (!glx_swap_interval)
         SDL_SYM_WRAP(glx_swap_interval, "glXSwapIntervalMESA");

      if (glx_swap_interval) 
         glx_swap_interval(g_interval);
      else 
         SSNES_WARN("Could not find GLX VSync call. :(\n");
#endif
   }
#endif
}

bool sdlwrap_init(void)
{
#if SDL_MODERN
   return SDL_VideoInit(NULL) == 0;
#else
   if (SDL_WasInit(SDL_INIT_VIDEO))
      return true;

   return SDL_Init(SDL_INIT_VIDEO) == 0;
#endif
}

#if SDL_MODERN
void sdlwrap_destroy(void)
{
   if (g_ctx)
      SDL_GL_DeleteContext(g_ctx);
   if (g_window)
      SDL_DestroyWindow(g_window);

   g_ctx = NULL;
   g_window = NULL;
   SDL_VideoQuit();
}
#else
void sdlwrap_destroy(void) 
{
   SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
#endif

bool sdlwrap_set_video_mode(
      unsigned width, unsigned height,
      unsigned bits, bool fullscreen)
{
#if SDL_MODERN
   if (g_window)
      return true;
#endif

#if SDL_MODERN
   static const int resizable = SDL_WINDOW_RESIZABLE;
#else
#ifndef __APPLE__ // Resizing on OSX is broken in 1.2 it seems :)
   static const int resizable = SDL_RESIZABLE;
#else
   static const int resizable = 0;
#endif
#endif

   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

#if !SDL_MODERN
   SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, g_interval);
#endif

#if SDL_MODERN
      if (bits == 15)
      {
         SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
         SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
         SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
      }
      g_window = SDL_CreateWindow("SSNES", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | (fullscreen ? SDL_WINDOW_FULLSCREEN : resizable));
      if (!g_window)
         return false;
      g_ctx = SDL_GL_CreateContext(g_window);
#else
   if (!SDL_SetVideoMode(width, height, bits,
         SDL_OPENGL | (fullscreen ? SDL_FULLSCREEN : resizable)))
      return false;
#endif

   int attr = 0;
#if SDL_MODERN
   SDL_GL_SetSwapInterval(g_interval);
#else
   SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &attr);
   if (attr <= 0 && g_interval)
   {
      SSNES_WARN("SDL failed to setup VSync, attempting to recover using native calls!\n");
      sdlwrap_set_swap_interval(g_interval, true);
   }
#endif

   g_fullscreen = fullscreen;

   attr = 0;
   SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &attr);
   if (attr <= 0)
      SSNES_WARN("GL double buffer has not been enabled!\n");

   return true;
}

// SDL 1.2 has an awkward model where you need to "confirm" window resizing.
// SDL 1.3 luckily removes this quirk.
void sdlwrap_set_resize(unsigned width, unsigned height)
{
#if SDL_MODERN
   (void)width;
   (void)height;
#else
#ifndef __APPLE__ // Resizing on OSX is broken in 1.2 it seems :)
   static const int resizable = SDL_RESIZABLE;
#else
   static const int resizable = 0;
#endif
   SDL_SetVideoMode(width, height, 0, SDL_OPENGL | (g_fullscreen ? SDL_FULLSCREEN : resizable));
#endif
}

void sdlwrap_wm_set_caption(const char *str)
{
#if SDL_MODERN
   SDL_SetWindowTitle(g_window, str);
#else
   SDL_WM_SetCaption(str, NULL);
#endif
}

void sdlwrap_swap_buffers(void)
{
#if SDL_MODERN
   SDL_GL_SwapWindow(g_window);
#else
   SDL_GL_SwapBuffers();
#endif
}

bool sdlwrap_key_pressed(int key)
{
   int num_keys;
#if SDL_MODERN
   Uint8 *keymap = SDL_GetKeyboardState(&num_keys);
   key = SDL_GetScancodeFromKey(key);
   if (key >= num_keys)
      return false;

   return keymap[key];
#else
   Uint8 *keymap = SDL_GetKeyState(&num_keys);
   if (key >= num_keys)
      return false;

   return keymap[key];
#endif
}

// 1.2 specific workaround for tiling WMs. In 1.3 we call GetSize directly, so we don't need to rely on
// proper event handling (I hope).
#if !defined(__APPLE__) && !defined(_WIN32) && !SDL_MODERN && !defined(XENON)
static void sdlwrap_get_window_size(unsigned *width, unsigned *height)
{
   SDL_SysWMinfo info;
   SDL_VERSION(&info.version);
   SDL_GetWMInfo(&info);
   XWindowAttributes target;

   info.info.x11.lock_func();
   XGetWindowAttributes(info.info.x11.display, info.info.x11.window,
         &target);
   info.info.x11.unlock_func();

   *width = target.width;
   *height = target.height;
}
#endif

void sdlwrap_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   *quit = false;
   *resize = false;
   SDL_Event event;
#if SDL_MODERN
   while (SDL_PollEvent(&event))
   {
      switch (event.type)
      {
         case SDL_QUIT:
            *quit = true;
            return;

         case SDL_WINDOWEVENT:
            switch (event.window.event)
            {
               case SDL_WINDOWEVENT_CLOSE:
                  *quit = true;
                  return;

               case SDL_WINDOWEVENT_RESIZED:
                  *resize = true;
                  *width = event.window.data1;
                  *height = event.window.data2;
                  break;
            }
            break;
      }
   }

   if (!*resize)
   {
      int w, h;
      SDL_GetWindowSize(g_window, &w, &h);
      if (*width != (unsigned)w || *height != (unsigned)h)
      {
         *resize = true;
         *width = w;
         *height = h;
      }
   }
#else
   while (SDL_PollEvent(&event))
   {
      switch (event.type)
      {
         case SDL_QUIT:
            *quit = true;
            break;

         case SDL_VIDEORESIZE:
            *resize = true;
            *width = event.resize.w;
            *height = event.resize.h;
            break;
      }
   }

#if !defined(__APPLE__) && !defined(_WIN32) && !defined(XENON)
   // Hack to workaround limitations in tiling WMs ...
   if (!*resize && !g_fullscreen)
   {
      unsigned new_width, new_height;
      sdlwrap_get_window_size(&new_width, &new_height);
      if ((new_width != *width || new_height != *height) || (frame_count == 10)) // Ugly hack :D
      {
         *resize = true;
         *width = new_width;
         *height = new_height;
         SSNES_LOG("GL: Verified window size: %u x %u\n", *width, *height);
      }
   }
#endif
#endif
}

bool sdlwrap_get_wm_info(SDL_SysWMinfo *info)
{
#ifdef XENON
   (void)info;
   return false;
#elif SDL_MODERN
   if (g_window)
      return SDL_GetWindowWMInfo(g_window, info);
   else
      return SDL_GetWMInfo(info) == 1;
#else
   return SDL_GetWMInfo(info) == 1;
#endif
}

bool sdlwrap_window_has_focus(void)
{
#if SDL_MODERN
   Uint32 flags = SDL_GetWindowFlags(g_window);
   flags &= SDL_WINDOW_INPUT_FOCUS;
   return flags == SDL_WINDOW_INPUT_FOCUS;
#else
   return (SDL_GetAppState() & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) == (SDL_APPINPUTFOCUS | SDL_APPACTIVE);
#endif
}

