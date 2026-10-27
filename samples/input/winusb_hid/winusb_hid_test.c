/* Regression test for input/drivers_hid/winusb_hid.c: the WinUSB HID
 * driver, included here, with the real pad handlers behind it and a
 * stand-in for winusb.dll put in its table before it starts.
 *
 *   start       -> a DualShock 3 is started on the control pipe: its LED
 *                  report written, 0xF2 and 0xF5 read, as HID class
 *                  requests with the report's type and ID; 0xF4 is
 *                  tried and refused, as a pad on USB refuses it
 *   poll        -> the newest report the reader has had is the pad's
 *                  state, taken without a call into the driver
 *   bluetooth   -> the address the pad connects to is feature 0xF5
 *   gone        -> a read failing disconnects the pad and frees its port
 *   free        -> the reader stopped through the pipe's abort, joined,
 *                  and everything given back
 *   hid class   -> a DualSense on Windows' own HID driver: read through
 *                  ReadFile, lit and rumbled through writes that never
 *                  wait, the newest waiting behind one in flight; a pad
 *                  whose reports have no ID given them without
 *                  Windows' leading 0; gone, and freed with a write in
 *                  flight; the pad's state lent where its handler
 *                  keeps it, a generic pad's hat in it; a
 *                  PlayStation adapter's pad rumbled through its output
 *                  report, Windows' parser placing the values; a pad on
 *                  a chip's shared id taken by the handler its name
 *                  says, and read from its layout where it says none
 *   switch 2    -> the Switch 2 Pro Controller woken through its bulk
 *                  pipes: every command whole, each answer read, an
 *                  answer that never comes given up on; the player lit
 *                  once it reports */

#include "input/drivers_hid/winusb_hid.c"
#include "input/common/sony_pad_output.h"
#include "../joypad_bridge_checked/bridge_view.h"
#include "input/common/input_ignore_devices.h"

/* the frontend's; the user's list is the settings', here none */
bool input_device_ignored(uint16_t vid, uint16_t pid)
{ return input_ignore_devices_check(NULL, vid, pid); }

/* winusb as the HID joypad driver (hid_joypad.c) has it */
static void *bridge_hid;
static int32_t bridge_button(unsigned pad, uint16_t key)
{ return winusb_hid.button(bridge_hid, pad, key); }
static int16_t bridge_axis(unsigned pad, uint32_t axis)
{ return winusb_hid.axis(bridge_hid, pad, axis); }
static void bridge_get_buttons(unsigned pad, input_bits_t *state)
{ winusb_hid.get_buttons(bridge_hid, pad, state); }
static int16_t bridge_state(rarch_joypad_info_t *info,
      const uint16_t *keys, const uint32_t *axes, unsigned port)
{ return winusb_hid.state(bridge_hid, info, keys, axes, port); }
static const input_pad_view_t *bridge_view(unsigned pad)
{ return winusb_hid.view(bridge_hid, pad); }

/* the disagreements the snapshot bridge would find on pad 0 of @hid
 * (samples/input/joypad_bridge_checked) */
static unsigned bridge_check(void *hid)
{
   input_device_driver_t drv;
   memset(&drv, 0, sizeof(drv));
   drv.button      = bridge_button;
   drv.axis        = bridge_axis;
   drv.get_buttons = bridge_get_buttons;
   drv.state       = bridge_state;
   drv.view        = bridge_view;
   bridge_hid      = hid;
   return bv_compare(&drv, 0) + bv_compare_view(&drv, 0);
}

static int failures;
#define CHECK(cond, msg) do { if (!(cond)) { \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); failures++; } } while (0)

/* ---- the rest of RetroArch, stood in for ---- */

void RARCH_LOG(const char *f, ...)  { (void)f; }
void RARCH_DBG(const char *f, ...)  { (void)f; }
void RARCH_ERR(const char *f, ...)  { (void)f; }
void RARCH_WARN(const char *f, ...) { (void)f; }

static int connected_port = -1;
static int disconnects;
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port,
      unsigned vid, unsigned pid)
{
   connected_port = (int)port;
   return true;
}
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{
   disconnects++;
   return true;
}
void input_pad_connect(unsigned port, input_device_driver_t *driver) { }
/* the hid joypad driver's numbering: slot n is device n */
int input_driver_hid_claim(const void *hid_data, unsigned slot,
      const char *name, uint16_t vid, uint16_t pid) { return (int)slot; }
int input_driver_hid_number(const void *hid_data, unsigned slot)
{
   return (int)slot;
}
void input_driver_hid_release(const void *hid_data, unsigned slot) { }
static bool beside;
bool input_driver_hid_beside(const void *hid_data) { return beside; }
/* the scan's word to the menu on an adapter nothing could open */
void winusb_bind_gca_seen(bool waiting) { }
void winusb_bind_xid_seen(bool waiting) { }

/* ---- winusb.dll ---- */

static int frees;
static int aborts;
/* control transfers seen, in order */
static winusb_setup_packet_t setups[32];
static uint8_t setup_data[32][64];
static int n_setups;
static ULONG ctrl_len;
static uint8_t bt_host[6];

/* reports waiting for the reader, and whether it waits for more */
static CRITICAL_SECTION q_lock; /* the stand-in's own, not the driver's */
static HANDLE submit_entered, allow_submit, close_done, abort_seen;
static uint8_t q[16][49];
static int q_head, q_tail;
static volatile LONG reader_waiting;
static volatile LONG unplugged, pause_submit;
static bool read_pending, read_ok;
static UCHAR *read_buffer;
static ULONG read_len;
static OVERLAPPED *read_ov;

static BOOL WINAPI f_initialize(HANDLE file, void **usb)
{
   *usb = (void*)0x1234;
   return TRUE;
}
static BOOL WINAPI f_free(void *usb) { frees++; return TRUE; }

static BOOL WINAPI f_control(void *usb, winusb_setup_packet_t s,
      UCHAR *data, ULONG len, ULONG *got, OVERLAPPED *ov)
{
   int i = n_setups++ & 31;
   setups[i] = s;
   if (s.RequestType == 0x21)
      memcpy(setup_data[i], data, len < 64 ? len : 64);
   else if ((s.Value & 0xff) == 0xf5)
   {
      memset(data, 0, len);
      memcpy(data + 2, bt_host, 6);
   }
   else
      memset(data, 0, len);
   if ((s.Value & 0xff) == 0xf5 && s.RequestType == 0x21)
      memcpy(bt_host, data + 2, 6);
   ctrl_len = len;
   /* a DualShock 3 on USB stalls 0xF4, Bluetooth's start */
   if (s.RequestType == 0x21 && s.Value == 0x03f4)
   {
      SetLastError(ERROR_GEN_FAILURE);
      return FALSE;
   }
   return TRUE;
}

static void complete_read(void)
{
   if (!read_pending || (!unplugged && q_head == q_tail))
      return;
   read_ok = !unplugged;
   if (read_ok)
   {
      memcpy(read_buffer, q[q_head++ & 15], 49);
      read_len = 49;
   }
   read_pending = false;
   InterlockedExchange(&reader_waiting, 0);
   SetEvent(read_ov->hEvent);
}

static BOOL WINAPI f_read(void *usb, UCHAR pipe, UCHAR *buf, ULONG len,
      ULONG *got, OVERLAPPED *ov)
{
   if (InterlockedExchange(&pause_submit, 0))
   {
      SetEvent(submit_entered);
      WaitForSingleObject(allow_submit, INFINITE);
   }
   EnterCriticalSection(&q_lock);
   read_ov      = ov;
   read_buffer  = buf;
   read_pending = true;
   read_ok      = false;
   complete_read();
   if (!read_pending)
   {
      BOOL ok = read_ok;
      LeaveCriticalSection(&q_lock);
      if (!ok)
         SetLastError(ERROR_DEVICE_NOT_CONNECTED);
      return ok;
   }
   InterlockedExchange(&reader_waiting, 1);
   LeaveCriticalSection(&q_lock);
   SetLastError(ERROR_IO_PENDING);
   return FALSE;
}

static BOOL WINAPI f_abort(void *usb, UCHAR pipe)
{
   aborts++;
   EnterCriticalSection(&q_lock);
   if (read_pending)
   {
      read_pending = false;
      read_ok      = false;
      InterlockedExchange(&reader_waiting, 0);
      SetEvent(read_ov->hEvent);
   }
   LeaveCriticalSection(&q_lock);
   SetEvent(abort_seen);
   return TRUE;
}

static BOOL WINAPI f_result(void *usb, OVERLAPPED *ov, ULONG *got, BOOL wait)
{
   BOOL ok = TRUE;
   if (ov == read_ov)
   {
      if (wait)
         WaitForSingleObject(ov->hEvent, INFINITE);
      EnterCriticalSection(&q_lock);
      *got = read_len;
      ok   = read_ok;
      LeaveCriticalSection(&q_lock);
      if (!ok)
         SetLastError(ERROR_OPERATION_ABORTED);
   }
   else
      *got = ctrl_len;
   return ok;
}

static void close_driver(void *data)
{
   winusb_hid.free(data);
   SetEvent(close_done);
}

/* a report queued, and returned once the reader has taken everything
 * and waits again */
static void push_report(uint32_t buttons, uint8_t circle)
{
   uint8_t *r;
   EnterCriticalSection(&q_lock);
   r = q[q_tail++ & 15];
   memset(r, 0, 49);
   r[0]  = 0x01;
   r[2]  = (uint8_t)buttons;
   r[3]  = (uint8_t)(buttons >> 8);
   r[6]  = r[7] = r[8] = r[9] = 128;
   r[23] = circle;
   InterlockedExchange(&reader_waiting, 0);
   complete_read();
   LeaveCriticalSection(&q_lock);
}

static void wait_reader(void)
{
   int i;
   for (i = 0; i < 2000; i++)
   {
      bool idle;
      EnterCriticalSection(&q_lock);
      idle = (q_head == q_tail) && reader_waiting;
      LeaveCriticalSection(&q_lock);
      if (idle)
         return;
      Sleep(1);
   }
}

/* ---- hid.dll and the device's reads and writes ---- */

static uint8_t hq[16][80];
static DWORD hq_len[16];
static int hq_head, hq_tail;
static bool h_pending, h_ok, h_unplugged;
static uint8_t *h_buf;
static DWORD h_len;
static OVERLAPPED *h_ov;
static volatile LONG h_waiting;
static int cancels;
/* the pad's report lengths and whether its reports have IDs */
static USHORT caps_in, caps_out;
static UCHAR caps_id;
/* writes seen; held in flight while write_hold */
static uint8_t writes[16][WINUSB_OUT_MAX];
static DWORD write_len[16];
static int n_writes;
static bool write_hold, write_pending;
static unsigned firmware;

static BOOLEAN WINAPI h_attr(HANDLE h, PHIDD_ATTRIBUTES a) { return FALSE; }
static BOOLEAN WINAPI h_pp(HANDLE h, PHIDP_PREPARSED_DATA *pp)
{
   *pp = (PHIDP_PREPARSED_DATA)0x5678;
   return TRUE;
}
static BOOLEAN WINAPI h_free_pp(PHIDP_PREPARSED_DATA pp) { return TRUE; }
static bool generic_mode;
static bool generic_dup; /* X named twice */
static bool generic_xbox; /* an Xbox pad: both triggers on Z */
static bool sjoy_mode;   /* an MP-8866: one output field of three bytes */
static NTSTATUS WINAPI h_caps(PHIDP_PREPARSED_DATA pp, PHIDP_CAPS c)
{
   memset(c, 0, sizeof(*c));
   c->UsagePage              = 0x01;
   c->Usage                  = 0x05;
   c->InputReportByteLength  = caps_in;
   c->OutputReportByteLength = caps_out;
   c->NumberInputButtonCaps  = 1;
   c->NumberInputValueCaps   = generic_mode ? 5 : 1;
   c->NumberOutputValueCaps  = sjoy_mode ? 1 : 0;
   return HIDP_STATUS_SUCCESS;
}
/* a pad with no handler: twelve buttons, X, Y and Rz from 0 to 255,
 * a signed Rx, and a hat of eight ways */
static USAGE g_pressed[8];
static ULONG g_npressed;
static ULONG g_value[0x40];
static int frees_pp;

static BOOLEAN WINAPI h_free_pp_counted(PHIDP_PREPARSED_DATA pp)
{
   frees_pp++;
   return TRUE;
}

static NTSTATUS WINAPI h_bcaps(HIDP_REPORT_TYPE t, PHIDP_BUTTON_CAPS c,
      PUSHORT n, PHIDP_PREPARSED_DATA pp)
{
   memset(c, 0, sizeof(*c) * *n);
   *n          = 1;
   c->ReportID = caps_id;
   if (generic_mode)
   {
      c->UsagePage      = 0x09;
      c->IsRange        = TRUE;
      c->Range.UsageMin = 1;
      c->Range.UsageMax = 12;
   }
   return HIDP_STATUS_SUCCESS;
}
static NTSTATUS WINAPI h_vcaps(HIDP_REPORT_TYPE t, PHIDP_VALUE_CAPS c,
      PUSHORT n, PHIDP_PREPARSED_DATA pp)
{
   static const USAGE usage[5] = { 0x30, 0x31, 0x35, 0x33, 0x39 };
   static const USAGE dup[5]   = { 0x30, 0x30, 0x31, 0x33, 0x39 };
   USHORT i;
   memset(c, 0, sizeof(*c) * *n);
   if (t == HidP_Output)
   {
      if (!sjoy_mode)
         return HIDP_STATUS_INVALID_PREPARSED_DATA;
      c->ReportID       = 1;
      c->UsagePage      = 0x08;
      c->NotRange.Usage = 0x4b;
      c->ReportCount    = 3;
      c->BitSize        = 8;
      *n                = 1;
      return HIDP_STATUS_SUCCESS;
   }
   if (!generic_mode)
   {
      *n          = 1;
      c->ReportID = caps_id;
      return HIDP_STATUS_SUCCESS;
   }
   if (generic_xbox)
   {
      static const USAGE xbox[5] = { 0x30, 0x31, 0x33, 0x34, 0x32 };
      for (i = 0; i < 5 && i < *n; i++)
      {
         c[i].UsagePage      = 0x01;
         c[i].NotRange.Usage = xbox[i];
         c[i].BitSize        = 16;
         c[i].LogicalMin     = 0;
         /* 65535 as Windows gives it: its descriptor's two bytes,
          * signed */
         c[i].LogicalMax     = -1;
      }
      *n = i;
      return HIDP_STATUS_SUCCESS;
   }
   for (i = 0; i < 5 && i < *n; i++)
   {
      c[i].UsagePage       = 0x01;
      c[i].NotRange.Usage  = generic_dup ? dup[i] : usage[i];
      c[i].BitSize         = 8;
      c[i].LogicalMin      = 0;
      c[i].LogicalMax      = 255;
   }
   c[3].LogicalMin = -128;   /* Rx, signed */
   c[3].LogicalMax = 127;
   c[4].BitSize    = 4;      /* the hat */
   c[4].LogicalMax = 7;
   *n              = i;
   return HIDP_STATUS_SUCCESS;
}
static NTSTATUS WINAPI h_set_value(HIDP_REPORT_TYPE t, USAGE page,
      USHORT lc, USAGE usage, ULONG v, PHIDP_PREPARSED_DATA pp, PCHAR r,
      ULONG len)
{
   return HIDP_STATUS_USAGE_NOT_FOUND;
}
static NTSTATUS WINAPI h_set_array(HIDP_REPORT_TYPE t, USAGE page,
      USHORT lc, USAGE usage, PCHAR v, USHORT bytes,
      PHIDP_PREPARSED_DATA pp, PCHAR r, ULONG len)
{
   if (t != HidP_Output || page != 0x08 || usage != 0x4b || bytes != 3
         || len < 4)
      return HIDP_STATUS_USAGE_NOT_FOUND;
   r[0] = 1;
   memcpy(r + 1, v, 3);
   return HIDP_STATUS_SUCCESS;
}
static NTSTATUS WINAPI h_usages(HIDP_REPORT_TYPE t, USAGE page, USHORT lc,
      PUSAGE list, PULONG n, PHIDP_PREPARSED_DATA pp, PCHAR r, ULONG len)
{
   ULONG i;
   if (page != 0x09)
      return HIDP_STATUS_USAGE_NOT_FOUND;
   for (i = 0; i < g_npressed && i < *n; i++)
      list[i] = g_pressed[i];
   *n = i;
   return HIDP_STATUS_SUCCESS;
}
static NTSTATUS WINAPI h_value(HIDP_REPORT_TYPE t, USAGE page, USHORT lc,
      USAGE usage, PULONG v, PHIDP_PREPARSED_DATA pp, PCHAR r, ULONG len)
{
   if (page != 0x01 || usage >= 0x40)
      return HIDP_STATUS_USAGE_NOT_FOUND;
   *v = g_value[usage];
   return HIDP_STATUS_SUCCESS;
}
static const char *product_name = "Generic Pad";
static BOOLEAN WINAPI h_product(HANDLE h, PVOID buf, ULONG len)
{
   WCHAR *w = (WCHAR*)buf;
   ULONG i;
   for (i = 0; product_name[i] && (i + 1) * sizeof(WCHAR) < len; i++)
      w[i] = (WCHAR)product_name[i];
   w[i] = 0;
   return TRUE;
}
static BOOLEAN WINAPI h_get_feature(HANDLE h, PVOID buf, ULONG len)
{
   uint8_t *b = (uint8_t*)buf;
   if (b[0] != 0x20 || len < 64 || !firmware)
      return FALSE;
   memset(b + 1, 0, len - 1);
   b[44] = (uint8_t)firmware;
   b[45] = (uint8_t)(firmware >> 8);
   return TRUE;
}
static BOOLEAN WINAPI h_set_feature(HANDLE h, PVOID buf, ULONG len) { return TRUE; }

static void h_complete(void)
{
   if (!h_pending || (!h_unplugged && hq_head == hq_tail))
      return;
   h_ok = !h_unplugged;
   if (h_ok)
   {
      int i = hq_head++ & 15;
      memcpy(h_buf, hq[i], hq_len[i]);
      h_len = hq_len[i];
   }
   h_pending = false;
   InterlockedExchange(&h_waiting, 0);
   SetEvent(h_ov->hEvent);
}

static BOOL WINAPI h_read(HANDLE file, LPVOID buf, DWORD len, LPDWORD got,
      LPOVERLAPPED ov)
{
   EnterCriticalSection(&q_lock);
   h_ov      = ov;
   h_buf     = (uint8_t*)buf;
   h_pending = true;
   h_ok      = false;
   h_complete();
   if (!h_pending)
   {
      BOOL ok = h_ok;
      LeaveCriticalSection(&q_lock);
      if (!ok)
         SetLastError(ERROR_DEVICE_NOT_CONNECTED);
      return ok;
   }
   InterlockedExchange(&h_waiting, 1);
   LeaveCriticalSection(&q_lock);
   SetLastError(ERROR_IO_PENDING);
   return FALSE;
}

static BOOL WINAPI h_write(HANDLE file, LPCVOID buf, DWORD len, LPDWORD got,
      LPOVERLAPPED ov)
{
   int i = n_writes++ & 15;
   memcpy(writes[i], buf, len < WINUSB_OUT_MAX ? len : WINUSB_OUT_MAX);
   write_len[i] = len;
   if (write_hold)
   {
      write_pending = true;
      SetLastError(ERROR_IO_PENDING);
      return FALSE;
   }
   return TRUE;
}

static BOOL WINAPI h_result(HANDLE file, LPOVERLAPPED ov, LPDWORD got,
      BOOL wait)
{
   BOOL ok;
   if (ov != h_ov)
   {
      /* a write */
      if (write_pending && !wait)
      {
         SetLastError(ERROR_IO_INCOMPLETE);
         return FALSE;
      }
      write_pending = false;
      return TRUE;
   }
   if (wait)
      WaitForSingleObject(ov->hEvent, INFINITE);
   EnterCriticalSection(&q_lock);
   *got = h_len;
   ok   = h_ok;
   LeaveCriticalSection(&q_lock);
   if (!ok)
      SetLastError(ERROR_OPERATION_ABORTED);
   return ok;
}

static BOOL WINAPI h_cancel(HANDLE file)
{
   cancels++;
   write_pending = false;
   EnterCriticalSection(&q_lock);
   if (h_pending)
   {
      h_pending = false;
      h_ok      = false;
      InterlockedExchange(&h_waiting, 0);
      SetEvent(h_ov->hEvent);
   }
   LeaveCriticalSection(&q_lock);
   return TRUE;
}

static void push_hreport(const uint8_t *r, DWORD len)
{
   int i;
   EnterCriticalSection(&q_lock);
   i = hq_tail++ & 15;
   memcpy(hq[i], r, len);
   hq_len[i] = len;
   InterlockedExchange(&h_waiting, 0);
   h_complete();
   LeaveCriticalSection(&q_lock);
}

static void wait_hreader(void)
{
   int i;
   for (i = 0; i < 2000; i++)
   {
      bool idle;
      EnterCriticalSection(&q_lock);
      idle = (hq_head == hq_tail) && h_waiting;
      LeaveCriticalSection(&q_lock);
      if (idle)
         return;
      Sleep(1);
   }
}

/* the last write is the shared builder's report for this state, padded
 * to the device's length */
static bool written_is(bool v2, uint8_t strong, uint8_t weak, int player)
{
   uint8_t want[SONY_PAD_REPORT_MAX];
   int i      = (n_writes - 1) & 15;
   size_t len = sony_pad_output_report(want, sizeof(want),
         SONY_PAD_DUALSENSE, false, v2, strong, weak, player);
   return n_writes && write_len[i] == caps_out
      && !memcmp(writes[i], want, len);
}

static void test_hidclass(const char *path)
{
   winusb_hid_t *hid;
   winusb_device_t *dev;
   uint8_t r[80];
   int i, base_disconnects;

   winusb_hidclass_api.GetAttributes     = h_attr;
   winusb_hidclass_api.GetPreparsedData  = h_pp;
   winusb_hidclass_api.FreePreparsedData = h_free_pp;
   winusb_hidclass_api.GetCaps           = h_caps;
   winusb_hidclass_api.GetButtonCaps     = h_bcaps;
   winusb_hidclass_api.GetValueCaps      = h_vcaps;
   winusb_hidclass_api.GetFeature        = h_get_feature;
   winusb_hidclass_api.SetFeature        = h_set_feature;
   winusb_hidclass_api.Read              = h_read;
   winusb_hidclass_api.Write             = h_write;
   winusb_hidclass_api.Result            = h_result;
   winusb_hidclass_api.Cancel            = h_cancel;
   winusb_hidclass_api.GetProductString  = h_product;
   winusb_hidclass_api.GetUsages         = h_usages;
   winusb_hidclass_api.GetUsageValue     = h_value;
   winusb_hidclass_api.SetUsageValue     = h_set_value;
   winusb_hidclass_api.SetUsageValueArray = h_set_array;

   hid = (winusb_hid_t*)winusb_hid.init();
   CHECK(hid != NULL, "hid class: init failed");
   if (!hid)
      return;

   /* a DualSense on USB, firmware 2.24: lit as player 1 at its first
    * report, the newer motor control */
   caps_in = 64; caps_out = 48; caps_id = 1; firmware = 0x0224;
   connected_port = -1;
   winusb_hid_open(hid, path, "HID\\VID_054C&PID_0CE6&MI_03\\1",
         0x054c, 0x0ce6, true, false);
   CHECK(hid->devices && connected_port == 0, "hid class: the pad is not on port 1");
   if (!hid->devices)
      return;
   dev = hid->devices;
   CHECK(!strcmp(dev->name, "DualSense Wireless Controller"),
         "hid class: the pad is not known by its name");
   memset(r, 0, sizeof(r));
   r[0] = 0x01;
   r[1] = r[2] = r[3] = r[4] = 0x80;
   r[8] = 0x08 | 0x20; /* hat at rest, cross */
   push_hreport(r, 64);
   wait_hreader();
   winusb_hid.poll(hid);
   CHECK(winusb_hid.button(hid, 0, RETRO_DEVICE_ID_JOYPAD_B)
         && !winusb_hid.button(hid, 0, RETRO_DEVICE_ID_JOYPAD_A),
         "hid class: the report was not read");
   {
      const input_pad_view_t *v = winusb_hid.view(hid, 0);
      input_bits_t bits;
      winusb_hid.get_buttons(hid, 0, &bits);
      CHECK(v && !memcmp(&v->buttons, &bits, sizeof(bits)) && !v->hats
            && v->axes[0] == winusb_hid.axis(hid, 0, AXIS_POS(0))
            && !winusb_hid.view(hid, 1),
            "hid class: the pad's state is not lent as the reads answer");
      CHECK(!bridge_check(hid), "hid class: the DualSense's reads say what the bridge would not");
   }
   CHECK(n_writes == 1 && written_is(true, 0, 0, 1),
         "hid class: the first report did not light player 1, at the device's length");

   /* rumble with a write in flight: the newest waits, and goes at a
    * later poll */
   write_hold = true;
   winusb_hid.set_rumble(hid, 0, RETRO_RUMBLE_STRONG, 0x4000);
   push_hreport(r, 64);
   wait_hreader();
   winusb_hid.poll(hid);
   CHECK(n_writes == 2 && written_is(true, 0x40, 0, 0), "hid class: rumble was not written");
   winusb_hid.set_rumble(hid, 0, RETRO_RUMBLE_STRONG, 0x8000);
   push_hreport(r, 64);
   wait_hreader();
   winusb_hid.poll(hid);
   winusb_hid.set_rumble(hid, 0, RETRO_RUMBLE_STRONG, 0xc000);
   push_hreport(r, 64);
   wait_hreader();
   winusb_hid.poll(hid);
   CHECK(n_writes == 2 && dev->out_waiting,
         "hid class: a write went out over one in flight, or none waits");
   write_hold    = false;
   write_pending = false;
   winusb_hid.poll(hid);
   CHECK(n_writes == 3 && written_is(true, 0xc0, 0, 0) && !dev->out_waiting,
         "hid class: the newest rumble did not go once the write was done");

   /* gone */
   base_disconnects = disconnects;
   EnterCriticalSection(&q_lock);
   h_unplugged = true;
   h_complete();
   LeaveCriticalSection(&q_lock);
   for (i = 0; i < 2000 && !retro_atomic_load_acquire_int(&dev->gone); i++)
      Sleep(1);
   winusb_hid.poll(hid);
   CHECK(disconnects == base_disconnects + 1 && !hid->devices,
         "hid class: the pad was not disconnected");
   h_unplugged = false;

   /* a pad whose reports have no ID: Windows puts a 0 first; the
    * handler is given the report hidraw would give */
   caps_in = 9; caps_out = 0; caps_id = 0;
   winusb_hid_open(hid, path, "HID\\VID_081F&PID_E401\\1",
         0x081f, 0xe401, true, false);
   CHECK(hid->devices && hid->devices->strip_id && hid->devices->in_len == 9,
         "hid class: a pad without report IDs was not known as one");
   memset(r, 0, sizeof(r));
   r[1 + 6] = 0x20;   /* start: bit 5 of the SNES pad's byte 6 */
   r[1 + 0] = r[1 + 1] = 0x7f;
   push_hreport(r, 9);
   wait_hreader();
   winusb_hid.poll(hid);
   CHECK(winusb_hid.button(hid, 0, RETRO_DEVICE_ID_JOYPAD_START),
         "hid class: Windows' leading 0 reached the handler");

   CHECK(!winusb_hid_is_bluetooth(hid->devices),
         "hid class: a pad on USB is said to be on Bluetooth");
   /* the stand-in has one device's reads at a time: this one goes */
   dev = hid->devices;
   EnterCriticalSection(&q_lock);
   h_unplugged = true;
   h_complete();
   LeaveCriticalSection(&q_lock);
   for (i = 0; i < 2000 && dev && !retro_atomic_load_acquire_int(&dev->gone); i++)
      Sleep(1);
   winusb_hid.poll(hid);
   h_unplugged = false;

   /* a pad with no handler: read from its layout, numbered as the
    * Mac numbers it, named as it names itself */
   generic_mode = true;
   caps_in = 9; caps_out = 0; caps_id = 0;
   winusb_hidclass_api.FreePreparsedData = h_free_pp_counted;
   frees_pp = 0;
   winusb_hid_open(hid, path, "HID\\VID_1234&PID_5678\\1",
         0x1234, 0x5678, true, false);
   CHECK(hid->devices && hid->devices->generic && !hid->devices->strip_id
         && !strcmp(hid->devices->name, "Generic Pad"),
         "hid class: a pad with no handler was not taken, read whole, by its name");
   g_pressed[0] = 1; g_pressed[1] = 3; g_npressed = 2;
   g_value[0x30] = 255;  /* X: full right */
   g_value[0x31] = 0;    /* Y: full up */
   g_value[0x35] = 128;  /* Rz: centre */
   g_value[0x33] = 0x80; /* Rx: -128, signed */
   g_value[0x39] = 2;    /* hat: right */
   memset(r, 0, sizeof(r));
   push_hreport(r, 9);
   wait_hreader();
   winusb_hid.poll(hid);
   CHECK(   winusb_hid.button(hid, 0, 0) && !winusb_hid.button(hid, 0, 1)
         && winusb_hid.button(hid, 0, 2),
         "hid class: the buttons are not numbered by usage");
   CHECK(   winusb_hid.axis(hid, 0, AXIS_POS(0)) == 32767
         && winusb_hid.axis(hid, 0, AXIS_NEG(1)) == -32767
         && winusb_hid.axis(hid, 0, AXIS_NEG(2)) == -32767
         && winusb_hid.axis(hid, 0, AXIS_POS(5)) < 200
         && winusb_hid.axis(hid, 0, AXIS_NEG(5)) > -200,
         "hid class: the axes are not X, Y, Rx, Ry, Z, Rz as on the Mac, or a signed one misread");
   CHECK(   winusb_hid.button(hid, 0, HAT_MAP(0, HAT_RIGHT_MASK))
         && !winusb_hid.button(hid, 0, HAT_MAP(0, HAT_UP_MASK)),
         "hid class: the hat is not read");
   {
      const input_pad_view_t *v = winusb_hid.view(hid, 0);
      CHECK(v && v->hats == 8 && v->axes[0] == 32767
            && BIT256_GET(v->buttons, 0) && BIT256_GET(v->buttons, 2)
            && !BIT256_GET(v->buttons, 1),
            "hid class: a pad with no handler: its state, hat and all, is not lent");
      CHECK(!bridge_check(hid), "hid class: a pad with no handler: its reads say what the bridge would not");
   }
   g_value[0x39] = 8;    /* at rest: out of the hat's range */
   push_hreport(r, 9);
   wait_hreader();
   winusb_hid.poll(hid);
   CHECK(!winusb_hid.button(hid, 0, HAT_MAP(0, HAT_RIGHT_MASK)),
         "hid class: a hat at rest still reads a way");
   dev = hid->devices;
   EnterCriticalSection(&q_lock);
   h_unplugged = true;
   h_complete();
   LeaveCriticalSection(&q_lock);
   for (i = 0; i < 2000 && !retro_atomic_load_acquire_int(&dev->gone); i++)
      Sleep(1);
   winusb_hid.poll(hid);
   h_unplugged = false;
   CHECK(frees_pp == 1, "hid class: the pad's layout was not let go with it");
   winusb_hidclass_api.FreePreparsedData = h_free_pp;

   /* X named twice: Y keeps axis 1, and the second X takes the first
    * free one once the others have theirs, as on the Mac */
   generic_dup = true;
   winusb_hid_open(hid, path, "HID\\VID_1234&PID_5678\\2",
         0x1234, 0x5678, true, false);
   CHECK(   hid->devices && hid->devices->gen.values == 5
         && hid->devices->gen.value[0].id == 0
         && hid->devices->gen.value[1].id == 3
         && hid->devices->gen.value[2].id == 1
         && hid->devices->gen.value[3].id == 2,
         "hid class: an axis named twice is not placed as on the Mac");
   dev = hid->devices;
   EnterCriticalSection(&q_lock);
   h_unplugged = true;
   h_complete();
   LeaveCriticalSection(&q_lock);
   for (i = 0; i < 2000 && !retro_atomic_load_acquire_int(&dev->gone); i++)
      Sleep(1);
   winusb_hid.poll(hid);
   h_unplugged = false;
   generic_dup  = false;

   /* an Xbox pad on Windows' HID driver: its triggers, both on Z, split
    * onto axes 4 and 5, 0 at rest, as SDL splits them */
   generic_xbox = true;
   winusb_hid_open(hid, path, "HID\\VID_045E&PID_02FF&IG_00\\1",
         0x045e, 0x02ff, true, false);
   g_npressed    = 0;
   g_value[0x30] = g_value[0x31] = g_value[0x33] = g_value[0x34] = 32768;
   g_value[0x32] = 32768;   /* both at rest */
   push_hreport(r, 9);
   wait_hreader();
   winusb_hid.poll(hid);
   CHECK(   winusb_hid.axis(hid, 0, AXIS_POS(4)) == 0
         && winusb_hid.axis(hid, 0, AXIS_POS(5)) == 0,
         "hid class: an Xbox pad's triggers not at rest");
   g_value[0x30] = 65535;   /* left stick right, right stick up */
   g_value[0x34] = 0;
   push_hreport(r, 9);
   wait_hreader();
   winusb_hid.poll(hid);
   CHECK(   winusb_hid.axis(hid, 0, AXIS_POS(0)) == 32767
         && winusb_hid.axis(hid, 0, AXIS_NEG(3)) == -32767
         && winusb_hid.axis(hid, 0, AXIS_POS(1)) <= 1,
         "hid class: an Xbox pad's sticks dead, its maximum taken as -1");
   g_value[0x30] = g_value[0x34] = 32768;
   g_value[0x32] = 65535;   /* the left pulled */
   push_hreport(r, 9);
   wait_hreader();
   winusb_hid.poll(hid);
   CHECK(   winusb_hid.axis(hid, 0, AXIS_POS(4)) == 32767
         && winusb_hid.axis(hid, 0, AXIS_POS(5)) == 0,
         "hid class: an Xbox pad's left trigger not on axis 4");
   /* without Windows.Gaming.Input, rumble asked of it goes nowhere */
   CHECK(hid->devices && hid->devices->gen.xbox && !hid->devices->gen.wgi,
         "hid class: an Xbox pad not known for one, or paired with nothing");
   winusb_hid.set_rumble(hid, 0, RETRO_RUMBLE_STRONG, 0xffff);
   winusb_hid.poll(hid);
   {
      /* paired by its look: buttons and sticks alike, near enough */
      winusb_wgi_look_t look;
      int16_t axes[4] = { 0, 0, 1000, -1000 };
      look.sticks[0] = 2000; look.sticks[1] = -2000;
      look.sticks[2] = 1000; look.sticks[3] = -1000;
      look.buttons   = 0x5;
      CHECK(winusb_wgi_alike(&look, axes, 0x5)
            && !winusb_wgi_alike(&look, axes, 0x4),
            "hid class: a gamepad told from its HID collection by its buttons");
      axes[0] = 9000;
      CHECK(!winusb_wgi_alike(&look, axes, 0x5),
            "hid class: a gamepad taken for one whose stick is elsewhere");
   }
   g_value[0x32] = 0;       /* the right pulled */
   push_hreport(r, 9);
   wait_hreader();
   winusb_hid.poll(hid);
   CHECK(   winusb_hid.axis(hid, 0, AXIS_POS(4)) == 0
         && winusb_hid.axis(hid, 0, AXIS_POS(5)) == 32767,
         "hid class: an Xbox pad's right trigger not on axis 5");
   dev = hid->devices;
   EnterCriticalSection(&q_lock);
   h_unplugged = true;
   h_complete();
   LeaveCriticalSection(&q_lock);
   for (i = 0; i < 2000 && !retro_atomic_load_acquire_int(&dev->gone); i++)
      Sleep(1);
   winusb_hid.poll(hid);
   h_unplugged  = false;
   generic_xbox = false;

   /* an MP-8866's port: rumbled at the poll, through its output
    * report's three values - on, the small motor, the large one */
   sjoy_mode = true;
   caps_in = 9; caps_out = 5; caps_id = 0;
   winusb_hid_open(hid, path, "HID\\VID_0925&PID_8866&Col01\\1",
         0x0925, 0x8866, true, false);
   CHECK(hid->devices && hid->devices->generic
         && hid->devices->gen.out.fields == 1,
         "hid class: an MP-8866's rumble report was not found");
   if (hid->devices)
   {
      int before = n_writes;
      int w;
      winusb_hid.set_rumble(hid, 0, RETRO_RUMBLE_STRONG, 0x8000);
      winusb_hid.set_rumble(hid, 0, RETRO_RUMBLE_WEAK, 0x0100);
      CHECK(n_writes == before, "hid class: the adapter was written outside the poll");
      memset(r, 0, sizeof(r));
      push_hreport(r, 9);
      wait_hreader();
      winusb_hid.poll(hid);
      w = (n_writes - 1) & 15;
      CHECK(n_writes == before + 1 && write_len[w] == 5
            && writes[w][0] == 1 && writes[w][1] == 1 && writes[w][2] == 1
            && writes[w][3] == 0x80 && writes[w][4] == 0,
            "hid class: the adapter's report is not on, small on, large 0x80");
      push_hreport(r, 9);
      wait_hreader();
      winusb_hid.poll(hid);
      CHECK(n_writes == before + 1, "hid class: the adapter was written again unchanged");
      dev = hid->devices;
      EnterCriticalSection(&q_lock);
      h_unplugged = true;
      h_complete();
      LeaveCriticalSection(&q_lock);
      for (i = 0; i < 2000 && !retro_atomic_load_acquire_int(&dev->gone); i++)
         Sleep(1);
      winusb_hid.poll(hid);
      h_unplugged = false;
   }
   sjoy_mode    = false;
   caps_out     = 0;

   /* 0079:0011 is a chip's: a pad on it that names itself otherwise is
    * read from its layout and known by its own name; one that calls
    * itself "USB Gamepad", as the chip does, has the chip's handler */
   {
      static const char *names[2] = { "Other Pad", "USB Gamepad " };
      unsigned k;
      for (k = 0; k < 2; k++)
      {
         product_name = names[k];
         caps_in = 9; caps_out = 0; caps_id = 0;
         winusb_hid_open(hid, path, k ? "HID\\VID_0079&PID_0011\\2"
               : "HID\\VID_0079&PID_0011\\1", 0x0079, 0x0011, true, false);
         CHECK(hid->devices, "hid class: a pad on 0079:0011 was not taken");
         if (!hid->devices)
            continue;
         if (k == 0)
            CHECK(hid->devices->generic
                  && !strcmp(hid->devices->name, "Other Pad"),
                  "hid class: a pad on 0079:0011 naming itself otherwise was taken by the chip's handler");
         else
            CHECK(!hid->devices->generic
                  && !strcmp(hid->devices->name, "USB Gamepad"),
                  "hid class: a \"USB Gamepad\" on 0079:0011 lost its handler");
         dev = hid->devices;
         EnterCriticalSection(&q_lock);
         h_unplugged = true;
         h_complete();
         LeaveCriticalSection(&q_lock);
         for (i = 0; i < 2000 && !retro_atomic_load_acquire_int(&dev->gone); i++)
            Sleep(1);
         winusb_hid.poll(hid);
         h_unplugged = false;
      }
      product_name = "Generic Pad";
   }
   generic_mode = false;

   /* freed with a write in flight: the write is cancelled, the reader
    * stopped. This one is on Bluetooth: the service's id is in its
    * instance. */
   caps_in = 78; caps_out = 78; caps_id = 1;
   winusb_hid_open(hid, path,
         "HID\\{00001124-0000-1000-8000-00805F9B34FB}_VID&0002054C_PID&0CE6\\2",
         0x054c, 0x0ce6, true, false);
   CHECK(hid->devices && winusb_hid_is_bluetooth(hid->devices),
         "hid class: a pad on Bluetooth is not said to be");
   r[0] = 0x01;
   write_hold = true;
   push_hreport(r, 78);
   wait_hreader();
   winusb_hid.poll(hid);
   cancels = 0;
   CHECK(hid->devices && hid->devices->out_busy, "hid class: no write in flight to free with");
   winusb_hid.free(hid);
   CHECK(cancels >= 2 && !write_pending,
         "hid class: free did not cancel the read and the write in flight");
   write_hold = false;
   firmware   = 0;
}

/* ---- the Switch 2 Pro Controller's bulk pipes ---- */

static uint8_t w2_cmd[16][32];
static ULONG w2_len[16];
static int w2_writes, w2_reads, w2_aborts;
static bool w2_silent;     /* answers never come */
static OVERLAPPED *w2_pending;

static BOOL WINAPI w2_write(void *usb, UCHAR pipe, UCHAR *buf, ULONG len,
      ULONG *got, OVERLAPPED *ov)
{
   int i = w2_writes++ & 15;
   memcpy(w2_cmd[i], buf, len < 32 ? len : 32);
   w2_len[i] = len;
   if (usb != (void*)0x5678 || pipe != 0x02)
      w2_len[i] = 0;
   ov->InternalHigh = len;
   SetEvent(ov->hEvent);
   return TRUE;
}

static BOOL WINAPI w2_read(void *usb, UCHAR pipe, UCHAR *buf, ULONG len,
      ULONG *got, OVERLAPPED *ov)
{
   w2_reads++;
   if (w2_silent)
   {
      w2_pending = ov;
      SetLastError(ERROR_IO_PENDING);
      return FALSE;
   }
   memset(buf, 0, len);
   ov->InternalHigh = 8;
   SetEvent(ov->hEvent);
   return TRUE;
}

static BOOL WINAPI w2_abort(void *usb, UCHAR pipe)
{
   w2_aborts++;
   if (w2_pending)
   {
      w2_pending->Internal = 1; /* failed */
      SetEvent(w2_pending->hEvent);
      w2_pending = NULL;
   }
   return TRUE;
}

static BOOL WINAPI w2_result(void *usb, OVERLAPPED *ov, ULONG *got, BOOL wait)
{
   if (wait)
      WaitForSingleObject(ov->hEvent, INFINITE);
   *got = (ULONG)ov->InternalHigh;
   return ov->Internal == 0;
}

static void test_switch2_wake(void)
{
   winusb_api_t saved = winusb_api;
   winusb_device_t *dev = (winusb_device_t*)calloc(1, sizeof(*dev));
   int i, whole = 0;

   winusb_api.WritePipe           = w2_write;
   winusb_api.ReadPipe            = w2_read;
   winusb_api.AbortPipe           = w2_abort;
   winusb_api.GetOverlappedResult = w2_result;
   dev->quit_event = CreateEventA(NULL, TRUE, FALSE, NULL);
   dev->ctrl_event = CreateEventA(NULL, TRUE, FALSE, NULL);
   dev->bulk_usb   = (void*)0x5678;
   dev->bulk_out   = 0x02;
   dev->bulk_in    = 0x82;
   dev->slot       = 1;
   strlcpy(dev->name, "Nintendo Switch 2 Pro Controller", sizeof(dev->name));

   winusb_switch2_wake(dev);
   for (i = 0; i < w2_writes && i < 16; i++)
      if (w2_len[i] == (ULONG)w2_cmd[i][5] + 8 && w2_cmd[i][1] == 0x91)
         whole++;
   CHECK(w2_writes == 10 && whole == 10,
         "switch 2: not every wake command went out whole on the pipe out");
   CHECK(w2_reads == 10 && w2_aborts == 0, "switch 2: an answer not read");
   CHECK(w2_cmd[0][0] == 0x07 && w2_cmd[9][0] == 0x03 && w2_cmd[9][3] == 0x0d,
         "switch 2: the wake is not in its order, reports started last");
   winusb_switch2_lights(dev, 2);
   CHECK(w2_writes == 11 && w2_len[10] == 16 && w2_cmd[10][0] == 0x09
         && w2_cmd[10][3] == 0x07 && w2_cmd[10][8] == 0x03,
         "switch 2: player 2's lights not set");

   /* a pad that never answers: each wait given up on, the wake done */
   w2_writes = w2_reads = 0;
   w2_silent = true;
   winusb_switch2_wake(dev);
   CHECK(w2_writes == 10 && w2_aborts == 10,
         "switch 2: an answer that never came held the wake up");
   w2_silent = false;

   CloseHandle(dev->quit_event);
   CloseHandle(dev->ctrl_event);
   free(dev);
   winusb_api = saved;
}

/* ---- the Wii U GameCube adapter on WinUSB ---- */

/* its pipes where the stand-in puts them, to be looked up, not assumed */
#define GA_IN  0x83
#define GA_OUT 0x04
static uint8_t ga_out[8][40];
static ULONG ga_out_len[8];
static UCHAR ga_out_pipe[8];
static int ga_writes;
static UCHAR ga_read_pipe;
static bool ga_in_aborted;
static OVERLAPPED *ga_read_ov;

static BOOL WINAPI ga_iface(void *usb, UCHAR alt, void *d)
{
   winusb_interface_desc_t *desc = (winusb_interface_desc_t*)d;
   memset(desc, 0, sizeof(*desc));
   desc->bNumEndpoints = 2;
   return TRUE;
}

static BOOL WINAPI ga_pipe(void *usb, UCHAR alt, UCHAR i, void *p)
{
   winusb_pipe_info_t *pipe = (winusb_pipe_info_t*)p;
   memset(pipe, 0, sizeof(*pipe));
   pipe->PipeType          = WINUSB_PIPE_INTERRUPT;
   pipe->PipeId            = i ? GA_OUT : GA_IN;
   pipe->MaximumPacketSize = 37;
   return i < 2;
}

static BOOL WINAPI ga_write(void *usb, UCHAR pipe, UCHAR *buf, ULONG len,
      ULONG *got, OVERLAPPED *ov)
{
   int i = ga_writes++ & 7;
   memcpy(ga_out[i], buf, len < 40 ? len : 40);
   ga_out_len[i]  = len;
   ga_out_pipe[i] = pipe;
   SetEvent(ov->hEvent);
   return TRUE;
}

/* reads wait until aborted */
static BOOL WINAPI ga_read(void *usb, UCHAR pipe, UCHAR *buf, ULONG len,
      ULONG *got, OVERLAPPED *ov)
{
   ga_read_pipe = pipe;
   ga_read_ov   = ov;
   SetLastError(ERROR_IO_PENDING);
   return FALSE;
}

static BOOL WINAPI ga_abort(void *usb, UCHAR pipe)
{
   if (pipe == GA_IN)
      ga_in_aborted = true;
   if (ga_read_ov && pipe == GA_IN)
      SetEvent(ga_read_ov->hEvent);
   return TRUE;
}

static BOOL WINAPI ga_result(void *usb, OVERLAPPED *ov, ULONG *got, BOOL wait)
{
   *got = 0;
   if (ov == ga_read_ov)
   {
      if (wait)
         WaitForSingleObject(ov->hEvent, INFINITE);
      SetLastError(ERROR_OPERATION_ABORTED);
      return FALSE;
   }
   return TRUE;
}

static void test_gc_adapter(const char *path)
{
   winusb_api_t saved = winusb_api;
   winusb_hid_t *hid;
   int i, setups_before;
   bool started = false;

   winusb_api.Initialize             = f_initialize;
   winusb_api.Free                   = f_free;
   winusb_api.ControlTransfer        = f_control;
   winusb_api.ReadPipe               = ga_read;
   winusb_api.WritePipe              = ga_write;
   winusb_api.AbortPipe              = ga_abort;
   winusb_api.GetOverlappedResult    = ga_result;
   winusb_api.QueryInterfaceSettings = ga_iface;
   winusb_api.QueryPipe              = ga_pipe;

   CHECK(winusb_hid_handled(0x057e, 0x0337),
         "gc adapter: not taken through WinUSB");
   CHECK(winusb_hid_handled(0x054c, 0x0268),
         "gc adapter: the DualShock 3 is no longer taken through WinUSB");

   hid = (winusb_hid_t*)winusb_hid.init();
   CHECK(hid != NULL, "gc adapter: init failed");
   if (!hid)
   {
      winusb_api = saved;
      return;
   }
   setups_before = n_setups;
   winusb_hid_open(hid, path, "USB\\VID_057E&PID_0337\\1", 0x057e, 0x0337,
         false, false);
   CHECK(hid->devices != NULL, "gc adapter: not opened");
   if (hid->devices)
   {
      winusb_device_t *dev = hid->devices;
      CHECK(dev->ep_in == GA_IN && dev->ep_out == GA_OUT,
            "gc adapter: its interrupt pipes were not the ones it has");
      /* the start, report 0x13 alone, on the OUT pipe */
      for (i = 0; i < ga_writes && i < 8; i++)
         if (ga_out_pipe[i] == GA_OUT && ga_out_len[i] == 1
               && ga_out[i][0] == 0x13)
            started = true;
      CHECK(started, "gc adapter: 0x13 did not go out on its OUT pipe");
      for (i = setups_before; i < n_setups; i++)
         CHECK(setups[i & 31].RequestType != 0x21,
               "gc adapter: output went as SET_REPORT on the control pipe");
      /* read from its IN pipe, and that read stopped on its way out */
      for (i = 0; i < 200 && !ga_read_pipe; i++)
         Sleep(5);
      CHECK(ga_read_pipe == GA_IN, "gc adapter: not read from its IN pipe");
   }
   winusb_hid.free(hid);
   CHECK(ga_in_aborted, "gc adapter: its read was not stopped at free");
   winusb_api = saved;
}

/* An original Xbox pad, by a maker the table does not know: opened as
 * the XID handler's, read from its IN pipe, written to on its OUT pipe;
 * nothing sent to start it. */
static void test_xid_pad(const char *path)
{
   winusb_api_t saved = winusb_api;
   winusb_hid_t *hid;
   int writes_before;

   winusb_api.Initialize             = f_initialize;
   winusb_api.Free                   = f_free;
   winusb_api.ControlTransfer        = f_control;
   winusb_api.ReadPipe               = ga_read;
   winusb_api.WritePipe              = ga_write;
   winusb_api.AbortPipe              = ga_abort;
   winusb_api.GetOverlappedResult    = ga_result;
   winusb_api.QueryInterfaceSettings = ga_iface;
   winusb_api.QueryPipe              = ga_pipe;

   CHECK(!winusb_hid_handled(0x0738, 0x4522),
         "xid: a third-party pad is in the table, so this lane proves nothing");
   hid = (winusb_hid_t*)winusb_hid.init();
   CHECK(hid != NULL, "xid: init failed");
   if (!hid)
   {
      winusb_api = saved;
      return;
   }
   ga_read_pipe  = 0;
   ga_in_aborted = false;
   writes_before = ga_writes;
   winusb_hid_open(hid, path, "USB\\VID_0738&PID_4522\\1", 0x0738, 0x4522,
         false, true);
   CHECK(hid->devices != NULL, "xid: not opened");
   if (hid->devices)
   {
      winusb_device_t *dev = hid->devices;
      int i;
      CHECK(string_is_equal(dev->name, "Original Xbox Controller"),
            "xid: not named as the XID handler names it");
      CHECK(dev->slot >= 0, "xid: given no slot");
      CHECK(dev->ep_in == GA_IN && dev->ep_out == GA_OUT,
            "xid: its interrupt pipes were not the ones it has");
      CHECK(ga_writes == writes_before, "xid: something was sent to start it");
      for (i = 0; i < 200 && !ga_read_pipe; i++)
         Sleep(5);
      CHECK(ga_read_pipe == GA_IN, "xid: not read from its IN pipe");
   }
   winusb_hid.free(hid);
   CHECK(ga_in_aborted, "xid: its read was not stopped at free");
   winusb_api = saved;
}

int main(void)
{
   char path[MAX_PATH], dir[MAX_PATH];
   winusb_hid_t *hid;
   winusb_device_t *dev;
   input_bits_t bits;
   uint8_t addr[6];
   static const uint8_t host[6] = { 0x00, 0x1a, 0x7d, 0xda, 0x71, 0x13 };
   const uint32_t cross  = 1u << 14;
   const uint32_t circle = 1u << 13;
   int i, f4 = 0, f2 = 0, f5 = 0, led = 0;
   sthread_t *closer;
   HANDLE stop_events[2];
   bool stopped;

   InitializeCriticalSection(&q_lock);
   submit_entered = CreateEventA(NULL, TRUE, FALSE, NULL);
   allow_submit   = CreateEventA(NULL, TRUE, FALSE, NULL);
   close_done     = CreateEventA(NULL, TRUE, FALSE, NULL);
   abort_seen     = CreateEventA(NULL, TRUE, FALSE, NULL);

   winusb_api.Initialize          = f_initialize;
   winusb_api.Free                = f_free;
   winusb_api.ControlTransfer     = f_control;
   winusb_api.ReadPipe            = f_read;
   winusb_api.AbortPipe           = f_abort;
   winusb_api.GetOverlappedResult = f_result;

   /* a file stands for the pad's interface */
   GetTempPathA(sizeof(dir), dir);
   GetTempFileNameA(dir, "wu", 0, path);

   hid = (winusb_hid_t*)winusb_hid.init();
   CHECK(hid != NULL, "init failed");
   if (!hid)
      return 1;

   /* held by another program: left to it, said once, taken when free */
   {
      HANDLE other = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      CHECK(other != INVALID_HANDLE_VALUE, "the stand-in could not be held");
      winusb_hid_open(hid, path, "USB\\VID_054C&PID_0268\\1", 0x054c, 0x0268,
            false, false);
      winusb_hid_open(hid, path, "USB\\VID_054C&PID_0268\\1", 0x054c, 0x0268,
            false, false);
      CHECK(!hid->devices && connected_port == -1 && hid->busy_count == 1,
            "a pad another program has open was not left to it, once");
      CloseHandle(other);
   }
   winusb_hid_open(hid, path, "USB\\VID_054C&PID_0268\\1", 0x054c, 0x0268,
            false, false);
   CHECK(hid->devices && connected_port == 0, "the pad is not on port 1");
   CHECK(hid->busy_count == 0, "the pad was still counted as held elsewhere");
   dev = hid->devices;

   /* start */
   for (i = 0; i < n_setups && i < 32; i++)
   {
      if (setups[i].RequestType == 0x21 && setups[i].Value == 0x0201
            && setup_data[i][0] == 0x01 && setup_data[i][10] == 0x02)
         led++;
      if (setups[i].RequestType == 0x21 && setups[i].Value == 0x03f4
            && setup_data[i][0] == 0xf4 && setup_data[i][1] == 0x42)
         f4++;
      if (     setups[i].RequestType == 0xa1 && setups[i].Request == 0x01
            && setups[i].Value == 0x03f2 && setups[i].Length == 17)
         f2++;
      if (setups[i].RequestType == 0xa1 && setups[i].Value == 0x03f5)
         f5++;
   }
   CHECK(led == 1, "the LED report for port 1 was not sent as SET_REPORT");
   CHECK(f4 == 1, "0xF4 was not tried");
   CHECK(f2 == 1 && f5 == 1, "0xF2 and 0xF5 were not read");

   /* the newest report is the state */
   push_report(cross, 0);
   push_report(cross | circle, 0);
   push_report(circle, 200);
   wait_reader();
   winusb_hid.poll(hid);
   winusb_hid.get_buttons(hid, 0, &bits);
   CHECK(   BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_A)
         && !BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_B),
         "the state is not the newest report's");
   CHECK(winusb_hid.axis(hid, 0, AXIS_POS(7)) == ((200 << 7) | (200 >> 1)),
         "circle's pressure is not axis 7");
   /* nothing new: the state stays */
   winusb_hid.poll(hid);
   CHECK(winusb_hid.button(hid, 0, RETRO_DEVICE_ID_JOYPAD_A),
         "the state was lost with nothing new");
   /* and again, through all three buffers */
   for (i = 0; i < 5; i++)
   {
      push_report((i & 1) ? cross : circle, 0);
      wait_reader();
      winusb_hid.poll(hid);
      CHECK(winusb_hid.button(hid, 0, (i & 1)
               ? RETRO_DEVICE_ID_JOYPAD_B : RETRO_DEVICE_ID_JOYPAD_A),
            "a later report did not come through");
   }

   /* bluetooth */
   CHECK(winusb_hid.set_bt_host(hid, 0, host), "the address could not be set");
   CHECK(winusb_hid.get_bt_host(hid, 0, addr) && !memcmp(addr, host, 6),
         "the address set is not the one read");
   CHECK(!winusb_hid.get_bt_host(hid, 1, addr),
         "a port with no pad has an address");

   /* gone */
   EnterCriticalSection(&q_lock);
   unplugged = 1;
   complete_read();
   LeaveCriticalSection(&q_lock);
   for (i = 0; i < 2000 && !retro_atomic_load_acquire_int(&dev->gone); i++)
      Sleep(1);
   winusb_hid.poll(hid);
   CHECK(disconnects == 1 && !hid->devices, "the pad was not disconnected");
   CHECK(frees == 1, "the WinUSB handle was not freed");
   CHECK(!winusb_hid.button(hid, 0, RETRO_DEVICE_ID_JOYPAD_A),
         "the port still reads the pad");

   /* back, then the driver freed with it: the reader is aborted */
   unplugged = 0;
   InterlockedExchange(&pause_submit, 1);
   winusb_hid_open(hid, path, "USB\\VID_054C&PID_0268\\1", 0x054c, 0x0268,
            false, false);
   CHECK(hid->devices != NULL, "the pad did not come back");
   CHECK(WaitForSingleObject(submit_entered, 2000) == WAIT_OBJECT_0,
         "the reader did not reach the submission gate");
   dev = hid->devices;
   closer = sthread_create(close_driver, hid);
   stop_events[0] = dev->quit_event;
   stop_events[1] = abort_seen;
   CHECK(WaitForMultipleObjects(2, stop_events, FALSE, 2000) != WAIT_TIMEOUT,
         "close did not signal the reader to stop");
   CHECK(retro_atomic_load_acquire_int(&dev->quit),
         "close did not request the reader to stop");
   SetEvent(allow_submit);
   stopped = WaitForSingleObject(close_done, 2000) == WAIT_OBJECT_0;
   CHECK(stopped, "a read submitted after shutdown left close waiting");
   if (!stopped)
      f_abort((void*)0x1234, WINUSB_EP_IN);
   sthread_join(closer);
   /* let go, and not said to have left: it is still plugged in */
   CHECK(aborts >= 1 && frees == 2 && disconnects == 1,
         "free did not stop the reader and let the pad go quietly");

   test_hidclass(path);
   test_switch2_wake();
   test_gc_adapter(path);
   test_xid_pad(path);

   CloseHandle(submit_entered);
   CloseHandle(allow_submit);
   CloseHandle(close_done);
   CloseHandle(abort_seen);
   DeleteCriticalSection(&q_lock);
   DeleteFileA(path);
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("[pass] winusb_hid_test\n");
   return 0;
}
