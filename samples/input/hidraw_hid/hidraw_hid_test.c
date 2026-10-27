/* Regression test for input/drivers_hid/hidraw_hid.c: the Linux hidraw
 * HID driver, against stand-ins for the calls it makes on /dev and
 * /sys (linked with --wrap), with the real pad handlers behind it.
 *
 *   scan        -> only the nodes whose sysfs ids name a handled pad are
 *                  opened; one that cannot be opened is skipped
 *   start       -> a DualShock 3 is started through hidraw: its LED
 *                  report written, 0xF4 set, 0xF2 and 0xF5 read
 *   poll        -> the newest of the reports waiting is the pad's state;
 *                  with none waiting, one read and the state stays
 *   pressures   -> L2, R2 and the face, shoulder and D-pad buttons'
 *                  pressures are axes 4 to 15
 *   rumble      -> the motors go in the output report with the lights,
 *                  written at the pad's next report, once a change
 *   bluetooth   -> the address the pad connects to is feature 0xF5,
 *                  read and set most significant byte first
 *   gone        -> a read failing with ENODEV disconnects the pad, frees
 *                  its port and closes the node
 *   free        -> every node closed
 *   dualsense   -> any pad with a handler is taken, not the DualShock 3
 *                  alone: a DualSense's node is opened, on port 1 with
 *                  its ids, and a report read through its handler
 *   bus         -> a handler is told Bluetooth by the node's bus: a
 *                  Switch Pro Controller greeted on USB, not on Bluetooth
 *   no handler  -> a pad with none is read from its report descriptor,
 *                  numbered as on the Mac, named as it names itself; a
 *                  keyboard's descriptor is no pad
 *   lend        -> a pad's state is lent where its handler keeps it:
 *                  what the reads answer, the same storage poll after
 *                  poll, the hat of a pad read from its descriptor in
 *                  it; none for a port with no pad
 *   bridge      -> what the snapshot bridge assumes of a driver it
 *                  serves (samples/input/joypad_bridge_checked): its
 *                  button(), axis() and state() say what get_buttons()
 *                  and the lent state say - a home button's menu
 *                  toggle and a descriptor pad's hat among them */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/hidraw.h>

#include <boolean.h>

#include "input/input_driver.h"
#include "input/connect/joypad_connection.h"
#include "input/common/input_ignore_devices.h"

/* the frontend's; the user's list is the settings', here none */
bool input_device_ignored(uint16_t vid, uint16_t pid)
{ return input_ignore_devices_check(NULL, vid, pid); }

extern hid_driver_t hidraw_hid;

#include "../joypad_bridge_checked/bridge_view.h"

/* hidraw as the HID joypad driver (hid_joypad.c) has it */
static void *bridge_hid;
static int32_t bridge_button(unsigned pad, uint16_t key)
{ return hidraw_hid.button(bridge_hid, pad, key); }
static int16_t bridge_axis(unsigned pad, uint32_t axis)
{ return hidraw_hid.axis(bridge_hid, pad, axis); }
static void bridge_get_buttons(unsigned pad, input_bits_t *state)
{ hidraw_hid.get_buttons(bridge_hid, pad, state); }
static int16_t bridge_state(rarch_joypad_info_t *info,
      const uint16_t *keys, const uint32_t *axes, unsigned port)
{ return hidraw_hid.state(bridge_hid, info, keys, axes, port); }
static const input_pad_view_t *bridge_view(unsigned pad)
{ return hidraw_hid.view(bridge_hid, pad); }

/* the disagreements the bridge would find on pad 0 of @hid */
static unsigned bridge_check(void *hid)
{
   input_device_driver_t drv;
   memset(&drv, 0, sizeof(drv));
   drv.button      = bridge_button;
   drv.axis        = bridge_axis;
   drv.get_buttons = bridge_get_buttons;
   drv.state       = bridge_state;
   drv.view        = bridge_view;
   bridge_hid      = hid;
   return bv_compare(&drv, 0) + bv_compare_view(&drv, 0);
}

static int failures;
#define CHECK(cond, msg) do { if (!(cond)) { \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); failures++; } } while (0)

/* ---- the rest of RetroArch, stood in for ---- */

void RARCH_LOG(const char *f, ...)  { (void)f; }
void RARCH_DBG(const char *f, ...)  { (void)f; }
void RARCH_ERR(const char *f, ...)  { (void)f; }
static int warnings;
void RARCH_WARN(const char *f, ...) { (void)f; warnings++; }

static int connected_port = -1;
static unsigned connected_vid, connected_pid;
static int disconnects;
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port,
      unsigned vid, unsigned pid)
{
   connected_port = (int)port;
   connected_vid  = vid;
   connected_pid  = pid;
   return true;
}
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{
   disconnects++;
   return true;
}
void input_pad_connect(unsigned port, input_device_driver_t *driver) { }

/* ---- the nodes ---- */

typedef struct
{
   const char *node;
   const char *uevent;
   int open_errno;     /* the node cannot be opened */
   const uint8_t *desc; /* its report descriptor, in sysfs */
   int desc_len;
   int open_flags;
   int opens;
   int closes;
   int uevent_opens;
   /* reports waiting */
   uint8_t queue[8][49];
   int queued;
   int gone;           /* reads fail with ENODEV */
   int reads;
   /* what was written and set */
   uint8_t out[49];
   int writes;
   uint8_t feature_set[8];
   int features_set;
   int features_got[256];
} fake_node_t;

/* a keyboard's report descriptor, and a common USB pad's */
static const uint8_t keyboard_desc[] = {
   0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7,
   0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01,
   0x75, 0x08, 0x81, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
   0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xc0
};
static const uint8_t pad_desc[] = {
   0x05, 0x01, 0x09, 0x04, 0xa1, 0x01, 0xa1, 0x02, 0x75, 0x08, 0x95, 0x05,
   0x15, 0x00, 0x26, 0xff, 0x00, 0x35, 0x00, 0x46, 0xff, 0x00, 0x09, 0x30,
   0x09, 0x30, 0x09, 0x30, 0x09, 0x30, 0x09, 0x31, 0x81, 0x02, 0x75, 0x04,
   0x95, 0x01, 0x25, 0x07, 0x46, 0x3b, 0x01, 0x65, 0x14, 0x09, 0x39, 0x81,
   0x42, 0x65, 0x00, 0x75, 0x01, 0x95, 0x0c, 0x25, 0x01, 0x45, 0x01, 0x05,
   0x09, 0x19, 0x01, 0x29, 0x0c, 0x81, 0x02, 0x06, 0x00, 0xff, 0x75, 0x01,
   0x95, 0x08, 0x25, 0x01, 0x45, 0x01, 0x09, 0x01, 0x81, 0x02, 0xc0, 0xa1,
   0x02, 0x75, 0x08, 0x95, 0x07, 0x46, 0xff, 0x00, 0x26, 0xff, 0x00, 0x09,
   0x02, 0x91, 0x02, 0xc0, 0xc0
};

static fake_node_t nodes[] = {
   { "hidraw0", "DRIVER=hid-generic\nHID_ID=0003:0000046D:0000C31C\n"
                "HID_NAME=Logitech USB Keyboard\n", 0,
                keyboard_desc, (int)sizeof(keyboard_desc) },
   { "hidraw1", "DRIVER=sony\nHID_ID=0003:0000054C:00000268\n"
                "HID_NAME=Sony PLAYSTATION(R)3 Controller\n" },
   { "hidraw2", "DRIVER=sony\nHID_ID=0003:0000054C:00000268\n"
                "HID_NAME=Sony PLAYSTATION(R)3 Controller\n", EACCES },
};
#define NODES (int)(sizeof(nodes) / sizeof(nodes[0]))

/* fds: 100 + n for a node, 200 + n for its uevent */
static int uevent_pos[NODES];

int __wrap_open(const char *path, int flags, ...)
{
   int i;
   for (i = 0; i < NODES; i++)
   {
      char p[128];
      snprintf(p, sizeof(p), "/dev/%s", nodes[i].node);
      if (!strcmp(path, p))
      {
         if (nodes[i].open_errno)
         {
            errno = nodes[i].open_errno;
            return -1;
         }
         nodes[i].opens++;
         nodes[i].open_flags = flags;
         return 100 + i;
      }
      snprintf(p, sizeof(p), "/sys/class/hidraw/%s/device/report_descriptor",
            nodes[i].node);
      if (!strcmp(path, p))
      {
         if (!nodes[i].desc)
         {
            errno = ENOENT;
            return -1;
         }
         return 300 + i;
      }
      snprintf(p, sizeof(p), "/sys/class/hidraw/%s/device/uevent",
            nodes[i].node);
      if (!strcmp(path, p))
      {
         nodes[i].uevent_opens++;
         uevent_pos[i] = 0;
         return 200 + i;
      }
   }
   errno = ENOENT;
   return -1;
}

ssize_t __wrap_read(int fd, void *buf, size_t len)
{
   if (fd >= 200 && fd < 200 + NODES)
   {
      const char *s = nodes[fd - 200].uevent + uevent_pos[fd - 200];
      size_t n      = strlen(s);
      if (n > len)
         n = len;
      memcpy(buf, s, n);
      uevent_pos[fd - 200] += (int)n;
      return (ssize_t)n;
   }
   if (fd >= 300 && fd < 300 + NODES)
   {
      fake_node_t *nd = &nodes[fd - 300];
      size_t n        = (size_t)nd->desc_len < len ? (size_t)nd->desc_len : len;
      memcpy(buf, nd->desc, n);
      return (ssize_t)n;
   }
   if (fd >= 100 && fd < 100 + NODES)
   {
      fake_node_t *nd = &nodes[fd - 100];
      nd->reads++;
      if (nd->gone)
      {
         errno = ENODEV;
         return -1;
      }
      if (!nd->queued)
      {
         errno = EAGAIN;
         return -1;
      }
      memcpy(buf, nd->queue[0], len < 49 ? len : 49);
      memmove(nd->queue[0], nd->queue[1], sizeof(nd->queue[0]) * 7);
      nd->queued--;
      return 49;
   }
   errno = EBADF;
   return -1;
}

ssize_t __wrap_write(int fd, const void *buf, size_t len)
{
   fake_node_t *nd;
   if (fd < 100 || fd >= 100 + NODES)
   {
      errno = EBADF;
      return -1;
   }
   nd = &nodes[fd - 100];
   memcpy(nd->out, buf, len < sizeof(nd->out) ? len : sizeof(nd->out));
   nd->writes++;
   return (ssize_t)len;
}

int __wrap_ioctl(int fd, unsigned long req, ...)
{
   va_list ap;
   uint8_t *buf;
   fake_node_t *nd;
   unsigned size = _IOC_SIZE(req);

   va_start(ap, req);
   buf = va_arg(ap, uint8_t*);
   va_end(ap);
   if (fd < 100 || fd >= 100 + NODES)
   {
      errno = EBADF;
      return -1;
   }
   nd = &nodes[fd - 100];
   if (req == HIDIOCSFEATURE(size))
   {
      memcpy(nd->feature_set, buf, size < 8 ? size : 8);
      nd->features_set++;
      /* a DualShock 3 on USB stalls 0xF4, Bluetooth's start */
      if (buf[0] == 0xf4)
      {
         errno = EPIPE;
         return -1;
      }
      return (int)size;
   }
   if (req == HIDIOCGFEATURE(size))
   {
      nd->features_got[buf[0]]++;
      memset(buf + 1, 0, size - 1);
      /* the address the pad connects to, as last set */
      if (buf[0] == 0xf5 && size >= 8)
         memcpy(buf + 2, nd->feature_set + 2, 6);
      return (int)size;
   }
   errno = EINVAL;
   return -1;
}

int __wrap_close(int fd)
{
   if (fd >= 300)
      return 0;
   if (fd >= 100 && fd < 100 + NODES)
      nodes[fd - 100].closes++;
   return 0;
}

static struct dirent dir_entries[NODES + 2];
static int dir_pos;
static int dir_open;
DIR *__wrap_opendir(const char *path)
{
   if (strcmp(path, "/dev"))
      return NULL;
   dir_pos = 0;
   dir_open++;
   return (DIR*)&dir_pos;
}
struct dirent *__wrap_readdir(DIR *d)
{
   int i = dir_pos++;
   if (i == 0)
   {
      strcpy(dir_entries[0].d_name, "null");
      return &dir_entries[0];
   }
   if (i - 1 < NODES)
   {
      strcpy(dir_entries[i].d_name, nodes[i - 1].node);
      return &dir_entries[i];
   }
   return NULL;
}
int __wrap_closedir(DIR *d)
{
   dir_open--;
   return 0;
}

/* ---- the DualShock 3's reports ---- */

static void ds3_report(uint8_t *r, uint32_t buttons, uint8_t lx)
{
   memset(r, 0, 49);
   r[0] = 0x01;
   r[2] = (uint8_t)buttons;
   r[3] = (uint8_t)(buttons >> 8);
   r[6] = lx;
   r[7] = r[8] = r[9] = 128;
   /* circle's pressure, and R2's */
   if (buttons & (1u << 13))
      r[23] = 200;
   r[19] = 255;
}

static void queue_report(fake_node_t *nd, uint32_t buttons, uint8_t lx)
{
   ds3_report(nd->queue[nd->queued++], buttons, lx);
}

int main(void)
{
   fake_node_t *kbd = &nodes[0];
   fake_node_t *ds3 = &nodes[1];
   fake_node_t *shut = &nodes[2];
   input_bits_t bits;
   void *hid;
   int16_t x;
   /* report byte 3 bit 6: cross, the RetroPad's B */
   const uint32_t cross  = 1u << 14;
   const uint32_t circle = 1u << 13;

   hid = hidraw_hid.init();
   CHECK(hid != NULL, "init failed");
   CHECK(!dir_open, "/dev left open");

   /* scan */
   CHECK(kbd->uevent_opens == 1 && !kbd->opens,
         "a node no handler takes was opened, or its ids not read");
   CHECK(ds3->opens == 1, "the DualShock 3's node was not opened");
   CHECK((ds3->open_flags & O_ACCMODE) == O_RDWR
         && (ds3->open_flags & O_NONBLOCK),
         "the node is not opened for reads that do not block");
   CHECK(!shut->opens && warnings == 1,
         "a node that cannot be opened is not reported");
   CHECK(connected_port == 0 && connected_vid == 0x054c
         && connected_pid == 0x0268,
         "the pad is not on port 1 with its ids");

   /* start */
   CHECK(ds3->writes >= 1 && ds3->out[0] == 0x01 && ds3->out[10] == 0x02,
         "the LED report for port 1 was not written");
   CHECK(ds3->features_set == 1 && ds3->feature_set[0] == 0xf4
         && ds3->feature_set[1] == 0x42,
         "0xF4 was not tried");
   CHECK(ds3->features_got[0xf2] == 1 && ds3->features_got[0xf5] == 1,
         "0xF2 and 0xF5 were not read");

   /* the newest report is the state */
   queue_report(ds3, cross, 0);
   queue_report(ds3, cross | circle, 0);
   queue_report(ds3, cross, 0);
   queue_report(ds3, circle, 255);
   ds3->reads = 0;
   hidraw_hid.poll(hid);
   CHECK(ds3->reads == 5, "the reports waiting were not all taken");
   hidraw_hid.get_buttons(hid, 0, &bits);
   CHECK(   BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_A)
         && !BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_B),
         "the state is not the newest report's");
   CHECK(hidraw_hid.button(hid, 0, RETRO_DEVICE_ID_JOYPAD_A)
         && !hidraw_hid.button(hid, 0, RETRO_DEVICE_ID_JOYPAD_B),
         "the buttons are not the newest report's");
   x = hidraw_hid.axis(hid, 0, AXIS_POS(0));
   CHECK(x > 30000, "the stick is not the newest report's");

   /* lend: the handler's own state, what the reads answered */
   {
      const input_pad_view_t *v = hidraw_hid.view(hid, 0);
      CHECK(v != NULL, "the DualShock 3's state is not lent");
      CHECK(!hidraw_hid.view(hid, 1), "a port with no pad lends a state");
      if (v)
      {
         CHECK(!memcmp(&v->buttons, &bits, sizeof(bits)) && v->axes[0] == x
               && v->axes[5] == hidraw_hid.axis(hid, 0, AXIS_POS(5))
               && !v->hats,
               "the lent state is not what the reads answer");
         queue_report(ds3, cross, 0);
         hidraw_hid.poll(hid);
         CHECK(hidraw_hid.view(hid, 0) == v
               && BIT256_GET(v->buttons, RETRO_DEVICE_ID_JOYPAD_B)
               && !BIT256_GET(v->buttons, RETRO_DEVICE_ID_JOYPAD_A)
               && v->axes[0] < -30000,
               "the lent state is not the handler's, kept up at each poll");
         queue_report(ds3, circle, 255);
         hidraw_hid.poll(hid);
      }
      CHECK(!bridge_check(hid),
            "the DualShock 3: the reads say what the bridge would not");
   }

   /* rumble: one output report at the next report the pad sends,
    * the strong motor's force and the weak one only on or off; none
    * again unchanged */
   {
      int writes = ds3->writes;
      hidraw_hid.set_rumble(hid, 0, RETRO_RUMBLE_STRONG, 0x8000);
      hidraw_hid.set_rumble(hid, 0, RETRO_RUMBLE_WEAK, 0x0100);
      CHECK(ds3->writes == writes, "rumble was written outside the pad's report");
      queue_report(ds3, circle, 255);
      hidraw_hid.poll(hid);
      CHECK(ds3->writes == writes + 1 && ds3->out[0] == 0x01
            && ds3->out[2] == 0xff && ds3->out[3] == 1
            && ds3->out[4] == 0xff && ds3->out[5] == 0x80
            && ds3->out[10] == 0x02,
            "the rumble report is not the motors asked for, with the lights");
      queue_report(ds3, circle, 255);
      hidraw_hid.poll(hid);
      CHECK(ds3->writes == writes + 1, "rumble was written again unchanged");
      hidraw_hid.set_rumble(hid, 0, RETRO_RUMBLE_STRONG, 0);
      hidraw_hid.set_rumble(hid, 0, RETRO_RUMBLE_WEAK, 0);
      queue_report(ds3, circle, 255);
      hidraw_hid.poll(hid);
      CHECK(ds3->writes == writes + 2 && !ds3->out[3] && !ds3->out[5],
            "the motors were not stopped");
   }

   /* the left stick half up and the right one fully down: up is
    * negative, down positive, neither turned round */
   ds3->reads = 0;
   queue_report(ds3, 0, 128);
   ds3->queue[ds3->queued - 1][7] = 64;
   ds3->queue[ds3->queued - 1][9] = 255;
   hidraw_hid.poll(hid);
   CHECK(hidraw_hid.axis(hid, 0, AXIS_NEG(1)) < -15000
         && !hidraw_hid.axis(hid, 0, AXIS_POS(1)),
         "the left stick held half up does not read up");
   CHECK(hidraw_hid.axis(hid, 0, AXIS_POS(3)) > 32000,
         "the right stick held down does not read fully down");
   CHECK(!hidraw_hid.axis(hid, 0, AXIS_NEG(0))
         && hidraw_hid.axis(hid, 0, AXIS_POS(0)) < 1000,
         "the centred left stick reads off centre");
   queue_report(ds3, circle, 255);
   hidraw_hid.poll(hid);
   /* pressures: axes 4 to 15, circle the eighth */
   CHECK(hidraw_hid.axis(hid, 0, AXIS_POS(7)) == ((200 << 7) | (200 >> 1)),
         "circle's pressure is not axis 7");
   CHECK(hidraw_hid.axis(hid, 0, AXIS_POS(5)) == 0x7fff,
         "R2's pressure is not axis 5");
   CHECK(!hidraw_hid.axis(hid, 0, AXIS_POS(6)),
         "cross has pressure without being pressed");

   /* every pressure from its byte - L2, R2, cross, circle, square,
    * triangle, L1, R1, up, down, left, right - and kept, not the
    * report: a report that is not the input report leaves them */
   {
      static const uint8_t byte_of[12] = {
         18, 19, 24, 23, 25, 22, 20, 21, 14, 16, 17, 15 };
      unsigned a;
      bool all = true;
      queue_report(ds3, circle, 128);
      for (a = 0; a < 12; a++)
         ds3->queue[ds3->queued - 1][byte_of[a]] = (uint8_t)(10 + a * 20);
      hidraw_hid.poll(hid);
      ds3->queue[ds3->queued][0] = 0x02;
      ds3->queued++;
      hidraw_hid.poll(hid);
      for (a = 0; a < 12; a++)
      {
         unsigned b = 10 + a * 20;
         if (hidraw_hid.axis(hid, 0, AXIS_POS(4 + a))
               != (int16_t)((b << 7) | (b >> 1)))
            all = false;
      }
      CHECK(all, "a pressure is not from its byte, or another report changed it");
   }

   /* nothing waiting */
   ds3->reads = 0;
   hidraw_hid.poll(hid);
   CHECK(ds3->reads == 1, "more than one read with nothing waiting");
   CHECK(hidraw_hid.button(hid, 0, RETRO_DEVICE_ID_JOYPAD_A),
         "the state was lost with nothing waiting");

   /* a report whose ID is not the one named is not sent */
   {
      uint8_t r[5] = { 0xf4, 0x42, 0x03, 0x00, 0x00 };
      hid_driver_t *drv = &hidraw_hid;
      CHECK(drv->set_report(NULL, HID_REPORT_FEATURE, 0xf4, r, 5) < 0,
            "a report with no device was sent");
   }

   /* the Bluetooth address it connects to: read, and set */
   {
      static const uint8_t host[6] = { 0x00, 0x1a, 0x7d, 0xda, 0x71, 0x13 };
      uint8_t addr[6];
      hid_driver_t *drv = &hidraw_hid;
      CHECK(drv->set_bt_host(hid, 0, host),
            "the address could not be set");
      CHECK(   ds3->feature_set[0] == 0xf5 && ds3->feature_set[1] == 0x00
            && !memcmp(ds3->feature_set + 2, host, 6),
            "0xF5 was not set to the address, most significant byte first");
      CHECK(drv->get_bt_host(hid, 0, addr) && !memcmp(addr, host, 6),
            "the address set is not the one read");
      CHECK(!drv->get_bt_host(hid, 1, addr),
            "a port with no pad has an address");
   }

   /* gone */
   ds3->gone = 1;
   hidraw_hid.poll(hid);
   CHECK(disconnects == 1, "the pad was not disconnected");
   CHECK(ds3->closes == 1, "the node was not closed");
   CHECK(!hidraw_hid.button(hid, 0, RETRO_DEVICE_ID_JOYPAD_A),
         "the port still reads the pad");
   ds3->reads = 0;
   hidraw_hid.poll(hid);
   CHECK(!ds3->reads, "a node that is gone is still read");

   /* back, on the same port */
   ds3->gone   = 0;
   ds3->queued = 0;
   connected_port = -1;
   hidraw_hid.free(hid);
   hid = hidraw_hid.init();
   CHECK(connected_port == 0 && ds3->opens == 2,
         "the pad did not come back on port 1");

   /* free: the node closed, and the pad not said to have left - it is
    * still plugged in, and the next driver reports it again */
   hidraw_hid.free(hid);
   CHECK(ds3->closes == 2 && disconnects == 1,
         "free left the node open, or said a pad still there had left");

   /* a DualSense where the keyboard was */
   kbd->uevent = "DRIVER=playstation\nHID_ID=0003:0000054C:00000CE6\n"
                 "HID_NAME=Sony Interactive Entertainment DualSense Wireless Controller\n";
   ds3->uevent = nodes[0].uevent;
   ds3->open_errno = EACCES;
   kbd->opens = kbd->closes = 0;
   connected_port = -1;
   warnings       = 0;
   hid = hidraw_hid.init();
   CHECK(kbd->opens == 1 && connected_port == 0 && connected_vid == 0x054c
         && connected_pid == 0x0ce6,
         "a DualSense's node was not opened, on port 1 with its ids");
   {
      /* the stand-in reads 49 bytes: under USB's 64, so read as
       * Bluetooth's short report - cross, the hat at rest */
      memset(kbd->queue[0], 0, sizeof(kbd->queue[0]));
      kbd->queue[0][0] = 0x01;
      kbd->queue[0][1] = kbd->queue[0][2] = kbd->queue[0][3] = kbd->queue[0][4] = 0x80;
      kbd->queue[0][5] = 0x08 | 0x20;
      kbd->queued      = 1;
      hidraw_hid.poll(hid);
      hidraw_hid.get_buttons(hid, 0, &bits);
      CHECK(BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_B)
            && !BIT256_GET(bits, RETRO_DEVICE_ID_JOYPAD_A),
            "the DualSense's report was not read through its handler");
      /* the PS button held: the menu toggle, past the first 32 */
      memset(kbd->queue[0], 0, sizeof(kbd->queue[0]));
      kbd->queue[0][0] = 0x01;
      kbd->queue[0][1] = kbd->queue[0][2] = kbd->queue[0][3] = kbd->queue[0][4] = 0x80;
      kbd->queue[0][5] = 0x08 | 0x20;
      kbd->queue[0][7] = 0x01;
      kbd->queued      = 1;
      hidraw_hid.poll(hid);
      CHECK(hidraw_hid.button(hid, 0, RARCH_MENU_TOGGLE),
            "the DualSense's PS button is not the menu toggle through button()");
      CHECK(!bridge_check(hid),
            "the DualSense: the reads say what the bridge would not");
   }
   hidraw_hid.free(hid);
   CHECK(kbd->closes == 1, "the DualSense's node was not closed");

   /* a Switch Pro Controller: on USB it is greeted; on Bluetooth, which
    * the bus tells, asked for its full reports at once */
   kbd->uevent = "DRIVER=nintendo\nHID_ID=0003:0000057E:00002009\n"
                 "HID_NAME=Nintendo Co., Ltd. Pro Controller\n";
   kbd->writes = 0;
   hid = hidraw_hid.init();
   CHECK(kbd->writes == 1 && kbd->out[0] == 0x80 && kbd->out[1] == 0x02,
         "a Switch Pro Controller on USB was not greeted");
   hidraw_hid.free(hid);
   kbd->uevent = "DRIVER=nintendo\nHID_ID=0005:0000057E:00002009\n"
                 "HID_NAME=Pro Controller\n";
   kbd->writes = 0;
   hid = hidraw_hid.init();
   CHECK(kbd->writes == 1 && kbd->out[0] == 0x01 && kbd->out[10] == 0x03
         && kbd->out[11] == 0x30,
         "a Switch Pro Controller on Bluetooth was not asked for full reports");
   hidraw_hid.free(hid);

   /* a pad with no handler where the DualSense was */
   kbd->uevent   = "DRIVER=hid-generic\nHID_ID=0003:00001234:00005678\n"
                   "HID_NAME=Generic USB Joystick\n";
   kbd->desc     = pad_desc;
   kbd->desc_len = (int)sizeof(pad_desc);
   kbd->opens    = kbd->closes = 0;
   connected_port = -1;
   hid = hidraw_hid.init();
   CHECK(kbd->opens == 1 && connected_port == 0 && connected_vid == 0x1234,
         "a pad with no handler was not opened from its descriptor");
   CHECK(!strcmp(hidraw_hid.name(hid, 0), "Generic USB Joystick"),
         "a pad with no handler is not known by its name");
   memset(kbd->queue[0], 0, sizeof(kbd->queue[0]));
   kbd->queue[0][0] = 0xff;                 /* X: right */
   kbd->queue[0][4] = 0x00;                 /* Y: up */
   kbd->queue[0][5] = 0x06 | (0x02 << 4);   /* hat left, button 2 */
   kbd->queued      = 1;
   hidraw_hid.poll(hid);
   CHECK(   hidraw_hid.axis(hid, 0, AXIS_POS(0)) == 32767
         && hidraw_hid.axis(hid, 0, AXIS_NEG(1)) == -32767,
         "a pad with no handler: its axes are not read");
   CHECK(   hidraw_hid.button(hid, 0, 1) && !hidraw_hid.button(hid, 0, 0)
         && hidraw_hid.button(hid, 0, HAT_MAP(0, HAT_LEFT_MASK))
         && !hidraw_hid.button(hid, 0, HAT_MAP(0, HAT_RIGHT_MASK)),
         "a pad with no handler: its buttons or hat are not read");
   {
      const input_pad_view_t *v = hidraw_hid.view(hid, 0);
      CHECK(v && v->hats == 4 && v->axes[0] == 32767
            && BIT256_GET(v->buttons, 1) && !BIT256_GET(v->buttons, 0),
            "a pad with no handler: its state, hat and all, is not lent");
      CHECK(!bridge_check(hid),
            "a pad with no handler: the reads say what the bridge would not");
   }
   hidraw_hid.free(hid);

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("[pass] hidraw_hid_test\n");
   return 0;
}
