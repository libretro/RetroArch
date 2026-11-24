#include <stdio.h>
#include <string.h>
#include <boolean.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#else
typedef unsigned char BYTE;
typedef unsigned DWORD;
typedef long LONG_PTR;
typedef void *HWND;
#define GWL_EXSTYLE (-20)
#define WS_EX_LAYERED 0x00080000
#define WS_EX_TOOLWINDOW 0x00000080
#define LWA_ALPHA 2
#define WM_APP 0x8000
static DWORD opacity_thread = 1;
static unsigned opacity_queued;
static DWORD GetCurrentThreadId(void) { return opacity_thread; }
static DWORD GetWindowThreadProcessId(HWND hwnd, DWORD *pid)
{ (void)pid; return hwnd ? 1 : 0; }
static bool PostMessage(HWND hwnd, unsigned message, unsigned wp, unsigned lp)
{ (void)hwnd; (void)message; (void)wp; (void)lp; opacity_queued++; return true; }
static LONG_PTR opacity_style;
static BYTE opacity_alpha;
static LONG_PTR GetWindowLongPtr(HWND hwnd, int index)
{ (void)hwnd; (void)index; return opacity_style; }
static LONG_PTR SetWindowLongPtr(HWND hwnd, int index, LONG_PTR style)
{
   LONG_PTR previous = opacity_style;
   (void)hwnd; (void)index;
   opacity_style = style;
   return previous;
}
static void SetLastError(unsigned error) { (void)error; }
static unsigned GetLastError(void) { return 0; }
static bool SetLayeredWindowAttributes(HWND hwnd, unsigned key, BYTE alpha,
      unsigned flags)
{
   (void)hwnd; (void)key;
   opacity_alpha = alpha;
   return flags == LWA_ALPHA;
}
#endif

#include "../../../gfx/common/win32_opacity.h"
#include "opacity_impl.inc"

static unsigned failures;
#define CHECK(test) do { if (!(test)) { \
   fprintf(stderr, "FAIL line %u: %s\n", (unsigned)__LINE__, #test); \
   failures++; } } while (0)

static void opacity_check(HWND hwnd, unsigned opacity, bool fullscreen,
      bool workaround, bool layered, unsigned alpha)
{
   CHECK(win32_window_set_opacity(hwnd, opacity, fullscreen, workaround));
   CHECK((GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0);
   CHECK(((GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) != 0) == layered);
   if (layered)
   {
#ifdef _WIN32
      BYTE actual = 0;
      DWORD flags = 0;
      COLORREF key = 0;
      CHECK(GetLayeredWindowAttributes(hwnd, &key, &actual, &flags));
      CHECK(flags == LWA_ALPHA && actual == alpha);
#else
      CHECK(opacity_alpha == alpha);
#endif
   }
}

#if defined(_WIN32) && defined(HAVE_WINDOW_TRANSP)
static HANDLE owner_ready, owner_release, owner_applied, owner_finish, request_done;
static HWND owned_window;
static bool request_result;

static LRESULT CALLBACK opacity_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
#include "opacity_message.inc"
   return DefWindowProc(hwnd, message, wp, lp);
}

static void opacity_pump(void)
{
   MSG msg;
   while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
      DispatchMessage(&msg);
}

static DWORD WINAPI opacity_owner(void *unused)
{
   DWORD result = 0;
   (void)unused;
   owned_window = CreateWindowExA(WS_EX_TOOLWINDOW, "RetroArch Opacity Test", "Owned opacity",
         WS_POPUP, 0, 0, 40, 40, NULL, NULL, GetModuleHandle(NULL), NULL);
   SetEvent(owner_ready);
   WaitForSingleObject(owner_release, 5000);
   opacity_pump();
   SetEvent(owner_applied);
   WaitForSingleObject(owner_finish, 5000);
   /* A fullscreen transition supersedes an earlier queued windowed edit. */
   if (!win32_window_set_opacity(owned_window, 50, true, true)) result = 1;
   opacity_pump();
   if (GetWindowLongPtr(owned_window, GWL_EXSTYLE) & WS_EX_LAYERED) result = 1;
   DestroyWindow(owned_window);
   return result;
}

static DWORD WINAPI opacity_sender(void *unused)
{
   (void)unused;
   request_result = win32_window_set_opacity(owned_window, 40, false, false);
   SetEvent(request_done);
   return 0;
}

static void opacity_threaded(void)
{
   HANDLE owner, sender;
   DWORD result = 1;
   bool returned;
   WNDCLASSA cls;
   memset(&cls, 0, sizeof(cls));
   cls.lpfnWndProc = opacity_proc;
   cls.hInstance = GetModuleHandle(NULL);
   cls.lpszClassName = "RetroArch Opacity Test";
   CHECK(RegisterClassA(&cls));
   owner_ready = CreateEvent(NULL, TRUE, FALSE, NULL);
   owner_release = CreateEvent(NULL, TRUE, FALSE, NULL);
   owner_applied = CreateEvent(NULL, TRUE, FALSE, NULL);
   owner_finish = CreateEvent(NULL, TRUE, FALSE, NULL);
   request_done = CreateEvent(NULL, TRUE, FALSE, NULL);
   owner = CreateThread(NULL, 0, opacity_owner, NULL, 0, NULL);
   CHECK(owner && WaitForSingleObject(owner_ready, 5000) == WAIT_OBJECT_0);
   CHECK(owned_window != NULL);
   sender = CreateThread(NULL, 0, opacity_sender, NULL, 0, NULL);
   returned = sender && WaitForSingleObject(request_done, 1000) == WAIT_OBJECT_0;
   CHECK(returned);
   SetEvent(owner_release);
   if (returned)
   {
      CHECK(request_result);
      CHECK(WaitForSingleObject(owner_applied, 5000) == WAIT_OBJECT_0);
      opacity_check(owned_window, 40, false, false, true, 102);
      CHECK(win32_window_set_opacity(owned_window, 10, false, false));
   }
   SetEvent(owner_finish);
   CHECK(WaitForSingleObject(owner, 5000) == WAIT_OBJECT_0);
   CHECK(WaitForSingleObject(sender, 5000) == WAIT_OBJECT_0);
   CHECK(GetExitCodeThread(owner, &result) && result == 0);
   CloseHandle(owner); CloseHandle(sender);
   CloseHandle(owner_ready); CloseHandle(owner_release); CloseHandle(owner_applied);
   CloseHandle(owner_finish); CloseHandle(request_done);
   UnregisterClassA(cls.lpszClassName, cls.hInstance);
}
#endif

int main(void)
{
   HWND hwnd;
#ifdef _WIN32
   hwnd = CreateWindowExA(WS_EX_TOOLWINDOW, "STATIC", "Opacity regression",
         WS_POPUP, 0, 0, 40, 40, NULL, NULL, GetModuleHandle(NULL), NULL);
   CHECK(hwnd != NULL);
   if (!hwnd) return 1;
#else
   hwnd = &opacity_style;
   opacity_style = WS_EX_TOOLWINDOW;
#endif
#ifdef HAVE_WINDOW_TRANSP
   CHECK(!win32_window_set_opacity(NULL, 100, false, true));
   opacity_check(hwnd, 100, false, false, false, 255);
   opacity_check(hwnd, 100, false, true, true, 254);
   opacity_check(hwnd, 100, true, true, false, 255);
   opacity_check(hwnd, 100, false, true, true, 254);
   opacity_check(hwnd, 100, false, false, false, 255);
   opacity_check(hwnd, 99, false, true, true, 252);
   opacity_check(hwnd, 50, false, true, true, 127);
   opacity_check(hwnd, 50, true, true, false, 255);
   opacity_check(hwnd, 0, true, false, false, 255);
   opacity_check(hwnd, 50, false, false, true, 127);
   opacity_check(hwnd, 100, true, true, false, 255);
#ifndef _WIN32
   opacity_thread = 2;
   CHECK(win32_window_set_opacity(hwnd, 50, false, false));
   CHECK(opacity_queued == 1 && !(opacity_style & WS_EX_LAYERED));
   opacity_thread = 1;
   CHECK(win32_opacity_apply(hwnd));
   CHECK(opacity_style & WS_EX_LAYERED);
   opacity_thread = 2;
   CHECK(win32_window_set_opacity(hwnd, 10, false, false));
   opacity_thread = 1;
   CHECK(win32_window_set_opacity(hwnd, 50, true, true));
   CHECK(win32_opacity_apply(hwnd) && !(opacity_style & WS_EX_LAYERED));
#else
   opacity_threaded();
#endif
#else
   CHECK(!win32_window_set_opacity(hwnd, 100, false, true));
   CHECK(GetWindowLongPtr(hwnd, GWL_EXSTYLE) == WS_EX_TOOLWINDOW);
#endif
#ifdef _WIN32
   DestroyWindow(hwnd);
#endif
   printf("window opacity: %u failure(s)\n", failures);
   return failures ? 1 : 0;
}
