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
typedef long LONG_PTR;
typedef void *HWND;
#define GWL_EXSTYLE (-20)
#define WS_EX_LAYERED 0x00080000
#define WS_EX_TOOLWINDOW 0x00000080
#define LWA_ALPHA 2
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
   opacity_check(hwnd, 50, true, true, true, 127);
   opacity_check(hwnd, 100, true, true, false, 255);
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
