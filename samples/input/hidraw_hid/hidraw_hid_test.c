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
 *   bluetooth   -> the address the pad connects to is feature 0xF5,
 *                  read and set most significant byte first
 *   gone        -> a read failing with ENODEV disconnects the pad, frees
 *                  its port and closes the node
 *   free        -> every node closed */

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

extern hid_driver_t hidraw_hid;

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

static fake_node_t nodes[] = {
   { "hidraw0", "DRIVER=hid-generic\nHID_ID=0003:0000046D:0000C31C\n"
                "HID_NAME=Logitech USB Keyboard\n" },
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
         "0xF4 was not set");
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
   /* pressures: axes 4 to 15, circle the eighth */
   CHECK(hidraw_hid.axis(hid, 0, AXIS_POS(7)) == ((200 << 7) | (200 >> 1)),
         "circle's pressure is not axis 7");
   CHECK(hidraw_hid.axis(hid, 0, AXIS_POS(5)) == 0x7fff,
         "R2's pressure is not axis 5");
   CHECK(!hidraw_hid.axis(hid, 0, AXIS_POS(6)),
         "cross has pressure without being pressed");

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

   /* free */
   hidraw_hid.free(hid);
   CHECK(ds3->closes == 2 && disconnects == 2,
         "free left the node open or the pad connected");

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("[pass] hidraw_hid_test\n");
   return 0;
}
