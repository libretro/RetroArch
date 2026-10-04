/* winraw: the keyboard and mouse read in bulk by the poll (the
 * default; RETROARCH_RAWINPUT_POLL=0 turns it off).
 *
 * The real driver, cross-built with mingw-w64 and run under Wine on a
 * virtual display. The window that has the focus is made and pumped
 * by a second thread, as the RetroArch window is by the video thread
 * under threaded video; the driver is started from that thread too,
 * as a video driver's init starts it; the test's main thread polls.
 *
 * Checked here:
 *
 * - with the switch off the driver is as it was: its window is made by
 *   init, on init's thread, and nothing is read in bulk;
 * - with it on, init makes no window; the first poll does, on the
 *   polling thread;
 * - keys injected with SendInput() while nothing pumps are all there
 *   after one poll: in the key table, and as key events delivered on
 *   the polling thread, in order, with the modifier held at the time;
 *   they were read in bulk, none as a message, and none is left in the
 *   queue afterwards to be read twice;
 * - reports the pump gets to first: the callback takes the one its
 *   message carries and reads the rest in bulk, in order, and their
 *   key events still wait for the poll;
 * - with nothing waiting, a poll makes one read and a pump makes none;
 * - a read made for the controller driver, whose poll comes first, is
 *   not repeated by this driver's poll, which still delivers what it
 *   took;
 * - a wait on the queue for any input ends at once with a report
 *   waiting, and a wait that leaves raw input out does not, which is
 *   the wait the paused loop makes on this thread;
 * - a pump that asks for the messages below WM_INPUT and above it, as
 *   the frontend's does on this thread, leaves the reports in the
 *   queue, and the next poll reads them all in one;
 * - a controller's report in a bulk read is passed to the controller
 *   driver;
 * - which modifier keys are held is read from the key table, and only
 *   the lock keys' toggles from the published value;
 * - background input is refused, unless registered as a sink with the
 *   main window focused;
 * - free takes the window down, and twenty restarts each do the same;
 * - with everything on one thread, as without threaded video, the
 *   same holds.
 *
 * What it cannot check is the thing only Windows can answer: whether
 * Windows, like Wine, gives raw input to a window whose thread does
 * not have the focus. */
#include <stdio.h>
#include <stdlib.h>

#include <retro_atomic.h>

#include "input/drivers/winraw_input.c"

/* The frontend, as far as the driver links against it. */
uint8_t g_win32_flags;
ui_window_win32_t main_window;
retro_keybind_set input_config_binds[MAX_USERS];
retro_keybind_set input_autoconf_binds[MAX_USERS];
enum retro_key rarch_keysym_lut[RETROK_LAST];
const struct rarch_key_map rarch_key_map_winraw[] = { { 0, RETROK_UNKNOWN } };
static settings_t stub_settings;
static struct menu_state stub_menu;
settings_t *config_get_ptr(void) { return &stub_settings; }

/* A port's keyboard, as the frontend works it out from the port's pin
 * and number: the rule itself (input/common/input_device_pins.h), fed
 * from the stub settings each time it is asked. */
#include "input/common/input_device_pins.h"
static char     stub_pin_ident[MAX_INPUT_DEVICES][INPUT_PIN_LEN];
static unsigned stub_pin_listed;
void input_keyboard_pins_set_devices(const char (*base)[64], unsigned n)
{
   memset(stub_pin_ident, 0, sizeof(stub_pin_ident));
   if (n)
      input_pins_identities(stub_pin_ident, base, n);
   stub_pin_listed = n;
}
int input_keyboard_port_choice(unsigned port)
{
   int8_t choice[MAX_USERS];
   input_pins_resolve(choice,
         (const char (*)[INPUT_PIN_LEN])stub_settings.arrays.input_keyboard_device,
         stub_settings.uints.input_keyboard_index, MAX_USERS,
         (const char (*)[INPUT_PIN_LEN])stub_pin_ident, stub_pin_listed);
   return choice[port];
}

/* and its mouse, the same way */
static char     stub_mouse_ident[MAX_INPUT_DEVICES][INPUT_PIN_LEN];
static unsigned stub_mouse_listed;
void input_mouse_pins_set_devices(const char (*base)[64], unsigned n)
{
   memset(stub_mouse_ident, 0, sizeof(stub_mouse_ident));
   if (n > MAX_INPUT_DEVICES)
      n = MAX_INPUT_DEVICES;
   if (n)
      input_pins_identities(stub_mouse_ident, base, n);
   stub_mouse_listed = n;
}
unsigned input_mouse_port_index(unsigned port)
{
   int16_t choice[MAX_USERS];
   input_pins_resolve_mice(choice,
         (const char (*)[INPUT_PIN_LEN])stub_settings.arrays.input_mouse_device,
         stub_settings.uints.input_mouse_index, MAX_USERS,
         (const char (*)[INPUT_PIN_LEN])stub_mouse_ident, stub_mouse_listed);
   return choice[port] < 0 ? MAX_INPUT_DEVICES : (unsigned)choice[port];
}
struct menu_state *menu_state_get_ptr(void) { return &stub_menu; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void input_config_set_mouse_display_name(unsigned port, const char *name)
{ (void)port; (void)name; }
void input_config_set_keyboard_display_name(unsigned idx, const char *name)
{ (void)idx; (void)name; }
void input_config_clear_keyboard_display_names(void) { }
void input_config_clear_mouse_info(void) { }
void input_config_set_mouse_device(unsigned idx, const char *device,
      uint16_t vid, uint16_t pid, bool hidden)
{ (void)idx; (void)device; (void)vid; (void)pid; (void)hidden; }
void input_config_set_keyboard_ids(unsigned idx, uint16_t vid, uint16_t pid)
{ (void)idx; (void)vid; (void)pid; }
unsigned input_driver_lightgun_id_convert(unsigned id) { return id; }
bool input_driver_pointer_is_offscreen(int16_t x, int16_t y)
{ (void)x; (void)y; return false; }
void input_keymaps_init_keyboard_lut(const struct rarch_key_map *map)
{ (void)map; }
void joypad_driver_reinit(void *data, const char *name)
{ (void)data; (void)name; }
retro_task_t *task_init(void) { return NULL; }
bool task_queue_push(retro_task_t *task) { (void)task; return false; }
void task_set_flags(retro_task_t *task, uint8_t flags, bool set)
{ (void)task; (void)flags; (void)set; }
bool video_driver_get_viewport_info(struct video_viewport *vp)
{ (void)vp; return false; }
bool video_driver_translate_coord_viewport(struct video_viewport *vp,
      int mouse_x, int mouse_y, int16_t *res_x, int16_t *res_y,
      int16_t *res_screen_x, int16_t *res_screen_y, bool report_oob)
{
   (void)vp; (void)mouse_x; (void)mouse_y; (void)res_x; (void)res_y;
   (void)res_screen_x; (void)res_screen_y; (void)report_oob;
   return false;
}
uintptr_t video_driver_window_get(void) { return 0; }
void win32_clip_window(bool grab) { (void)grab; }
void win32_hotplug_arm(void) { }
bool win32_hotplug_due(void) { return false; }



/* a key event as the frontend receives it, and on which thread */
#define SEEN_MAX 64
static struct { bool down; unsigned code; uint16_t mod; DWORD tid; } seen[SEEN_MAX];
static unsigned seen_n;

void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device)
{
   (void)character; (void)device;
   if (seen_n < SEEN_MAX)
   {
      seen[seen_n].down = down;
      seen[seen_n].code = code;
      seen[seen_n].mod  = mod;
      seen[seen_n].tid  = GetCurrentThreadId();
      seen_n++;
   }
}
/* the scancode itself, so the test can see which key came through */
enum retro_key input_keymaps_translate_keysym_to_rk(unsigned sym)
{ return (enum retro_key)sym; }

static uint16_t published_mods;
uint16_t win32_get_keyboard_mods(void) { return published_mods; }

/* whether the controller driver can stay across a video restart */
static bool joypad_can_stay;
bool winraw_joypad_survives_video(void) { return joypad_can_stay; }

/* the controller driver's entry for reports read in bulk */
static unsigned hid_taken;
static DWORD    hid_taken_size;
void winraw_joypad_take_hid(HANDLE device, const BYTE *data,
      DWORD report_size, DWORD count)
{
   (void)device; (void)data;
   hid_taken++;
   hid_taken_size = report_size * count;
}

static unsigned failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

static winraw_input_t *wr;

/* ---- the "video thread": owns the focused window, pumps it, and is
 * where the driver is started from ---------------------------------- */
static HANDLE vt_ready, vt_quit;
static HWND   vt_window;
static DWORD  vt_tid;
static void  *vt_init_result;
static retro_atomic_int_t vt_do_init;

static LRESULT CALLBACK plain_proc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{ return DefWindowProcA(w, m, wp, lp); }

static DWORD WINAPI video_thread(LPVOID arg)
{
   WNDCLASSA wc;
   MSG msg;
   (void)arg;

   vt_tid = GetCurrentThreadId();
   memset(&wc, 0, sizeof(wc));
   wc.lpfnWndProc   = plain_proc;
   wc.hInstance     = GetModuleHandleA(NULL);
   wc.lpszClassName = "winraw-poll-test";
   RegisterClassA(&wc);
   vt_window = CreateWindowExA(0, wc.lpszClassName, "t",
         WS_OVERLAPPEDWINDOW | WS_VISIBLE, 10, 10, 200, 100,
         NULL, NULL, wc.hInstance, NULL);
   if (vt_window)
   {
      SetForegroundWindow(vt_window);
      SetFocus(vt_window);
   }
   SetEvent(vt_ready);

   while (WaitForSingleObject(vt_quit, 2) == WAIT_TIMEOUT)
   {
      while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
         DispatchMessageA(&msg);
      if (retro_atomic_load_acquire_int(&vt_do_init) == 1)
      {
         vt_init_result = winraw_init("null");
         retro_atomic_store_release_int(&vt_do_init, 2);
      }
   }
   if (vt_window)
      DestroyWindow(vt_window);
   return 0;
}

static void *init_on_video_thread(void)
{
   unsigned spins;
   vt_init_result = NULL;
   retro_atomic_store_release_int(&vt_do_init, 1);
   for (spins = 0; spins < 2000
         && retro_atomic_load_acquire_int(&vt_do_init) != 2; spins++)
      Sleep(1);
   retro_atomic_store_release_int(&vt_do_init, 0);
   return vt_init_result;
}

static void key(WORD scan, bool down)
{
   INPUT in;
   memset(&in, 0, sizeof(in));
   in.type       = INPUT_KEYBOARD;
   in.ki.wScan   = scan;
   in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
   SendInput(1, &in, sizeof(in));
}

/* ui_application_win32_process_events() on a thread whose raw input
 * the poll reads: everything but WM_INPUT. */
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

static unsigned wm_input_waiting(void)
{
   MSG msg;
   unsigned n = 0;
   while (PeekMessageA(&msg, NULL, WM_INPUT, WM_INPUT, PM_REMOVE))
      n++;
   return n;
}

int main(void)
{
   DWORD  main_tid = GetCurrentThreadId();
   HANDLE vt;
   HWND   raw_window;
   unsigned i;
   bool   raw_arrives = false;

   vt_ready = CreateEventA(NULL, TRUE, FALSE, NULL);
   vt_quit  = CreateEventA(NULL, TRUE, FALSE, NULL);
   vt       = CreateThread(NULL, 0, video_thread, NULL, 0, NULL);
   WaitForSingleObject(vt_ready, 5000);
   if (!vt_window)
      printf("   note no display for a window: injected input is not checked\n");

   /* ---- the switch off: as it was -------------------------------- */
   _putenv("RETROARCH_RAWINPUT_POLL=0");
   wr = (winraw_input_t*)init_on_video_thread();
   CHECK(wr != NULL, "init with the switch off");
   if (!wr)
      return 1;
   CHECK(!wr->poll_drain, "reading in bulk with the switch off");
   CHECK(wr->window && GetWindowThreadProcessId(wr->window, NULL) == vt_tid,
         "with the switch off the window is not made by init, on init's thread");
   CHECK(winraw_drain_tid == 0, "a bulk-read thread is set with the switch off");
   joypad_can_stay = true;
   CHECK(!input_winraw.survives_video(wr),
         "not read by the poll, the driver says it survives a video restart");
   winraw_poll(wr);
   CHECK(wr->drain_reads == 0, "a bulk read was made with the switch off");
   raw_window = wr->window;
   /* destroyed by its own thread, as the frontend does */
   SetWindowLongPtr(raw_window, GWLP_USERDATA, 0);
   PostMessageA(raw_window, WM_CLOSE, 0, 0);
   wr->window = NULL;
   winraw_free(wr);
   printf("   ok   =0: the window is made by init on init's thread, nothing read in bulk\n");

   /* ---- the switch on -------------------------------------------- */
   /* unset: on is the default */
   _putenv("RETROARCH_RAWINPUT_POLL=");
   wr = (winraw_input_t*)init_on_video_thread();
   CHECK(wr != NULL, "init with nothing set");
   if (!wr)
      return 1;
   CHECK(wr->poll_drain && !wr->sink, "not reading in bulk by default");
   CHECK(!wr->window, "init made the window");
   winraw_poll(wr);
   raw_window = wr->window;
   CHECK(raw_window && IsWindow(raw_window), "the first poll made no window");
   CHECK(GetWindowThreadProcessId(raw_window, NULL) == main_tid
         && main_tid != vt_tid && winraw_poll_owns_thread(),
         "the window belongs to thread %lu; the poll's is %lu, init's %lu",
         (unsigned long)GetWindowThreadProcessId(raw_window, NULL),
         (unsigned long)main_tid, (unsigned long)vt_tid);
   printf("   ok   by default: init makes no window; the first poll does, on the polling thread\n");

   /* ---- left running across a video driver restart ---------------- */
   /* Read by the poll, with a joypad driver that can stay too: yes.
    * With one that cannot: no. */
   joypad_can_stay = true;
   CHECK(input_winraw.survives_video && input_winraw.survives_video(wr),
         "read by the poll, the driver does not say it survives a video restart");
   joypad_can_stay = false;
   CHECK(!input_winraw.survives_video(wr),
         "the driver says it survives with a joypad driver that does not");
   joypad_can_stay = true;
   printf("   ok   survives a video driver restart: yes read by the poll, no if the joypad driver cannot\n");

   /* ---- injected keys, read in bulk by one poll ------------------ */
   if (vt_window)
   {
      unsigned long reads0;
      unsigned spins;

      /* does raw input for an injected key arrive here at all? */
      key(0x1E, true);
      for (spins = 0; spins < 100 && !wr->kb_keys[0x1E]; spins++)
      {
         Sleep(5);
         winraw_poll(wr);
      }
      raw_arrives = wr->kb_keys[0x1E] != 0;
      key(0x1E, false);
      for (spins = 0; spins < 100 && wr->kb_keys[0x1E]; spins++)
      {
         Sleep(5);
         winraw_poll(wr);
      }
      seen_n = 0;

      if (raw_arrives)
      {
         /* A, then B with shift held: six reports, nothing pumping on
          * this thread while they arrive */
         wr->drained = wr->by_message = 0;
         reads0      = wr->drain_reads;
         key(0x1E, true);
         key(0x1E, false);
         key(SC_LSHIFT, true);
         key(0x30, true);
         key(0x30, false);
         key(SC_LSHIFT, false);
         Sleep(150);
         CHECK(seen_n == 0 && !wr->kb_keys[0x30], "something was taken before the poll");

         winraw_poll(wr);

         CHECK(wr->drained == 6 && wr->by_message == 0,
               "%lu reports read in bulk and %lu as messages; 6 were injected",
               wr->drained, wr->by_message);
         CHECK(wr->drain_reads - reads0 == 1, "it took %lu bulk reads",
               wr->drain_reads - reads0);
         CHECK(seen_n == 6, "%u key events out of the poll", seen_n);
         CHECK(seen_n == 6
               &&  seen[0].down && seen[0].code == 0x1E      && !(seen[0].mod & RETROKMOD_SHIFT)
               && !seen[1].down && seen[1].code == 0x1E
               &&  seen[2].down && seen[2].code == SC_LSHIFT
               &&  seen[3].down && seen[3].code == 0x30      &&  (seen[3].mod & RETROKMOD_SHIFT)
               && !seen[4].down && seen[4].code == 0x30      &&  (seen[4].mod & RETROKMOD_SHIFT)
               && !seen[5].down && seen[5].code == SC_LSHIFT && !(seen[5].mod & RETROKMOD_SHIFT),
               "the key events are not the six injected, in order, with shift on the B");
         for (i = 0; i < seen_n; i++)
            CHECK(seen[i].tid == main_tid, "event %u was delivered on thread %lu, not the poll's",
                  i, (unsigned long)seen[i].tid);
         CHECK(wm_input_waiting() == 0, "reports read in bulk were left in the queue as messages");
         seen_n = 0;
         winraw_poll(wr);
         CHECK(seen_n == 0, "a second poll delivered them again");
         printf("   ok   six injected reports: one bulk read, none as a message, six key events from the poll, in order\n");

         /* ---- reports the pump gets to first --------------------- */
         {
            MSG msg;
            unsigned long reads0 = wr->drain_reads;
            wr->drained = wr->by_message = 0;
            key(0x2E, true);
            key(0x2E, false);
            key(0x2F, true);
            key(0x2F, false);
            key(0x31, true);
            key(0x31, false);
            Sleep(150);
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
               DispatchMessageA(&msg);
            CHECK(wr->by_message == 1 && wr->drained == 5
                  && wr->drain_reads - reads0 == 1,
                  "six reports met by the pump: %lu as messages, %lu in %lu bulk reads;"
                  " wanted 1, and 5 in 1", wr->by_message, wr->drained,
                  wr->drain_reads - reads0);
            CHECK(seen_n == 0, "their key events did not wait for the poll");
            winraw_poll(wr);
            CHECK(seen_n == 6
                  &&  seen[0].down && seen[0].code == 0x2E && !seen[1].down && seen[1].code == 0x2E
                  &&  seen[2].down && seen[2].code == 0x2F && !seen[3].down && seen[3].code == 0x2F
                  &&  seen[4].down && seen[4].code == 0x31 && !seen[5].down && seen[5].code == 0x31,
                  "the six did not come out of the poll in order (%u events)", seen_n);
            for (i = 0; i < seen_n; i++)
               CHECK(seen[i].tid == main_tid, "event %u was delivered off the polling thread", i);
            seen_n = 0;
            printf("   ok   six reports met by the pump: one from its message, five in one bulk read, in order\n");
         }

         /* ---- nothing waiting ------------------------------------ */
         {
            MSG msg;
            unsigned long empty0, reads0;
            Sleep(50);
            winraw_poll(wr);
            empty0 = wr->drain_empty;
            reads0 = wr->drain_reads;
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
               DispatchMessageA(&msg);
            CHECK(wr->drain_empty == empty0 && wr->drain_reads == reads0,
                  "a pump with nothing waiting made a read");
            winraw_poll(wr);
            CHECK(wr->drain_empty == empty0 + 1 && wr->drain_reads == reads0,
                  "a poll with nothing waiting made %lu reads, not one",
                  (wr->drain_empty - empty0) + (wr->drain_reads - reads0));
            printf("   ok   nothing waiting: a poll makes one read, a pump makes none\n");
         }

         /* ---- one read for both drivers --------------------------- */
         /* winraw_joypad's poll comes first and makes the read through
          * winraw_queue_read(); this driver's poll must not make a
          * second, and still hands on what the first one took. */
         {
            unsigned long total0;
            wr->drained = wr->by_message = 0;
            key(0x21, true);
            key(0x21, false);
            Sleep(150);
            total0 = wr->drain_reads + wr->drain_empty;
            winraw_queue_read();
            CHECK(wr->drained == 2 && wr->kb_keys[0x21] == 0 && seen_n == 0,
                  "the read made for the controller driver took %lu reports, delivered %u events",
                  wr->drained, seen_n);
            winraw_poll(wr);
            CHECK(wr->drain_reads + wr->drain_empty - total0 == 1,
                  "the two polls made %lu reads between them, not one",
                  wr->drain_reads + wr->drain_empty - total0);
            CHECK(seen_n == 2 && seen[0].down && seen[0].code == 0x21 && !seen[1].down,
                  "the poll did not hand on what that read took (%u events)", seen_n);
            seen_n = 0;
            total0 = wr->drain_reads + wr->drain_empty;
            winraw_poll(wr);
            CHECK(wr->drain_reads + wr->drain_empty - total0 == 1,
                  "the next poll, on its own, did not make its read");
            printf("   ok   one read for both drivers: made for the controller driver, not repeated by this poll\n");
         }

         /* ---- the wait on the queue ------------------------------- */
         /* The paused and unfocused loop waits on this thread's queue
          * (win32_display_server_idle_wait()). With reports waiting, a
          * wait for any input ends at once - and a controller sends a
          * thousand a second - so on this thread the wait leaves raw
          * input out, and then lasts its bound. */
         {
            DWORD t0, any_ms, bound_ms;
            key(0x22, true);
            Sleep(100);
            t0 = GetTickCount();
            MsgWaitForMultipleObjectsEx(0, NULL, 300, QS_ALLINPUT,
                  MWMO_INPUTAVAILABLE);
            any_ms = GetTickCount() - t0;
            t0 = GetTickCount();
            MsgWaitForMultipleObjectsEx(0, NULL, 300,
                  QS_ALLINPUT & ~QS_RAWINPUT, MWMO_INPUTAVAILABLE);
            bound_ms = GetTickCount() - t0;
            CHECK(any_ms < 100, "a wait for any input, with a report waiting, took %lu ms",
                  (unsigned long)any_ms);
            CHECK(bound_ms >= 250, "a wait that leaves raw input out, with a report"
                  " waiting, ended after %lu ms of 300", (unsigned long)bound_ms);
            key(0x22, false);
            Sleep(100);
            winraw_poll(wr);
            seen_n = 0;
            printf("   ok   the wait on the queue: a report waiting ends a wait for any input (%lu ms),"
                  " not one that leaves raw input out (%lu ms)\n",
                  (unsigned long)any_ms, (unsigned long)bound_ms);
         }

         /* ---- the frontend's pump on this thread ----------------- */
         {
            unsigned long reads0;
            CHECK(winraw_poll_owns_thread(), "the polling thread is not known as the one that is polled");
            wr->drained = wr->by_message = 0;
            reads0      = wr->drain_reads;
            key(0x10, true);
            key(0x10, false);
            key(0x11, true);
            key(0x11, false);
            key(0x12, true);
            key(0x12, false);
            /* a message that is not raw input, to see the pump still
             * dispatches those */
            PostMessageA(raw_window, WM_APP, 0, 0);
            Sleep(150);
            pump_leaving_raw_input();
            {
               MSG probe;
               CHECK(!PeekMessageA(&probe, raw_window, WM_APP, WM_APP, PM_NOREMOVE),
                     "the pump left an ordinary message behind");
            }
            CHECK(wr->by_message == 0 && wr->drained == 0 && !wr->kb_keys[0x12],
                  "the pump took raw input: %lu as messages, %lu in bulk",
                  wr->by_message, wr->drained);
            winraw_poll(wr);
            CHECK(wr->drained == 6 && wr->by_message == 0
                  && wr->drain_reads - reads0 == 1,
                  "after that pump the poll read %lu reports in %lu reads; wanted 6 in 1",
                  wr->drained, wr->drain_reads - reads0);
            CHECK(seen_n == 6 && seen[0].code == 0x10 && seen[2].code == 0x11
                  && seen[4].code == 0x12 && !seen[5].down,
                  "the six did not come out of the poll in order (%u events)", seen_n);
            seen_n = 0;
            printf("   ok   the frontend's pump on this thread: ordinary messages dispatched, six reports left for one bulk read\n");
         }
      }
      else
         printf("   note an injected key did not arrive as raw input here: the bulk read is not checked\n");
   }

   /* ---- a controller's report in a bulk read --------------------- */
   {
      /* What the loop in winraw_drain() does with a record of this
       * type, on a record made here: the payload it hands over is the
       * report bytes and their size. */
      union { RAWINPUT ri; BYTE bytes[64]; } rec;
      RAWHID *hid;
      memset(&rec, 0, sizeof(rec));
      rec.ri.header.dwType = RIM_TYPEHID;
      hid            = (RAWHID*)((BYTE*)&rec.ri.data + winraw_wow64_shift());
      hid->dwSizeHid = 7;
      hid->dwCount   = 2;
      hid_taken      = 0;
      winraw_joypad_take_hid(rec.ri.header.hDevice, hid->bRawData,
            hid->dwSizeHid, hid->dwCount);
      CHECK(hid_taken == 1 && hid_taken_size == 14, "a controller report of 2 x 7 bytes: %u, %lu",
            hid_taken, (unsigned long)hid_taken_size);
   }

   /* ---- modifiers ------------------------------------------------ */
   {
      published_mods = RETROKMOD_CTRL | RETROKMOD_NUMLOCK;
      memset(wr->kb_keys, 0, SC_LAST);
      CHECK(winraw_held_mods(wr) == RETROKMOD_NUMLOCK,
            "nothing held: %#x", winraw_held_mods(wr));
      wr->kb_keys[SC_RSHIFT] = 1;
      wr->kb_keys[SC_RALT]   = 1;
      wr->kb_keys[SC_LSUPER] = 1;
      CHECK(winraw_held_mods(wr) == (RETROKMOD_SHIFT | RETROKMOD_ALT
               | RETROKMOD_META | RETROKMOD_NUMLOCK),
            "right shift, right alt and a windows key held: %#x", winraw_held_mods(wr));
      memset(wr->kb_keys, 0, SC_LAST);
      published_mods = 0;
      printf("   ok   modifiers: held keys from the key table, toggles from the published value\n");
   }

   /* ---- background input ----------------------------------------- */
   {
      CHECK(!winraw_in_background(wr, RIM_INPUT), "foreground input refused");
      CHECK(winraw_in_background(wr, RIM_INPUTSINK), "background input taken");
      wr->sink     = true;
      winraw_focus = false;
      CHECK(winraw_in_background(wr, RIM_INPUTSINK),
            "a sink took input with the main window unfocused");
      winraw_focus = true;
      CHECK(!winraw_in_background(wr, RIM_INPUTSINK),
            "a sink refused input with the main window focused");
      wr->sink     = false;
      winraw_focus = false;
      printf("   ok   background input: refused, unless a sink with the main window focused\n");
   }

   /* ---- free ----------------------------------------------------- */
   winraw_free(wr);
   CHECK(!IsWindow(raw_window), "free left the window");
   CHECK(!winraw_poll_owns_thread(), "free left the thread marked as read by the poll");
   /* a read made for the controller driver with no keyboard and mouse
    * driver there: nothing to take, nothing to touch */
   winraw_queue_read();
   winraw_queue_was_read = false;
   printf("   ok   free: the window is gone, the thread's mark with it\n");

   /* ---- restarted, as it is with every video driver restart ------ */
   {
      DWORD t0 = GetTickCount();
      unsigned ok = 0;
      for (i = 0; i < 20; i++)
      {
         HWND w;
         wr = (winraw_input_t*)init_on_video_thread();
         if (!wr)
            break;
         winraw_poll(wr);
         w = wr->window;
         if (w && GetWindowThreadProcessId(w, NULL) == main_tid)
         {
            winraw_free(wr);
            if (!IsWindow(w))
               ok++;
         }
         else
            winraw_free(wr);
      }
      CHECK(ok == 20, "%u of 20 restarts made their window on the polling thread and took it down", ok);
      printf("   ok   20 restarts: each window made by the poll, each gone after free (%lu ms)\n",
            (unsigned long)(GetTickCount() - t0));
   }

   SetEvent(vt_quit);
   WaitForSingleObject(vt, 5000);

   /* ---- video not threaded: everything on the one thread ---------- */
   /* The focused window, the driver's start, the pump and the poll are
    * all this thread's, as they are without threaded video. */
   {
      WNDCLASSA wc;
      HWND top;

      memset(&wc, 0, sizeof(wc));
      wc.lpfnWndProc   = plain_proc;
      wc.hInstance     = GetModuleHandleA(NULL);
      wc.lpszClassName = "winraw-poll-test-same";
      RegisterClassA(&wc);
      top = CreateWindowExA(0, wc.lpszClassName, "t",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, 10, 10, 200, 100,
            NULL, NULL, wc.hInstance, NULL);
      wr  = (winraw_input_t*)winraw_init("null");
      CHECK(wr != NULL, "init on the polling thread");
      if (top && wr && raw_arrives)
      {
         SetForegroundWindow(top);
         SetFocus(top);
         pump_leaving_raw_input();
         winraw_poll(wr);
         CHECK(wr->window && GetWindowThreadProcessId(wr->window, NULL) == main_tid,
               "the window is not this thread's");
         seen_n = 0;
         key(0x1E, true);
         key(0x1E, false);
         PostMessageA(top, WM_APP, 0, 0);
         Sleep(150);
         pump_leaving_raw_input();
         CHECK(wr->by_message == 0 && wr->drained == 0,
               "the pump took raw input on the one thread");
         winraw_poll(wr);
         CHECK(wr->drained == 2 && wr->by_message == 0 && seen_n == 2
               && seen[0].down && seen[0].code == 0x1E && !seen[1].down,
               "on the one thread: %lu read in bulk, %lu as messages, %u events",
               wr->drained, wr->by_message, seen_n);
         seen_n = 0;
         printf("   ok   video not threaded: window, pump and poll on one thread, reports read in bulk\n");
      }
      if (wr)
         winraw_free(wr);
      if (top)
         DestroyWindow(top);
   }

   if (failures)
   {
      printf("FAIL winraw_poll_test: %u\n", failures);
      return 1;
   }
   printf("PASS winraw_poll_test%s\n", raw_arrives ? "" : " (without injected input)");
   return 0;
}
