/* linux_pad_members_test.c -- which of a pad's nodes under its HID
 * device takes which role (input/common/linux_pad_members.h).
 *
 *   1. A DualSense: the tagged accelerometer node is the sensors', the
 *      tagged touchpad node the touchpad's; the pad's own node and its
 *      hidraw node take neither.
 *   2. A Wii Remote: no node is tagged; the untagged ones are tried for
 *      the sensors in the order listed, never the pad's own.
 *   3. Tagged before untagged, whatever the listing's order; the
 *      touchpad is never tried as the sensors.
 *   4. A DualShock 3: its hidraw node gives the reports; for any other
 *      pad it is left alone.
 *   5. More nodes than it holds: the rest are not looked at.
 */
#include <stdio.h>
#include <string.h>

#include "common/linux_pad_members.h"

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

static linux_pad_node_t ev(uint64_t rdev, bool accel, bool touchpad)
{
   linux_pad_node_t n;
   memset(&n, 0, sizeof(n));
   n.rdev = rdev; n.event = true; n.accel = accel; n.touchpad = touchpad;
   return n;
}

static linux_pad_node_t raw(uint64_t rdev)
{
   linux_pad_node_t n;
   memset(&n, 0, sizeof(n));
   n.rdev = rdev; n.hidraw = true;
   return n;
}

int main(void)
{
   linux_pad_node_t nodes[LINUX_PAD_MEMBERS_MAX + 2];
   linux_pad_members_t m;
   unsigned i;

   /* 1: a DualSense, listed pad, touchpad, motion, hidraw */
   nodes[0] = ev(100, false, false);
   nodes[1] = ev(101, false, true);
   nodes[2] = ev(102, true, false);
   nodes[3] = raw(200);
   linux_pad_members_choose(nodes, 4, 100, false, &m);
   CHECK(m.sensors[0] == 2 && m.sensors[1] == -1,
         "the DualSense's motion node is not the sensors' alone");
   CHECK(m.touchpad == 1, "the DualSense's touchpad node is not the touchpad's");
   CHECK(m.reports == -1, "a pad that reads no reports was given its hidraw node");

   /* 2: a Wii Remote: the pad, two untagged extension nodes */
   nodes[0] = ev(300, false, false);
   nodes[1] = ev(100, false, false);
   nodes[2] = ev(301, false, false);
   linux_pad_members_choose(nodes, 3, 100, false, &m);
   CHECK(m.sensors[0] == 0 && m.sensors[1] == 2 && m.sensors[2] == -1,
         "untagged nodes not tried in order, or the pad's own tried");
   CHECK(m.touchpad == -1, "a touchpad was made up");

   /* 3: tagged before untagged; the touchpad never as sensors */
   nodes[0] = ev(400, false, false);
   nodes[1] = ev(401, false, true);
   nodes[2] = ev(402, true, false);
   linux_pad_members_choose(nodes, 3, 100, false, &m);
   CHECK(m.sensors[0] == 2 && m.sensors[1] == 0 && m.sensors[2] == -1,
         "an untagged node came before the tagged one, or the touchpad was tried");

   /* 4: a DualShock 3's hidraw node */
   nodes[0] = ev(100, false, false);
   nodes[1] = raw(500);
   nodes[2] = ev(501, true, false);
   linux_pad_members_choose(nodes, 3, 100, true, &m);
   CHECK(m.reports == 1, "the DualShock 3's hidraw node was not taken");
   CHECK(m.sensors[0] == 2 && m.sensors[1] == -1,
         "the hidraw node was tried as the sensors");

   /* 5: too many */
   for (i = 0; i < LINUX_PAD_MEMBERS_MAX + 2; i++)
      nodes[i] = ev(600 + i, false, false);
   nodes[LINUX_PAD_MEMBERS_MAX + 1] = ev(700, true, false);
   linux_pad_members_choose(nodes, LINUX_PAD_MEMBERS_MAX + 2, 100, false, &m);
   for (i = 0; m.sensors[i] >= 0; i++)
      CHECK(m.sensors[i] < LINUX_PAD_MEMBERS_MAX, "a node past the table was looked at");
   CHECK(i == LINUX_PAD_MEMBERS_MAX, "the table's nodes were not all tried");

   if (failures)
   {
      fprintf(stderr, "FAIL linux_pad_members_test: %u failures\n", failures);
      return 1;
   }
   fprintf(stderr, "PASS linux_pad_members_test: sensors, touchpad and reports"
         " given to the right nodes; tagged first, the pad's own never\n");
   return 0;
}
