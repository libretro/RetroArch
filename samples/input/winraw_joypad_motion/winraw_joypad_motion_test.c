/* winraw joypad: a DualShock 4's or a DualSense's motion sensors.
 *
 * Raw input reads these pads by their reports' HID description, which
 * covers the sticks and buttons. The gyroscope and the accelerometer
 * are in the same report, in a part the description gives no meaning
 * to (input/common/sony_pad_motion.h reads them from it). The driver
 * now gives them to a core that asks: it reads them out of a pad's
 * reports while a core wants them, and only then.
 *
 * The driver is included whole, with its raw input and HID calls
 * routed to fake controllers; reports are handed to its parser as raw
 * input would hand them. That the values are read from the right
 * places and come out in the right units is
 * samples/input/sony_pad_motion's to hold.
 *
 * Checked here:
 *
 * - a core's request is taken for a DualSense and a DualShock 4, and
 *   refused for a pad of another make and for a port with no pad;
 * - nothing is given before a report has had the sensors in it, and
 *   only the sensor that was asked for is given;
 * - a pad nothing was asked of has no sensors read out of its reports;
 * - a short report, as over Bluetooth, changes nothing;
 * - a pad unplugged and plugged in again goes on giving the core what
 *   it asked for, without being asked again;
 * - a sensor a core is done with is given no more. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>

#include <boolean.h>

/* ---- The fake controllers ------------------------------------------- */

#define FAKE_MAX 8
static struct { HANDLE h; bool alive; unsigned vid, pid; char path[64]; } fake[FAKE_MAX];
static unsigned fake_n;

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

/* The reports' HID description says nothing here: no buttons and no
 * values, as far as the parser is concerned. The sensors are not in
 * it on a real pad either. */
static NTSTATUS NTAPI fake_HidP_GetCaps(PHIDP_PREPARSED_DATA pp,
      PHIDP_CAPS caps)
{
   (void)pp;
   memset(caps, 0, sizeof(*caps));
   return HIDP_STATUS_SUCCESS;
}
static NTSTATUS NTAPI fake_HidP_GetUsages(HIDP_REPORT_TYPE t, USAGE page,
      USHORT link, PUSAGE list, PULONG len, PHIDP_PREPARSED_DATA pp,
      PCHAR report, ULONG report_len)
{
   (void)t; (void)page; (void)link; (void)list; (void)pp; (void)report; (void)report_len;
   *len = 0;
   return HIDP_STATUS_SUCCESS;
}

#define GetRawInputDeviceInfoA fake_GetRawInputDeviceInfoA
#define HidP_GetCaps           fake_HidP_GetCaps
#define HidP_GetUsages         fake_HidP_GetUsages
/* the paths are not files: no product string, and no rumble */
#define CreateFileA(a,b,c,d,e,f,g) INVALID_HANDLE_VALUE

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

static unsigned failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

#define H(n) ((HANDLE)(uintptr_t)(0x1000 + (n)))

static void put16(uint8_t *p, int v)
{
   p[0] = (uint8_t)(v & 0xFF);
   p[1] = (uint8_t)((v >> 8) & 0xFF);
}

/* a DualSense's USB report: the accelerometer's Y at @ay, the
 * gyroscope's X at @gx */
static void report_ds5(uint8_t *r, int ay, int gx)
{
   memset(r, 0, 64);
   r[0] = 0x01;
   put16(r + 16, gx);
   put16(r + 24, ay);
}

static bool sensor(unsigned port, unsigned id, float *v)
{
   *v = -99.0f;
   return winraw_joypad_joypad_get_sensor_input(port, id, v);
}

static bool close_to(float a, float b) { return fabs(a - b) < 0.001; }

int main(void)
{
   uint8_t r[64];
   float v;
   bool ok;

   fake_plug(H(1), 0x054C, 0x0DF2, "\\\\?\\HID#VID_054C&PID_0DF2#edge"); /* DualSense Edge */
   fake_plug(H(2), 0x054C, 0x09CC, "\\\\?\\HID#VID_054C&PID_09CC#ds4");  /* DualShock 4    */
   fake_plug(H(3), 0x046D, 0xC216, "\\\\?\\HID#VID_046D&PID_C216#pad");  /* another make   */
   winraw_joypad_initialised = true;
   winraw_joypad_add_device(H(1));
   winraw_joypad_add_device(H(2));
   winraw_joypad_add_device(H(3));
   CHECK(winraw_joypad_pads[0].connected && winraw_joypad_pads[1].connected
         && winraw_joypad_pads[2].connected, "the three controllers did not connect");

   /* ---- who has sensors ------------------------------------------- */
   CHECK(winraw_joypad_joypad_set_sensor_state(0, RETRO_SENSOR_ACCELEROMETER_ENABLE, 60),
         "the DualSense's accelerometer was refused");
   CHECK(!winraw_joypad_joypad_set_sensor_state(2, RETRO_SENSOR_ACCELEROMETER_ENABLE, 60),
         "a pad of another make was said to have an accelerometer");
   CHECK(!winraw_joypad_joypad_set_sensor_state(7, RETRO_SENSOR_GYROSCOPE_ENABLE, 60),
         "a port with no pad was said to have a gyroscope");
   CHECK(winraw_joypad_joypad_set_sensor_state(7, RETRO_SENSOR_GYROSCOPE_DISABLE, 60)
         && winraw_joypad_joypad_set_sensor_state(2, RETRO_SENSOR_ILLUMINANCE_DISABLE, 60),
         "being done with a sensor that is not there was a failure");
   winraw_joypad_motion_want[7] = 0;
   printf("   ok   a core's request is taken for a Sony pad, and refused for another make of pad and for a port with none\n");

   /* ---- nothing before a report, and only what was asked ---------- */
   CHECK(!sensor(0, RETRO_SENSOR_ACCELEROMETER_Z, &v), "a value was given before any report had the sensors");
   report_ds5(r, 8192, 90 * 16);   /* flat, and turning about X */
   winraw_joypad_parse_hid_report(&winraw_joypad_pads[0], r, 64);
   ok = sensor(0, RETRO_SENSOR_ACCELEROMETER_Z, &v);
   CHECK(ok && close_to(v, 1.0f), "flat on a table: the accelerometer's Z is %f (given: %d), want 1", v, ok);
   CHECK(!sensor(0, RETRO_SENSOR_GYROSCOPE_X, &v), "the gyroscope was given without being asked for");
   CHECK(winraw_joypad_joypad_set_sensor_state(0, RETRO_SENSOR_GYROSCOPE_ENABLE, 60), "the DualSense's gyroscope was refused");
   ok = sensor(0, RETRO_SENSOR_GYROSCOPE_X, &v);
   CHECK(ok && close_to(v, 1.5708f), "turning 90 degrees a second about X: the gyroscope's X is %f, want pi/2", v);
   printf("   ok   nothing is given before a report has had the sensors, and only the sensor asked for is given\n");

   /* ---- a pad nothing was asked of -------------------------------- */
   winraw_joypad_parse_hid_report(&winraw_joypad_pads[1], r, 64);
   CHECK(!winraw_joypad_pads[1].motion_seen && !sensor(1, RETRO_SENSOR_ACCELEROMETER_Z, &v),
         "a pad nothing was asked of had its sensors read");

   /* ---- a short report -------------------------------------------- */
   memset(r, 0, sizeof(r));
   r[0] = 0x01;
   winraw_joypad_parse_hid_report(&winraw_joypad_pads[0], r, 10);
   ok = sensor(0, RETRO_SENSOR_ACCELEROMETER_Z, &v);
   CHECK(ok && close_to(v, 1.0f), "a short report changed the accelerometer's Z to %f", v);
   printf("   ok   a pad nothing was asked of has no sensors read; a short report, as over Bluetooth, changes nothing\n");

   /* ---- unplugged and plugged in again ---------------------------- */
   winraw_joypad_remove_device(H(1));
   CHECK(!sensor(0, RETRO_SENSOR_ACCELEROMETER_Z, &v), "an unplugged pad still gave a value");
   winraw_joypad_add_device(H(1));
   CHECK(winraw_joypad_pads[0].connected && winraw_joypad_pads[0].motion_on == 3,
         "the pad came back to slot 0 with %u wanted of it, want both sensors",
         winraw_joypad_pads[0].motion_on);
   report_ds5(r, -8192, 0);        /* face down */
   winraw_joypad_parse_hid_report(&winraw_joypad_pads[0], r, 64);
   ok = sensor(0, RETRO_SENSOR_ACCELEROMETER_Z, &v);
   CHECK(ok && close_to(v, -1.0f), "after coming back, face down: the accelerometer's Z is %f (given: %d), want -1", v, ok);
   printf("   ok   a pad unplugged and plugged in again goes on giving the core what it asked for\n");

   /* ---- done with a sensor ---------------------------------------- */
   CHECK(winraw_joypad_joypad_set_sensor_state(0, RETRO_SENSOR_ACCELEROMETER_DISABLE, 0), "being done with the accelerometer was a failure");
   CHECK(!sensor(0, RETRO_SENSOR_ACCELEROMETER_Z, &v), "the accelerometer was given after the core was done with it");
   CHECK(sensor(0, RETRO_SENSOR_GYROSCOPE_X, &v), "the gyroscope was taken away with the accelerometer");
   winraw_joypad_joypad_set_sensor_state(0, RETRO_SENSOR_GYROSCOPE_DISABLE, 0);
   CHECK(!sensor(0, RETRO_SENSOR_GYROSCOPE_X, &v) && !winraw_joypad_pads[0].motion_on,
         "the gyroscope was given after the core was done with it");
   printf("   ok   a sensor a core is done with is given no more\n");

   winraw_joypad_joypad_destroy();
   if (failures)
   {
      printf("FAIL winraw_joypad_motion_test: %u\n", failures);
      return 1;
   }
   printf("PASS winraw_joypad_motion_test\n");
   return 0;
}
