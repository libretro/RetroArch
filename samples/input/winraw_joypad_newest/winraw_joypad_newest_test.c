/* winraw joypad: parsing only the newest report leaves the same state
 * as parsing every report.
 *
 * Reports read in bulk are not parsed as they come. The newest of
 * each report ID is held per controller (winraw_joypad_take_hid()) and
 * parsed once the read is done (winraw_joypad_parse_held()). That is
 * only right if what it leaves - buttons, axes, hats - is what parsing
 * every report in turn would have left.
 *
 * The driver is included whole, with its Win32/HID calls routed to a
 * fake controller that splits its state across report IDs: buttons, X
 * and Y in one, Z and a hat in another, single axes in two more, and
 * two that carry nothing the driver reads. Two of that controller are
 * connected. Thousands of random bursts of reports - any IDs, any
 * values, several reports in one record, a report too big to hold,
 * more IDs in a burst than are held - go to one through the holding
 * path and to the other one report at a time, and after every burst
 * the two must read the same.
 *
 * Also checked: how many parses each took, and that removing a
 * controller forgets what was held for it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>

/* ---- The fake controller -------------------------------------------- */
/* report: [id, b1, b2, b3, ...]
 *   id 1: b1 buttons 1..8 as bits, b2 X, b3 Y
 *   id 2: b1 Z, b2 hat (0..7, anything else centred)
 *   id 4: b1 Y
 *   id 5: b1 Z
 *   id 3, 6: nothing the driver reads */

static unsigned long parses;   /* one HidP_GetUsages per parsed report */

static UINT WINAPI fake_GetRawInputDeviceInfoA(HANDLE dev, UINT cmd,
      LPVOID data, PUINT size)
{
   switch (cmd)
   {
      case RIDI_DEVICEINFO:
      {
         RID_DEVICE_INFO *info = (RID_DEVICE_INFO*)data;
         info->dwType          = RIM_TYPEHID;
         info->hid.dwVendorId  = 0x054c;
         /* a product ID of its own each: the driver takes a second
          * device with the same IDs for the first one reconnecting */
         info->hid.dwProductId = (DWORD)(uintptr_t)dev;
         info->hid.usUsagePage = 0x01;
         info->hid.usUsage     = 0x05;
         return sizeof(*info);
      }
      case RIDI_DEVICENAME:
         *size = 0;
         return 0;
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
   caps->NumberInputButtonCaps = 1;
   caps->NumberInputValueCaps  = 4;
   return HIDP_STATUS_SUCCESS;
}

static NTSTATUS NTAPI fake_HidP_GetButtonCaps(HIDP_REPORT_TYPE type,
      PHIDP_BUTTON_CAPS caps, PUSHORT len, PHIDP_PREPARSED_DATA pp)
{
   (void)type; (void)pp;
   memset(caps, 0, sizeof(*caps));
   caps->UsagePage      = 0x09;
   caps->IsRange        = TRUE;
   caps->Range.UsageMin = 1;
   caps->Range.UsageMax = 8;
   *len                 = 1;
   return HIDP_STATUS_SUCCESS;
}

static NTSTATUS NTAPI fake_HidP_GetValueCaps(HIDP_REPORT_TYPE type,
      PHIDP_VALUE_CAPS caps, PUSHORT len, PHIDP_PREPARSED_DATA pp)
{
   static const USAGE usages[4] = { 0x30, 0x31, 0x32, 0x39 };
   unsigned i;
   (void)type; (void)pp;
   memset(caps, 0, 4 * sizeof(*caps));
   for (i = 0; i < 4; i++)
   {
      caps[i].UsagePage      = 0x01;
      caps[i].IsRange        = FALSE;
      caps[i].NotRange.Usage = usages[i];
      caps[i].BitSize        = 8;
      caps[i].LogicalMin     = 0;
      caps[i].LogicalMax     = (usages[i] == 0x39) ? 7 : 255;
   }
   *len = 4;
   return HIDP_STATUS_SUCCESS;
}

static NTSTATUS NTAPI fake_HidP_GetUsages(HIDP_REPORT_TYPE type,
      USAGE page, USHORT link, PUSAGE list, PULONG len,
      PHIDP_PREPARSED_DATA pp, PCHAR report, ULONG report_len)
{
   const BYTE *r = (const BYTE*)report;
   ULONG n       = 0;
   unsigned i;
   (void)type; (void)page; (void)link; (void)pp; (void)report_len;
   parses++;
   if (r[0] != 1)
   {
      *len = 0;
      return HIDP_STATUS_INCOMPATIBLE_REPORT_ID;
   }
   for (i = 0; i < 8; i++)
      if (r[1] & (1 << i))
         list[n++] = (USAGE)(1 + i);
   *len = n;
   return HIDP_STATUS_SUCCESS;
}

static NTSTATUS NTAPI fake_HidP_GetUsageValue(HIDP_REPORT_TYPE type,
      USAGE page, USHORT link, USAGE usage, PULONG value,
      PHIDP_PREPARSED_DATA pp, PCHAR report, ULONG report_len)
{
   const BYTE *r = (const BYTE*)report;
   (void)type; (void)page; (void)link; (void)pp; (void)report_len;
   switch (r[0])
   {
      case 1:
         if (usage == 0x30) { *value = r[2]; return HIDP_STATUS_SUCCESS; }
         if (usage == 0x31) { *value = r[3]; return HIDP_STATUS_SUCCESS; }
         break;
      case 2:
         if (usage == 0x32) { *value = r[1]; return HIDP_STATUS_SUCCESS; }
         if (usage == 0x39) { *value = r[2]; return HIDP_STATUS_SUCCESS; }
         break;
      case 4:
         if (usage == 0x31) { *value = r[1]; return HIDP_STATUS_SUCCESS; }
         break;
      case 5:
         if (usage == 0x32) { *value = r[1]; return HIDP_STATUS_SUCCESS; }
         break;
   }
   return HIDP_STATUS_INCOMPATIBLE_REPORT_ID;
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
bool winraw_raw_input_polled(void) { return true; }
void winraw_queue_read(void) { }
void winraw_queue_claim_thread(bool claim) { (void)claim; }

/* ---- The test ------------------------------------------------------- */

#define HELD   ((HANDLE)(uintptr_t)0x1111)   /* goes through the holding path */
#define EVERY  ((HANDLE)(uintptr_t)0x2222)   /* every report parsed in turn */

static unsigned rng_state = 12345;
static unsigned rng(void)
{
   rng_state = rng_state * 1103515245u + 12345u;
   return (rng_state >> 16) & 0x7fff;
}

static int same_state(void)
{
   const winraw_joypad_joypad_data_t *a = &winraw_joypad_pads[0];
   const winraw_joypad_joypad_data_t *b = &winraw_joypad_pads[1];
   return !memcmp(a->buttons, b->buttons, sizeof(a->buttons))
       && !memcmp(a->axes,    b->axes,    sizeof(a->axes))
       && !memcmp(a->hats,    b->hats,    sizeof(a->hats));
}

int main(void)
{
   /* IDs a burst draws from: mostly the full-state report, as a real
    * controller sends, and now and then every other kind */
   static const BYTE ids[16] = { 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 3, 4, 5, 6, 1, 2 };
   unsigned long parses_held = 0, parses_every = 0, reports = 0;
   unsigned burst, failures = 0;
   BYTE rec[3 * 250];

   memset(winraw_joypad_pads, 0, sizeof(winraw_joypad_pads));
   memset(winraw_joypad_held, 0, sizeof(winraw_joypad_held));
   if (   !winraw_joypad_add_device(HELD)
       || !winraw_joypad_add_device(EVERY)
       || winraw_joypad_find_pad(HELD)  != 0
       || winraw_joypad_find_pad(EVERY) != 1)
   {
      printf("FAIL: the two controllers did not connect in slots 0 and 1\n");
      return 1;
   }

   for (burst = 0; burst < 20000 && failures < 5; burst++)
   {
      /* a poll's worth: a thousand a second is 8 a frame at 120 fps
       * and 17 at 60; go past that too */
      unsigned n = 1 + rng() % 40;
      unsigned i;

      for (i = 0; i < n; i++)
      {
         /* one record: usually one report, sometimes several, and now
          * and then one too big to hold */
         DWORD count = (rng() % 10 == 0) ? 2 + rng() % 2 : 1;
         DWORD size  = (rng() % 50 == 0) ? 200 + rng() % 50 : 4 + rng() % 60;
         DWORD c, k;
         unsigned long p0;

         for (c = 0; c < count; c++)
         {
            BYTE *r = rec + c * size;
            for (k = 0; k < size; k++)
               r[k] = (BYTE)rng();
            r[0] = ids[rng() % 16];
         }
         reports += count;

         p0 = parses;
         winraw_joypad_take_hid(HELD, rec, size, count);
         parses_held += parses - p0;

         p0 = parses;
         for (c = 0; c < count; c++)
            winraw_joypad_parse_hid_report(&winraw_joypad_pads[1],
                  rec + c * size, size);
         parses_every += parses - p0;
      }

      {
         unsigned long p0 = parses;
         winraw_joypad_parse_held();
         parses_held += parses - p0;
      }

      if (!same_state())
      {
         printf("FAIL burst %u (%u records): the newest-only state differs from"
               " parsing every report\n", burst, n);
         failures++;
      }
      if (winraw_joypad_held[0].count)
      {
         printf("FAIL burst %u: reports still held after the poll's parse\n", burst);
         failures++;
      }
   }

   /* a controller that goes takes what was held for it with it */
   rec[0] = 1;
   winraw_joypad_take_hid(HELD, rec, 4, 1);
   if (!winraw_joypad_held[0].count)
   {
      printf("FAIL: a report read in bulk was not held\n");
      failures++;
   }
   winraw_joypad_remove_device(HELD);
   if (winraw_joypad_held[0].count)
   {
      printf("FAIL: removing the controller left its held report\n");
      failures++;
   }
   /* and reports for a controller that is not there go nowhere */
   winraw_joypad_take_hid(HELD, rec, 4, 1);
   winraw_joypad_parse_held();

   if (failures)
   {
      printf("FAIL winraw_joypad_newest_test: %u\n", failures);
      return 1;
   }
   printf("   %lu reports in 20000 bursts: %lu parses holding the newest,"
         " %lu parsing every one\n", reports, parses_held, parses_every);
   printf("PASS winraw_joypad_newest_test: the state is the same after every burst\n");
   return 0;
}
