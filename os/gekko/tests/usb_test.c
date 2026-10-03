/* Wii USB checks, run in Dolphin by run-dolphin.sh with its emulated
 * Wii Speak plugged in: the device list, the interface description,
 * control transfers both ways (a register written and read back) and
 * a bulk transfer. */

#include <stdio.h>
#include <string.h>

#include <gekko/power.h>
#include <gekko/thread.h>
#include <gekko/usb.h>

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
   gk_debug_printf("%s (%u failure(s))", failures ? "FAILED" : "PASSED",
         failures);
   gk_power_off();
   return 0;
}
