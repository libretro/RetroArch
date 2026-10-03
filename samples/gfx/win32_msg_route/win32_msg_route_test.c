/* win32_msg_route_test.c -- what the Win32 window procedure does with
 * a message, held to the three switches it replaced.
 *
 * gfx/common/win32_common.c had three window procedures that were one
 * long switch each - for the DirectInput driver, the raw input driver,
 * and any other - and twelve exported ones built on them. They are one
 * procedure now, which asks win32_msg_route() what to do with a
 * message.
 *
 * The three switches are written out below as they stood, case list
 * for case list and #if for #if, each returning the steps its case
 * took. The claim:
 *
 *   For every message, with and without the hotplug timer's id in
 *   wparam, on the desktop and on the Xbox, with and without threads,
 *   win32_msg_route() gives the steps the old switch for that kind of
 *   input driver took.
 *
 * A sabotage mode routes one message the way another kind's switch did
 * - raw input's keys made into keyboard events here, which would give
 * every key press twice - and is asserted to be caught. */

#include <stdio.h>

#include <boolean.h>

#include "win32_msg_route.h"

/* winuser.h, as far as the switches use it */
#define WM_DESTROY            0x0002
#define WM_MOVE               0x0003
#define WM_SIZE               0x0005
#define WM_SETFOCUS           0x0007
#define WM_KILLFOCUS          0x0008
#define WM_CLOSE              0x0010
#define WM_QUIT               0x0012
#define WM_GETMINMAXINFO      0x0024
#define WM_DISPLAYCHANGE      0x007E
#define WM_NCLBUTTONDBLCLK    0x00A3
#define WM_KEYDOWN            0x0100
#define WM_KEYUP              0x0101
#define WM_CHAR               0x0102
#define WM_SYSKEYDOWN         0x0104
#define WM_SYSKEYUP           0x0105
#define WM_IME_ENDCOMPOSITION 0x010E
#define WM_IME_COMPOSITION    0x010F
#define WM_COMMAND            0x0111
#define WM_SYSCOMMAND         0x0112
#define WM_TIMER              0x0113
#define WM_MOUSEMOVE          0x0200
#define WM_MOUSEWHEEL         0x020A
#define WM_MOUSEHWHEEL        0x020E
#define WM_ENTERMENULOOP      0x0211
#define WM_EXITMENULOOP       0x0212
#define WM_POWERBROADCAST     0x0218
#define WM_DEVICECHANGE       0x0219
#define WM_ENTERSIZEMOVE      0x0231
#define WM_EXITSIZEMOVE       0x0232
#define WM_DROPFILES          0x0233
#define WM_POINTERUPDATE      0x0245
#define WM_POINTERDOWN        0x0246
#define WM_POINTERUP          0x0247
#define WM_USER               0x0400
#define WM_BROWSER_OPEN_RESULT (WM_USER + 0)
#define WM_BROWSER_CANCELLED   (WM_USER + 1)

static unsigned failures = 0;
static bool     quiet    = false;

/* The steps a case took, in the new names. */
#define IME_END   WIN32_ROUTE_IME_END
#define IME_TEXT  WIN32_ROUTE_IME_TEXT
#define KEY       WIN32_ROUTE_KEY
#define KEY_MODS  WIN32_ROUTE_KEY_MODS
#define KEY_EVENT WIN32_ROUTE_KEY_EVENT
#define CLIP_ON   WIN32_ROUTE_CLIP_ON
#define CLIP_OFF  WIN32_ROUTE_CLIP_OFF
#define INPUT     WIN32_ROUTE_INPUT
#define COMMON    WIN32_ROUTE_COMMON
#define DISPLAY   WIN32_ROUTE_DISPLAY

/* wnd_proc_common_internal(), as it stood. `xbox` stands for
 * defined(_XBOX) and `threads` for defined(HAVE_THREADS). */
static unsigned old_common(unsigned message, bool hotplug, bool xbox, bool threads)
{
   (void)hotplug;
   switch (message)
   {
      case WM_KEYUP:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
         return KEY | KEY_EVENT;
      case WM_MOUSEMOVE:
      case WM_POINTERDOWN:
      case WM_POINTERUP:
      case WM_POINTERUPDATE:
      case WM_DEVICECHANGE:
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL:
      case WM_NCLBUTTONDBLCLK:
         return 0;
      case WM_DROPFILES:
      case WM_SYSCOMMAND:
      case WM_CHAR:
      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
      case WM_MOVE:
      case WM_SIZE:
         return COMMON;
      /* #if !defined(_XBOX) */
      case WM_ENTERSIZEMOVE:
      case WM_EXITSIZEMOVE:
      case WM_ENTERMENULOOP:
      case WM_EXITMENULOOP:
      case WM_TIMER:
      case WM_POWERBROADCAST:
         return xbox ? 0 : COMMON;
      /* #endif */
      case WM_GETMINMAXINFO:
      case WM_COMMAND:
         return COMMON;
      /* #ifdef HAVE_THREADS */
      case WM_BROWSER_OPEN_RESULT:
      case WM_BROWSER_CANCELLED:
         return threads ? COMMON : 0;
      /* #endif */
      case WM_SETFOCUS:
         return CLIP_ON;
      case WM_KILLFOCUS:
         return CLIP_OFF;
      case WM_DISPLAYCHANGE:
         return DISPLAY;
   }
   return 0;
}

/* wnd_proc_winraw_common_internal(), as it stood. */
static unsigned old_winraw(unsigned message, bool hotplug, bool xbox, bool threads)
{
   switch (message)
   {
      case WM_KEYUP:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
         /* keyboard_event in winraw_callback */
         return KEY;
      case WM_MOUSEMOVE:
      case WM_POINTERDOWN:
      case WM_POINTERUP:
      case WM_POINTERUPDATE:
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL:
      case WM_NCLBUTTONDBLCLK:
         return 0;
      case WM_DROPFILES:
      case WM_SYSCOMMAND:
      case WM_CHAR:
      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
      case WM_MOVE:
      case WM_SIZE:
         /* On the Xbox the timer block below is compiled out and these
          * fall through to the group that only calls wnd_proc_common. */
         if (xbox)
            return COMMON;
         /* fall-through */
      case WM_TIMER:
         if (xbox)
            return 0; /* not a case there */
         /* if (wparam == WIN32_HOTPLUG_TIMER_ID
          *       && winraw_handle_message(...)) return 0; */
         return (hotplug ? INPUT : 0) | COMMON;
      case WM_ENTERSIZEMOVE:
      case WM_EXITSIZEMOVE:
      case WM_ENTERMENULOOP:
      case WM_EXITMENULOOP:
      case WM_POWERBROADCAST:
         return xbox ? 0 : COMMON;
      case WM_GETMINMAXINFO:
      case WM_COMMAND:
         return COMMON;
      case WM_BROWSER_OPEN_RESULT:
      case WM_BROWSER_CANCELLED:
         return threads ? COMMON : 0;
      case WM_SETFOCUS:
         return CLIP_ON  | (xbox ? 0 : INPUT);
      case WM_KILLFOCUS:
         return CLIP_OFF | (xbox ? 0 : INPUT);
      case WM_DISPLAYCHANGE:
         return DISPLAY;
      case WM_DEVICECHANGE:
         return xbox ? 0 : INPUT;
   }
   return 0;
}

/* wnd_proc_common_dinput_internal(), as it stood. */
static unsigned old_dinput(unsigned message, bool hotplug, bool xbox, bool threads)
{
   switch (message)
   {
      case WM_IME_ENDCOMPOSITION:
         return IME_END;
      case WM_IME_COMPOSITION:
         return IME_TEXT;
      case WM_KEYUP:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
         /* tell the driver about shift and alt key events */
         return KEY | KEY_MODS | KEY_EVENT;
      case WM_MOUSEMOVE:
      case WM_POINTERDOWN:
      case WM_POINTERUP:
      case WM_POINTERUPDATE:
      case WM_DEVICECHANGE:
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL:
      case WM_NCLBUTTONDBLCLK:
         return xbox ? 0 : INPUT;
      case WM_DROPFILES:
      case WM_SYSCOMMAND:
      case WM_CHAR:
      case WM_CLOSE:
      case WM_DESTROY:
      case WM_QUIT:
      case WM_MOVE:
      case WM_SIZE:
         if (xbox)
            return COMMON;
         /* fall-through */
      case WM_TIMER:
         if (xbox)
            return 0;
         /* if (wparam == WIN32_HOTPLUG_TIMER_ID) { ...
          *    dinput_handle_message(...) -> return 0 } */
         return (hotplug ? INPUT : 0) | COMMON;
      case WM_ENTERSIZEMOVE:
      case WM_EXITSIZEMOVE:
      case WM_ENTERMENULOOP:
      case WM_EXITMENULOOP:
      case WM_POWERBROADCAST:
         return xbox ? 0 : COMMON;
      case WM_GETMINMAXINFO:
      case WM_COMMAND:
         return COMMON;
      case WM_BROWSER_OPEN_RESULT:
      case WM_BROWSER_CANCELLED:
         return threads ? COMMON : 0;
      case WM_SETFOCUS:
         return CLIP_ON  | (xbox ? 0 : INPUT);
      case WM_KILLFOCUS:
         return CLIP_OFF | (xbox ? 0 : INPUT);
      case WM_DISPLAYCHANGE:
         return DISPLAY;
   }
   return 0;
}

/* raw input's keys routed the way the other switches route theirs */
static bool sabotage_winraw_keys = false;

static unsigned route(enum win32_input_kind kind, bool desktop,
      bool threads, unsigned message, bool hotplug)
{
   if (     sabotage_winraw_keys
         && kind == WIN32_INPUT_WINRAW
         && message == WM_KEYDOWN)
      kind = WIN32_INPUT_OTHER;
   return win32_msg_route(kind, desktop, threads, message, hotplug);
}

static void lane_all_messages(void)
{
   static const char *const kind_name[3] = { "other", "dinput", "raw" };
   unsigned kind, xbox, threads, hotplug, message;
   unsigned long compared = 0, wrong = 0;

   for (kind = 0; kind < 3; kind++)
      for (xbox = 0; xbox < 2; xbox++)
         for (threads = 0; threads < 2; threads++)
            for (hotplug = 0; hotplug < 2; hotplug++)
               for (message = 0; message <= 0xffff; message++)
               {
                  unsigned was, is;

                  switch (kind)
                  {
                     case WIN32_INPUT_DINPUT:
                        was = old_dinput(message, hotplug != 0, xbox != 0, threads != 0);
                        break;
                     case WIN32_INPUT_WINRAW:
                        was = old_winraw(message, hotplug != 0, xbox != 0, threads != 0);
                        break;
                     default:
                        was = old_common(message, hotplug != 0, xbox != 0, threads != 0);
                        break;
                  }
                  is = route((enum win32_input_kind)kind, !xbox,
                        threads != 0, message, hotplug != 0);
                  compared++;
                  if (was != is)
                  {
                     if (!wrong && !quiet)
                        fprintf(stderr, "FAIL: message 0x%04x, %s input,"
                              " %s%s%s: the switch took steps 0x%03x,"
                              " the route says 0x%03x\n",
                              message, kind_name[kind],
                              xbox ? "Xbox" : "desktop",
                              threads ? ", threads" : "",
                              hotplug ? ", hotplug timer id" : "",
                              was, is);
                     wrong++;
                  }
               }

   if (wrong)
      failures += (unsigned)wrong;
   else if (!quiet)
      fprintf(stderr, "[pass] %lu message routes are what the three"
            " switches did\n", compared);
}

int main(void)
{
   lane_all_messages();
   if (failures)
   {
      fprintf(stderr, "FAIL win32_msg_route_test: %u routes differ\n", failures);
      return 1;
   }

   quiet                = true;
   sabotage_winraw_keys = true;
   lane_all_messages();
   quiet                = false;
   sabotage_winraw_keys = false;

   if (!failures)
   {
      fprintf(stderr, "FAIL win32_msg_route_test: raw input's keys made"
            " into keyboard events a second time went unnoticed\n");
      return 1;
   }

   fprintf(stderr, "PASS win32_msg_route_test (sabotage caught by %u"
         " checks)\n", failures);
   return 0;
}
