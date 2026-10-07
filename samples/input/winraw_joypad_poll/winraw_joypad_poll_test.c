/* winraw joypad: its window, when raw input is read by the poll.
 *
 * The real driver, cross-built with mingw-w64 and run under Wine. The
 * driver is started from a second thread, as it is from the video
 * thread under threaded video; the test's main thread polls.
 *
 * Checked here:
 *
 * - not read by the poll (RETROARCH_RAWINPUT_POLL=0): init makes the
 *   window, on init's thread, as it always did, and the poll makes no
 *   bulk read;
 * - read by the poll: init makes no window; the first poll does, on
 *   the polling thread, and marks that thread as one whose pump leaves
 *   raw input alone; every poll makes one bulk read;
 * - an arrival or removal waiting for the window is taken within eight
 *   polls, the window's messages being looked for every eighth;
 * - destroy on the polling thread takes the window down at once;
 *   destroy from another thread - a controller plugged in restarts the
 *   driver from the main window's thread - has it gone once its own
 *   thread has pumped; either way the thread's mark is given back;
 * - a restart from that other thread, then a poll: a new window, on
 *   the polling thread.
 *
 * Reports themselves cannot be injected under Wine. That a report
 * read in bulk reaches this driver is the other half, in
 * samples/input/winraw_poll; what this driver does with one is in
 * samples/input/winraw_joypad_axes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "input/drivers_joypad/winraw_joypad.c"

/* ---- The frontend, as far as the driver links against it ------------ */

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port,
      unsigned vid, unsigned pid)
{
   (void)name; (void)display_name; (void)phys; (void)driver;
   (void)port; (void)vid; (void)pid;
   return true;
}
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{ (void)port; (void)name; return true; }

/* winraw_input.c's side of reading by the poll */
static bool polled;
static unsigned queue_reads;
static int  claims;
static DWORD claim_tid;
bool winraw_raw_input_polled(void) { return polled; }
void winraw_queue_read(void)       { queue_reads++; }
void winraw_queue_claim_thread(bool claim)
{
   if (claim)
   {
      claims++;
      claim_tid = GetCurrentThreadId();
   }
   else
      claims--;
}

static int failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

/* ---- the other thread: starts and stops the driver ------------------ */
static DWORD other_tid;
static void *other_result;

static DWORD WINAPI do_init(LPVOID arg)
{
   (void)arg;
   other_tid    = GetCurrentThreadId();
   other_result = winraw_joypad_joypad_init(NULL);
   return 0;
}

static DWORD WINAPI do_destroy(LPVOID arg)
{
   (void)arg;
   winraw_joypad_joypad_destroy();
   return 0;
}

static void on_other_thread(LPTHREAD_START_ROUTINE fn)
{
   HANDLE t = CreateThread(NULL, 0, fn, NULL, 0, NULL);
   WaitForSingleObject(t, 5000);
   CloseHandle(t);
}

/* the frontend's pump on a thread whose raw input the poll reads */
static void pump_leaving_raw_input(void)
{
   MSG msg;
   for (;;)
   {
      bool any = false;
      if (PeekMessageA(&msg, 0, 0, WM_INPUT - 1, PM_REMOVE))
      {
         DispatchMessageA(&msg);
         any = true;
      }
      if (PeekMessageA(&msg, 0, WM_INPUT + 1, 0xFFFFFFFF, PM_REMOVE))
      {
         DispatchMessageA(&msg);
         any = true;
      }
      if (!any)
         break;
   }
}

int main(void)
{
   DWORD main_tid = GetCurrentThreadId();
   HWND  w;
   MSG   probe;

   /* ---- not read by the poll: as it was -------------------------- */
   polled = false;
   CHECK(winraw_joypad_joypad_init(NULL) != NULL, "init, not read by the poll");
   w = winraw_joypad_msg_window;
   CHECK(w && GetWindowThreadProcessId(w, NULL) == main_tid,
         "not read by the poll: init did not make the window on its own thread");
   winraw_joypad_joypad_poll();
   CHECK(queue_reads == 0 && claims == 0,
         "not read by the poll: %u bulk reads, %d threads marked", queue_reads, claims);
   winraw_joypad_joypad_destroy();
   CHECK(!IsWindow(w), "not read by the poll: destroy left the window");
   printf("   ok   =0: the window is made by init, no bulk read, no thread marked\n");

   /* ---- read by the poll ----------------------------------------- */
   polled = true;
   on_other_thread(do_init);
   CHECK(other_result != NULL, "init on the other thread");
   CHECK(!winraw_joypad_msg_window, "init made the window");
   winraw_joypad_joypad_poll();
   w = winraw_joypad_msg_window;
   CHECK(w && IsWindow(w) && GetWindowThreadProcessId(w, NULL) == main_tid
         && main_tid != other_tid,
         "the first poll did not make the window on the polling thread");
   CHECK(claims == 1 && claim_tid == main_tid, "the polling thread was not marked (%d)", claims);
   CHECK(queue_reads == 1, "the first poll made %u bulk reads", queue_reads);
   winraw_joypad_joypad_poll();
   winraw_joypad_joypad_poll();
   CHECK(queue_reads == 3 && winraw_joypad_msg_window == w && claims == 1,
         "three polls: %u bulk reads, %d marks", queue_reads, claims);
   printf("   ok   read by the poll: init makes no window, the first poll does on its own thread, one bulk read a poll\n");

   /* ---- an arrival or removal waiting for the window ------------- */
   /* a removal of a device the driver does not have: taken, and
    * nothing to do */
   /* The window's messages are looked for every eighth poll: within
    * eight polls it is taken, and not by every poll. */
   {
      unsigned polls = 0, taken_at = 0, i;
      unsigned reads0 = queue_reads;
      PostMessageA(w, WM_INPUT_DEVICE_CHANGE, GIDC_REMOVAL, (LPARAM)0x1234);
      pump_leaving_raw_input();
      for (i = 1; i <= 8; i++)
      {
         winraw_joypad_joypad_poll();
         polls++;
         if (!taken_at && !PeekMessageA(&probe, w, WM_INPUT_DEVICE_CHANGE,
                  WM_INPUT_DEVICE_CHANGE, PM_NOREMOVE))
            taken_at = i;
      }
      CHECK(taken_at >= 1, "eight polls left a device change waiting");
      CHECK(queue_reads - reads0 == polls, "the bulk read is not made by every poll (%u in %u)",
            queue_reads - reads0, polls);
      /* and the one after a look does not look: a second message waits */
      PostMessageA(w, WM_INPUT_DEVICE_CHANGE, GIDC_REMOVAL, (LPARAM)0x1234);
      winraw_joypad_joypad_poll();
      if (taken_at == 8)
         CHECK(PeekMessageA(&probe, w, WM_INPUT_DEVICE_CHANGE, WM_INPUT_DEVICE_CHANGE, PM_NOREMOVE),
               "the poll straight after a look looked again");
      for (i = 0; i < 8; i++)
         winraw_joypad_joypad_poll();
      CHECK(!PeekMessageA(&probe, w, WM_INPUT_DEVICE_CHANGE, WM_INPUT_DEVICE_CHANGE, PM_NOREMOVE),
            "the second device change was never taken");
      printf("   ok   a device change waiting for the window is taken within eight polls, not looked for by every one\n");
   }

   /* ---- destroy from another thread ------------------------------ */
   on_other_thread(do_destroy);
   CHECK(winraw_joypad_msg_window == NULL && claims == 0,
         "destroy from another thread: window %p, %d marks",
         (void*)winraw_joypad_msg_window, claims);
   pump_leaving_raw_input();
   CHECK(!IsWindow(w), "destroyed from another thread, the window is still there after its thread pumped");
   printf("   ok   destroy from another thread: the window goes when its own thread pumps\n");

   /* ---- a restart from that thread, then a poll ------------------ */
   on_other_thread(do_init);
   CHECK(other_result != NULL && !winraw_joypad_msg_window, "restart from the other thread");
   winraw_joypad_joypad_poll();
   w = winraw_joypad_msg_window;
   CHECK(w && GetWindowThreadProcessId(w, NULL) == main_tid && claims == 1,
         "after a restart the poll did not make a new window on its thread");

   /* ---- destroy on the polling thread ---------------------------- */
   winraw_joypad_joypad_destroy();
   CHECK(!IsWindow(w) && claims == 0, "destroy on the polling thread left the window or the mark");
   printf("   ok   restart then poll: a new window on the polling thread; destroy there takes it down at once\n");

   if (failures)
   {
      printf("FAIL winraw_joypad_poll_test: %d\n", failures);
      return 1;
   }
   printf("PASS winraw_joypad_poll_test\n");
   return 0;
}
