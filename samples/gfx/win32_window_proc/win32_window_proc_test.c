/* win32_window_proc_test.c -- gfx/common/win32_common.c's window
 * procedure on a real window, held to what the twelve procedures it
 * replaced did on one.
 *
 * win32_common.c exported twelve window procedures - Direct3D, WGL,
 * Vulkan and GDI, each for the DirectInput driver, the raw input driver
 * and any other - and has one now. samples/gfx/win32_msg_route holds
 * the new routing to a transcription of the old switches; this holds
 * the procedure itself to the old code, run.
 *
 * It is built for Windows with mingw-w64 and run under Wine. The real
 * win32_common.c is linked with stand-ins for everything it calls
 * outside itself (win32_window_proc_stubs.c), which record each call.
 * For every video family and kind of input driver the test has the
 * window code register its class and create its window, sends it a
 * script of messages - keys, system keys, the mouse, focus, timers,
 * the move and menu loops, size, device and display changes, IME - with
 * the input driver's handler first passing on what it is offered and
 * then taking it, and writes down each answer and each call made on
 * the way. Then twenty fullscreen toggles: the window made and
 * destroyed, windowed and borderless in turn.
 *
 * expected_trace.txt is that trace as the twelve old procedures gave
 * it: this file built with W32T_OLD against the tree before they went
 * (dae95f4cc). run.sh fails on any difference. What the trace leaves
 * out is what the desktop decides and not the procedure - sizes, and
 * whatever arrives while a window is made, destroyed or left to
 * settle. */

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <imm.h>

#include <boolean.h>
#include <string/stdstring.h>

#include "configuration.h"
#include "gfx/video_defines.h"
#include "gfx/common/win32_common.h"

#include "win32_window_proc_trace.h"

#ifdef W32T_OLD
/* The twelve procedures this test was recorded against, as the video
 * drivers chose among them. Building with W32T_OLD against a tree that
 * still has them is how expected_trace.txt was made. */
#define W32T_PROC(name) LRESULT CALLBACK name(HWND, UINT, WPARAM, LPARAM)
W32T_PROC(wnd_proc_d3d_common); W32T_PROC(wnd_proc_d3d_dinput); W32T_PROC(wnd_proc_d3d_winraw);
W32T_PROC(wnd_proc_wgl_common); W32T_PROC(wnd_proc_wgl_dinput); W32T_PROC(wnd_proc_wgl_winraw);
W32T_PROC(wnd_proc_vk_common);  W32T_PROC(wnd_proc_vk_dinput);  W32T_PROC(wnd_proc_vk_winraw);
W32T_PROC(wnd_proc_gdi_common); W32T_PROC(wnd_proc_gdi_dinput); W32T_PROC(wnd_proc_gdi_winraw);
#endif

enum { FAM_D3D = 0, FAM_WGL, FAM_VK, FAM_GDI, FAM_COUNT };
enum { IN_OTHER = 0, IN_DINPUT, IN_WINRAW, IN_COUNT };

static const char *const fam_name[FAM_COUNT] = { "d3d", "wgl", "vulkan", "gdi" };
static const char *const in_name[IN_COUNT]   = { "sdl2", "dinput", "raw" };

static WNDPROC proc_for(int fam, int in)
{
#ifdef W32T_OLD
   static const WNDPROC table[FAM_COUNT][IN_COUNT] = {
      { wnd_proc_d3d_common, wnd_proc_d3d_dinput, wnd_proc_d3d_winraw },
      { wnd_proc_wgl_common, wnd_proc_wgl_dinput, wnd_proc_wgl_winraw },
      { wnd_proc_vk_common,  wnd_proc_vk_dinput,  wnd_proc_vk_winraw  },
      { wnd_proc_gdi_common, wnd_proc_gdi_dinput, wnd_proc_gdi_winraw }
   };
   return table[fam][in];
#else
   static const enum win32_window_family family[FAM_COUNT] = {
      WIN32_WINDOW_D3D, WIN32_WINDOW_WGL, WIN32_WINDOW_VULKAN, WIN32_WINDOW_GDI
   };
   win32_window_proc_setup(family[fam]);
   return win32_window_proc;
#endif
}

static void pump(void)
{
   MSG msg;
   while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
}

static void send_msg(HWND hwnd, const char *name, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   LRESULT ret;
   trace(" %s w=0x%llx\n", name, (unsigned long long)wparam);
   ret = SendMessage(hwnd, message, wparam, lparam);
   trace("  -> %lld\n", (long long)ret);
}

#define KEY_LPARAM(scan, ext) ((LPARAM)(((scan) << 16) | ((ext) ? (1 << 24) : 0) | 1))

static void script(HWND hwnd)
{
   MINMAXINFO mmi;

   send_msg(hwnd, "WM_KEYDOWN return",     WM_KEYDOWN,    VK_RETURN, KEY_LPARAM(0x1C, 0));
   send_msg(hwnd, "WM_KEYUP return",       WM_KEYUP,      VK_RETURN, KEY_LPARAM(0x1C, 0));
   send_msg(hwnd, "WM_KEYDOWN lshift",     WM_KEYDOWN,    VK_SHIFT,  KEY_LPARAM(0x2A, 0));
   send_msg(hwnd, "WM_KEYUP lshift",       WM_KEYUP,      VK_SHIFT,  KEY_LPARAM(0x2A, 0));
   send_msg(hwnd, "WM_KEYDOWN right ext",  WM_KEYDOWN,    VK_RIGHT,  KEY_LPARAM(0x4D, 1));
   send_msg(hwnd, "WM_KEYDOWN pause",      WM_KEYDOWN,    VK_PAUSE,  KEY_LPARAM(0x45, 0));
   send_msg(hwnd, "WM_KEYDOWN numlock",    WM_KEYDOWN,    VK_NUMLOCK,KEY_LPARAM(0x45, 1));
   send_msg(hwnd, "WM_SYSKEYDOWN alt",     WM_SYSKEYDOWN, VK_MENU,   KEY_LPARAM(0x38, 0));
   send_msg(hwnd, "WM_SYSKEYUP alt",       WM_SYSKEYUP,   VK_MENU,   KEY_LPARAM(0x38, 0));
   send_msg(hwnd, "WM_SYSKEYDOWN ralt",    WM_SYSKEYDOWN, VK_MENU,   KEY_LPARAM(0x38, 1));
   send_msg(hwnd, "WM_SYSKEYDOWN f10",     WM_SYSKEYDOWN, VK_F10,    KEY_LPARAM(0x44, 0));
   send_msg(hwnd, "WM_SYSKEYDOWN rshift",  WM_SYSKEYDOWN, VK_RSHIFT, KEY_LPARAM(0x36, 0));
   send_msg(hwnd, "WM_SYSKEYDOWN a",       WM_SYSKEYDOWN, 'A',       KEY_LPARAM(0x1E, 0));
   send_msg(hwnd, "WM_CHAR x",             WM_CHAR,       'x',       1);

   send_msg(hwnd, "WM_MOUSEMOVE",          WM_MOUSEMOVE,  0, MAKELPARAM(10, 20));
   send_msg(hwnd, "WM_MOUSEWHEEL",         WM_MOUSEWHEEL, MAKEWPARAM(0, 120), 0);
   send_msg(hwnd, "WM_MOUSEHWHEEL",        0x020E,        MAKEWPARAM(0, 120), 0);
   send_msg(hwnd, "WM_POINTERUPDATE",      0x0245,        0, 0);
   send_msg(hwnd, "WM_NCLBUTTONDBLCLK",    WM_NCLBUTTONDBLCLK, HTNOWHERE, 0);
   send_msg(hwnd, "WM_DEVICECHANGE",       WM_DEVICECHANGE, 0x0007, 0);

   send_msg(hwnd, "WM_TIMER hotplug",      WM_TIMER,      WIN32_HOTPLUG_TIMER_ID, 0);
   send_msg(hwnd, "WM_TIMER other",        WM_TIMER,      1, 0);
   send_msg(hwnd, "WM_MOVE hotplug id",    WM_MOVE,       WIN32_HOTPLUG_TIMER_ID, MAKELPARAM(5, 5));
   send_msg(hwnd, "WM_MOVE",               WM_MOVE,       0, MAKELPARAM(5, 5));
   send_msg(hwnd, "WM_SIZE",               WM_SIZE,       SIZE_RESTORED, MAKELPARAM(320, 240));

   send_msg(hwnd, "WM_SETFOCUS",           WM_SETFOCUS,   0, 0);
   send_msg(hwnd, "WM_KILLFOCUS",          WM_KILLFOCUS,  0, 0);

   send_msg(hwnd, "WM_ENTERSIZEMOVE",      WM_ENTERSIZEMOVE, 0, 0);
   send_msg(hwnd, "WM_TIMER in sizemove",  WM_TIMER,      1, 0);
   send_msg(hwnd, "WM_EXITSIZEMOVE",       WM_EXITSIZEMOVE,  0, 0);
   send_msg(hwnd, "WM_ENTERMENULOOP",      WM_ENTERMENULOOP, 0, 0);
   send_msg(hwnd, "WM_EXITMENULOOP",       WM_EXITMENULOOP,  0, 0);

   memset(&mmi, 0, sizeof(mmi));
   send_msg(hwnd, "WM_GETMINMAXINFO",      WM_GETMINMAXINFO, 0, (LPARAM)&mmi);

   send_msg(hwnd, "WM_COMMAND",            WM_COMMAND,    12345, 0);
   send_msg(hwnd, "WM_SYSCOMMAND screensave", WM_SYSCOMMAND, SC_SCREENSAVE, 0);
   send_msg(hwnd, "WM_SYSCOMMAND monitorpower", WM_SYSCOMMAND, SC_MONITORPOWER, 0);
   send_msg(hwnd, "WM_IME_ENDCOMPOSITION", WM_IME_ENDCOMPOSITION, 0, 0);
   send_msg(hwnd, "WM_IME_COMPOSITION",    WM_IME_COMPOSITION, 0, GCS_RESULTSTR);
   send_msg(hwnd, "WM_DISPLAYCHANGE",      WM_DISPLAYCHANGE, 32, MAKELPARAM(1280, 720));
   send_msg(hwnd, "WM_PAINT",              WM_PAINT,      0, 0);
   send_msg(hwnd, "WM_NULL",               WM_NULL,       0, 0);
   send_msg(hwnd, "WM_CLOSE",              WM_CLOSE,      0, 0);
}

int main(int argc, char **argv)
{
   int fam, in, takes;
   settings_t *settings = config_get_ptr();
   FILE *out;

   stubs_init();

   for (fam = 0; fam < FAM_COUNT; fam++)
      for (in = 0; in < IN_COUNT; in++)
      {
         WNDCLASSEX wndclass;
         HWND hwnd;

         trace_on          = true;
         trace("== %s window, %s input\n", fam_name[fam], in_name[in]);
         trace_on          = false;
         stub_input_takes  = false;
         stub_create_steps = 0;

         strlcpy(settings->arrays.input_driver, in_name[in],
               sizeof(settings->arrays.input_driver));
         settings->bools.video_fullscreen           = false;
         settings->bools.video_windowed_fullscreen  = true;
         settings->bools.video_window_save_positions = false;
         settings->bools.video_window_show_decorations = true;
         settings->bools.ui_menubar_enable          = false;
         settings->uints.video_window_opacity       = 100;
         settings->floats.video_refresh_rate        = 60.0f;

         win32_window_reset();
         win32_monitor_init();

         memset(&wndclass, 0, sizeof(wndclass));
         wndclass.lpfnWndProc = proc_for(fam, in);
         if (!win32_window_init(&wndclass, true, NULL))
         {
            trace_on = true;
            trace(" window class not registered\n");
            continue;
         }

         if (!win32_set_video_mode(NULL, VIDEO_SCALE_PACK(640, 480), false))
         {
            trace_on = true;
            trace(" window not created\n");
            continue;
         }
         hwnd = win32_get_window();

         /* what dinput.c leaves there for the procedure to find */
         SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)1);
         pump();

         trace_on = true;
         trace(" created: window %s, inited %s, creation step: %s%s%s\n",
               IsWindow(hwnd) ? "yes" : "no",
               (win32_get_flags() & WIN32_CMN_FLAG_INITED) ? "yes" : "no",
               (stub_create_steps & 1) ? "GL context" : "",
               (stub_create_steps & 2) ? "Vulkan surface" : "",
               stub_create_steps ? "" : "none");

         for (takes = 0; takes < 2; takes++)
         {
            stub_input_takes = (takes != 0);
            trace(" -- the input driver %s what it is offered\n",
                  takes ? "takes" : "passes on");
            win32_window_reset();
            script(hwnd);
            trace(" after: quit %s, resized %s\n",
                  (win32_get_flags() & WIN32_CMN_FLAG_QUIT)    ? "yes" : "no",
                  (win32_get_flags() & WIN32_CMN_FLAG_RESIZED) ? "yes" : "no");
         }

         trace_on = false;
         win32_monitor_from_window();
         win32_destroy_window();
         pump();
         trace_on = true;
         trace(" destroyed: window %s\n",
               IsWindow(hwnd) ? "still there" : "gone");
      }

   /* Fullscreen toggles, as a video driver restart does them: the
    * window made, destroyed and made again, windowed and borderless
    * fullscreen in turn, with the class registered and unregistered
    * each time. */
   {
      int cycle, made = 0, gone = 0, inited = 0;
      const int cycles = 20;
      DWORD objects_first = 0, objects_last = 0;

      strlcpy(settings->arrays.input_driver, "raw",
            sizeof(settings->arrays.input_driver));
      trace_on = false;

      for (cycle = 0; cycle < cycles; cycle++)
      {
         WNDCLASSEX wndclass;
         HWND hwnd;
         bool fullscreen = (cycle & 1) != 0;

         settings->bools.video_fullscreen = fullscreen;
         win32_window_reset();
         win32_monitor_init();
         memset(&wndclass, 0, sizeof(wndclass));
         wndclass.lpfnWndProc = proc_for(FAM_VK, IN_WINRAW);
         if (!win32_window_init(&wndclass, true, NULL))
            break;
         if (!win32_set_video_mode(NULL, VIDEO_SCALE_PACK(640, 480), fullscreen))
            break;
         hwnd = win32_get_window();
         pump();
         if (IsWindow(hwnd))
            made++;
         if (win32_get_flags() & WIN32_CMN_FLAG_INITED)
            inited++;
         SendMessage(hwnd, WM_KEYDOWN, 'F', KEY_LPARAM(0x21, 0));
         win32_monitor_from_window();
         win32_destroy_window();
         pump();
         if (!IsWindow(hwnd))
            gone++;

         if (cycle == 3)
            objects_first = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
         objects_last = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
      }

      trace_on = true;
      trace("== %d fullscreen toggles\n", cycles);
      trace(" windows made %d, inited %d, gone again %d\n", made, inited, gone);
      trace(" window objects left behind: %s\n",
            (objects_last > objects_first) ? "yes" : "no");
   }

#ifndef W32T_OLD
   /* The same toggles on one window: win32_window_set_fullscreen()
    * takes the window between windowed and borderless fullscreen where
    * it stands. It has to stay the same window, wear the right style,
    * end up the right size, and tell the video driver through a
    * resize. (The twelve old procedures had no such thing: these lines
    * of the expected trace are this function's own.) */
   {
      WNDCLASSEX wndclass;
      HWND hwnd;
      MONITORINFO mon;
      int cycle, same = 0, styled = 0, sized = 0, resized = 0;
      const int cycles = 20;
      bool refused;
      bool big_grew = false;
      bool big_told = false;

      strlcpy(settings->arrays.input_driver, "raw",
            sizeof(settings->arrays.input_driver));
      settings->bools.video_fullscreen = false;
      trace_on = false;

      win32_window_reset();
      win32_monitor_init();
      memset(&wndclass, 0, sizeof(wndclass));
      wndclass.lpfnWndProc = proc_for(FAM_VK, IN_WINRAW);
      if (     win32_window_init(&wndclass, true, NULL)
            && win32_set_video_mode(NULL, VIDEO_SCALE_PACK(640, 480), false))
      {
         hwnd = win32_get_window();
         pump();

         memset(&mon, 0, sizeof(mon));
         mon.cbSize = sizeof(mon);
         GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mon);

         for (cycle = 0; cycle < cycles; cycle++)
         {
            RECT client;
            bool fullscreen = !(cycle & 1);
            bool popup;

            win32_window_reset();
            settings->bools.video_fullscreen = fullscreen;
            if (!win32_window_set_fullscreen(VIDEO_SCALE_PACK(640, 480), fullscreen))
               break;
            pump();

            if (win32_get_window() == hwnd && IsWindow(hwnd))
               same++;
            popup = (GetWindowLongPtr(hwnd, GWL_STYLE) & WS_POPUP) != 0;
            if (popup == fullscreen && IsWindowVisible(hwnd))
               styled++;
            GetClientRect(hwnd, &client);
            if (fullscreen
                  ? (   client.right  == mon.rcMonitor.right  - mon.rcMonitor.left
                     && client.bottom == mon.rcMonitor.bottom - mon.rcMonitor.top)
                  : (client.right == 640 && client.bottom == 480))
               sized++;
            if (win32_get_flags() & WIN32_CMN_FLAG_RESIZED)
               resized++;
         }

         /* A core whose picture is bigger than the window asked for,
          * with the menu bar on. The window code does not let a window
          * be smaller than the core's picture, so the window that
          * comes out of the toggle is bigger than the one asked for -
          * and the size the video driver is then told has to be the
          * size the window has, not one worked out on the way. */
         {
            RECT client;
            bool quit          = false;
            bool resize        = false;
            unsigned told_dims = 0;

            settings->bools.ui_menubar_enable = true;
            stub_set_geometry(900, 600);

            settings->bools.video_fullscreen = true;
            win32_window_set_fullscreen(VIDEO_SCALE_PACK(640, 480), true);
            pump();
            win32_window_reset();
            settings->bools.video_fullscreen = false;
            win32_window_set_fullscreen(VIDEO_SCALE_PACK(640, 480), false);
            pump();

            win32_check_window(NULL, &quit, &resize, &told_dims);
            GetClientRect(hwnd, &client);
            big_grew  = (client.right >= 900 && client.bottom >= 600);
            big_told  = resize
               && (LONG)VIDEO_SCALE_W(told_dims) == client.right
               && (LONG)VIDEO_SCALE_H(told_dims) == client.bottom;

            settings->bools.ui_menubar_enable = false;
            stub_set_geometry(320, 240);
         }

         /* exclusive fullscreen changes the display mode: not done in
          * place, left to the driver restart */
         settings->bools.video_windowed_fullscreen = false;
         refused = !win32_window_set_fullscreen(VIDEO_SCALE_PACK(640, 480), true);
         settings->bools.video_windowed_fullscreen = true;

         win32_monitor_from_window();
         win32_destroy_window();
         pump();

         trace_on = true;
         trace("== %d fullscreen toggles on one window\n", cycles);
         trace(" same window %d, right style %d, right size %d, resize seen %d\n",
               same, styled, sized, resized);
         trace(" a window grown to fit a bigger core: grew %s, driver told its real size %s\n",
               big_grew ? "yes" : "no", big_told ? "yes" : "no");
         trace(" exclusive fullscreen left to a restart: %s\n",
               refused ? "yes" : "no");
         trace(" destroyed: window %s\n", IsWindow(hwnd) ? "still there" : "gone");
      }
      else
      {
         trace_on = true;
         trace("== fullscreen toggles on one window: no window\n");
      }
   }
#endif

   out = fopen(argc > 1 ? argv[1] : "trace.txt", "wb");
   if (!out)
      return 2;
   fwrite(trace_buf, 1, trace_len, out);
   fclose(out);
   return 0;
}
