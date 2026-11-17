/* Oracle for gfx/common/win32_foreground.h: the foreground window as
 * the input drivers and win32_has_focus() read it every frame.
 *
 * Built with mingw-w64 and run under Wine (or on Windows). As with
 * threaded video, RetroArch's window is made and pumped on a thread of
 * its own and read from another. A second window, not RetroArch's,
 * stands in for any other program: its procedure never tells the
 * cache anything.
 *
 * 1. RetroArch's window in front reads as in front;
 * 2. read again and again with nothing changing, Windows is not asked;
 * 3. another window in front reads as not RetroArch's, though only
 *    RetroArch's window was told;
 * 4. back in front, it reads so again;
 * 5. destroyed, it no longer reads as in front.
 *
 * It also times a million cached reads against a million calls to
 * GetForegroundWindow(); on Windows that is the saving per read. */
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include <retro_atomic.h>

static retro_atomic_int_t asked;
static HWND counted_foreground(void)
{
   retro_atomic_fetch_add_int(&asked, 1);
   return GetForegroundWindow();
}
#define GetForegroundWindow counted_foreground
#include "gfx/common/win32_foreground.h"
#undef GetForegroundWindow

#define WM_TEST_FRONT   (WM_APP + 1)
#define WM_TEST_DESTROY (WM_APP + 2)

static win32_foreground_t fg;
static HWND   ours;
static HANDLE ready;
static DWORD  ours_tid;
static unsigned failures;

#define CHECK(cond, msg) do { if (!(cond)) { failures++; \
   printf("FAIL %s\n", msg); } else printf("ok   %s\n", msg); } while (0)

static LRESULT CALLBACK ours_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
   win32_foreground_message(&fg, msg);
   switch (msg)
   {
      case WM_TEST_FRONT:
         SetForegroundWindow(hwnd);
         return 0;
      case WM_TEST_DESTROY:
         DestroyWindow(hwnd);
         return 0;
      case WM_DESTROY:
         PostQuitMessage(0);
         return 0;
      default:
         break;
   }
   return DefWindowProcA(hwnd, msg, wp, lp);
}

static DWORD WINAPI window_thread(LPVOID arg)
{
   WNDCLASSA wc;
   MSG msg;
   (void)arg;

   memset(&wc, 0, sizeof(wc));
   wc.lpfnWndProc   = ours_proc;
   wc.hInstance     = GetModuleHandleA(NULL);
   wc.lpszClassName = "win32_foreground_ours";
   RegisterClassA(&wc);
   ours = CreateWindowA(wc.lpszClassName, "ours",
         WS_OVERLAPPEDWINDOW | WS_VISIBLE, 20, 20, 200, 150,
         NULL, NULL, wc.hInstance, NULL);
   SetEvent(ready);
   while (GetMessageA(&msg, NULL, 0, 0) > 0)
   {
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
   }
   return 0;
}

static void pump(void)
{
   MSG msg;
   while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
   {
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
   }
}

/* Reads, as the frontend's poll does, until 'want' (or until the read
 * is not 'want' when 'is' is 0); about two seconds at most. */
static HWND read_until(HWND want, int is)
{
   HWND got = NULL;
   int i;
   for (i = 0; i < 200; i++)
   {
      pump();
      got = win32_foreground_get(&fg);
      if ((got == want) == (is != 0))
         break;
      Sleep(10);
   }
   return got;
}

/* A switch to another process's window ends with messages that
 * process's thread sends ours (WM_ACTIVATE, WM_ACTIVATEAPP,
 * WM_KILLFOCUS), and they can come after the foreground has already
 * moved. Each one rightly makes the next read ask again, so the steady
 * reads wait for them: 100 ms with no activation message. */
static void settle(void)
{
   int i, quiet = 0, moved = retro_atomic_load_acquire_int(&fg.moved);
   for (i = 0; i < 200 && quiet < 10; i++)
   {
      int now;
      pump();
      Sleep(10);
      now   = retro_atomic_load_acquire_int(&fg.moved);
      quiet = (now == moved) ? quiet + 1 : 0;
      moved = now;
   }
   win32_foreground_get(&fg);
}

static int steady_reads_ask_nothing(HWND want)
{
   int i, ok = 1;
   int before = retro_atomic_load_acquire_int(&asked);
   for (i = 0; i < 100000; i++)
      if (win32_foreground_get(&fg) != want)
         ok = 0;
   return ok && retro_atomic_load_acquire_int(&asked) == before;
}

static void bench(void)
{
   LARGE_INTEGER f, t0, t1, t2;
   volatile HWND sink;
   int i;
   QueryPerformanceFrequency(&f);
   QueryPerformanceCounter(&t0);
   for (i = 0; i < 1000000; i++)
      sink = win32_foreground_get(&fg);
   QueryPerformanceCounter(&t1);
   for (i = 0; i < 1000000; i++)
      sink = GetForegroundWindow();
   QueryPerformanceCounter(&t2);
   (void)sink;
   printf("time 1e6 cached reads: %.2f ms, 1e6 GetForegroundWindow(): %.2f ms\n",
         (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)f.QuadPart,
         (double)(t2.QuadPart - t1.QuadPart) * 1000.0 / (double)f.QuadPart);
}

int main(void)
{
   WNDCLASSA wc;
   HWND other, got;
   HANDLE thread;
   int i;

   ready  = CreateEventA(NULL, TRUE, FALSE, NULL);
   thread = CreateThread(NULL, 0, window_thread, NULL, 0, &ours_tid);
   WaitForSingleObject(ready, INFINITE);
   if (!ours)
   {
      printf("FAIL no window\n");
      return 1;
   }

   memset(&wc, 0, sizeof(wc));
   wc.lpfnWndProc   = DefWindowProcA;
   wc.hInstance     = GetModuleHandleA(NULL);
   wc.lpszClassName = "win32_foreground_other";
   RegisterClassA(&wc);
   other = CreateWindowA(wc.lpszClassName, "other",
         WS_OVERLAPPEDWINDOW | WS_VISIBLE, 260, 20, 200, 150,
         NULL, NULL, wc.hInstance, NULL);

   /* 1 */
   PostMessageA(ours, WM_TEST_FRONT, 0, 0);
   for (i = 0; i < 200 && GetForegroundWindow() != ours; i++)
   {
      pump();
      Sleep(10);
   }
   if (GetForegroundWindow() != ours)
   {
      printf("FAIL the window could not be brought to the front\n");
      return 1;
   }
   CHECK(read_until(ours, 1) == ours, "RetroArch's window in front reads as in front");

   /* 2 */
   settle();
   CHECK(steady_reads_ask_nothing(ours),
         "100000 reads with nothing changing asked Windows nothing");

   /* 3 */
   SetForegroundWindow(other);
   for (i = 0; i < 200 && GetForegroundWindow() != other; i++)
   {
      pump();
      Sleep(10);
   }
   got = read_until(ours, 0);
   CHECK(got != ours, "another program's window in front does not read as RetroArch's");
   settle();
   got = win32_foreground_get(&fg);
   CHECK(steady_reads_ask_nothing(got),
         "with another window in front, reads asked Windows nothing");

   /* 4 */
   PostMessageA(ours, WM_TEST_FRONT, 0, 0);
   CHECK(read_until(ours, 1) == ours, "back in front, it reads as in front");

   bench();

   /* 5 */
   PostMessageA(ours, WM_TEST_DESTROY, 0, 0);
   WaitForSingleObject(thread, 5000);
   CHECK(read_until(ours, 0) != ours, "destroyed, it no longer reads as in front");

   DestroyWindow(other);
   if (failures)
   {
      printf("FAIL win32_foreground_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS win32_foreground_test\n");
   return 0;
}
