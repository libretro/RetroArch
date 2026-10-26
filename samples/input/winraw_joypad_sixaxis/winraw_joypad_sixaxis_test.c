/* winraw joypad: a DualShock 3 on Sony's own driver.
 *
 * Sony's sixaxis.sys, and DsHidMini in the mode that stands in for
 * it, send no input reports for a DualShock 3: its state is feature
 * report 0, asked for. The driver's thread asks every 4 ms and hands
 * the newest report to the poll through three buffers; the pad's
 * motors and lights are that driver's commands in output report 0.
 *
 * The driver is included whole, its raw input, HID and device calls
 * routed to fakes. Checked here:
 *
 * - a DualShock 3 is read from its feature report: buttons 1 to 19 and
 *   the sticks X, Y, Z and Rz, as its own HID description has them,
 *   taken by the poll and not before; asked for every few ms;
 * - rumble and the player's lights go as that driver's commands;
 * - on Windows' own HID driver, which has no report 0, it is asked
 *   once, then left, and nothing is written to it;
 * - one whose input reports come after all is no longer asked;
 * - with no such pad the thread waits on nothing but wakes. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>

#include <boolean.h>

#define FAKE_MAX 4
static struct
{
   HANDLE h;
   unsigned pid;
   bool sony_driver;          /* has feature report 0 */
   volatile LONG asks;        /* feature reports asked for */
   uint8_t state[49];
   volatile LONG writes;
   uint8_t last[4][49];       /* the last write of each command */
} fake[FAKE_MAX];
static unsigned fake_n;

#define FAKE_FILE(i) ((HANDLE)(uintptr_t)(0x7000 + (i)))

static int fake_of_file(HANDLE f)
{
   uintptr_t v = (uintptr_t)f;
   return (v >= 0x7000 && v < 0x7000 + FAKE_MAX) ? (int)(v - 0x7000) : -1;
}

static UINT WINAPI fake_GetRawInputDeviceInfoA(HANDLE dev, UINT cmd,
      LPVOID data, PUINT size)
{
   unsigned i;
   for (i = 0; i < fake_n; i++)
      if (fake[i].h == dev)
         break;
   if (i == fake_n)
      return (UINT)-1;
   switch (cmd)
   {
      case RIDI_DEVICEINFO:
      {
         RID_DEVICE_INFO *info = (RID_DEVICE_INFO*)data;
         info->dwType          = RIM_TYPEHID;
         info->hid.dwVendorId  = 0x054C;
         info->hid.dwProductId = fake[i].pid;
         info->hid.usUsagePage = 0x01;
         info->hid.usUsage     = 0x04;
         return sizeof(*info);
      }
      case RIDI_DEVICENAME:
      {
         char path[32];
         snprintf(path, sizeof(path), "\\\\?\\HID#fake%u", i);
         if (!data)
         {
            *size = (UINT)strlen(path) + 1;
            return 0;
         }
         strcpy((char*)data, path);
         return (UINT)strlen(path) + 1;
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
   caps->OutputReportByteLength  = 49;
   caps->FeatureReportByteLength = 49;
   return HIDP_STATUS_SUCCESS;
}

/* opened for writing: the pad's file; for nothing (its name): not */
static HANDLE WINAPI fake_CreateFileA(LPCSTR name, DWORD access, DWORD share,
      LPSECURITY_ATTRIBUTES sec, DWORD disp, DWORD flags, HANDLE tmpl)
{
   unsigned i;
   (void)share; (void)sec; (void)disp; (void)flags; (void)tmpl;
   if (!(access & GENERIC_WRITE) || sscanf(name, "\\\\?\\HID#fake%u", &i) != 1)
      return INVALID_HANDLE_VALUE;
   return FAKE_FILE(i);
}

static BOOL WINAPI fake_CloseHandle(HANDLE h)
{
   return fake_of_file(h) >= 0 ? TRUE : CloseHandle(h);
}

static BOOL WINAPI fake_DeviceIoControl(HANDLE h, DWORD code, LPVOID in,
      DWORD in_len, LPVOID out, DWORD out_len, LPDWORD moved,
      LPOVERLAPPED ov)
{
   int i = fake_of_file(h);
   (void)in; (void)in_len; (void)ov;
   if (i < 0 || code != 0x000B0192)
      return FALSE;
   InterlockedIncrement(&fake[i].asks);
   if (!fake[i].sony_driver || out_len < 49 || ((uint8_t*)out)[0] != 0)
   {
      SetLastError(ERROR_INVALID_PARAMETER);
      return FALSE;
   }
   memcpy(out, fake[i].state, 49);
   *moved = 49;
   return TRUE;
}

static BOOL WINAPI fake_WriteFile(HANDLE h, LPCVOID buf, DWORD len,
      LPDWORD written, LPOVERLAPPED ov)
{
   int i = fake_of_file(h);
   const uint8_t *b = (const uint8_t*)buf;
   (void)ov;
   if (i < 0)
      return FALSE;
   if (len == 49 && b[0] == 0 && b[1] < 4)
      memcpy(fake[i].last[b[1]], b, 49);
   InterlockedIncrement(&fake[i].writes);
   *written = len;
   return TRUE;
}

#define GetRawInputDeviceInfoA fake_GetRawInputDeviceInfoA
#define HidP_GetCaps           fake_HidP_GetCaps
#define CreateFileA            fake_CreateFileA
#define CloseHandle            fake_CloseHandle
#define DeviceIoControl        fake_DeviceIoControl
#define WriteFile              fake_WriteFile

#include "input/drivers_joypad/winraw_joypad.c"

#undef CloseHandle

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
static settings_t stub_settings;
settings_t *config_get_ptr(void) { return &stub_settings; }
#include "../input_config_stubs.h"
void winraw_queue_read(void) { }
void winraw_queue_claim_thread(bool claim) { (void)claim; }

static unsigned failures;
#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

#define H(n) ((HANDLE)(uintptr_t)(0x1000 + (n)))

/* the pad's state as sixaxis.sys gives it: report 0, then the DS3's
 * input report */
static void set_state(unsigned i, uint32_t buttons, uint8_t lx, uint8_t ry)
{
   uint8_t *r = fake[i].state + 1;
   memset(fake[i].state, 0, sizeof(fake[i].state));
   r[0] = 0x01;
   r[2] = (uint8_t)buttons;
   r[3] = (uint8_t)(buttons >> 8);
   r[4] = (uint8_t)(buttons >> 16);
   r[6] = lx;
   r[7] = 0x80;
   r[8] = 0x80;
   r[9] = ry;
}

/* polls until slot's button @b reads @want; false after a second */
static bool poll_until(unsigned slot, unsigned b, bool want)
{
   unsigned tries;
   for (tries = 0; tries < 200; tries++)
   {
      winraw_joypad_joypad_poll();
      if ((WINRAW_BUTTON(&winraw_joypad_pads[slot], b) != 0) == want)
         return true;
      Sleep(5);
   }
   return false;
}

/* waits until command @cmd's last write has @at at @value */
static bool wait_cmd(unsigned i, unsigned cmd, unsigned at, uint8_t value)
{
   unsigned tries;
   for (tries = 0; tries < 200; tries++)
   {
      if (fake[i].last[cmd][1] == cmd && fake[i].last[cmd][at] == value)
         return true;
      Sleep(5);
   }
   return false;
}

int main(void)
{
   LONG asks;

   fake[0].h = H(1); fake[0].pid = 0x0268; fake[0].sony_driver = true;
   fake[1].h = H(2); fake[1].pid = 0x0268; fake[1].sony_driver = false;
   fake_n    = 2;
   winraw_joypad_sixaxis_init();
   winraw_joypad_initialised = true;

   CHECK(winraw_joypad_out_timeout(NULL) == -1,
         "with no DualShock 3 the thread is given a timeout");

   /* cross (bit 14) held, the left stick right, the right one down */
   set_state(0, 1u << 14, 0xFF, 0xFF);
   winraw_joypad_add_device(H(1));
   winraw_joypad_add_device(H(2));
   CHECK(winraw_joypad_out[0].sixaxis && winraw_joypad_out[1].sixaxis,
         "the DualShock 3s were not taken as maybe on Sony's driver");
   CHECK(!WINRAW_BUTTON(&winraw_joypad_pads[0], 14),
         "the state was read before a poll");
   CHECK(poll_until(0, 14, true), "the DualShock 3's state never reached the poll");
   CHECK(winraw_joypad_pads[0].num_buttons == 19
         && winraw_joypad_pads[0].view.axes[0] == 0x7fff
         && winraw_joypad_pads[0].view.axes[5] == 0x7fff
         && winraw_joypad_pads[0].view.axes[1] < 300
         && winraw_joypad_pads[0].view.axes[1] > -300,
         "laid out wrong: %u buttons, axes %d %d %d",
         winraw_joypad_pads[0].num_buttons, winraw_joypad_pads[0].view.axes[0],
         winraw_joypad_pads[0].view.axes[1], winraw_joypad_pads[0].view.axes[5]);
   set_state(0, 1u << 16, 0x80, 0x80);  /* PS */
   CHECK(poll_until(0, 16, true) && !WINRAW_BUTTON(&winraw_joypad_pads[0], 14),
         "a new state did not replace the last");
   asks = fake[0].asks;
   Sleep(200);
   CHECK(fake[0].asks - asks >= 20,
         "asked %ld times in 200 ms, want about one every 4 ms",
         (long)(fake[0].asks - asks));
   printf("   ok   read from its feature report, laid out as its own description, by the poll, every few ms\n");

   /* rumble and lights: that driver's commands */
   winraw_joypad_joypad_set_rumble(0, RETRO_RUMBLE_STRONG, 0x8000);
   winraw_joypad_joypad_set_rumble(0, RETRO_RUMBLE_WEAK, 0x0100);
   CHECK(wait_cmd(0, 2, 6, 1) && wait_cmd(0, 2, 8, 0x80)
         && fake[0].last[2][5] == 0xff && fake[0].last[2][6] == 1
         && fake[0].last[2][7] == 0xff && fake[0].last[2][8] == 0x80,
         "the motors command is not what was asked for");
   retro_atomic_store_release_int(&winraw_joypad_out[0].player, 2);
   input_output_writer_wake(winraw_joypad_out_writer);
   CHECK(wait_cmd(0, 1, 7, 1)
         && fake[0].last[1][7] == 1 && !fake[0].last[1][5]
         && !fake[0].last[1][6] && !fake[0].last[1][8],
         "player 2's light was not the one lit");
   printf("   ok   rumble and the player's light go as that driver's commands\n");

   /* Windows' own driver: asked once, then left; never written to */
   Sleep(50);
   CHECK(fake[1].asks == 1, "a pad with no report 0 was asked %ld times",
         (long)fake[1].asks);
   winraw_joypad_joypad_set_rumble(1, RETRO_RUMBLE_STRONG, 0xFFFF);
   Sleep(50);
   CHECK(fake[1].writes == 0, "a pad not on Sony's driver was written to");
   printf("   ok   on Windows' own HID driver it is asked once and left alone\n");

   /* its reports come after all: no longer asked */
   retro_atomic_store_release_int(&winraw_joypad_out[0].raw_seen, 1);
   Sleep(50);
   asks = fake[0].asks;
   Sleep(100);
   CHECK(fake[0].asks == asks, "a pad sending its reports was still asked");
   CHECK(winraw_joypad_out_timeout(NULL) == -1,
         "with nothing to ask the thread still has a timeout");
   printf("   ok   one whose reports come is no longer asked; the thread then waits on wakes alone\n");

   /* unplugged and stopped: the motors put back */
   winraw_joypad_out_stop();
   CHECK(fake[0].last[2][8] == 0 && fake[0].last[2][6] == 0,
         "the motors were left running");

   if (failures)
   {
      printf("[FAIL] winraw_joypad_sixaxis_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] winraw_joypad_sixaxis_test\n");
   return 0;
}
