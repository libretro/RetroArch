/* A DualShock 3's evdev node has no button pressures; the udev joypad
 * driver reads them from the pad's hidraw node, beside it, into the
 * axes after the pad's own: cross, circle, square, triangle, L1, R1,
 * up, down, left, right, as SDL orders them. The real driver is
 * included and the hidraw node is a socket that keeps each report
 * whole.
 *
 *   reports      -> the newest waiting sets the pressures, 0 to 0x7fff
 *   other IDs    -> a report that is not the input report is passed by
 *   none waiting -> the pressures stay
 *   node gone    -> closed, the pressures back at rest, the pad's own
 *                   axes untouched */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

#include <boolean.h>

#include "input/drivers_joypad/udev_joypad.c"

/* ---- what the driver links against --------------------------------- */

static settings_t stub_settings;
settings_t *config_get_ptr(void) { return &stub_settings; }
#include "../input_config_stubs.h"
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port, unsigned vid,
      unsigned pid)
{ return true; }
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{ return true; }
const char *input_config_get_device_name(unsigned port) { return NULL; }

/* ---- the test ------------------------------------------------------- */

static unsigned failures;
#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

static int hidraw_peer = -1;

static void send_report(uint8_t id, uint8_t cross, uint8_t up, uint8_t r1)
{
   uint8_t r[49];
   memset(r, 0, sizeof(r));
   r[0]  = id;
   r[24] = cross;
   r[14] = up;
   r[21] = r1;
   if (write(hidraw_peer, r, sizeof(r)) != sizeof(r))
      exit(2);
}

/* axis i of the ten, through the driver's axis read */
static int16_t pressure(unsigned i)
{
   return udev_joypad_axis(0, AXIS_POS(6 + i));
}

int main(void)
{
   int sv[2], evdev[2];
   unsigned axes = 6;
   struct udev_joypad *pad = &udev_pads[0];

   if (     socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv)
         || pipe(evdev))
      return 2;
   fcntl(sv[0], F_SETFL, fcntl(sv[0], F_GETFL) | O_NONBLOCK);
   fcntl(evdev[0], F_SETFL, fcntl(evdev[0], F_GETFL) | O_NONBLOCK);
   hidraw_peer = sv[1];

   /* a pad with six axes of its own, the left stick pushed right */
   memset(pad, 0, sizeof(*pad));
   pad->sensor_fd = -1;
   pad->axes[0]   = 20000;
   udev_pad_set_fd(0, evdev[0]);
   udev_pressure_attach(pad, sv[0], &axes);
   CHECK(axes == 16 && pad->pressure_axis == 6,
         "the pressures are not axes 6 to 15");

   send_report(0x01, 10, 0, 0);
   send_report(0x01, 255, 128, 0);
   send_report(0x01, 200, 0, 64);
   udev_joypad_poll();
   CHECK(pressure(0) == ((200 << 7) | (200 >> 1)),
         "cross is not the newest report's (%d)", pressure(0));
   CHECK(pressure(6) == 0, "up is not the newest report's");
   CHECK(pressure(5) == ((64 << 7) | (64 >> 1)), "R1 is not the newest report's");

   send_report(0x01, 255, 255, 0);
   udev_joypad_poll();
   CHECK(pressure(0) == 0x7fff && pressure(6) == 0x7fff,
         "a full press is not 0x7fff");

   /* another report: passed by, after an input report too */
   send_report(0xef, 0, 0, 0);
   udev_joypad_poll();
   CHECK(pressure(0) == 0x7fff, "a report of another ID was taken");
   send_report(0x01, 100, 0, 0);
   send_report(0xef, 0, 0, 0);
   send_report(0xef, 0, 0, 0);
   udev_joypad_poll();
   CHECK(pressure(0) == ((100 << 7) | (100 >> 1)),
         "reports of another ID overwrote the input report (%d)", pressure(0));
   send_report(0x01, 255, 255, 0);
   udev_joypad_poll();

   /* none waiting */
   udev_joypad_poll();
   CHECK(pressure(0) == 0x7fff, "the pressures were lost with nothing waiting");

   /* gone */
   close(sv[1]);
   udev_joypad_poll();
   CHECK(!pad->pressure, "a node that is gone is still read");
   CHECK(pressure(0) == 0 && pressure(6) == 0,
         "the pressures did not come back to rest");
   CHECK(udev_joypad_axis(0, AXIS_POS(0)) == 20000,
         "the pad's own axis was touched");
   CHECK(fcntl(sv[0], F_GETFD) < 0, "the node was not closed");

   udev_pad_set_fd(0, -1);
   close(evdev[0]);
   close(evdev[1]);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] udev_ds3_pressure_test\n");
   return 0;
}
