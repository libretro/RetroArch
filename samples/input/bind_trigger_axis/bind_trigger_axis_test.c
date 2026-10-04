/* Binding L2 or R2 by pressing it: which axis is the trigger's
 * (menu/menu_bind_trigger.h).
 *
 * The capture hands the function every axis as it is now and as it
 * was at rest, at the moment a button answers. It has to pick the
 * trigger's axis when there is one - an axis that rests at an end of
 * its range and has just left it - and nothing otherwise, so that a
 * trigger that is a button alone is still bound to its button. */

#include <stdio.h>
#include <string.h>

#include "../../../menu/menu_bind_trigger.h"

static unsigned failures;
#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } } while (0)

#define AXES 32

int main(void)
{
   int16_t rest[AXES], now[AXES];

   /* a DualSense under raw input: the sticks on 0, 1, 2 and 5, and
    * the triggers on 3 and 4, resting at the bottom */
   memset(rest, 0, sizeof(rest));
   rest[0] = 300; rest[1] = -200; rest[2] = 90; rest[5] = -40;
   rest[3] = -32768;
   rest[4] = -32768;

   /* nothing has moved: no axis, the button is what was pressed */
   memcpy(now, rest, sizeof(now));
   CHECK(menu_bind_trigger_axis(now, rest, AXES) == AXIS_NONE,
         "with every axis at rest an axis was picked");

   /* L2 leaves its rest by the first step it reports */
   now[3] = (int16_t)(-32768 + 257);
   CHECK(menu_bind_trigger_axis(now, rest, AXES) == AXIS_POS(3),
         "L2's first step did not pick +3");

   /* R2, with L2 at rest; and pulled all the way */
   memcpy(now, rest, sizeof(now));
   now[4] = 32767;
   CHECK(menu_bind_trigger_axis(now, rest, AXES) == AXIS_POS(4),
         "R2 pulled did not pick +4");

   /* the sticks moving does not make them triggers: a face button
    * pressed with a stick held over is still the button */
   memcpy(now, rest, sizeof(now));
   now[0] = 32767; now[1] = -32768; now[2] = 20000;
   CHECK(menu_bind_trigger_axis(now, rest, AXES) == AXIS_NONE,
         "a stick held over was taken for a trigger");

   /* noise at the bottom of a trigger is not a pull */
   memcpy(now, rest, sizeof(now));
   now[3] = (int16_t)(-32768 + 60);
   CHECK(menu_bind_trigger_axis(now, rest, AXES) == AXIS_NONE,
         "noise at a trigger's rest was taken for a pull");

   /* a trigger that rests at the top is bound the other way */
   memset(rest, 0, sizeof(rest));
   rest[6] = 32767;
   memcpy(now, rest, sizeof(now));
   now[6] = 30000;
   CHECK(menu_bind_trigger_axis(now, rest, AXES) == AXIS_NEG(6),
         "a trigger resting at the top did not pick -6");

   /* a trigger that rests in the middle is not found this way, and
    * is bound as it always was */
   memset(rest, 0, sizeof(rest));
   memcpy(now, rest, sizeof(now));
   now[2] = 9000;
   CHECK(menu_bind_trigger_axis(now, rest, AXES) == AXIS_NONE,
         "an axis resting in the middle was picked");

   if (failures)
   {
      fprintf(stderr, "%u check(s) failed\n", failures);
      return 1;
   }
   printf("PASS bind_trigger_axis_test: a trigger's axis is picked when it"
         " leaves an end it rests at, in the direction of the pull; sticks,"
         " noise and axes resting in the middle are not\n");
   return 0;
}
