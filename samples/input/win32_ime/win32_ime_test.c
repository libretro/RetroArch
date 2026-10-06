/* Oracle for gfx/common/win32_ime.h: RetroArch's window has its input
 * context - and so the IME - only while a line of text is open.
 *
 * Built with mingw-w64 and run under Wine (or on Windows). Like
 * RetroArch with threaded video, the window is made and pumped on a
 * thread of its own, and the "frontend" - this test's main thread -
 * opens and closes a line of text from another thread. Whether the
 * window has an input context is asked on the window's own thread,
 * where ImmGetContext() answers for it.
 *
 * 1. a window made with no line open has no input context;
 * 2. a line opened: the window gets one, once its thread has taken the
 *    posted message;
 * 3. the line closed: the context is gone again;
 * 4. a window made while a line is open keeps its context;
 * 5. saying the same thing twice posts nothing the second time. */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <imm.h>

#include "gfx/common/win32_ime.h"

#define WM_TEST_HAS_IMC (WM_APP + 1)
#define WM_TEST_QUIT    (WM_APP + 2)

static unsigned failures;
static retro_atomic_int_t posted;
#define CHECK(cond, msg) do { if (!(cond)) { failures++; \
   printf("FAIL %s\n", msg); } else printf("ok   %s\n", msg); } while (0)

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
   {
      case WM_CREATE:
         win32_ime_sync(hwnd);
         return 0;
      case WIN32_WM_TEXT_ENTRY:
         retro_atomic_fetch_add_int(&posted, 1);
         win32_ime_sync(hwnd);
         return 0;
      case WM_TEST_HAS_IMC:
         {
            HIMC imc = ImmGetContext(hwnd);
            if (imc)
               ImmReleaseContext(hwnd, imc);
            return imc != NULL;
         }
      case WM_TEST_QUIT:
         DestroyWindow(hwnd);
         return 0;
      case WM_DESTROY:
         PostQuitMessage(0);
         return 0;
   }
   return DefWindowProcA(hwnd, msg, wp, lp);
}

static HANDLE ready;
static HWND   window;

static DWORD WINAPI window_thread(LPVOID unused)
{
   MSG m;
   (void)unused;
   window = CreateWindowA("win32_ime_test", "win32_ime_test",
         WS_OVERLAPPEDWINDOW, 0, 0, 160, 120, NULL, NULL,
         GetModuleHandleA(NULL), NULL);
   SetEvent(ready);
   while (GetMessageA(&m, NULL, 0, 0) > 0)
   {
      TranslateMessage(&m);
      DispatchMessageA(&m);
   }
   return 0;
}

static HANDLE start_window(void)
{
   HANDLE t;
   window = NULL;
   ResetEvent(ready);
   t = CreateThread(NULL, 0, window_thread, NULL, 0, NULL);
   WaitForSingleObject(ready, 5000);
   return t;
}

static void stop_window(HANDLE t)
{
   SendMessageA(window, WM_TEST_QUIT, 0, 0);
   WaitForSingleObject(t, 5000);
   CloseHandle(t);
}

/* A sent message can be handled before a posted one queued ahead of it,
 * so wait until the window's thread has taken @want_posted posts. */
static int has_imc_after(int want_posted)
{
   int i;
   for (i = 0; i < 200 && retro_atomic_load_acquire_int(&posted) < want_posted; i++)
      Sleep(5);
   return (int)SendMessageA(window, WM_TEST_HAS_IMC, 0, 0);
}

static void text_entry(bool active)
{
   if (win32_ime_set(active))
      win32_ime_post(window);
}

int main(void)
{
   WNDCLASSA wc;
   HANDLE t;
   int before;

   memset(&wc, 0, sizeof(wc));
   wc.lpfnWndProc   = proc;
   wc.hInstance     = GetModuleHandleA(NULL);
   wc.lpszClassName = "win32_ime_test";
   RegisterClassA(&wc);
   ready = CreateEventA(NULL, TRUE, FALSE, NULL);

   /* 1 */
   t = start_window();
   CHECK(window != NULL, "the window is made on its own thread");
   CHECK(!has_imc_after(0), "made with no line of text open: no input context");

   /* 2 */
   text_entry(true);
   CHECK(has_imc_after(1), "a line opened from another thread: the window has an input context");

   /* 3 */
   text_entry(false);
   CHECK(!has_imc_after(2), "the line closed: the input context is gone");

   /* 5 */
   before = retro_atomic_load_acquire_int(&posted);
   text_entry(false);
   Sleep(50);
   CHECK(retro_atomic_load_acquire_int(&posted) == before,
         "closing a line already closed posts nothing");
   stop_window(t);

   /* 4 */
   text_entry(true);
   t = start_window();
   CHECK(has_imc_after(0), "made while a line is open: the window keeps its input context");
   text_entry(false);
   stop_window(t);

   if (failures)
   {
      printf("FAIL win32_ime_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS win32_ime_test\n");
   return 0;
}
