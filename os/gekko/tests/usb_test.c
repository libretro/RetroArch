/* Wii USB checks, run in Dolphin by run-dolphin.sh with its emulated
 * Wii Speak and Skylander portal plugged in.  The Wii Speak (vendor
 * class): the device list, the interface description, control
 * transfers both ways (a register written and read back) and a bulk
 * transfer.  The portal (HID class): its own list and description, a
 * report sent by control transfer and the answer on the interrupt IN
 * endpoint, and the interrupt OUT endpoint. */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <gekko/power.h>
#include <gekko/thread.h>
#include <gekko/usb.h>

#include "../kernel/kernel.h"

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

#define VID 0x057e
#define PID 0x0308

/* The Wii Speak's registers: vendor requests 1 (set) and 2 (get) on
 * the interface, six bytes: register at 1, value at 2. */
static int set_reg(uint32_t id, uint8_t reg, uint16_t value)
{
   uint8_t b[6];
   memset(b, 0, sizeof(b));
   b[1] = reg;
   b[2] = (uint8_t)(value >> 8);
   b[3] = (uint8_t)value;
   return gk_usb_ctrl(id, 0x41, 1, 0, 0, b, sizeof(b));
}

static int get_reg(uint32_t id, uint16_t *value)
{
   uint8_t b[6];
   int ret;
   memset(b, 0xee, sizeof(b));
   ret = gk_usb_ctrl(id, 0xc1, 2, 0, 0, b, sizeof(b));
   *value = (uint16_t)((b[2] << 8) | b[3]);
   return ret;
}

#define PORTAL_VID 0x1430
#define PORTAL_PID 0x0150

static void test_hid(void)
{
   gk_usb_dev_t devs[GK_USB_MAX_DEVICES];
   gk_usb_info_t info;
   uint8_t rep[64];
   char what[128];
   int n, i, found = -1, ven_has = 0, ret;
   uint32_t id;

   n = gk_usb_hid_list(devs, GK_USB_MAX_DEVICES);
   for (i = 0; i < n; i++)
   {
      if (devs[i].vid == PORTAL_VID && devs[i].pid == PORTAL_PID)
         found = i;
      if (devs[i].vid == VID && devs[i].pid == PID)
         ven_has = 1;
   }
   CHECK(found >= 0 && !ven_has, "hid: the portal listed, the Wii Speak not");
   if (found < 0)
      return;
   id  = devs[found].id;
   ret = gk_usb_open(id, &info);
   snprintf(what, sizeof(what), "hid: open: %d, %04x:%04x, class %02x, %u "
         "endpoint(s)", ret, info.vid, info.pid, info.if_class, info.num_ep);
   CHECK(ret == 0 && info.vid == PORTAL_VID && info.if_class == 3
         && info.num_ep == 2 && info.ep[0].address == 0x81
         && info.ep[0].attributes == 3 && info.ep[0].max_packet == 0x40
         && info.ep[1].address == 0x02, what);
   CHECK(gk_usb_set_alt(id, 1, &info) == -ENODEV, "hid: no alternate settings");

   /* SET_REPORT 'R' (reset); the portal answers on the interrupt pipe. */
   memset(rep, 0, sizeof(rep));
   rep[0] = 'R';
   CHECK(gk_usb_ctrl(id, 0x21, 0x09, 0x0200, 0, rep, 2) >= 0,
         "hid: a report by control transfer");
   memset(rep, 0xee, sizeof(rep));
   ret = gk_usb_intr(id, 0x81, rep, 32);
   snprintf(what, sizeof(what), "hid: interrupt in: %d, %02x %02x %02x", ret,
         rep[0], rep[1], rep[2]);
   CHECK(ret == 32 && rep[0] == 'R' && rep[1] == 0x02 && rep[2] == 0x1b, what);
   memset(rep, 0xee, sizeof(rep));
   ret = gk_usb_intr(id, 0x81, rep, 32);
   CHECK(ret == 32 && rep[0] == 'S', "hid: then the status");
   memset(rep, 0x80, sizeof(rep));
   CHECK(gk_usb_intr(id, 0x02, rep, sizeof(rep)) == sizeof(rep),
         "hid: interrupt out");
   gk_usb_close(id);
}

/* What exit() and gk_exec() run: the interfaces close for good. */
static void test_shutdown(void)
{
   gk_usb_dev_t devs[GK_USB_MAX_DEVICES];
   gk_usb_info_t info;
   uint32_t id = 0;
   if (gk_usb_hid_list(devs, GK_USB_MAX_DEVICES) > 0)
   {
      id = devs[0].id;
      gk_usb_open(id, &info);
   }
   gk_shutdown();
   CHECK(id && gk_usb_hid_list(devs, GK_USB_MAX_DEVICES) == 0
         && gk_usb_list(devs, GK_USB_MAX_DEVICES) == 0
         && gk_usb_open(id, &info) == -ENODEV,
         "shut down: nothing listed or opened after");
}

int main(int argc, char **argv)
{
   gk_usb_dev_t devs[GK_USB_MAX_DEVICES];
   gk_usb_info_t info;
   char what[128];
   int n, i, found = -1;
   uint16_t v = 0xffff;
   uint8_t byte = 0xee, bulk[64];
   (void)argc;
   (void)argv;

   n = gk_usb_list(devs, GK_USB_MAX_DEVICES);
   for (i = 0; i < n; i++)
      if (devs[i].vid == VID && devs[i].pid == PID)
         found = i;
   snprintf(what, sizeof(what), "%d device(s), the Wii Speak among them", n);
   CHECK(found >= 0, what);
   CHECK(gk_usb_list(devs, GK_USB_MAX_DEVICES) == n, "the list again");
   if (found >= 0)
   {
      uint32_t id = devs[found].id;
      int ret = gk_usb_open(id, &info);
      snprintf(what, sizeof(what), "open: %d, %04x:%04x, %u endpoint(s), "
            "class %02x", ret, info.vid, info.pid, info.num_ep, info.if_class);
      CHECK(ret == 0 && info.vid == VID && info.pid == PID
            && info.num_ep == 0 && info.if_class == 0xff, what);
      ret = gk_usb_set_alt(id, 1, &info);
      snprintf(what, sizeof(what), "alternate setting 1: %d, %u endpoint(s)",
            ret, info.num_ep);
      CHECK(ret == 0 && info.num_ep == 3 && info.ep[0].address == 0x81
            && info.ep[1].address == 0x02 && info.ep[1].attributes == 2
            && info.ep[2].max_packet == 0x40, what);

      /* Vendor request 0 resets a flag that request 6 reads as 0,
       * then 1. */
      CHECK(gk_usb_ctrl(id, 0x41, 0, 0, 0, NULL, 0) >= 0, "control, no data");
      CHECK(gk_usb_ctrl(id, 0xc1, 6, 0, 0, &byte, 1) >= 0 && byte == 0,
            "control to the host");
      CHECK(gk_usb_ctrl(id, 0xc1, 6, 0, 0, &byte, 1) >= 0 && byte == 1,
            "control to the host, again");

      CHECK(set_reg(id, 0, 1) >= 0 && get_reg(id, &v) >= 0 && v == 1,
            "a register set and read back");
      CHECK(set_reg(id, 0, 0) >= 0 && get_reg(id, &v) >= 0 && v == 0,
            "and cleared");

      memset(bulk, 0x5a, sizeof(bulk));
      CHECK(gk_usb_bulk(id, 0x02, bulk, sizeof(bulk)) >= 0, "bulk out");
      gk_usb_close(id);
   }
   CHECK(gk_usbstorage_open() == NULL, "no USB drive");
   test_hid();
   test_shutdown();
   gk_debug_printf("%s (%u failure(s))", failures ? "FAILED" : "PASSED",
         failures);
   gk_power_off();
   return 0;
}
