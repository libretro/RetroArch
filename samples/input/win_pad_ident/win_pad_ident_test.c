/* Which of Windows' views reads a controller (input/common/win_pad_ident.h),
 * held to a table of real controllers: under every Windows joypad
 * driver each is one port - a pad on two links, cable and Bluetooth,
 * too, where its Bluetooth address says so. */

#include <stdio.h>
#include <string.h>

#include "common/win_pad_ident.h"
#include "common/input_ignore_devices.h"

static unsigned failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; \
   fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
   fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

/* One link of a controller, as Windows shows it */
typedef struct
{
   const char *path;       /* raw input's device name */
   bool        hid_class;  /* on Windows' HID driver */
   uint16_t    vid, pid;
   const char *serial;     /* over Bluetooth: its serial number string */
   uint8_t     feature[7]; /* over USB: its address report's first bytes */
} link_t;

typedef struct
{
   const char *name;
   link_t      link[2];
   unsigned    links;
   /* the ports it takes under winraw, winraw + XInput, hybrid, dinput */
   unsigned    ports[4];
   bool        steam;      /* Steam Input is running and stands in for it */
} pad_t;

/* a DualShock 4's address, 1c:66:6d:0a:4b:2f, from each link */
#define DS4_FEATURE  { 0x12, 0x2f, 0x4b, 0x0a, 0x6d, 0x66, 0x1c }
#define DS4_SERIAL   "1c666d0a4b2f"
/* a DualSense's, a0:5a:5c:91:23:e4 */
#define DS_FEATURE   { 0x09, 0xe4, 0x23, 0x91, 0x5c, 0x5a, 0xa0 }
#define DS_SERIAL    "a0:5a:5c:91:23:e4"

static const pad_t pads[] = {
   { "Xbox 360 wired", {
      { "\\\\?\\HID#VID_045E&PID_028E&IG_00#8&1b2b1c4b&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x045E, 0x028E, NULL, { 0 } } }, 1, { 1, 1, 1, 1 }, false },
   { "Xbox Series, USB, a lower-case path", {
      { "\\\\?\\hid#vid_045e&pid_0b12&ig_00#9&2f0b6e3a&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x045E, 0x0B12, NULL, { 0 } } }, 1, { 1, 1, 1, 1 }, false },
   { "Xbox One S, Bluetooth", {
      { "\\\\?\\HID#{00001124-0000-1000-8000-00805f9b34fb}_VID&0002045e_PID&02e0&IG_00#9&1d2a6b11&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x045E, 0x02E0, NULL, { 0 } } }, 1, { 1, 1, 1, 1 }, false },
   { "Xbox Series, Bluetooth LE", {
      { "\\\\?\\HID#{00001812-0000-1000-8000-00805f9b34fb}_Dev_VID&02045e_PID&0b13_REV&0509_d4ef5a1b2c3d&IG_00#a&3b0c&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x045E, 0x0B13, NULL, { 0 } } }, 1, { 1, 1, 1, 1 }, false },
   { "8BitDo in X mode", {
      { "\\\\?\\HID#VID_045E&PID_028E&IG_00#8&3a91c2d&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x045E, 0x028E, NULL, { 0 } } }, 1, { 1, 1, 1, 1 }, false },
   { "8BitDo in D mode", {
      { "\\\\?\\HID#VID_2DC8&PID_6001#7&1f0a33c&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x2DC8, 0x6001, NULL, { 0 } } }, 1, { 1, 1, 1, 1 }, false },
   { "DualShock 4, USB", {
      { "\\\\?\\HID#VID_054C&PID_09CC&MI_03#8&2c1f&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x054C, 0x09CC, NULL, DS4_FEATURE } }, 1, { 1, 1, 1, 1 }, false },
   { "DualShock 4, Bluetooth", {
      { "\\\\?\\HID#{00001124-0000-1000-8000-00805f9b34fb}_VID&0002054c_PID&09cc#9&2a1b&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x054C, 0x09CC, DS4_SERIAL, { 0 } } }, 1, { 1, 1, 1, 1 }, false },
   /* on Bluetooth, and plugged in to charge: one pad, its address
    * read from either link */
   { "DualShock 4, Bluetooth and USB", {
      { "\\\\?\\HID#{00001124-0000-1000-8000-00805f9b34fb}_VID&0002054c_PID&09cc#9&2a1b&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x054C, 0x09CC, DS4_SERIAL, { 0 } },
      { "\\\\?\\HID#VID_054C&PID_09CC&MI_03#8&2c1f&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x054C, 0x09CC, NULL, DS4_FEATURE } }, 2, { 1, 1, 1, 1 }, false },
   { "DualSense, Bluetooth and USB", {
      { "\\\\?\\hid#{00001124-0000-1000-8000-00805f9b34fb}_vid&0002054c_pid&0ce6#9&11&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x054C, 0x0CE6, DS_SERIAL, { 0 } },
      { "\\\\?\\HID#VID_054C&PID_0CE6&MI_03#8&3d&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x054C, 0x0CE6, NULL, DS_FEATURE } }, 2, { 1, 1, 1, 1 }, false },
   { "Switch Pro, Bluetooth", {
      { "\\\\?\\HID#{00001124-0000-1000-8000-00805f9b34fb}_VID&0002057e_PID&2009#9&3&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x057E, 0x2009, NULL, { 0 } } }, 1, { 1, 1, 1, 1 }, false },
   /* off Windows' HID driver: the HID driver beside the joypad driver's */
   { "GameCube adapter, WinUSB", {
      { "\\\\?\\USB#VID_057E&PID_0337#5&1a2b&0&3#{dee824ef-729b-4a0e-9c14-b7117d33a817}",
        false, 0x057E, 0x0337, NULL, { 0 } } }, 1, { 1, 1, 1, 1 }, false },
   { "Original Xbox pad, WinUSB", {
      { "\\\\?\\USB#VID_045E&PID_0289#5&2c&0&4#{dee824ef-729b-4a0e-9c14-b7117d33a817}",
        false, 0x045E, 0x0289, NULL, { 0 } } }, 1, { 1, 1, 1, 1 }, false },
   { "Steam Input's pad", {
      { "\\\\?\\HID#VID_28DE&PID_11FF&IG_00#3&1&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x28DE, 0x11FF, NULL, { 0 } } }, 1, { 1, 1, 1, 1 }, false },
   /* ... and the pad it stands in for, which Steam names */
   { "a DualSense Steam Input stands in for", {
      { "\\\\?\\HID#VID_054C&PID_0CE6&MI_03#8&3d&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
        true, 0x054C, 0x0CE6, NULL, DS_FEATURE } }, 1, { 0, 0, 0, 0 }, true },
};

static const char steam_list[] = "0x054c/0x0ce6";

static uint64_t link_addr(const link_t *l)
{
   if (win_pad_path_is_bluetooth(l->path))
      return win_pad_sony_addr_serial(l->serial);
   return win_pad_sony_addr_feature(l->feature, sizeof(l->feature));
}

int main(void)
{
   static const char *drv_name[4] = {
      "winraw", "winraw + XInput", "xinput hybrid", "dinput" };
   unsigned p, d, l;

   /* the paths' markers, in either case */
   CHECK(win_pad_path_is_xinput("\\\\?\\HID#VID_045E&PID_02EA&IG_00#x"), "IG_ upper");
   CHECK(win_pad_path_is_xinput("\\\\?\\hid#vid_045e&pid_02ea&ig_00#x"), "ig_ lower");
   CHECK(!win_pad_path_is_xinput("\\\\?\\HID#VID_054C&PID_09CC&MI_03#x"), "a DS4 is not XInput");
   CHECK(!win_pad_path_is_xinput(NULL), "no path");
   CHECK(win_pad_path_is_bluetooth("x{00001124-0000-1000-8000-00805F9B34FB}y"), "BT upper");
   CHECK(!win_pad_path_is_bluetooth("\\\\?\\HID#VID_054C&PID_09CC"), "USB is not BT");

   /* the address from each link */
   {
      static const uint8_t ds4[7] = DS4_FEATURE;
      CHECK(win_pad_sony_addr_feature(ds4, 7) == 0x1c666d0a4b2fULL, "DS4 USB address");
      CHECK(win_pad_sony_addr_serial(DS4_SERIAL) == 0x1c666d0a4b2fULL, "DS4 BT address");
      CHECK(win_pad_sony_addr_serial(DS_SERIAL) == 0xa05a5c9123e4ULL, "separators");
      CHECK(win_pad_sony_addr_serial("1c666d0a4b") == 0, "too short");
      CHECK(win_pad_sony_addr_serial("1c666d0a4b2f00") == 0, "too long");
      CHECK(win_pad_sony_addr_serial("1c666d0a4bzz") == 0, "not hex");
      CHECK(win_pad_sony_addr_feature(ds4, 6) == 0, "a short report");
   }

   for (p = 0; p < sizeof(pads) / sizeof(pads[0]); p++)
      for (d = 0; d < 4; d++)
      {
         unsigned ports = 0;
         uint64_t raw_addr[2];
         unsigned raws = 0;
         for (l = 0; l < pads[p].links; l++)
         {
            const link_t *lk = &pads[p].link[l];
            enum win_pad_view v = win_pad_owner((enum win_pad_driver)d,
                  lk->path, lk->hid_class,
                  pads[p].steam
                  && input_ignore_devices_names(steam_list, lk->vid, lk->pid));
            if (v == WIN_PAD_VIEW_NONE)
               continue;
            /* raw input and DirectInput read a pad's second link as
             * its first */
            if (v == WIN_PAD_VIEW_RAW || v == WIN_PAD_VIEW_DINPUT)
            {
               uint64_t a = link_addr(lk);
               unsigned r;
               bool twin = false;
               for (r = 0; r < raws; r++)
                  if (a && raw_addr[r] == a)
                     twin = true;
               raw_addr[raws++] = a;
               if (twin)
                  continue;
            }
            ports++;
         }
         CHECK(ports == pads[p].ports[d], "%s under %s: %u port(s), not %u",
               pads[p].name, drv_name[d], ports, pads[p].ports[d]);
      }

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] win_pad_ident: %u controllers, one port each under every"
         " Windows joypad driver; a pad on cable and Bluetooth is one\n",
         (unsigned)(sizeof(pads) / sizeof(pads[0])));
   return 0;
}
