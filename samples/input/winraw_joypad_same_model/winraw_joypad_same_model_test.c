/* winraw joypad: two controllers of the same model are two controllers.
 *
 * When a controller arrives, the driver looks for a slot that holds
 * the same device under an old handle: Windows can announce a
 * reconnected controller's new handle before it announces the old one
 * gone, and the old slot has to be reused rather than the controller
 * turn up twice. "The same device" used to mean the same vendor and
 * product ID, so a second controller of the same model evicted the
 * first.
 *
 * The driver is included whole, with its Win32/HID calls routed to
 * fake controllers that all have one vendor and product ID, each with
 * a device path and a handle that is alive or not as the test says.
 *
 * Checked here:
 *
 * - two of the same model, both plugged in: two slots, both connected;
 *   and a third makes three;
 * - one of them reconnecting - a new handle, the old one dead, arrival
 *   before removal - on the same port or another: it gets its own slot
 *   back, the other is not touched, and no disconnect is announced;
 * - the old handle's late removal then removes nothing;
 * - a reconnect whose old handle Windows still answers for is known by
 *   its path;
 * - with no path to go by, a live handle is another controller and a
 *   dead one is this one coming back. */
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
static struct { HANDLE h; bool alive; char path[64]; } fake[FAKE_MAX];
static unsigned fake_n;

static void fake_plug(HANDLE h, const char *path)
{
   fake[fake_n].h     = h;
   fake[fake_n].alive = true;
   strcpy(fake[fake_n].path, path ? path : "");
   fake_n++;
}

static void fake_unplug(HANDLE h)
{
   unsigned i;
   for (i = 0; i < fake_n; i++)
      if (fake[i].h == h)
         fake[i].alive = false;
}

static UINT WINAPI fake_GetRawInputDeviceInfoA(HANDLE dev, UINT cmd,
      LPVOID data, PUINT size)
{
   unsigned i;
   for (i = 0; i < fake_n; i++)
      if (fake[i].h == dev)
         break;
   /* a handle that names no device */
   if (i == fake_n || !fake[i].alive)
      return (UINT)-1;

   switch (cmd)
   {
      case RIDI_DEVICEINFO:
      {
         RID_DEVICE_INFO *info = (RID_DEVICE_INFO*)data;
         info->dwType          = RIM_TYPEHID;
         info->hid.dwVendorId  = 0x054c;
         info->hid.dwProductId = 0x0df2;
         info->hid.usUsagePage = 0x01;
         info->hid.usUsage     = 0x05;
         return sizeof(*info);
      }
      case RIDI_DEVICENAME:
      {
         UINT need = (UINT)strlen(fake[i].path);
         if (!need)
         {
            *size = 0;
            return 0;
         }
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
   return HIDP_STATUS_SUCCESS;
}

#define GetRawInputDeviceInfoA fake_GetRawInputDeviceInfoA
#define HidP_GetCaps           fake_HidP_GetCaps
/* the paths are not files: no product string, the driver names the pad */
#define CreateFileA(a,b,c,d,e,f,g) INVALID_HANDLE_VALUE

#include "input/drivers_joypad/winraw_joypad.c"

/* ---- The frontend, as far as the driver links against it ------------ */

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
static unsigned connects, disconnects;
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port,
      unsigned vid, unsigned pid)
{
   (void)name; (void)display_name; (void)phys; (void)driver;
   (void)port; (void)vid; (void)pid;
   connects++;
   return true;
}
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{ (void)port; (void)name; disconnects++; return true; }
bool winraw_raw_input_polled(void) { return true; }
void winraw_queue_read(void) { }
void winraw_queue_claim_thread(bool claim) { (void)claim; }

/* ---- The test ------------------------------------------------------- */

static unsigned failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

#define H(n) ((HANDLE)(uintptr_t)(0x1000 + (n)))

static unsigned connected(void)
{
   unsigned i, n = 0;
   for (i = 0; i < MAX_USERS; i++)
      if (winraw_joypad_pads[i].connected)
         n++;
   return n;
}

static void reset(void)
{
   memset(winraw_joypad_pads, 0, sizeof(winraw_joypad_pads));
   memset(winraw_joypad_held, 0, sizeof(winraw_joypad_held));
   winraw_joypad_pad_count = 0;
   fake_n      = 0;
   connects    = 0;
   disconnects = 0;
}

int main(void)
{
   /* ---- two of the same model, and a third ----------------------- */
   reset();
   fake_plug(H(1), "\\\\?\\HID#VID_054C&PID_0DF2#port-a");
   fake_plug(H(2), "\\\\?\\HID#VID_054C&PID_0DF2#port-b");
   CHECK(winraw_joypad_add_device(H(1)) && winraw_joypad_add_device(H(2)),
         "a controller was refused");
   CHECK(winraw_joypad_find_pad(H(1)) == 0 && winraw_joypad_find_pad(H(2)) == 1
         && connected() == 2,
         "two of the same model: slots %d and %d, %u connected",
         winraw_joypad_find_pad(H(1)), winraw_joypad_find_pad(H(2)), connected());
   fake_plug(H(3), "\\\\?\\HID#VID_054C&PID_0DF2#port-c");
   CHECK(winraw_joypad_add_device(H(3)) && winraw_joypad_find_pad(H(3)) == 2
         && winraw_joypad_find_pad(H(1)) == 0 && winraw_joypad_find_pad(H(2)) == 1,
         "a third of the same model did not take a third slot");
   printf("   ok   three of the same model, all plugged in: three slots\n");

   /* ---- the second reconnects on its port: arrival before removal - */
   fake_unplug(H(2));
   fake_plug(H(12), "\\\\?\\HID#VID_054C&PID_0DF2#port-b");
   disconnects = 0;
   CHECK(winraw_joypad_add_device(H(12)), "the reconnected controller was refused");
   CHECK(winraw_joypad_find_pad(H(12)) == 1 && winraw_joypad_find_pad(H(2)) < 0
         && winraw_joypad_find_pad(H(1)) == 0 && winraw_joypad_find_pad(H(3)) == 2
         && connected() == 3,
         "reconnect on the same port: it is in slot %d, %u connected",
         winraw_joypad_find_pad(H(12)), connected());
   CHECK(disconnects == 0, "a disconnect was announced for a reconnect");
   /* the old handle's removal, arriving late */
   winraw_joypad_remove_device(H(2));
   CHECK(connected() == 3 && disconnects == 0, "the late removal of the old handle removed something");
   printf("   ok   one reconnects on its port: its own slot back, the others untouched, nothing announced\n");

   /* ---- the first reconnects on another port ---------------------- */
   fake_unplug(H(1));
   fake_plug(H(11), "\\\\?\\HID#VID_054C&PID_0DF2#port-d");
   CHECK(winraw_joypad_add_device(H(11)) && winraw_joypad_find_pad(H(11)) == 0
         && winraw_joypad_find_pad(H(12)) == 1 && winraw_joypad_find_pad(H(3)) == 2
         && connected() == 3,
         "reconnect on another port: it is in slot %d, %u connected",
         winraw_joypad_find_pad(H(11)), connected());
   printf("   ok   one reconnects on another port: the slot its dead handle held\n");

   /* ---- a reconnect whose old handle still answers ----------------- */
   /* same path, old handle not yet dead as far as Windows says */
   fake_plug(H(13), "\\\\?\\hid#vid_054c&pid_0df2#PORT-C");   /* case differs */
   CHECK(winraw_joypad_add_device(H(13)) && winraw_joypad_find_pad(H(13)) == 2
         && winraw_joypad_find_pad(H(3)) < 0 && connected() == 3,
         "same path, old handle still alive: it is in slot %d, %u connected",
         winraw_joypad_find_pad(H(13)), connected());
   printf("   ok   a reconnect whose old handle still answers is known by its path\n");

   /* ---- no path to go by ------------------------------------------ */
   reset();
   fake_plug(H(1), NULL);
   fake_plug(H(2), NULL);
   CHECK(winraw_joypad_add_device(H(1)) && winraw_joypad_add_device(H(2))
         && connected() == 2, "no paths, both alive: %u connected", connected());
   fake_unplug(H(1));
   fake_plug(H(11), NULL);
   CHECK(winraw_joypad_add_device(H(11)) && winraw_joypad_find_pad(H(11)) == 0
         && winraw_joypad_find_pad(H(2)) == 1 && connected() == 2,
         "no paths, one dead: the newcomer is in slot %d, %u connected",
         winraw_joypad_find_pad(H(11)), connected());
   printf("   ok   with no path: a live handle is another controller, a dead one is this one back\n");

   if (failures)
   {
      printf("FAIL winraw_joypad_same_model_test: %u\n", failures);
      return 1;
   }
   printf("PASS winraw_joypad_same_model_test\n");
   return 0;
}
