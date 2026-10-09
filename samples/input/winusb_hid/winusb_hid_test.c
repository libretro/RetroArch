/* Regression test for input/drivers_hid/winusb_hid.c: the WinUSB HID
 * driver, included here, with the real pad handlers behind it and a
 * stand-in for winusb.dll put in its table before it starts.
 *
 *   start       -> a DualShock 3 is started on the control pipe: its LED
 *                  report written, 0xF4 set, 0xF2 and 0xF5 read, as HID
 *                  class requests with the report's type and ID
 *   poll        -> the newest report the reader has had is the pad's
 *                  state, taken without a call into the driver
 *   bluetooth   -> the address the pad connects to is feature 0xF5
 *   gone        -> a read failing disconnects the pad and frees its port
 *   free        -> the reader stopped through the pipe's abort, joined,
 *                  and everything given back */

#include "input/drivers_hid/winusb_hid.c"

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
   winusb_hid_open(hid, path, "USB\\VID_054C&PID_0268\\1", 0x054c, 0x0268);
   CHECK(hid->devices && connected_port == 0, "the pad is not on port 1");
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
   CHECK(f4 == 1, "0xF4 was not set");
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
   winusb_hid_open(hid, path, "USB\\VID_054C&PID_0268\\1", 0x054c, 0x0268);
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
   CHECK(aborts >= 1 && frees == 2 && disconnects == 2,
         "free did not stop the reader and let the pad go");

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
