/* A pad's player lights on Linux: the udev joypad driver finds the LED
 * class devices under the pad and writes them as the console lights
 * its pads (input/common/linux_pad_leds.h). The real driver is
 * included; /sys/class/leds and /sys/devices are a directory of the
 * test's, laid out the same way, the lights' files plain files.
 *
 *   names        -> each kernel driver's lights told apart; others not
 *   DualSense    -> its five player lights, its light bar left
 *   DualShock 4  -> its light bar's colour, by multi_intensity
 *   another pad  -> a light not under the pad is not touched
 *   put out      -> lights it lit go out; ones it never lit are left
 *   arriving     -> a pad that comes to a slot already told is lit
 *   receiver     -> four xpad rings under one USB device: none taken */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <boolean.h>

#include "input/drivers_joypad/udev_joypad.c"

/* ---- what the driver links against --------------------------------- */

static settings_t stub_settings;
settings_t *config_get_ptr(void) { return &stub_settings; }
#include "../input_config_stubs.h"
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port, unsigned vid,
      unsigned pid)
{ return true; }
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{ return true; }
const char *input_config_get_device_name(unsigned port) { return NULL; }

/* ---- the test ------------------------------------------------------- */

static unsigned failures;
#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

static char root[256];
static char leds[300];

static void mkdirs(const char *path)
{
   char buf[512];
   char *s;
   strlcpy(buf, path, sizeof(buf));
   for (s = buf + 1; *s; s++)
      if (*s == '/')
      {
         *s = '\0';
         mkdir(buf, 0755);
         *s = '/';
      }
   mkdir(buf, 0755);
}

static void touch(const char *path)
{
   int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
   if (fd >= 0)
      close(fd);
}

/* a light @name under the device /sys/devices/@dev */
static void light(const char *dev, const char *name, bool rgb)
{
   char dir[512], file[600], link[512];
   snprintf(dir, sizeof(dir), "%s/sys/devices/%s/leds/%s", root, dev, name);
   mkdirs(dir);
   snprintf(file, sizeof(file), "%s/brightness", dir);
   touch(file);
   if (rgb)
   {
      snprintf(file, sizeof(file), "%s/multi_intensity", dir);
      touch(file);
   }
   snprintf(link, sizeof(link), "../../devices/%s/leds/%s", dev, name);
   snprintf(file, sizeof(file), "%s/%s", leds, name);
   if (symlink(link, file) != 0)
      exit(2);
}

/* what a light's file holds: its first line, "" for nothing written */
static const char *read_light(const char *name, const char *file)
{
   static char buf[64];
   char path[600];
   ssize_t len;
   int fd;
   snprintf(path, sizeof(path), "%s/%s/%s", leds, name, file);
   buf[0] = '\0';
   if ((fd = open(path, O_RDONLY)) < 0)
      return buf;
   len = read(fd, buf, sizeof(buf) - 1);
   close(fd);
   buf[len > 0 ? len : 0] = '\0';
   if (strchr(buf, '\n'))
      *strchr(buf, '\n') = '\0';
   return buf;
}

static int bright(const char *name)
{
   const char *s = read_light(name, "brightness");
   return *s ? atoi(s) : -1;
}

/* the DualSense's five, as a mask of the lit ones */
static unsigned ds_lit(void)
{
   unsigned i, mask = 0;
   char name[64];
   for (i = 1; i <= 5; i++)
   {
      snprintf(name, sizeof(name), "input5:white:player-%u", i);
      if (bright(name) > 0)
         mask |= 1u << (i - 1);
   }
   return mask;
}

static void pad_at(unsigned p, const char *dev)
{
   struct udev_joypad *pad = &udev_pads[p];
   memset(pad, 0, sizeof(*pad));
   pad->fd        = -1;
   pad->sensor_fd = -1;
   pad->touch_fd  = -1;
   snprintf(pad->led_scope, sizeof(pad->led_scope), "/sys/devices/%s", dev);
   udev_led_attach(p);
}

static void names(void)
{
   unsigned n;
   uint8_t rgb[3];
   CHECK(linux_pad_led_of("input5:rgb:indicator", &n) == LINUX_PAD_LED_RGB, "rgb");
   CHECK(linux_pad_led_of("input5:white:player-3", &n) == LINUX_PAD_LED_DS_PLAYER && n == 3,
         "DualSense player 3");
   CHECK(linux_pad_led_of("input5:white:mute", &n) == LINUX_PAD_LED_NONE, "a mute light");
   CHECK(linux_pad_led_of("0005:057E:2009.0003:green:player-2", &n) == LINUX_PAD_LED_SW_PLAYER
         && n == 2, "Switch Pro, newer kernels");
   CHECK(linux_pad_led_of("0005:057E:2009.0003:player4", &n) == LINUX_PAD_LED_SW_PLAYER
         && n == 4, "Switch Pro, older kernels");
   CHECK(linux_pad_led_of("0003:054C:0268.0001::sony2", &n) == LINUX_PAD_LED_PS3_PLAYER
         && n == 2, "DualShock 3");
   CHECK(linux_pad_led_of("0005:054C:09CC.0002:red", &n) == LINUX_PAD_LED_RED, "hid-sony red");
   CHECK(linux_pad_led_of("0005:054C:09CC.0002:global", &n) == LINUX_PAD_LED_NONE, "global");
   CHECK(linux_pad_led_of("xpad0", &n) == LINUX_PAD_LED_XPAD, "xpad");
   CHECK(linux_pad_led_of("input3::capslock", &n) == LINUX_PAD_LED_NONE, "a keyboard's");
   CHECK(linux_pad_led_of("input5:white:player-9", &n) == LINUX_PAD_LED_NONE, "no such light");

   /* as the consoles light them */
   CHECK(linux_pad_led_value(LINUX_PAD_LED_SW_PLAYER, 3, 3, true, rgb) == 1
         && linux_pad_led_value(LINUX_PAD_LED_SW_PLAYER, 4, 3, true, rgb) == 0,
         "a Switch pad's player 3 is its first three");
   CHECK(linux_pad_led_value(LINUX_PAD_LED_PS3_PLAYER, 4, 5, true, rgb) == 1
         && linux_pad_led_value(LINUX_PAD_LED_PS3_PLAYER, 1, 5, true, rgb) == 1
         && linux_pad_led_value(LINUX_PAD_LED_PS3_PLAYER, 2, 5, true, rgb) == 0,
         "a DualShock 3's player 5 is 4 and 1");
   CHECK(linux_pad_led_value(LINUX_PAD_LED_XPAD, 0, 2, false, rgb) == 7,
         "an Xbox 360 pad's player 2 is xpad's 7");
   CHECK(linux_pad_led_value(LINUX_PAD_LED_XPAD, 0, 5, false, rgb) == 0,
         "an Xbox 360 pad has four quarters");
   CHECK(linux_pad_led_value(LINUX_PAD_LED_RED, 0, 2, false, rgb) == 0x40
         && linux_pad_led_value(LINUX_PAD_LED_BLUE, 0, 2, false, rgb) == 0,
         "a DualShock 4's player 2 is red");
   CHECK(linux_pad_led_value(LINUX_PAD_LED_RGB, 0, 1, true, rgb) == -1,
         "a light bar beside player lights was not left");
}

int main(void)
{
   unsigned p;

   names();

   snprintf(root, sizeof(root), "/tmp/udev_player_lights.%d", (int)getpid());
   snprintf(leds, sizeof(leds), "%s/sys/class/leds", root);
   mkdirs(leds);
   udev_led_dir = leds;

   for (p = 0; p < MAX_USERS; p++)
   {
      udev_rumble_out[p].fd = -1;
      retro_atomic_store_release_int(&udev_rumble_slot[p], -1);
   }
   udev_led_init();

   /* a DualSense over USB, a DualShock 4 over Bluetooth, a keyboard */
   light("pci0/usb1/1-2/1-2:1.3/0003:054C:0CE6.0005", "input5:rgb:indicator", true);
   for (p = 1; p <= 5; p++)
   {
      char name[64];
      snprintf(name, sizeof(name), "input5:white:player-%u", p);
      light("pci0/usb1/1-2/1-2:1.3/0003:054C:0CE6.0005", name, false);
   }
   light("pci0/usb1/1-2/1-2:1.3/0003:054C:0CE6.0005", "input5:white:mute", false);
   light("pci0/bt/hci0/hci0:256/0005:054C:09CC.0009", "input9:rgb:indicator", true);
   light("pci0/usb1/1-3/1-3:1.0/0003:046D:C31C.0001", "input3::capslock", false);

   pad_at(0, "pci0/usb1/1-2/1-2:1.3/0003:054C:0CE6.0005");
   CHECK(udev_pads[0].led_count == 7, "the DualSense's lights found: %u, want 7"
         " (player five, light bar and its colour)", udev_pads[0].led_count);

   udev_joypad_set_player(0, 2);
   CHECK(ds_lit() == 0x0A, "player 2 on the DualSense is not its second and"
         " fourth lights (%#x)", ds_lit());
   CHECK(bright("input5:rgb:indicator") < 0,
         "the DualSense's light bar was touched");
   CHECK(bright("input5:white:mute") < 0, "its mute light was touched");
   CHECK(bright("input3::capslock") < 0, "a keyboard's light was touched");
   udev_joypad_set_player(0, 1);
   CHECK(ds_lit() == 0x04, "player 1 is not its middle light (%#x)", ds_lit());

   /* a slot told before its pad arrives */
   udev_joypad_set_player(1, 3);
   pad_at(1, "pci0/bt/hci0/hci0:256/0005:054C:09CC.0009");
   CHECK(bright("input9:rgb:indicator") == 255
         && !strcmp(read_light("input9:rgb:indicator", "multi_intensity"), "0 64 0"),
         "a DualShock 4 arriving as player 3 is not green (%d, \"%s\")",
         bright("input9:rgb:indicator"),
         read_light("input9:rgb:indicator", "multi_intensity"));

   /* no port: lights it lit go out */
   udev_joypad_set_player(0, 0);
   CHECK(ds_lit() == 0 && bright("input5:white:player-3") == 0,
         "the DualSense's lights did not go out");

   /* the pad goes; the next in its slot, never lit, is left as it was */
   udev_led_detach(1);
   retro_atomic_inc_int(&udev_rumble_gen[1]);
   udev_joypad_set_player(1, 0);
   light("pci0/usb1/1-4/1-4:1.0/0003:054C:0268.0002", "0003:054C:0268.0002::sony1", false);
   pad_at(1, "pci0/usb1/1-4/1-4:1.0/0003:054C:0268.0002");
   udev_led_write(1);
   CHECK(bright("0003:054C:0268.0002::sony1") < 0,
         "a pad on no port had its lights written");
   udev_joypad_set_player(1, 1);
   CHECK(bright("0003:054C:0268.0002::sony1") == 1,
         "a DualShock 3 as player 1 does not light its first");

   /* a wireless receiver's rings, all under its one USB device */
   light("pci0/usb1/1-5", "xpad0", false);
   light("pci0/usb1/1-5", "xpad1", false);
   pad_at(2, "pci0/usb1/1-5");
   CHECK(udev_pads[2].led_count == 0, "a receiver's rings were taken");
   light("pci0/usb1/1-6", "xpad2", false);
   pad_at(3, "pci0/usb1/1-6");
   udev_joypad_set_player(3, 4);
   CHECK(bright("xpad2") == 9, "an Xbox 360 pad as player 4 is not xpad's 9");

#ifdef HAVE_THREADS
   /* the writer's thread writes them, the caller only asks */
   udev_rumble_stop();
   udev_rumble_start();
   CHECK(udev_rumble_writer != NULL, "the writer did not start");
   pad_at(0, "pci0/usb1/1-2/1-2:1.3/0003:054C:0CE6.0005");
   udev_joypad_set_player(0, 5);
   {
      unsigned waited;
      for (waited = 0; waited < 2000 && ds_lit() != 0x1F; waited += 5)
         usleep(5000);
      CHECK(ds_lit() == 0x1F, "the writer did not light player 5 (%#x)",
            ds_lit());
   }
   udev_led_detach(0);
   retro_atomic_inc_int(&udev_rumble_gen[0]);
#endif

   udev_rumble_stop();
   {
      char cmd[300];
      snprintf(cmd, sizeof(cmd), "rm -rf %s", root);
      if (system(cmd) != 0)
         printf("   (left %s)\n", root);
   }
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] udev_player_lights_test\n");
   return 0;
}
