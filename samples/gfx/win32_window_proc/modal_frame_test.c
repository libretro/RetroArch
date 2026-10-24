#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/common/win32_pump.h"

static win32_pump_t pump;
static unsigned failures, enters, exits, sizes;

#define CHECK(test) do { if (!(test)) { \
   fprintf(stderr, "FAIL line %u: %s\n", (unsigned)__LINE__, #test); \
   failures++; } } while (0)

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message,
      WPARAM wparam, LPARAM lparam)
{
   switch (message)
   {
      case WM_ENTERSIZEMOVE:
         enters++;
         _controlfp(_RC_UP, _MCW_RC);
         CHECK(win32_pump_enter(&pump, hwnd));
         CHECK((_controlfp(0, 0) & _MCW_RC) == _RC_UP);
         break;
      case WM_EXITSIZEMOVE:
         exits++;
         win32_pump_leave(&pump);
         break;
      case WM_SIZING:
         sizes++;
         break;
      case WM_PAINT:
         _controlfp(_RC_UP, _MCW_RC);
         if (win32_pump_paint(&pump, hwnd))
         {
            CHECK((_controlfp(0, 0) & _MCW_RC) == _RC_UP);
            return 0;
         }
         break;
      case WM_DESTROY:
         win32_pump_abort(&pump, hwnd);
         break;
   }
   return DefWindowProc(hwnd, message, wparam, lparam);
}

static bool next_message(void)
{
   MSG msg;
   _controlfp(_RC_UP, _MCW_RC);
   if (!PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
      return false;
   TranslateMessage(&msg);
   DispatchMessage(&msg);
   return true;
}

static void run_case(unsigned mode)
{
   HWND hwnd;
   RECT rect, initial;
   POINT cursor;
   unsigned i, live = 0, painted = 0;
   unsigned original = _controlfp(0, 0);
   enters = exits = sizes = 0;
   _controlfp(_RC_DOWN, _MCW_RC);
   hwnd = CreateWindowA("RetroArchModalFrameTest", "Modal frame test",
         WS_OVERLAPPEDWINDOW, 30, 30, 320, 240, NULL, NULL,
         GetModuleHandle(NULL), NULL);
   CHECK(hwnd != NULL);
   if (!hwnd) return;
   ShowWindow(hwnd, SW_SHOWNOACTIVATE);
   GetWindowRect(hwnd, &initial);
   GetCursorPos(&cursor);
   SetCursorPos(initial.right - 4, initial.bottom - 4);
   CHECK(win32_pump_run(&pump, next_message));
   PostMessage(hwnd, WM_SYSCOMMAND, SC_SIZE | WMSZ_BOTTOMRIGHT, 0);
   for (i = 0; i < 80; i++)
   {
      CHECK(win32_pump_run(&pump, next_message));
      CHECK((_controlfp(0, 0) & _MCW_RC) == _RC_DOWN);
      if (pump.depth)
      {
         HDC dc = GetDC(hwnd);
         HBRUSH brush = CreateSolidBrush(RGB(i * 3, 20, 180));
         live++;
         GetClientRect(hwnd, &rect);
         if (dc && brush && FillRect(dc, &rect, brush)) painted++;
         DeleteObject(brush);
         ReleaseDC(hwnd, dc);
      }
      if (i == 10 || i == 20)
      {
         SetCursorPos(initial.right + (int)i * 2, initial.bottom + (int)i * 2);
         PostMessage(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(360 + i, 280 + i));
      }
      if (i == 40)
      {
         if (mode == 0)
            PostMessage(hwnd, WM_KEYDOWN, VK_RETURN, 0);
         else if (mode == 1)
            CHECK(win32_pump_cancel(&pump));
         else if (mode == 2)
         {
            DestroyWindow(hwnd);
            CHECK(win32_pump_cancel(&pump));
         }
         else
         {
            CHECK(win32_pump_free(&pump));
         }
      }
   }
   CHECK(enters == 1 && !pump.depth && !pump.busy);
   CHECK(mode == 2 || exits == 1);
   CHECK(live >= 20 && painted == live && sizes > 0);
   if (IsWindow(hwnd)) DestroyWindow(hwnd);
   SetCursorPos(cursor.x, cursor.y);
   CHECK(win32_pump_free(&pump));
   CHECK(!pump.caller && !pump.messages && !pump.busy);
   CHECK((_controlfp(0, 0) & _MCW_RC) == _RC_DOWN);
   _controlfp(original, _MCW_RC);
   printf("mode=%u modal_frames=%u painted=%u sizing=%u enter=%u exit=%u\n",
         mode, live, painted, sizes, enters, exits);
}

static void run_foreign(unsigned flags)
{
   void *caller = ConvertThreadToFiberEx(NULL, flags);
   unsigned original = _controlfp(0, 0) & _MCW_RC;
   unsigned mode;
   CHECK(caller != NULL);
   if (!caller) return;
   if (win32_pump_init(&pump))
   {
      CHECK(pump.caller == caller && !pump.converted);
      for (mode = 0; mode < 4; mode++)
      {
         run_case(mode);
         CHECK(GetCurrentFiber() == caller);
      }
   }
   else
   {
      CHECK(!flags && pump.disabled);
      CHECK((_controlfp(0, 0) & _MCW_RC) == original);
      CHECK(win32_pump_free(&pump));
      printf("UNAVAILABLE: foreign fiber flags=%u continuity; ownership retained\n", flags);
   }
   CHECK(GetCurrentFiber() == caller);
   CHECK(ConvertFiberToThread());
}

int main(int argc, char **argv)
{
   WNDCLASSA wc;
   unsigned mode;
   if (argc > 1 && strcmp(argv[1], "--fallback") == 0)
   {
      unsigned original = _controlfp(0, 0);
      for (mode = 0; mode < 2; mode++)
      {
         unsigned expected = mode ? _RC_UP : _RC_DOWN;
         _controlfp(expected, _MCW_RC);
         CHECK(!win32_pump_init(&pump) && pump.disabled);
         CHECK((_controlfp(0, 0) & _MCW_RC) == expected);
         CHECK(win32_pump_free(&pump));
         CHECK(!pump.messages && !pump.caller);
         CHECK((_controlfp(0, 0) & _MCW_RC) == expected);
      }
      _controlfp(original, _MCW_RC);
      printf("fallback safety: %u failure(s); modal continuity unavailable\n",
            failures);
      return failures ? 1 : 0;
   }
   if (!win32_pump_init(&pump))
   {
      fprintf(stderr, "UNAVAILABLE: runtime does not preserve fiber FP state\n");
      win32_pump_free(&pump);
      return 77;
   }
   SetProcessDPIAware();
   memset(&wc, 0, sizeof(wc));
   wc.lpfnWndProc = window_proc;
   wc.hInstance = GetModuleHandle(NULL);
   wc.lpszClassName = "RetroArchModalFrameTest";
   CHECK(RegisterClassA(&wc) != 0);
   for (mode = 0; mode < 4; mode++)
      run_case(mode);
   run_foreign(1);
   run_foreign(0);
   UnregisterClassA(wc.lpszClassName, wc.hInstance);
   printf("modal frames: %u failure(s)\n", failures);
   return failures ? 1 : 0;
}
