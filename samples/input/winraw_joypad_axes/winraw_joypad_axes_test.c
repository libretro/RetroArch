/* winraw joypad: axes sit in DirectInput's DIJOYSTATE2 slots.
 *
 * A dinput autoconfig profile binds an axis by its DIJOYSTATE2 slot
 * (lX lY lZ lRx lRy lRz rglSlider[0] rglSlider[1] = 0..7), and the
 * winraw driver takes those profiles. So whatever order the HID value
 * caps come back in, X must read on axis 0, Y on 1, Z on 2, Rx on 3,
 * Ry on 4, Rz on 5, and Slider/Dial on 6 and 7 in cap order.
 *
 * The driver is included whole and its Win32/HID calls are routed to
 * fakes describing a device: the real enumeration (add_device) and
 * report parsing run, and values are read back through the driver's
 * axis callback the way a bind reads them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>

/* ---- The fake device ------------------------------------------------ */

#define FAKE_MAX_CAPS 8

static HIDP_VALUE_CAPS fake_caps[FAKE_MAX_CAPS];
static USHORT          fake_num_caps;
static ULONG           fake_values[0x40]; /* raw value per usage */

static void fake_device(const USAGE *usages, unsigned count)
{
   unsigned i;
   memset(fake_caps, 0, sizeof(fake_caps));
   memset(fake_values, 0, sizeof(fake_values));
   for (i = 0; i < count; i++)
   {
      HIDP_VALUE_CAPS *c   = &fake_caps[i];
      c->UsagePage         = 0x01;
      c->IsRange           = FALSE;
      c->NotRange.Usage    = usages[i];
      c->BitSize           = 8;
      c->LogicalMin        = 0;
      c->LogicalMax        = (usages[i] == 0x39) ? 7 : 255;
   }
   fake_num_caps = (USHORT)count;
}

static UINT WINAPI fake_GetRawInputDeviceInfoA(HANDLE dev, UINT cmd,
      LPVOID data, PUINT size)
{
   (void)dev;
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
         *size = 0; /* no path: the driver names the pad itself */
         return 0;
      case RIDI_PREPARSEDDATA:
         /* Opaque to the driver; the fakes below ignore it. */
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
   caps->NumberInputValueCaps = fake_num_caps;
   return HIDP_STATUS_SUCCESS;
}

static NTSTATUS NTAPI fake_HidP_GetButtonCaps(HIDP_REPORT_TYPE type,
      PHIDP_BUTTON_CAPS caps, PUSHORT len, PHIDP_PREPARSED_DATA pp)
{
   (void)type; (void)caps; (void)pp;
   *len = 0;
   return HIDP_STATUS_SUCCESS;
}

static NTSTATUS NTAPI fake_HidP_GetValueCaps(HIDP_REPORT_TYPE type,
      PHIDP_VALUE_CAPS caps, PUSHORT len, PHIDP_PREPARSED_DATA pp)
{
   (void)type; (void)pp;
   memcpy(caps, fake_caps, fake_num_caps * sizeof(*caps));
   *len = fake_num_caps;
   return HIDP_STATUS_SUCCESS;
}

static NTSTATUS NTAPI fake_HidP_GetUsageValue(HIDP_REPORT_TYPE type,
      USAGE page, USHORT link, USAGE usage, PULONG value,
      PHIDP_PREPARSED_DATA pp, PCHAR report, ULONG report_len)
{
   (void)type; (void)page; (void)link; (void)pp;
   (void)report; (void)report_len;
   if (usage >= sizeof(fake_values) / sizeof(fake_values[0]))
      return HIDP_STATUS_USAGE_NOT_FOUND;
   *value = fake_values[usage];
   return HIDP_STATUS_SUCCESS;
}

static NTSTATUS NTAPI fake_HidP_GetUsages(HIDP_REPORT_TYPE type,
      USAGE page, USHORT link, PUSAGE list, PULONG len,
      PHIDP_PREPARSED_DATA pp, PCHAR report, ULONG report_len)
{
   (void)type; (void)page; (void)link; (void)list; (void)pp;
   (void)report; (void)report_len;
   *len = 0;
   return HIDP_STATUS_SUCCESS;
}

#define GetRawInputDeviceInfoA fake_GetRawInputDeviceInfoA
#define HidP_GetCaps           fake_HidP_GetCaps
#define HidP_GetButtonCaps     fake_HidP_GetButtonCaps
#define HidP_GetValueCaps      fake_HidP_GetValueCaps
#define HidP_GetUsageValue     fake_HidP_GetUsageValue
#define HidP_GetUsages         fake_HidP_GetUsages

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
/* winraw_input.c's side of reading by the poll: not used here */
bool winraw_raw_input_polled(void) { return false; }
void winraw_queue_read(void) { }
void winraw_queue_claim_thread(bool claim) { (void)claim; }

/* ---- The test ------------------------------------------------------- */

static int failures;

static int16_t scaled(ULONG raw)
{
   return winraw_joypad_scale_axis((LONG)raw, 0, 255);
}

/* Connect a device with these caps, give each usage a distinct value,
 * parse one report, and check every axis slot reads the usage
 * DirectInput puts there (0 for an empty slot). */
static void check(const char *what, const USAGE *usages, unsigned count,
      const USAGE *slot_usage, unsigned expect_axes)
{
   unsigned i;
   BYTE report[8];
   winraw_joypad_joypad_data_t *pad = &winraw_joypad_pads[0];

   fake_device(usages, count);
   for (i = 0; i < count; i++)
      if (usages[i] != 0x39)
         fake_values[usages[i]] = 130 + 10 * (usages[i] - 0x30);

   memset(winraw_joypad_pads, 0, sizeof(winraw_joypad_pads));
   if (!winraw_joypad_add_device((HANDLE)(uintptr_t)0x1234))
   {
      printf("FAIL %s: add_device refused the device\n", what);
      failures++;
      return;
   }

   memset(report, 0, sizeof(report));
   winraw_joypad_parse_hid_report(pad, report, sizeof(report));

   if (pad->num_axes != expect_axes)
   {
      printf("FAIL %s: num_axes %u, expected %u\n",
            what, (unsigned)pad->num_axes, expect_axes);
      failures++;
   }

   for (i = 0; i < RAWINPUT_MAX_AXES; i++)
   {
      int16_t want = slot_usage[i]
                   ? scaled(fake_values[slot_usage[i]]) : 0;
      int16_t got  = winraw_joypad_joypad_axis(0, AXIS_POS(i));
      if (got != want)
      {
         printf("FAIL %s: axis %u reads %d, expected %d (usage 0x%02x)\n",
               what, i, got, want, (unsigned)slot_usage[i]);
         failures++;
      }
   }

   winraw_joypad_remove_device((HANDLE)(uintptr_t)0x1234);
}

int main(void)
{
   /* DIJOYSTATE2 slots for a pad with all six axes */
   static const USAGE six_slots[8] = {
      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0, 0 };

   /* A DualSense-style layout: sticks on X/Y and Z/Rz, triggers on
    * Rx/Ry, plus a hat - in descriptor order and reversed, since the
    * order HidP_GetValueCaps returns is not something to rely on. */
   static const USAGE ds_fwd[7] = {
      0x30, 0x31, 0x32, 0x35, 0x33, 0x34, 0x39 };
   static const USAGE ds_rev[7] = {
      0x39, 0x34, 0x33, 0x35, 0x32, 0x31, 0x30 };

   /* Slider and Dial share the two slider slots in cap order; a third
    * has no slot. */
   static const USAGE sliders[5]      = { 0x30, 0x37, 0x36, 0x36, 0x31 };
   static const USAGE slider_slots[8] = {
      0x30, 0x31, 0, 0, 0, 0, 0x37, 0x36 };

   /* Sticks only: X/Y and Rz, nothing else. Slots 2-4 stay empty. */
   static const USAGE sparse[3]       = { 0x35, 0x31, 0x30 };
   static const USAGE sparse_slots[8] = {
      0x30, 0x31, 0, 0, 0, 0x35, 0, 0 };

   check("dualsense descriptor order", ds_fwd, 7, six_slots, 6);
   check("dualsense reversed order",   ds_rev, 7, six_slots, 6);
   check("slider and dial",            sliders, 5, slider_slots, 8);
   check("sparse sticks",              sparse, 3, sparse_slots, 6);

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("[pass] winraw joypad axes sit in DIJOYSTATE2 slots\n");
   return 0;
}
