/*  RetroArch - A frontend for libretro.
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

/* Which input driver a window gets.
 *
 * A video driver's init is still what starts the input driver, and it
 * does so on its own thread - which matters: a Windows raw input
 * window has to be made on the thread that pumps its messages, and
 * with threaded video that is the video thread (see winraw_input.c).
 * What these functions take out of the video code is the choice: the
 * contexts under gfx/ each had it written out, with the input
 * drivers' names in it, and in copies that had drifted apart. A
 * context calls the one for its kind of window and knows no driver's
 * name.
 *
 * Each returns with *input and *input_data both set or both NULL.
 * NULL is not a failure: the frontend then starts the configured
 * driver itself (video_driver_init_input()).
 *
 * samples/input/window_driver runs every branch of these with
 * stand-in drivers, on the host and - for the Windows one - under
 * Wine. */

#include <stddef.h>

#include <string/stdstring.h>

#include "input_driver.h"
#include "../configuration.h"

#if defined(_WIN32) || defined(_XBOX) || defined(__WINRT__)
/* A window of the Windows family: raw input if that is the setting and
 * it starts, DirectInput otherwise; on UWP plain XInput or the uwp
 * driver; on Xbox XInput. */
void input_driver_init_windows(const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   /* one left running across this video driver restart */
   if (input_driver_take_kept(input, input_data))
      return;
   {
#if defined(__WINRT__)
   /* Plain xinput is supported on UWP, but it
    * supports joypad only (uwp driver was added later) */
   if (string_is_equal(config_get_ptr()->arrays.input_driver, "xinput"))
   {
      void *xinput = input_driver_init_wrap(&input_xinput, joypad_name);
      *input       = xinput ? (input_driver_t*)&input_xinput : NULL;
      *input_data  = xinput;
   }
   else
   {
      void *uwp    = input_driver_init_wrap(&input_uwp, joypad_name);
      *input       = uwp ? (input_driver_t*)&input_uwp : NULL;
      *input_data  = uwp;
   }
#elif defined(_XBOX)
   void *xinput    = input_driver_init_wrap(&input_xinput, joypad_name);
   *input          = xinput ? (input_driver_t*)&input_xinput : NULL;
   *input_data     = xinput;
#else
   *input          = NULL;
   *input_data     = NULL;
#if _WIN32_WINNT >= 0x0501
#ifdef HAVE_WINRAWINPUT
   /* winraw only available since XP */
   if (string_is_equal(config_get_ptr()->arrays.input_driver, "raw"))
   {
      *input_data = input_driver_init_wrap(&input_winraw, joypad_name);
      if (*input_data)
      {
         *input = &input_winraw;
         return;
      }
   }
#endif
#endif

#ifdef HAVE_DINPUT
   *input_data = input_driver_init_wrap(&input_dinput, joypad_name);
   *input      = *input_data ? &input_dinput : NULL;
#endif
#endif
   }
}
#endif

#ifdef HAVE_X11
/* An X11 window: udev if that is the setting and it starts (it needs
 * to be allowed to read the event devices), the X11 driver otherwise. */
void input_driver_init_x11(const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;

#ifdef HAVE_UDEV
   if (string_is_equal(config_get_ptr()->arrays.input_driver, "udev"))
   {
      *input_data = input_driver_init_wrap(&input_udev, joypad_name);
      if (*input_data)
      {
         *input = &input_udev;
         return;
      }
   }
#endif

   *input_data = input_driver_init_wrap(&input_x, joypad_name);
   *input      = *input_data ? &input_x : NULL;
}
#endif

#ifdef HAVE_WAYLAND
bool input_wl_init(void *data, const char *joypad_name);

/* A Wayland surface. The seat's state - its keyboard, pointer and
 * touch - lives with the surface, in the video context, which hands
 * it over as @window_data; the Wayland input driver runs on that. */
void input_driver_init_wayland(const char *joypad_name, void *window_data,
      input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;

   if (!window_data || !input_wl_init(window_data, joypad_name))
      return;

   *input      = &input_wayland;
   *input_data = window_data;
   input_driver_init_joypads();
}
#endif

#if defined(HAVE_SDL) && !defined(HAVE_SDL2) && !defined(HAVE_SDL3)
/* An SDL 1.2 window: the SDL 1.2 input driver, whatever the setting
 * says. The window's keyboard and mouse come through SDL's event
 * queue and nowhere else. */
void input_driver_init_sdl1(const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   *input_data = input_driver_init_wrap(&input_sdl1, joypad_name);
   *input      = *input_data ? &input_sdl1 : NULL;
}
#endif

#ifdef HAVE_SDL3
/* The configured input driver, if it works alongside an SDL 3 window:
 * udev, linuxraw, raw and dinput read input devices themselves, so
 * they do. One tied to a window system - x, wayland, cocoa - does not,
 * and NULL is returned for it. */
static input_driver_t *input_driver_beside_sdl3(const char *ident)
{
#ifdef HAVE_UDEV
   if (string_is_equal(ident, "udev"))
      return &input_udev;
#endif
#if defined(__linux__) && !defined(ANDROID)
   if (string_is_equal(ident, "linuxraw"))
      return &input_linuxraw;
#endif
#if defined(_WIN32) && !defined(_XBOX) && _WIN32_WINNT >= 0x0501 && !defined(__WINRT__)
#ifdef HAVE_WINRAWINPUT
   if (string_is_equal(ident, "raw"))
      return &input_winraw;
#endif
#endif
#ifdef HAVE_DINPUT
   if (string_is_equal(ident, "dinput"))
      return &input_dinput;
#endif
   return NULL;
}

/* An SDL 3 window: the configured driver if it is one that reads
 * devices itself and it starts; the SDL 3 input driver, which reads
 * the window's event queue, otherwise. */
void input_driver_init_sdl3(const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   input_driver_t *beside = input_driver_beside_sdl3(
         config_get_ptr()->arrays.input_driver);

   *input      = NULL;
   *input_data = NULL;

   if (beside)
   {
      *input_data = input_driver_init_wrap(beside, joypad_name);
      if (*input_data)
      {
         *input = beside;
         return;
      }
      /* it did not start: the SDL 3 one, then */
   }

   *input_data = input_driver_init_wrap(&input_sdl3, joypad_name);
   *input      = *input_data ? &input_sdl3 : NULL;
}
#endif

/* The one input driver a platform has, where it has one of its own:
 * a console's, Android's, QNX's, the web's, UWP's. The conditions are
 * those input_drivers[] lists each driver under. */
static input_driver_t *input_driver_of_platform(void)
{
#if defined(__PSL1GHT__) || defined(__PS3__)
   return &input_ps3;
#elif defined(SN_TARGET_PSP2) || defined(PSP) || defined(VITA)
   return &input_psp;
#elif defined(PS2)
   return &input_ps2;
#elif defined(_3DS)
   return &input_ctr;
#elif defined(SWITCH)
   return &input_switch;
#elif defined(GEKKO)
   return &input_gx;
#elif defined(WIIU)
   return &input_wiiu;
#elif defined(ANDROID)
   return &input_android;
#elif defined(__QNX__)
   return &input_qnx;
#elif defined(__EMSCRIPTEN__)
   return &input_rwebinput;
#elif defined(__WINRT__)
   /* Plain xinput is supported on UWP, but it supports joypads only
    * (the uwp driver was added later): it is started when it is what
    * the setting says, and the uwp driver otherwise. */
   if (string_is_equal(config_get_ptr()->arrays.input_driver, "xinput"))
      return &input_xinput;
   return &input_uwp;
#else
   return NULL;
#endif
}

/* A platform with one input driver of its own: that driver, whatever
 * the setting says (UWP's two aside, see above). */
void input_driver_init_platform(const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   input_driver_t *own = input_driver_of_platform();

   *input      = NULL;
   *input_data = NULL;
   if (!own)
      return;

   *input_data = input_driver_init_wrap(own, joypad_name);
   if (*input_data)
      *input   = own;
}

/* A display with no window system - KMS/DRM, a Vulkan display. The
 * X11 driver cannot work there and udev may not be allowed, so when
 * the setting is either of those: udev if it starts, linuxraw if that
 * does. Any other setting is left for the frontend to start. */
void input_driver_init_kms(const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   const char *name = config_get_ptr()->arrays.input_driver;

   *input           = NULL;
   *input_data      = NULL;

   if (     !string_is_equal(name, "x")
         && !string_is_equal(name, "udev"))
      return;

#ifdef HAVE_UDEV
   *input_data = input_driver_init_wrap(&input_udev, joypad_name);
   if (*input_data)
   {
      *input = &input_udev;
      return;
   }
#endif
#if defined(__linux__) && !defined(ANDROID)
   *input_data = input_driver_init_wrap(&input_linuxraw, joypad_name);
   if (*input_data)
   {
      *input = &input_linuxraw;
      return;
   }
#endif
   (void)joypad_name;
}
