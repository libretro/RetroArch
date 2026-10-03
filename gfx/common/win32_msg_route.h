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

#ifndef _WIN32_MSG_ROUTE_H
#define _WIN32_MSG_ROUTE_H

#include <boolean.h>
#include <retro_inline.h>

/* What the Win32 window procedure does with a message.
 *
 * There used to be twelve window procedures: one for each video family
 * (Direct3D, WGL, Vulkan, GDI) times each kind of input driver
 * (DirectInput, raw input, any other), built on three copies of one
 * long switch that differed in a few cases each. A video driver chose
 * its procedure by comparing the input driver's name.
 *
 * There is one procedure now, and this says what it does. For a
 * message and the kind of input driver in use, win32_msg_route()
 * answers with the steps to take, which the procedure carries out in
 * a fixed order:
 *
 *    IME, key, clip, input, common, display - then DefWindowProc.
 *
 * Any step may answer the message and end it there, as marked below.
 * The three switches are gone; what each did is the route for its
 * kind, and samples/gfx/win32_msg_route holds the routes to the
 * switches they replace, message by message.
 *
 * No Windows types, so that the sample can build it on a host. The
 * message numbers are Windows' own, spelled out; win32_common.c checks
 * them against the SDK's at compile time. */

enum win32_input_kind
{
   WIN32_INPUT_OTHER = 0,  /* neither of the two below */
   WIN32_INPUT_DINPUT,     /* the "dinput" input driver */
   WIN32_INPUT_WINRAW      /* the "raw" input driver */
};

/* The steps, in the order they are taken. */

/* An IME composition ended: a keyboard event says so. */
#define WIN32_ROUTE_IME_END    (1 << 0)
/* IME composition text: a keyboard event per character. Answers 0. */
#define WIN32_ROUTE_IME_TEXT   (1 << 1)
/* A key message. Answers 0 unless it is a system key press other than
 * F10, Alt and right Shift, which goes on to the default. */
#define WIN32_ROUTE_KEY        (1 << 2)
/* ...whose Shift and Alt keys are offered to the input driver first;
 * answers 0 if the driver takes one. */
#define WIN32_ROUTE_KEY_MODS   (1 << 3)
/* ...and which is turned into a keyboard event here. (The raw input
 * driver makes its own from WM_INPUT.) */
#define WIN32_ROUTE_KEY_EVENT  (1 << 4)
/* Focus gained or lost: confine the cursor, or let it go, if the
 * mouse is grabbed. */
#define WIN32_ROUTE_CLIP_ON    (1 << 5)
#define WIN32_ROUTE_CLIP_OFF   (1 << 6)
/* Offered to the input driver. Answers 0 if the driver takes it. */
#define WIN32_ROUTE_INPUT      (1 << 7)
/* wnd_proc_common(): the window's own business. Answers with its
 * result if it says the message is done. */
#define WIN32_ROUTE_COMMON     (1 << 8)
/* The display mode changed. */
#define WIN32_ROUTE_DISPLAY    (1 << 9)

/* Windows' message numbers. */
#define WIN32_MSG_DESTROY            0x0002
#define WIN32_MSG_MOVE               0x0003
#define WIN32_MSG_SIZE               0x0005
#define WIN32_MSG_SETFOCUS           0x0007
#define WIN32_MSG_KILLFOCUS          0x0008
#define WIN32_MSG_CLOSE              0x0010
#define WIN32_MSG_QUIT               0x0012
#define WIN32_MSG_GETMINMAXINFO      0x0024
#define WIN32_MSG_DISPLAYCHANGE      0x007E
#define WIN32_MSG_NCLBUTTONDBLCLK    0x00A3
#define WIN32_MSG_KEYDOWN            0x0100
#define WIN32_MSG_KEYUP              0x0101
#define WIN32_MSG_CHAR               0x0102
#define WIN32_MSG_SYSKEYDOWN         0x0104
#define WIN32_MSG_SYSKEYUP           0x0105
#define WIN32_MSG_IME_ENDCOMPOSITION 0x010E
#define WIN32_MSG_IME_COMPOSITION    0x010F
#define WIN32_MSG_COMMAND            0x0111
#define WIN32_MSG_SYSCOMMAND         0x0112
#define WIN32_MSG_TIMER              0x0113
#define WIN32_MSG_MOUSEMOVE          0x0200
#define WIN32_MSG_MOUSEWHEEL         0x020A
#define WIN32_MSG_MOUSEHWHEEL        0x020E
#define WIN32_MSG_ENTERMENULOOP      0x0211
#define WIN32_MSG_EXITMENULOOP       0x0212
#define WIN32_MSG_POWERBROADCAST     0x0218
#define WIN32_MSG_DEVICECHANGE       0x0219
#define WIN32_MSG_ENTERSIZEMOVE      0x0231
#define WIN32_MSG_EXITSIZEMOVE       0x0232
#define WIN32_MSG_DROPFILES          0x0233
#define WIN32_MSG_POINTERUPDATE      0x0245
#define WIN32_MSG_POINTERDOWN        0x0246
#define WIN32_MSG_POINTERUP          0x0247
/* WM_USER + 0 and + 1: the file browser thread's answers */
#define WIN32_MSG_BROWSER_OPEN_RESULT 0x0400
#define WIN32_MSG_BROWSER_CANCELLED   0x0401

/* The timer that re-checks controllers after a device change; its id
 * arrives as a timer message's wparam. */
#define WIN32_MSG_HOTPLUG_TIMER_ID   0x5242

/* @desktop is false on the Xbox, where the window has no move or menu
 * loops, timers or power messages and the input drivers are not
 * offered anything. @threads says whether the file browser's messages
 * exist. @wparam_is_hotplug_timer is (wparam == WIN32_MSG_HOTPLUG_TIMER_ID). */
static INLINE unsigned win32_msg_route(enum win32_input_kind input,
      bool desktop, bool threads, unsigned message,
      bool wparam_is_hotplug_timer)
{
   bool driver = (input != WIN32_INPUT_OTHER);

   switch (message)
   {
      case WIN32_MSG_IME_ENDCOMPOSITION:
         return (input == WIN32_INPUT_DINPUT) ? WIN32_ROUTE_IME_END : 0;
      case WIN32_MSG_IME_COMPOSITION:
         return (input == WIN32_INPUT_DINPUT) ? WIN32_ROUTE_IME_TEXT : 0;

      case WIN32_MSG_KEYDOWN:
      case WIN32_MSG_KEYUP:
      case WIN32_MSG_SYSKEYDOWN:
      case WIN32_MSG_SYSKEYUP:
         switch (input)
         {
            case WIN32_INPUT_DINPUT:
               return WIN32_ROUTE_KEY | WIN32_ROUTE_KEY_MODS
                    | WIN32_ROUTE_KEY_EVENT;
            case WIN32_INPUT_WINRAW:
               return WIN32_ROUTE_KEY;
            default:
               break;
         }
         return WIN32_ROUTE_KEY | WIN32_ROUTE_KEY_EVENT;

      /* the mouse and the pen: DirectInput reads them from here */
      case WIN32_MSG_MOUSEMOVE:
      case WIN32_MSG_POINTERDOWN:
      case WIN32_MSG_POINTERUP:
      case WIN32_MSG_POINTERUPDATE:
      case WIN32_MSG_MOUSEWHEEL:
      case WIN32_MSG_MOUSEHWHEEL:
      case WIN32_MSG_NCLBUTTONDBLCLK:
         return (desktop && input == WIN32_INPUT_DINPUT)
            ? WIN32_ROUTE_INPUT : 0;

      case WIN32_MSG_DEVICECHANGE:
         return (desktop && driver) ? WIN32_ROUTE_INPUT : 0;

      /* The window's own business. With an input driver these are
       * also where the hotplug timer is looked for - under the timer
       * message, and under the rest of this group by the id alone,
       * which is how the switches had it. */
      case WIN32_MSG_DROPFILES:
      case WIN32_MSG_SYSCOMMAND:
      case WIN32_MSG_CHAR:
      case WIN32_MSG_CLOSE:
      case WIN32_MSG_DESTROY:
      case WIN32_MSG_QUIT:
      case WIN32_MSG_MOVE:
      case WIN32_MSG_SIZE:
         return WIN32_ROUTE_COMMON
            | ((desktop && driver && wparam_is_hotplug_timer)
                  ? WIN32_ROUTE_INPUT : 0);
      case WIN32_MSG_TIMER:
         if (!desktop)
            return 0;
         return WIN32_ROUTE_COMMON
            | ((driver && wparam_is_hotplug_timer) ? WIN32_ROUTE_INPUT : 0);

      case WIN32_MSG_ENTERSIZEMOVE:
      case WIN32_MSG_EXITSIZEMOVE:
      case WIN32_MSG_ENTERMENULOOP:
      case WIN32_MSG_EXITMENULOOP:
      case WIN32_MSG_POWERBROADCAST:
         return desktop ? WIN32_ROUTE_COMMON : 0;

      case WIN32_MSG_GETMINMAXINFO:
      case WIN32_MSG_COMMAND:
         return WIN32_ROUTE_COMMON;
      case WIN32_MSG_BROWSER_OPEN_RESULT:
      case WIN32_MSG_BROWSER_CANCELLED:
         return threads ? WIN32_ROUTE_COMMON : 0;

      case WIN32_MSG_SETFOCUS:
         return WIN32_ROUTE_CLIP_ON
            | ((desktop && driver) ? WIN32_ROUTE_INPUT : 0);
      case WIN32_MSG_KILLFOCUS:
         return WIN32_ROUTE_CLIP_OFF
            | ((desktop && driver) ? WIN32_ROUTE_INPUT : 0);

      case WIN32_MSG_DISPLAYCHANGE:
         return WIN32_ROUTE_DISPLAY;

      default:
         break;
   }

   return 0;
}

#endif
