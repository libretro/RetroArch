/* linux_not_joystick_test.c -- devices Linux tags as joysticks that
 * are not controllers.
 *
 * udev marks some keyboards, mice, tablets and their wireless
 * receivers as joysticks. A joypad driver that takes the tag at its
 * word gives such a device a port of its own, and it is enumerated
 * ahead of the real controller often enough to take port 1.
 *
 * The bug this pins (issue #19674): a Microsoft 2.4GHz Transceiver
 * v8.0, the receiver of a wireless keyboard and mouse, became pad 0
 * through its "System Control" node. The Xbox 360 controller was
 * configured on port 2, and the menu, which listens to port 1 by
 * default, had no controller.
 *
 * The claims:
 *
 *   1. That receiver is not a controller.
 *   2. Nor is any node the kernel names "System Control" or "Consumer
 *      Control", whoever made the device.
 *   3. Nor is a device on the list whose main node is mistagged.
 *   4. Real controllers are kept: gamepads, a pad whose product name
 *      says "Wireless", and a pedal set with axes and no buttons.
 *   5. The name rule is a suffix: a controller that merely has the
 *      words in its name is kept.
 *   6. A missing or empty name is not a reason to drop a device.
 *   7. The list holds no entry twice.
 *
 * The decision is the drivers' own, from linux_not_joystick.h. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include "linux_not_joystick.h"

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

struct device
{
   const char *name;
   unsigned vid;
   unsigned pid;
};

/* Not controllers. */
static const struct device ignored[] = {
   /* The receiver from issue #19674, as its log names it. */
   { "Microsoft Microsoft\xc2\xae 2.4GHz Transceiver v8.0 System Control",
      0x045e, 0x0745 },
   /* The same node on a device the list has never heard of. */
   { "Keychron Keychron Q9 System Control",          0x3434, 0x0999 },
   { "Some Vendor Some Keyboard Consumer Control",   0x1234, 0x5678 },
   /* Main node mistagged: found by id alone. */
   { "Microsoft Microsoft\xc2\xae 2.4GHz Transceiver v8.0",
      0x045e, 0x0745 },
   { "A4 Tech Co., Ltd Bloody V8 mouse",             0x09da, 0x3f8b },
   { "Wacom Intuos Pen (S)",                         0x056a, 0x030e },
   /* On the list, and reported with no name at all. */
   { NULL,                                           0x045e, 0x0745 }
};

/* Controllers. */
static const struct device kept[] = {
   { "Microsoft X-Box 360 pad",                      0x045e, 0x028e },
   { "Microsoft Xbox Series S|X Controller",         0x045e, 0x0b12 },
   { "Sony Interactive Entertainment Wireless Controller",
      0x054c, 0x09cc },
   { "Sony Interactive Entertainment DualSense Wireless Controller",
      0x054c, 0x0ce6 },
   { "8BitDo Pro 2",                                 0x2dc8, 0x6003 },
   { "Nintendo Switch Pro Controller",               0x057e, 0x2009 },
   /* Axes only, no buttons: still a controller. */
   { "Thrustmaster T3PA Pedals",                     0x044f, 0xb678 },
   /* The words are in the name, but it does not end in them. */
   { "System Control Deck Gamepad",                  0x1234, 0x0001 },
   { "Consumer Control Pad Pro",                     0x1234, 0x0002 },
   /* Nothing to go on is not a reason to drop it. */
   { NULL,                                           0x1234, 0x0003 },
   { "",                                             0x1234, 0x0004 }
};

int main(void)
{
   size_t i, j;
   size_t n = sizeof(linux_not_joystick_ids)
            / sizeof(linux_not_joystick_ids[0]);

   for (i = 0; i < sizeof(ignored) / sizeof(ignored[0]); i++)
   {
      if (!linux_input_is_not_joystick(ignored[i].name,
               (uint16_t)ignored[i].vid, (uint16_t)ignored[i].pid))
      {
         fprintf(stderr, "FAIL: \"%s\" (%04x:%04x) was taken for a"
               " controller\n",
               ignored[i].name ? ignored[i].name : "(no name)",
               ignored[i].vid, ignored[i].pid);
         failures++;
      }
   }

   for (i = 0; i < sizeof(kept) / sizeof(kept[0]); i++)
   {
      if (linux_input_is_not_joystick(kept[i].name,
               (uint16_t)kept[i].vid, (uint16_t)kept[i].pid))
      {
         fprintf(stderr, "FAIL: \"%s\" (%04x:%04x) is a controller and"
               " was dropped\n",
               kept[i].name ? kept[i].name : "(no name)",
               kept[i].vid, kept[i].pid);
         failures++;
      }
   }

   /* The list itself. */
   CHECK(n > 0, "the list is empty");
   for (i = 0; i < n; i++)
      for (j = i + 1; j < n; j++)
         CHECK(   linux_not_joystick_ids[i][0] != linux_not_joystick_ids[j][0]
               || linux_not_joystick_ids[i][1] != linux_not_joystick_ids[j][1],
               "the list holds an entry twice");

   if (failures)
   {
      fprintf(stderr, "FAIL linux_not_joystick_test: %u failures\n",
            failures);
      return 1;
   }

   fprintf(stderr, "PASS linux_not_joystick_test (%u listed devices)\n",
         (unsigned)n);
   return 0;
}
