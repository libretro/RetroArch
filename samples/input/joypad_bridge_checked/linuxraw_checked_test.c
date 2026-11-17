/* The linuxraw joypad driver, held to what the snapshot bridge assumes
 * of a driver it serves (see bridge_view.h). The driver source is
 * included for its pad table. Its pads are filled as its poll fills
 * them: from js_event records written down a pipe and read by the
 * driver's own linuxraw_poll_pad(), so what the bridge is lent is what
 * the kernel's events said. */
#include <fcntl.h>
#include <unistd.h>

#include "../../../input/drivers_joypad/linuxraw_joypad.c"

#include "bridge_view.h"

int main(void)
{
   unsigned n, i, bad = 0;
   int fds[2];
   uint32_t want_buttons[3] = { 0, 0, 0 };
   int16_t  want_axes[3][NUM_AXES];

   memset(want_axes, 0, sizeof(want_axes));
   if (pipe(fds) != 0)
      return 1;
   fcntl(fds[0], F_SETFL, O_NONBLOCK);

   for (n = 0; n < 4000; n++)
   {
      unsigned p                  = n % 3;
      struct linuxraw_joypad *pad = &linuxraw_pads[p];
      unsigned events             = 1 + bv_rand() % 12;

      /* a few events, as the kernel sends them, some past the 32
       * buttons and 32 axes the driver keeps */
      for (i = 0; i < events; i++)
      {
         struct js_event e;
         memset(&e, 0, sizeof(e));
         e.number = (uint8_t)(bv_rand() % 40);
         if (bv_rand() & 1)
         {
            e.type  = JS_EVENT_BUTTON;
            e.value = (int16_t)(bv_rand() & 1);
            if (e.number < NUM_BUTTONS)
               want_buttons[p] = e.value
                  ? (want_buttons[p] |  (1u << e.number))
                  : (want_buttons[p] & ~(1u << e.number));
         }
         else
         {
            e.type  = JS_EVENT_AXIS;
            e.value = bv_rand_axis();
            if (e.number < NUM_AXES)
               want_axes[p][e.number] = e.value;
         }
         if (write(fds[1], &e, sizeof(e)) != (ssize_t)sizeof(e))
            return 1;
      }
      pad->fd = fds[0];
      linuxraw_poll_pad(pad);

      /* what the events said */
      for (i = 0; i < NUM_BUTTONS; i++)
         if (     (linuxraw_joypad.button(p, (uint16_t)i) != 0)
               != (((want_buttons[p] >> i) & 1) != 0))
            bad++;
      for (i = 0; i < NUM_AXES; i++)
         if ((int16_t)(linuxraw_joypad.axis(p, AXIS_POS(i))
                  + linuxraw_joypad.axis(p, AXIS_NEG(i))) != want_axes[p][i])
            bad++;

      bad += bv_compare(&linuxraw_joypad, p);
      bad += bv_compare_view(&linuxraw_joypad, p);
   }

   if (bad)
   {
      printf("FAIL linuxraw_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS linuxraw_checked_test: 4000 random pads; what the bridge would"
         " read is what the driver says\n");
   return 0;
}
