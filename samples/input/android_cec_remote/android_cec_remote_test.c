/* android_cec_remote_test.c -- a television's remote over HDMI-CEC,
 * through the routing that decides whether an event's device is a pad
 * or a keyboard.
 *
 * Android turns CEC commands into key events with no device behind
 * them: device id -1, source AINPUT_SOURCE_HDMI. Asked for the name of
 * device -1 it answers "Virtual", and an event from an unknown device
 * that is not a keyboard is taken for a new gamepad.
 *
 * The bug this pins (issue #19249): the remote became an unconfigured
 * pad called "Virtual" on the next free port. Its keys did nothing,
 * and it held a port a controller should have had.
 *
 * The claims:
 *
 *   1. A key from a CEC remote registers no pad, whether it is the
 *      first event the driver sees or comes after a controller.
 *   2. Its keys land on the keyboard row, where the arrows, Center and
 *      Back already mean what a remote means by them.
 *   3. Only AINPUT_SOURCE_HDMI is taken for a remote: keyboards,
 *      d-pads, gamepads, joysticks, touchscreens, mice and their
 *      combinations are not.
 *   4. Controllers are untouched: a gamepad still gets a pad slot and
 *      its keys still land on its own row - including one that
 *      reports device id -1 from another source.
 *   5. The NVIDIA SHIELD keeps its own handling of CEC input.
 *
 * The source test is the driver's own, from android_kbd_route.h. The
 * loop around it - looking up a port, registering a keyboard or a pad,
 * picking the row a key is written to - is reproduced here, because
 * the driver only builds against the NDK. A sabotage mode ignores the
 * source, as the driver did, and is asserted to be caught. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include "android_kbd_route.h"

/* android/input.h, as far as this cares. */
#define SOURCE_KEYBOARD     0x00000101
#define SOURCE_DPAD         0x00000201
#define SOURCE_GAMEPAD      0x00000401
#define SOURCE_TOUCHSCREEN  0x00001002
#define SOURCE_MOUSE        0x00002002
#define SOURCE_STYLUS       0x00004002
#define SOURCE_JOYSTICK     0x01000010
#define SOURCE_HDMI         0x02000001

#define MAX_PADS      8
#define KEYBOARD_ROW  MAX_PADS  /* ANDROID_KEYBOARD_PORT */
#define NO_ROW        (-1)

static unsigned failures = 0;
static bool     quiet    = false;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         if (!quiet) \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

/* What the driver did: every unknown device that is not a keyboard is
 * a gamepad, whatever the event's source. */
static bool sabotage_ignore_source = false;

/* The driver's state, as far as routing touches it. */
struct driver
{
   const char *device_model;
   int  pad_ids[MAX_PADS];
   char pad_names[MAX_PADS][32];
   unsigned pads_connected;
   int  kbd_ids[4];
   unsigned kbd_num;
};

static void driver_reset(struct driver *d, const char *model)
{
   memset(d, 0, sizeof(*d));
   d->device_model = model;
}

/* InputDevice.getDevice(id).getName(), for the ids used here. */
static const char *device_name(int id)
{
   switch (id)
   {
      case -1: return "Virtual";
      case 7:  return "Xbox Wireless Controller";
      case 9:  return "Logitech K400";
      default: return "Unknown";
   }
}

static bool is_keyboard_id(const struct driver *d, int id)
{
   unsigned i;
   for (i = 0; i < d->kbd_num; i++)
      if (d->kbd_ids[i] == id)
         return true;
   return false;
}

static int port_of_id(const struct driver *d, int id)
{
   unsigned i;
   for (i = 0; i < d->pads_connected; i++)
      if (d->pad_ids[i] == id)
         return (int)i;
   return -1;
}

/* android_input_event_is_keyboard() in the driver. */
static bool event_is_keyboard(const struct driver *d, int id, int source)
{
   if (is_keyboard_id(d, id))
      return true;
   if (sabotage_ignore_source)
      return false;
   return    android_key_source_is_cec(source)
          && android_cec_remote_is_keyboard(d->device_model);
}

/* handle_hotplug(), as far as it decides between keyboard and pad. */
static void hotplug(struct driver *d, int *port, int id, int source)
{
   if (     (source == SOURCE_KEYBOARD
          || source == (SOURCE_KEYBOARD | SOURCE_DPAD))
         && d->kbd_num < 4)
   {
      d->kbd_ids[d->kbd_num++] = id;
      return;
   }

   if (d->pads_connected >= MAX_PADS)
      return;

   *port = (int)d->pads_connected;
   d->pad_ids[*port] = id;
   strncpy(d->pad_names[*port], device_name(id),
         sizeof(d->pad_names[*port]) - 1);
   d->pads_connected++;
}

/* One key event through the driver's loop. Returns the row of the key
 * state it is written to: a pad's port, KEYBOARD_ROW, or NO_ROW. */
static int key_event(struct driver *d, int id, int source)
{
   int  port        = port_of_id(d, id);
   bool is_keyboard = event_is_keyboard(d, id, source);

   if (port < 0 && !is_keyboard)
      hotplug(d, &port, id, source);

   /* A keyboard hotplug registers the id; the event is then its. */
   if (is_keyboard || is_keyboard_id(d, id))
      return KEYBOARD_ROW;
   return port < 0 ? NO_ROW : port;
}

static bool has_pad_named(const struct driver *d, const char *name)
{
   unsigned i;
   for (i = 0; i < d->pads_connected; i++)
      if (!strcmp(d->pad_names[i], name))
         return true;
   return false;
}

/* 1, 2: the remote is the first thing the driver sees. */
static void lane_remote_first(void)
{
   struct driver d;
   unsigned i;

   driver_reset(&d, "H616 TV Box");

   /* The route is decided by the event's device and source, so every
    * key of the remote takes it: the arrows, Center and Back alike. */
   for (i = 0; i < 6; i++)
      CHECK(key_event(&d, -1, SOURCE_HDMI) == KEYBOARD_ROW,
            "a CEC remote's key did not land on the keyboard row");

   CHECK(d.pads_connected == 0, "a CEC remote was registered as a pad");
   CHECK(!has_pad_named(&d, "Virtual"),
         "a pad called Virtual was registered for a CEC remote");

   /* A controller that connects afterwards gets the first port. */
   CHECK(key_event(&d, 7, SOURCE_GAMEPAD) == 0,
         "the first controller after a CEC remote did not get port 1");
}

/* 1, 4: a controller is already there. */
static void lane_remote_after_pad(void)
{
   struct driver d;
   driver_reset(&d, "H616 TV Box");

   CHECK(key_event(&d, 7, SOURCE_GAMEPAD) == 0,
         "a gamepad's key did not land on its own row");
   CHECK(key_event(&d, -1, SOURCE_HDMI) == KEYBOARD_ROW,
         "a CEC remote's key did not land on the keyboard row");
   CHECK(d.pads_connected == 1,
         "a CEC remote took a pad slot next to a controller");
   CHECK(key_event(&d, 7, SOURCE_GAMEPAD | SOURCE_JOYSTICK) == 0,
         "the controller lost its row after the remote was used");

   /* A keyboard is still a keyboard. */
   CHECK(key_event(&d, 9, SOURCE_KEYBOARD) == KEYBOARD_ROW,
         "a keyboard's key did not land on the keyboard row");
   CHECK(d.pads_connected == 1, "a keyboard took a pad slot");
}

/* 3: the source test. */
static void lane_sources(void)
{
   static const int not_cec[] = {
      0,
      SOURCE_KEYBOARD, SOURCE_DPAD, SOURCE_GAMEPAD, SOURCE_JOYSTICK,
      SOURCE_TOUCHSCREEN, SOURCE_MOUSE, SOURCE_STYLUS,
      SOURCE_KEYBOARD | SOURCE_DPAD,
      SOURCE_GAMEPAD  | SOURCE_JOYSTICK,
      SOURCE_KEYBOARD | SOURCE_GAMEPAD | SOURCE_JOYSTICK | SOURCE_DPAD,
      SOURCE_TOUCHSCREEN | SOURCE_STYLUS };
   unsigned i;

   CHECK(android_key_source_is_cec(SOURCE_HDMI),
         "AINPUT_SOURCE_HDMI was not taken for a CEC remote");
   CHECK(ANDROID_KEY_SOURCE_HDMI == SOURCE_HDMI,
         "the header's value for AINPUT_SOURCE_HDMI is wrong");

   for (i = 0; i < sizeof(not_cec) / sizeof(not_cec[0]); i++)
      CHECK(!android_key_source_is_cec(not_cec[i]),
            "a source other than HDMI was taken for a CEC remote");
}

/* 4: device id -1 from a source that is not HDMI is not the remote. */
static void lane_virtual_gamepad(void)
{
   struct driver d;
   driver_reset(&d, "H616 TV Box");

   CHECK(key_event(&d, -1, SOURCE_GAMEPAD) == 0,
         "a virtual gamepad's key did not land on a pad row");
   CHECK(d.pads_connected == 1, "a virtual gamepad was not registered");
}

/* 5: the SHIELD's own mapping is left alone. */
static void lane_shield(void)
{
   struct driver d;

   CHECK(!android_cec_remote_is_keyboard("SHIELD Android TV"),
         "the SHIELD's CEC input was taken off its own pad");
   CHECK(android_cec_remote_is_keyboard("H616 TV Box"),
         "a device that is not a SHIELD was treated as one");
   CHECK(android_cec_remote_is_keyboard(NULL),
         "an unknown device model was treated as a SHIELD");

   driver_reset(&d, "SHIELD Android TV");
   CHECK(key_event(&d, -1, SOURCE_HDMI) == 0,
         "on a SHIELD, CEC input did not go to a pad as before");
}

static void all_lanes(void)
{
   lane_remote_first();
   lane_remote_after_pad();
   lane_sources();
   lane_virtual_gamepad();
   lane_shield();
}

int main(void)
{
   all_lanes();
   if (failures)
   {
      fprintf(stderr, "FAIL android_cec_remote_test: %u failures\n",
            failures);
      return 1;
   }

   /* Ignoring the source has to be caught. */
   quiet                  = true;
   sabotage_ignore_source = true;
   all_lanes();
   quiet                  = false;
   sabotage_ignore_source = false;

   if (!failures)
   {
      fprintf(stderr, "FAIL android_cec_remote_test: a CEC remote"
            " registered as a pad went unnoticed\n");
      return 1;
   }

   fprintf(stderr, "PASS android_cec_remote_test (sabotage caught by"
         " %u checks)\n", failures);
   return 0;
}
