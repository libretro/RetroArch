/* Which of the devices a system reports are the keyboards on the desk,
 * and which the mice: input/common/input_keyboard_devices.h, on its own.
 *
 * A desk as Linux reports one, then the odd cases one at a time. */
#include <stdio.h>
#include <string.h>

#include "../../../input/common/input_keyboard_devices.h"

static unsigned failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

static input_kbdev_t dev(const char *key, bool keyboard, bool pointer, int boot)
{
   input_kbdev_t d;
   memset(&d, 0, sizeof(d));
   strncpy(d.key, key, sizeof(d.key) - 1);
   d.keyboard   = keyboard;
   d.pointer    = pointer;
   /* the one number is the part's own: a pointer's is whether it is a
    * boot mouse, a key-sending part's whether it is a boot keyboard */
   d.boot       = pointer ? -1 : (int8_t)boot;
   d.boot_mouse = pointer ? (int8_t)boot : -1;
   return d;
}

int main(void)
{
   input_kbdev_t d[16];
   unsigned n;

   /* ---- a desk --------------------------------------------------- */
   d[0] = dev("",          false, false, -1); /* power button                   */
   d[1] = dev("",          false, false, -1); /* video bus (brightness keys)    */
   d[2] = dev("usb-1.3",   true,  false,  1); /* the keyboard                   */
   d[3] = dev("usb-1.3",   false, false,  0); /* ... its media keys             */
   d[4] = dev("usb-1.3",   true,  false,  0); /* ... its extra-key interface    */
   d[5] = dev("usb-1.4",   false, true,   0); /* a mouse                        */
   d[6] = dev("usb-1.4",   true,  false,  0); /* ... whose buttons send keys    */
   d[7] = dev("usb-1.5",   false, false,  0); /* a headset's volume buttons     */
   n    = input_kbdev_group(d, 8, 16);
   CHECK(n == 1, "a desk with one keyboard: %u listed", n);
   CHECK(d[2].group == 0 && d[3].group == 0 && d[4].group == 0,
         "the keyboard's three parts are keyboards %u, %u and %u, want all 0",
         d[2].group, d[3].group, d[4].group);
   CHECK(d[0].group == INPUT_KBDEV_NONE && d[1].group == INPUT_KBDEV_NONE
         && d[6].group == INPUT_KBDEV_NONE && d[7].group == INPUT_KBDEV_NONE,
         "the power button, the video bus, the mouse's keys or the headset is a keyboard");
   printf("   ok   a desk: one keyboard in three parts is one keyboard; the power button, the mouse's keys and the headset are none\n");

   /* ---- two keyboards of one model ------------------------------- */
   d[0] = dev("usb-1.1", true,  false, 1);
   d[1] = dev("usb-1.2", true,  false, 1);
   d[2] = dev("usb-1.1", false, false, 0);
   d[3] = dev("usb-1.2", false, false, 0);
   n    = input_kbdev_group(d, 4, 16);
   CHECK(n == 2 && d[0].group == 0 && d[2].group == 0 && d[1].group == 1 && d[3].group == 1,
         "two keyboards of one model: %u listed, parts %u %u %u %u", n,
         d[0].group, d[1].group, d[2].group, d[3].group);
   printf("   ok   two keyboards of one model are two, each with its own parts\n");

   /* ---- a keyboard that is also a pointer ------------------------ */
   d[0] = dev("usb-2", true,  false, 1);  /* boot keyboard ...          */
   d[1] = dev("usb-2", false, true,  0);  /* ... with a touchpad        */
   d[2] = dev("bt-aa", true,  false, -1); /* a Bluetooth one ...        */
   d[3] = dev("bt-aa", false, true,  -1); /* ... with a trackpoint      */
   d[4] = dev("virt",  true,  false, -1); /* a vendor's virtual device  */
   d[5] = dev("virt",  false, true,  -1);
   n    = input_kbdev_group(d, 6, 16);
   CHECK(n == 3 && d[0].group == 0 && d[2].group == 1 && d[4].group == 2,
         "keyboards that are also pointers: %u listed, want 3", n);
   printf("   ok   a keyboard with a pointer built in is a keyboard, and so is one of which it is not known what its keyboard part is\n");

   /* ---- devices with nothing to tell them apart ------------------ */
   d[0] = dev("", true, false, -1); /* the laptop's own keyboard */
   d[1] = dev("", true, false, -1); /* and a second one like it  */
   d[2] = dev("", false, true, -1); /* the touchpad              */
   n    = input_kbdev_group(d, 3, 16);
   CHECK(n == 2 && d[0].group == 0 && d[1].group == 1,
         "two keyboards with nothing to tell them apart are not one each: %u listed", n);
   printf("   ok   keyboards with nothing to tell where they are plugged in are each their own\n");

   /* ---- nothing behind it; and no more than there is room for ---- */
   d[0] = dev("", true, false, -1);
   d[0].remote = true;
   d[1] = dev("a", true, false, 1);
   d[2] = dev("b", true, false, 1);
   d[3] = dev("c", true, false, 1);
   n    = input_kbdev_group(d, 4, 2);
   CHECK(n == 2 && d[0].group == INPUT_KBDEV_NONE && d[1].group == 0
         && d[2].group == 1 && d[3].group == INPUT_KBDEV_NONE,
         "a remote keyboard and a list of two: %u listed, groups %u %u %u %u", n,
         d[0].group, d[1].group, d[2].group, d[3].group);
   printf("   ok   a device with nothing behind it is no keyboard; no more are listed than there is room for\n");

   /* ---- the mice ------------------------------------------------- */
   d[0] = dev("usb-1.3", true,  false,  1); /* a keyboard ...                   */
   d[1] = dev("usb-1.3", false, true,   0); /* ... that can send pointer events */
   d[2] = dev("usb-1.4", false, true,   1); /* a mouse ...                      */
   d[3] = dev("usb-1.4", true,  false,  0); /* ... whose buttons send keys      */
   d[4] = dev("usb-1.5", true,  false,  1); /* a keyboard and a mouse ...       */
   d[5] = dev("usb-1.5", false, true,   1); /* ... on one receiver              */
   d[6] = dev("bt-aa",   true,  false, -1); /* a Bluetooth keyboard ...         */
   d[7] = dev("bt-aa",   false, true,  -1); /* ... with a trackpoint            */
   d[8] = dev("",        false, true,  -1); /* the laptop's touchpad            */
   d[9] = dev("",        false, true,  -1); /* a pointer nothing is behind      */
   d[9].remote = true;
   n    = input_kbdev_mice(d, 10);
   CHECK(n == 4, "%u mice on the desk, want 4", n);
   CHECK(!d[1].mouse, "a keyboard that can send pointer events is a mouse");
   CHECK(d[2].mouse && d[5].mouse && d[7].mouse && d[8].mouse,
         "the mouse, the one on a receiver with a keyboard, the trackpoint or the touchpad is not a mouse: %d %d %d %d",
         d[2].mouse, d[5].mouse, d[7].mouse, d[8].mouse);
   CHECK(!d[9].mouse, "a pointer nothing is behind is a mouse");
   /* and the keyboards of the same desk */
   n    = input_kbdev_group(d, 10, 16);
   CHECK(n == 3 && d[0].group == 0 && d[3].group == INPUT_KBDEV_NONE
         && d[4].group == 1 && d[6].group == 2,
         "the same desk's keyboards: %u listed, want the keyboard, the receiver's and the Bluetooth one", n);
   printf("   ok   the mice: a keyboard's pointer part and a pointer with nothing behind it are none; a mouse, a receiver's mouse, a trackpoint and a touchpad are\n");

   if (failures)
   {
      printf("FAIL keyboard_devices_test: %u\n", failures);
      return 1;
   }
   printf("PASS keyboard_devices_test\n");
   return 0;
}
