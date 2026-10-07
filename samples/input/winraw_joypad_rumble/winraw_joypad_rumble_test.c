/* winraw joypad: a DualShock 4 or a DualSense rumbles.
 *
 * Raw input reads a controller and has no way to write to one, and
 * the raw input joypad driver had no rumble. For the pads there is a
 * rumble report for (input/common/sony_pad_output.h) it now opens the
 * controller's HID device for writing when the pad arrives, and
 * writes the report from a thread of its own when the strength
 * wanted changes: set_rumble() only notes the strength and wakes the
 * thread. Nothing is under a lock: the strength, and the open device
 * itself, pass from the driver to the thread through retro_atomic,
 * and the thread alone writes to a device and closes it.
 *
 * The driver is included whole, with its raw input and HID calls
 * routed to fake controllers. A controller here is a file: the driver
 * opens it as it would the device, and what it writes is read back.
 * No pad is written to; that the reports are what a pad expects is
 * samples/input/sony_pad_output's to hold, as far as it can be held
 * without one.
 *
 * Checked here:
 *
 * - a DualSense Edge and a DualShock 4 are opened for writing when
 *   they arrive, and a controller of another make is not;
 * - set_rumble() answers at once and writes nothing itself: the
 *   report is written by the driver's thread, shortly after;
 * - the report written is the pad's own, with the strengths asked
 *   for, strong and weak each where it belongs, padded to the length
 *   Windows wants of a write;
 * - a run of changes ends with the last one written;
 * - unplugging a pad that is rumbling does not wait: the thread
 *   stills it and closes it, shortly after. Stopping the driver
 *   stills the rest before it returns;
 * - a pad that comes and goes before the thread has taken its device
 *   leaves no device open;
 * - set_rumble() for a pad that is not there, or cannot rumble, says
 *   so;
 * - an Xbox pad is rumbled through XInput, which is loaded only when
 *   one first rumbles. With one such pad and one pad of XInput's,
 *   they are the same pad, whichever number XInput gave it;
 * - with two, a pad is not rumbled while there is nothing to tell
 *   which of XInput's it is - not another player's - and is as soon
 *   as a button is held on it; the last one left is then known by
 *   there being no other;
 * - unplugging an Xbox pad that is rumbling stills it, and so does
 *   stopping the driver;
 * - with "XInput for Xbox Controllers" on, an Xbox pad is not taken
 *   from raw input: XInput's pad is listed, named and laid out as the
 *   XInput driver has it - ten buttons, the D-pad as a hat, two
 *   sticks and the two triggers each on its own axis - and is read
 *   once a poll; another make of pad is raw input's as before; the
 *   pad rumbles, and one that goes is noticed and its slot given up.
 *   With the setting off none of that happens. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>

#include <boolean.h>

/* ---- The fake controllers ------------------------------------------- */

#define FAKE_MAX 16
static struct
{
   HANDLE h;
   bool alive;
   unsigned vid, pid;
   char path[96];
} fake[FAKE_MAX];
static unsigned fake_n;
/* the length Windows would want of a write to them */
#define FAKE_REPORT_LEN 64

static void fake_plug(HANDLE h, unsigned vid, unsigned pid, const char *path)
{
   fake[fake_n].h     = h;
   fake[fake_n].alive = true;
   fake[fake_n].vid   = vid;
   fake[fake_n].pid   = pid;
   strcpy(fake[fake_n].path, path);
   fake_n++;
}

static UINT WINAPI fake_GetRawInputDeviceInfoA(HANDLE dev, UINT cmd,
      LPVOID data, PUINT size)
{
   unsigned i;
   for (i = 0; i < fake_n; i++)
      if (fake[i].h == dev)
         break;
   if (i == fake_n || !fake[i].alive)
      return (UINT)-1;

   switch (cmd)
   {
      case RIDI_DEVICEINFO:
      {
         RID_DEVICE_INFO *info = (RID_DEVICE_INFO*)data;
         info->dwType          = RIM_TYPEHID;
         info->hid.dwVendorId  = fake[i].vid;
         info->hid.dwProductId = fake[i].pid;
         info->hid.usUsagePage = 0x01;
         info->hid.usUsage     = 0x05;
         return sizeof(*info);
      }
      case RIDI_DEVICENAME:
      {
         UINT need = (UINT)strlen(fake[i].path);
         if (!data)
         {
            *size = need + 1;
            return 0;
         }
         strcpy((char*)data, fake[i].path);
         return need + 1;
      }
      case RIDI_PREPARSEDDATA:
         if (!data)
            *size = 16;
         return 16;
   }
   return (UINT)-1;
}

static NTSTATUS NTAPI fake_HidP_GetCaps(PHIDP_PREPARSED_DATA pp,
      PHIDP_CAPS caps)
{
   (void)pp;
   memset(caps, 0, sizeof(*caps));
   caps->OutputReportByteLength = FAKE_REPORT_LEN;
   return HIDP_STATUS_SUCCESS;
}

/* A controller is a file. Opened for writing it is the device the
 * reports go to; opened for nothing, as the driver does for a
 * product name, it is not there, and the driver names the pad. */
static unsigned opened_for_writing;
static HANDLE WINAPI fake_CreateFileA(LPCSTR name, DWORD access, DWORD share,
      LPSECURITY_ATTRIBUTES sec, DWORD disp, DWORD flags, HANDLE tmpl)
{
   (void)share; (void)sec; (void)disp; (void)tmpl;
   if (!(access & GENERIC_WRITE))
      return INVALID_HANDLE_VALUE;
   opened_for_writing++;
   return CreateFileA(name, GENERIC_READ | GENERIC_WRITE,
         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, flags, NULL);
}

/* XInput: four pads, each there or not, with buttons held and the
 * motors as they were last set */
static struct
{
   bool connected;
   WORD buttons, left, right;
   unsigned sets;
   BYTE lt, rt;
   SHORT lx, ly;
} xpad[4];
static unsigned xinput_loads;
struct fake_xstate { DWORD packet; WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; };
struct fake_xvib   { WORD left, right; };

static DWORD WINAPI fake_XInputGetState(DWORD i, struct fake_xstate *st)
{
   if (i >= 4 || !xpad[i].connected)
      return ERROR_DEVICE_NOT_CONNECTED;
   memset(st, 0, sizeof(*st));
   st->buttons = xpad[i].buttons;
   st->lt      = xpad[i].lt;
   st->rt      = xpad[i].rt;
   st->lx      = xpad[i].lx;
   st->ly      = xpad[i].ly;
   return ERROR_SUCCESS;
}
static DWORD WINAPI fake_XInputSetState(DWORD i, struct fake_xvib *v)
{
   if (i >= 4 || !xpad[i].connected)
      return ERROR_DEVICE_NOT_CONNECTED;
   xpad[i].left  = v->left;
   xpad[i].right = v->right;
   xpad[i].sets++;
   return ERROR_SUCCESS;
}
static HMODULE WINAPI fake_LoadLibraryA(LPCSTR name)
{
   (void)name;
   xinput_loads++;
   return (HMODULE)(uintptr_t)0x5150;
}
static FARPROC WINAPI fake_GetProcAddress(HMODULE mod, LPCSTR name)
{
   (void)mod;
   if (!strcmp(name, "XInputGetState"))
      return (FARPROC)fake_XInputGetState;
   if (!strcmp(name, "XInputSetState"))
      return (FARPROC)fake_XInputSetState;
   return NULL;
}
static BOOL WINAPI fake_FreeLibrary(HMODULE mod) { (void)mod; return TRUE; }

#define GetRawInputDeviceInfoA fake_GetRawInputDeviceInfoA
#define HidP_GetCaps           fake_HidP_GetCaps
#define CreateFileA            fake_CreateFileA
#define LoadLibraryA           fake_LoadLibraryA
#define GetProcAddress         fake_GetProcAddress
#define FreeLibrary            fake_FreeLibrary

#include "input/drivers_joypad/winraw_joypad.c"

#undef CreateFileA

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
/* the driver reads one setting: whether Xbox pads are XInput's */
static settings_t stub_settings;
settings_t *config_get_ptr(void) { return &stub_settings; }
/* the settings a driver asks for by name, read from the ones above */
#include "../input_config_stubs.h"
bool winraw_raw_input_polled(void) { return true; }
void winraw_queue_read(void) { }
void winraw_queue_claim_thread(bool claim) { (void)claim; }

/* ---- The test ------------------------------------------------------- */

static unsigned failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

#define H(n) ((HANDLE)(uintptr_t)(0x1000 + (n)))

/* what has been written to a controller: its length, and its bytes */
static DWORD written_to(const char *path, uint8_t *buf, DWORD cap)
{
   DWORD got = 0;
   HANDLE f  = CreateFileA(path, GENERIC_READ,
         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
   if (f == INVALID_HANDLE_VALUE)
      return 0;
   memset(buf, 0, cap);
   ReadFile(f, buf, cap, &got, NULL);
   CloseHandle(f);
   return got;
}

/* waits until XInput's pad @i has its motors at @left and @right;
 * false if it has not within a second */
static bool xwait(unsigned i, unsigned left, unsigned right)
{
   unsigned tries;
   for (tries = 0; tries < 200; tries++)
   {
      if (xpad[i].left == left && xpad[i].right == right)
         return true;
      Sleep(5);
   }
   return false;
}

/* waits until the bytes at @at and @at + 1 of what was written are
 * @a and @b; false if they are not within a second */
static bool wait_for(const char *path, unsigned at, unsigned a, unsigned b)
{
   uint8_t buf[256];
   unsigned tries;
   for (tries = 0; tries < 200; tries++)
   {
      if (     written_to(path, buf, sizeof(buf)) > at + 1
            && buf[at] == a && buf[at + 1] == b)
         return true;
      Sleep(5);
   }
   return false;
}

int main(void)
{
   uint8_t buf[256];
   DWORD len;
   bool ok;
   char dir[MAX_PATH], edge[MAX_PATH], ds4[MAX_PATH], other[MAX_PATH];

   GetTempPathA(sizeof(dir), dir);
   snprintf(edge,  sizeof(edge),  "%srumble_edge.bin",  dir);
   snprintf(ds4,   sizeof(ds4),   "%srumble_ds4.bin",   dir);
   snprintf(other, sizeof(other), "%srumble_other.bin", dir);
   DeleteFileA(edge); DeleteFileA(ds4); DeleteFileA(other);

   fake_plug(H(1), 0x054C, 0x0DF2, edge);  /* DualSense Edge       */
   fake_plug(H(2), 0x054C, 0x09CC, ds4);   /* DualShock 4          */
   fake_plug(H(3), 0x046D, 0xC216, other); /* some other gamepad   */
   winraw_joypad_initialised = true;

   /* ---- the pads arrive ------------------------------------------ */
   winraw_joypad_add_device(H(1));
   winraw_joypad_add_device(H(2));
   winraw_joypad_add_device(H(3));
   CHECK(winraw_joypad_pads[0].connected && winraw_joypad_pads[1].connected
         && winraw_joypad_pads[2].connected, "the three controllers did not connect");
   CHECK(opened_for_writing == 2 && winraw_joypad_out[0].present
         && winraw_joypad_out[1].present && !winraw_joypad_out[2].present,
         "opened for writing: %u, want the two Sony pads and not the third", opened_for_writing);
   CHECK(written_to(edge, buf, sizeof(buf)) == 0, "something was written before any rumble was asked for");
   printf("   ok   a DualSense Edge and a DualShock 4 are opened for writing when they arrive; another make of pad is not\n");

   /* ---- rumble ---------------------------------------------------- */
   ok = winraw_joypad_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0xFFFF);
   CHECK(ok, "set_rumble() said the DualSense cannot rumble");
   CHECK(wait_for(edge, 3, 0x00, 0xFF), "the strong motor's strength was not written within a second");
   len = written_to(edge, buf, sizeof(buf));
   CHECK(len == FAKE_REPORT_LEN && buf[0] == 0x02 && buf[1] == 0x02 && buf[39] == 0x04,
         "the report written: %lu bytes, id %02x, flags %02x .. %02x; want %u, 02, 02 .. 04",
         (unsigned long)len, buf[0], buf[1], buf[39], FAKE_REPORT_LEN);
   ok = winraw_joypad_joypad_set_rumble(0, RETRO_RUMBLE_WEAK, 0x8000);
   CHECK(ok && wait_for(edge, 3, 0x80, 0xFF),
         "the weak motor set beside the strong: both were not written");
   /* the DualShock 4, in its own format */
   winraw_joypad_joypad_set_rumble(1, RETRO_RUMBLE_STRONG, 0x4000);
   winraw_joypad_joypad_set_rumble(1, RETRO_RUMBLE_WEAK,   0x2000);
   CHECK(wait_for(ds4, 4, 0x20, 0x40), "the DualShock 4's strengths were not written");
   len = written_to(ds4, buf, sizeof(buf));
   CHECK(len == FAKE_REPORT_LEN && buf[0] == 0x05 && buf[1] == 0x01,
         "the DualShock 4's report: %lu bytes, id %02x, flags %02x", (unsigned long)len, buf[0], buf[1]);
   printf("   ok   the report is written by the driver's thread: the pad's own, strong and weak where they belong, at the length Windows wants\n");

   /* a run of changes: the last one is what is left written */
   {
      unsigned v;
      for (v = 1; v <= 200; v++)
         winraw_joypad_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, (uint16_t)(v << 8));
   }
   CHECK(wait_for(edge, 3, 0x80, 200), "after a run of changes the last was not what was written");
   printf("   ok   a run of changes ends with the last one written\n");

   /* Controller Player Lights. Off, as it has been so far: nothing
    * above asked for the lights */
   len = written_to(edge, buf, sizeof(buf));
   CHECK(len > 44 && !(buf[2] & 0x10) && buf[44] == 0,
         "with the setting off the DualSense's player lights were asked for");
   len = written_to(ds4, buf, sizeof(buf));
   CHECK(len > 8 && buf[1] == 0x01 && !buf[6] && !buf[7] && !buf[8],
         "with the setting off the DualShock 4's light bar was asked for");
   /* on: the DualSense is on the first port, the DualShock 4 on the
    * second, and their lights say so; the motors stay as they were */
   {
      unsigned i;
      stub_settings.bools.input_winraw_player_lights = true;
      stub_settings.uints.input_joypad_index[0]      = 0;
      stub_settings.uints.input_joypad_index[1]      = 1;
      for (i = 2; i < MAX_USERS; i++)
         stub_settings.uints.input_joypad_index[i]   = i;
      for (i = 0; i < 40; i++)
         winraw_joypad_player_lights();
      CHECK(wait_for(edge, 44, 0x04, 0x00), "the DualSense on the first port does not show player 1");
      len = written_to(edge, buf, sizeof(buf));
      CHECK((buf[2] & 0x10) && buf[3] == 0x80 && buf[4] == 200,
            "the player lights were not asked for, or the motors were not kept beside them");
      CHECK(wait_for(ds4, 6, 0x40, 0x00), "the DualShock 4 on the second port is not red");
      len = written_to(ds4, buf, sizeof(buf));
      CHECK(buf[1] == 0x03 && buf[8] == 0x00, "the DualShock 4's light bar was not asked for");
      /* the two swap ports: the lights follow */
      stub_settings.uints.input_joypad_index[0]      = 1;
      stub_settings.uints.input_joypad_index[1]      = 0;
      for (i = 0; i < 40; i++)
         winraw_joypad_player_lights();
      CHECK(wait_for(edge, 44, 0x0A, 0x00), "the DualSense moved to the second port does not show player 2");
      CHECK(wait_for(ds4, 7, 0x00, 0x40), "the DualShock 4 moved to the first port is not blue");
      /* turned off again: the lights that were lit are put out, with
       * the motors kept as they were */
      stub_settings.bools.input_winraw_player_lights = false;
      stub_settings.uints.input_joypad_index[0]      = 0;
      stub_settings.uints.input_joypad_index[1]      = 1;
      for (i = 0; i < 40; i++)
         winraw_joypad_player_lights();
      CHECK(wait_for(edge, 44, 0x00, 0x00), "turned off, the DualSense's player lights stayed lit");
      len = written_to(edge, buf, sizeof(buf));
      CHECK((buf[2] & 0x10) && buf[3] == 0x80 && buf[4] == 200,
            "turned off, the lights were not asked to go out, or the motors were not kept");
      CHECK(wait_for(ds4, 7, 0x00, 0x00), "turned off, the DualShock 4's light bar stayed lit");
      len = written_to(ds4, buf, sizeof(buf));
      CHECK(buf[1] == 0x03 && !buf[6] && !buf[8],
            "turned off, the DualShock 4's light bar was not asked to go out");
      printf("   ok   Controller Player Lights: off from the start, the lights are left alone; on, each pad shows its port and follows it; turned off again, they go out\n");
   }

   /* ---- what cannot rumble ---------------------------------------- */
   CHECK(!winraw_joypad_joypad_set_rumble(2, RETRO_RUMBLE_STRONG, 0xFFFF),
         "set_rumble() said a pad there is no report for can rumble");
   CHECK(!winraw_joypad_joypad_set_rumble(7, RETRO_RUMBLE_STRONG, 0xFFFF)
         && !winraw_joypad_joypad_set_rumble(MAX_USERS + 3, RETRO_RUMBLE_STRONG, 0xFFFF),
         "set_rumble() said a pad that is not there can rumble");
   printf("   ok   a pad of another make, or none, cannot rumble and says so\n");

   /* ---- unplugged while rumbling ---------------------------------- */
   winraw_joypad_remove_device(H(1));
   CHECK(!winraw_joypad_out[0].present, "the unplugged pad is still taken to be there");
   CHECK(!winraw_joypad_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0xFFFF),
         "set_rumble() for the unplugged pad did not say it cannot");
   /* the thread stills it and closes it, in its own time */
   CHECK(wait_for(edge, 3, 0, 0), "unplugged while rumbling: the motors were not stilled within a second");
   len = written_to(edge, buf, sizeof(buf));
   CHECK(len && buf[1] == 0x02, "the report that stilled it is not a rumble report: flags %02x", buf[1]);
   {
      /* closed: the file can be opened with nothing shared */
      unsigned tries;
      HANDLE f = INVALID_HANDLE_VALUE;
      for (tries = 0; tries < 200 && f == INVALID_HANDLE_VALUE; tries++)
      {
         f = CreateFileA(edge, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
         if (f == INVALID_HANDLE_VALUE)
            Sleep(5);
      }
      CHECK(f != INVALID_HANDLE_VALUE, "the unplugged pad's device was left open");
      if (f != INVALID_HANDLE_VALUE)
         CloseHandle(f);
   }

   /* ---- a pad that comes and goes at once -------------------------- */
   /* Plugged in and out again and again, faster than the thread takes
    * each device over: every device opened is closed by one side or
    * the other, and at the end none is left open. */
   {
      unsigned n, tries;
      HANDLE f = INVALID_HANDLE_VALUE;
      for (n = 0; n < 50; n++)
      {
         fake[0].alive = true;
         winraw_joypad_add_device(H(1));
         winraw_joypad_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0xFFFF);
         winraw_joypad_remove_device(H(1));
      }
      for (tries = 0; tries < 200 && f == INVALID_HANDLE_VALUE; tries++)
      {
         f = CreateFileA(edge, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
         if (f == INVALID_HANDLE_VALUE)
            Sleep(5);
      }
      CHECK(f != INVALID_HANDLE_VALUE, "after fifty quick plug-ins a device is still open");
      if (f != INVALID_HANDLE_VALUE)
         CloseHandle(f);
      CHECK(!winraw_joypad_out[0].present, "after fifty quick plug-ins the pad is taken to be there");
   }

   /* ---- the driver stops ------------------------------------------ */
   winraw_joypad_joypad_destroy();
   len = written_to(ds4, buf, sizeof(buf));
   CHECK(len && buf[4] == 0 && buf[5] == 0, "the driver stopped with the DualShock 4 rumbling at %u and %u", buf[4], buf[5]);
   CHECK(!winraw_joypad_out_writer && !winraw_joypad_out[1].present
         && !winraw_joypad_out[1].dev,
         "the driver stopped and left its thread or a device behind");
   {
      HANDLE f = CreateFileA(ds4, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
      CHECK(f != INVALID_HANDLE_VALUE, "the driver stopped and left the DualShock 4's device open");
      if (f != INVALID_HANDLE_VALUE)
         CloseHandle(f);
   }
   printf("   ok   an unplugged pad is stilled and closed by the thread, without the driver waiting; fifty quick plug-ins leave no device open; stopping the driver stills the rest and ends the thread\n");

   /* ================= Xbox pads, through XInput ================== */
   fake_n = 0;
   memset(winraw_joypad_pads, 0, sizeof(winraw_joypad_pads));
   winraw_joypad_pad_count   = 0;
   winraw_joypad_initialised = true;
   fake_plug(H(11), 0x045E, 0x028E, "\\\\?\\HID#VID_045E&PID_028E&IG_00#a");
   fake_plug(H(12), 0x045E, 0x028E, "\\\\?\\HID#VID_045E&PID_028E&IG_00#b");

   /* ---- one pad, one of XInput's ---------------------------------- */
   xpad[2].connected = true;           /* XInput calls it its third */
   winraw_joypad_add_device(H(11));
   CHECK(winraw_joypad_out[0].present && winraw_joypad_out[0].xinput,
         "the Xbox pad was not taken for one XInput rumbles");
   CHECK(xinput_loads == 0, "XInput was loaded before any pad was to rumble");
   ok = winraw_joypad_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0xFFFF);
   winraw_joypad_joypad_set_rumble(0, RETRO_RUMBLE_WEAK, 0x8000);
   CHECK(ok && xwait(2, 0xFFFF, 0x8000),
         "one Xbox pad, one of XInput's: its motors are at %u %u, want 65535 32768",
         xpad[2].left, xpad[2].right);
   CHECK(xinput_loads == 1, "XInput was loaded %u times, want once", xinput_loads);
   printf("   ok   one Xbox pad and one pad of XInput's are the same pad, whichever number XInput gave it; XInput is loaded when it first rumbles\n");

   /* unplugged while rumbling: stilled, and XInput's pad let go */
   winraw_joypad_remove_device(H(11));
   CHECK(xwait(2, 0, 0), "an Xbox pad unplugged while rumbling was left at %u %u", xpad[2].left, xpad[2].right);
   xpad[2].connected = false;

   /* ---- two pads, two of XInput's --------------------------------- */
   xpad[0].connected = xpad[1].connected = true;
   xpad[0].sets = xpad[1].sets = 0;
   fake[0].alive = true;
   winraw_joypad_add_device(H(11));    /* slot 0 */
   winraw_joypad_add_device(H(12));    /* slot 1 */
   CHECK(winraw_joypad_out[0].xinput && winraw_joypad_out[1].xinput, "the two Xbox pads are not both there");
   /* nothing held on either: nothing says which is which */
   winraw_joypad_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0x4000);
   Sleep(150);
   CHECK(!xpad[0].sets && !xpad[1].sets,
         "with nothing to tell two pads apart one was rumbled: XInput's were set %u and %u times",
         xpad[0].sets, xpad[1].sets);
   /* A held on the pad in slot 0, which XInput has as its second */
   winraw_joypad_pads[0].num_buttons = 10;
   winraw_joypad_pads[0].buttons[0]  = true;
   winraw_joypad_out_publish_buttons(&winraw_joypad_pads[0]);
   xpad[1].buttons = 0x1000;
   winraw_joypad_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0x5000);
   CHECK(xwait(1, 0x5000, 0), "the pad with A held was not rumbled as XInput's second: its motors are at %u %u",
         xpad[1].left, xpad[1].right);
   CHECK(!xpad[0].sets, "the other player's pad was rumbled");
   /* the other pad: the only one left */
   winraw_joypad_joypad_set_rumble(1, RETRO_RUMBLE_WEAK, 0x3000);
   CHECK(xwait(0, 0, 0x3000), "the last pad left was not rumbled as XInput's first: its motors are at %u %u",
         xpad[0].left, xpad[0].right);
   printf("   ok   of two Xbox pads none is rumbled while nothing tells them apart; one is as soon as a button is held on it, and the other is then the one left\n");

   /* ---- the driver stops ------------------------------------------ */
   winraw_joypad_joypad_destroy();
   CHECK(xpad[0].left == 0 && xpad[0].right == 0 && xpad[1].left == 0 && xpad[1].right == 0,
         "the driver stopped with Xbox pads rumbling at %u %u and %u %u",
         xpad[0].left, xpad[0].right, xpad[1].left, xpad[1].right);
   CHECK(!winraw_joypad_out_writer && !winraw_xinput_dll, "the driver stopped and left its thread or XInput behind");
   printf("   ok   an unplugged Xbox pad is stilled and XInput's pad let go; stopping the driver stills the rest\n");

   /* ============ Xbox pads read through XInput ================== */
   memset(xpad, 0, sizeof(xpad));
   fake_n = 0;
   fake_plug(H(21), 0x045E, 0x028E, "\\\\?\\HID#VID_045E&PID_028E&IG_00#c"); /* an Xbox pad      */
   fake_plug(H(22), 0x046D, 0xC216, other);                                    /* another make     */
   xpad[1].connected = true;                 /* XInput has it as its second */
   xpad[1].buttons   = 0x1000 | 0x0001;      /* A, and up on the D-pad      */
   xpad[1].lt        = 255;
   xpad[1].rt        = 128;
   xpad[1].lx        = -32768;
   xpad[1].ly        = 1234;

   /* ---- the setting off: the Xbox pad is raw input's -------------- */
   stub_settings.bools.input_winraw_xinput_enable = false;
   winraw_joypad_joypad_init(NULL);
   winraw_joypad_add_device(H(21));
   CHECK(winraw_joypad_pads[0].connected && winraw_joypad_pads[0].xuser < 0
         && winraw_joypad_pads[0].hDevice == H(21),
         "with the setting off the Xbox pad is not raw input's");
   winraw_joypad_joypad_destroy();

   /* ---- the setting on -------------------------------------------- */
   stub_settings.bools.input_winraw_xinput_enable = true;
   winraw_joypad_joypad_init(NULL);
   CHECK(winraw_joypad_pads[0].connected && winraw_joypad_pads[0].xuser == 1
         && !strcmp(winraw_joypad_pads[0].name, "XInput Controller"),
         "XInput's pad is not listed as the XInput driver names it: \"%s\", XInput's %d",
         winraw_joypad_pads[0].name, winraw_joypad_pads[0].xuser + 1);
   winraw_joypad_add_device(H(21));
   winraw_joypad_add_device(H(22));
   CHECK(winraw_joypad_pads[1].connected && winraw_joypad_pads[1].hDevice == H(22)
         && !winraw_joypad_pads[2].connected,
         "the Xbox pad was taken from raw input as well, or the other pad was not: slots 1 and 2 are %d %d",
         winraw_joypad_pads[1].connected, winraw_joypad_pads[2].connected);

   winraw_joypad_joypad_poll();
   CHECK(winraw_joypad_joypad_button(0, 0) && !winraw_joypad_joypad_button(0, 1),
         "A is held and B is not: the pad reads %d %d",
         (int)winraw_joypad_joypad_button(0, 0), (int)winraw_joypad_joypad_button(0, 1));
   CHECK(winraw_joypad_joypad_button(0, HAT_MAP(0, HAT_UP_MASK))
         && !winraw_joypad_joypad_button(0, HAT_MAP(0, HAT_DOWN_MASK)),
         "up on the D-pad is not up on the hat");
   CHECK(winraw_joypad_joypad_axis(0, AXIS_POS(4)) == 32767
         && winraw_joypad_joypad_axis(0, AXIS_POS(5)) == 128 * 32767 / 255,
         "the triggers, each on its own axis: %d and %d, want 32767 and %d",
         winraw_joypad_joypad_axis(0, AXIS_POS(4)), winraw_joypad_joypad_axis(0, AXIS_POS(5)), 128 * 32767 / 255);
   CHECK(winraw_joypad_joypad_axis(0, AXIS_NEG(0)) == -32767
         && winraw_joypad_joypad_axis(0, AXIS_POS(1)) == 1234,
         "the left stick: %d %d, want -32767 1234",
         winraw_joypad_joypad_axis(0, AXIS_NEG(0)), winraw_joypad_joypad_axis(0, AXIS_POS(1)));
   printf("   ok   with the setting on, XInput's pad is listed and read in the XInput driver's layout, triggers apart; another make is raw input's; with it off the Xbox pad is raw input's\n");

   /* it rumbles, as the pad XInput says it is */
   ok = winraw_joypad_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0x6000);
   CHECK(ok && xwait(1, 0x6000, 0), "the XInput pad's motors are at %u %u, want 24576 0", xpad[1].left, xpad[1].right);

   /* it goes: noticed at the next poll, stilled, its slot given up */
   xpad[1].connected = false;
   winraw_joypad_joypad_poll();
   CHECK(!winraw_joypad_pads[0].connected && winraw_joypad_pads[1].connected,
         "an XInput pad that went is still listed, or took the other pad with it");
   CHECK(!winraw_joypad_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0xFFFF), "a pad that went can still be rumbled");
   /* and comes back, when raw input says a device has come */
   xpad[1].connected = true;
   xpad[1].buttons   = 0;
   winraw_joypad_add_device(H(21));
   CHECK(winraw_joypad_pads[0].connected && winraw_joypad_pads[0].xuser == 1,
         "the XInput pad did not come back to its slot");
   winraw_joypad_joypad_destroy();
   stub_settings.bools.input_winraw_xinput_enable = false;
   printf("   ok   an XInput pad rumbles; one that goes is noticed at the next poll and gives its slot up, and comes back to it\n");

   DeleteFileA(edge); DeleteFileA(ds4); DeleteFileA(other);
   if (failures)
   {
      printf("FAIL winraw_joypad_rumble_test: %u\n", failures);
      return 1;
   }
   printf("PASS winraw_joypad_rumble_test\n");
   return 0;
}
