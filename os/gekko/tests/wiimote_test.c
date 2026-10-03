/* Wii Remote checks, run in Dolphin by run-dolphin.sh, whose emulated
 * remote 1 has a Nunchuk (or with WIIEXT=Classic a Classic
 * Controller, WIIEXT=Guitar a guitar): the remote connects over
 * Bluetooth to slot 0, reports continuously with the accelerometer at
 * rest, the extension is found with its sticks centred, the pointer
 * follows the remote (run with WMPADS=wiimote-ir.pipe, which turns it
 * right and up, then holds the guitar's green fret, strums down, pushes
 * the whammy bar and the stick up), and the remote goes when
 * dropped. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gekko/power.h>
#include <gekko/thread.h>
#include <gekko/wiimote.h>

static unsigned failures;

#define CHECK(cond, what) \
   do { \
      if (cond) \
         gk_debug_printf("ok   %s", what); \
      else \
      { \
         gk_debug_printf("FAIL %s", what); \
         failures++; \
      } \
   } while (0)

/* Wait up to ms for cond on slot 0's state. */
#define WAIT(ms, cond) \
   do { \
      unsigned waited_; \
      for (waited_ = 0; waited_ < (ms); waited_ += 20) \
      { \
         gk_wiimote_read(0, &st); \
         if (cond) \
            break; \
         gk_sleep_us(20000); \
      } \
   } while (0)

int main(int argc, char **argv)
{
   gk_wiimote_t st;
   char what[128];
   uint32_t r0;
   uint64_t t0;
   double rate;
   (void)argc;
   (void)argv;

   CHECK(gk_wiimote_init() == 0, "Bluetooth up");
   WAIT(15000, st.connected);
   CHECK(st.connected, "remote 1 connected to slot 0");
   WAIT(5000, st.ext != GK_WM_EXT_NONE && st.reports > 10);
   snprintf(what, sizeof(what), "extension found (%u), battery %u",
         st.ext, st.battery);
   CHECK((st.ext == GK_WM_EXT_NUNCHUK || st.ext == GK_WM_EXT_CLASSIC
         || st.ext == GK_WM_EXT_GUITAR) && st.battery > 0, what);

   gk_wiimote_read(0, &st);
   r0 = st.reports;
   t0 = gk_ticks();
   gk_sleep_us(1000000);
   gk_wiimote_read(0, &st);
   rate = (st.reports - r0) * (double)gk_tb_hz / (double)(gk_ticks() - t0);
   snprintf(what, sizeof(what), "%.0f reports a second", rate);
   CHECK(rate > 50, what);

   snprintf(what, sizeof(what), "at rest: accel %u %u %u, buttons %04x",
         st.accel[0], st.accel[1], st.accel[2], st.buttons);
   CHECK(st.accel[2] > st.accel[0] + 30 && st.accel[2] > st.accel[1] + 30
         && abs((int)st.accel[0] - 512) < 40 && !st.buttons, what);
   snprintf(what, sizeof(what), "extension at rest: sticks %d %d %d %d, "
         "triggers %u %u, buttons %x", st.stick[0][0], st.stick[0][1],
         st.stick[1][0], st.stick[1][1], st.trigger[0], st.trigger[1],
         st.ext_buttons);
   CHECK(abs(st.stick[0][0]) < 16 && abs(st.stick[0][1]) < 16
         && abs(st.stick[1][0]) < 16 && abs(st.stick[1][1]) < 16
         && st.trigger[0] < 16 && st.trigger[1] < 16 && !st.ext_buttons,
         what);

   /* The pointer, at the centre and then where WMPADS turns it. */
   WAIT(3000, st.ir_dots >= 2);
   snprintf(what, sizeof(what), "IR: %u dots, pointer %d %d, %s",
         st.ir_dots, st.ir_x, st.ir_y, st.ir_valid ? "on screen" : "off");
   CHECK(st.ir_dots >= 2 && st.ir_valid && abs(st.ir_x) < 2000
         && abs(st.ir_y) < 2000, what);
   WAIT(15000, st.ir_x > 16000 && abs(st.ir_y) < 4000);
   snprintf(what, sizeof(what), "IR: pointed right, %d %d", st.ir_x,
         st.ir_y);
   CHECK(st.ir_x > 16000 && abs(st.ir_y) < 4000, what);
   WAIT(10000, st.ir_y < -16000 && abs(st.ir_x) < 4000);
   snprintf(what, sizeof(what), "IR: pointed up, %d %d", st.ir_x, st.ir_y);
   CHECK(st.ir_y < -16000 && abs(st.ir_x) < 4000, what);
   WAIT(10000, abs(st.ir_x) < 2000 && abs(st.ir_y) < 2000);
   CHECK(abs(st.ir_x) < 2000 && abs(st.ir_y) < 2000, "IR: back at the centre");

   if (st.ext == GK_WM_EXT_GUITAR)
   {
      WAIT(10000, st.ext_buttons == (GK_GH_GREEN | GK_GH_DOWN)
            && st.trigger[1] > 240 && st.stick[0][1] > 100);
      snprintf(what, sizeof(what), "guitar: buttons %x, whammy %u, "
            "stick %d %d", st.ext_buttons, st.trigger[1], st.stick[0][0],
            st.stick[0][1]);
      CHECK(st.ext_buttons == (GK_GH_GREEN | GK_GH_DOWN)
            && st.trigger[1] > 240 && st.stick[0][1] > 100
            && abs(st.stick[0][0]) < 16, what);
   }

   gk_wiimote_rumble(0, 1);
   gk_sleep_us(100000);
   gk_wiimote_rumble(0, 0);
   gk_sleep_us(100000);
   gk_wiimote_read(0, &st);
   r0 = st.reports;
   gk_sleep_us(200000);
   gk_wiimote_read(0, &st);
   CHECK(st.reports > r0, "still reporting after rumble");

   gk_wiimote_disconnect(0);
   WAIT(3000, !st.connected);
   CHECK(!st.connected, "dropped");
   CHECK(gk_wiimote_read(1, &st) != 0, "slot 1 empty");

   gk_debug_printf("%s (%u failure(s))", failures ? "FAILED" : "PASSED",
         failures);
   gk_power_off();
   return 0;
}
