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
 *   so. */
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

#define GetRawInputDeviceInfoA fake_GetRawInputDeviceInfoA
#define HidP_GetCaps           fake_HidP_GetCaps
#define CreateFileA            fake_CreateFileA

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
   CHECK(!winraw_joypad_out_thread && !winraw_joypad_out[1].present
         && !winraw_joypad_out[1].dev,
         "the driver stopped and left its thread or a device behind");
   {
      HANDLE f = CreateFileA(ds4, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
      CHECK(f != INVALID_HANDLE_VALUE, "the driver stopped and left the DualShock 4's device open");
      if (f != INVALID_HANDLE_VALUE)
         CloseHandle(f);
   }
   printf("   ok   an unplugged pad is stilled and closed by the thread, without the driver waiting; fifty quick plug-ins leave no device open; stopping the driver stills the rest and ends the thread\n");

   DeleteFileA(edge); DeleteFileA(ds4); DeleteFileA(other);
   if (failures)
   {
      printf("FAIL winraw_joypad_rumble_test: %u\n", failures);
      return 1;
   }
   printf("PASS winraw_joypad_rumble_test\n");
   return 0;
}
