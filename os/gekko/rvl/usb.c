/* Wii: USB through IOS 57 on's USBv5 interfaces: /dev/usb/ven for
 * most devices, /dev/usb/hid for HID-class ones.  Both list devices by
 * id and take the same requests, with these differences on hid: the
 * description is shorter and names its two interrupt endpoints, there
 * are no alternate settings or bulk transfers, and an interrupt
 * transfer goes to the IN or OUT endpoint by a flag in place of an
 * endpoint and data address.  Ids stay unique across the two, so the
 * calls find a device's interface by its id.
 *
 * Every buffer IOS sees comes from the I/O buffers in MEM2, which is
 * where it wants them.  A device-change request stays queued
 * with IOS; its completion refreshes the device list, and the next
 * caller acknowledges it (AttachFinish) and queues the next one. */

#include <errno.h>
#include <string.h>

#include <gekko/ios.h>
#include <gekko/thread.h>
#include <gekko/usb.h>

#include "rvl.h"
#include "../kernel/kernel.h"

#define USB_GET_VERSION    0x00
#define USB_DEVICE_CHANGE  0x01
#define USB_SHUTDOWN       0x02
#define USB_GET_PARAMS     0x03
#define USB_ATTACH_FINISH  0x06
#define USB_SET_ALTERNATE  0x07
#define USB_SUSPEND_RESUME 0x10
#define USB_CANCEL         0x11
#define USB_CTRL           0x12
#define USB_INTR           0x13
#define USB_BULK           0x15

#define USB_VERSION        0x00050001u

#define CHANGE_SIZE        0x180
#define VEN_PARAMS_SIZE    0xc0
#define HID_PARAMS_SIZE    0x60
#define MSG_SIZE           64

struct bus
{
   const char        *path;
   int32_t            fd;
   gk_mutex_t         lock;
   uint8_t           *change_buf;
   gk_usb_dev_t       devices[GK_USB_MAX_DEVICES];
   int                num_devices;
   volatile uint32_t  generation;
   volatile uint32_t  acked;
   int                hid;
};

static struct bus ven = { "/dev/usb/ven", -1, GK_MUTEX_INIT, NULL, {{0}}, 0, 0,
   0, 0 };
static struct bus hid = { "/dev/usb/hid", -1, GK_MUTEX_INIT, NULL, {{0}}, 0, 0,
   0, 1 };

/* Devices open now, and their endpoints, for the exit hook. */
static struct
{
   uint32_t id;
   uint8_t  ep[GK_USB_MAX_ENDPOINTS];
   uint8_t  num_ep;
   uint8_t  used;
} opened[GK_USB_MAX_DEVICES];
static gk_mutex_t opened_lock = GK_MUTEX_INIT;

static void shutdown_hook(void);
static struct gk_exit_hook exit_hook = { shutdown_hook, NULL };
static int                 hooked;
static int                 shut;      /* nothing opens again */

/* ---- the device list ---- */

/* Interrupt context: IOS has written the list. */
static void change_done(int32_t result, void *data)
{
   struct bus *bus = (struct bus*)data;
   int i, n = result < 0 ? 0 : result;
   if (n > GK_USB_MAX_DEVICES)
      n = GK_USB_MAX_DEVICES;
   for (i = 0; i < n; i++)
   {
      const uint8_t *e = bus->change_buf + i * 12;
      gk_usb_dev_t  *d = &bus->devices[i];
      d->id           = ((uint32_t)e[0] << 24) | (e[1] << 16) | (e[2] << 8) | e[3];
      d->vid          = (uint16_t)((e[4] << 8) | e[5]);
      d->pid          = (uint16_t)((e[6] << 8) | e[7]);
      d->interface    = e[10];
      d->alt_settings = e[11];
   }
   bus->num_devices = n;
   bus->generation++;
   gk_futex_wake(&bus->generation, 0x7fffffff);
}

static int arm_change(struct bus *bus)
{
   return gk_ios_ioctl_async(bus->fd, USB_DEVICE_CHANGE, NULL, 0,
         bus->change_buf, CHANGE_SIZE, change_done, bus) < 0 ? -EIO : 0;
}

static int init(struct bus *bus)
{
   uint32_t *ver;
   int ret = 0;
   gk_mutex_lock(&bus->lock);
   if (bus->fd >= 0)
      goto out;
   if (shut)
   {
      ret = -ENODEV;
      goto out;
   }
   if (!bus->change_buf
         && !(bus->change_buf = (uint8_t*)gk_iobuf_get(CHANGE_SIZE)))
   {
      ret = -ENOMEM;
      goto out;
   }
   if ((bus->fd = gk_ios_open(bus->path, 0)) < 0)
   {
      bus->fd = -1;
      ret     = -ENODEV;
      goto out;
   }
   ver = (uint32_t*)gk_iobuf_get(32);
   if (gk_ios_ioctl(bus->fd, USB_GET_VERSION, NULL, 0, ver, 32) < 0
         || ver[0] != USB_VERSION || arm_change(bus))
   {
      gk_ios_close(bus->fd);
      bus->fd = -1;
      ret     = -ENODEV;
   }
   else if (!hooked)
   {
      hooked = 1;
      gk_exit_hook_add(&exit_hook);
   }
   gk_iobuf_put(ver, 32);
out:
   gk_mutex_unlock(&bus->lock);
   return ret;
}

static int list(struct bus *bus, gk_usb_dev_t *out, int max)
{
   uint32_t gen, level;
   int n;
   if (init(bus))
      return 0;
   /* The first list comes straight away. */
   if (!bus->generation)
      gk_futex_wait(&bus->generation, 0, GK_US_TO_TICKS(500000));
   gk_mutex_lock(&bus->lock);
   /* The list changes in the interrupt of a queued change request. */
   level = gk_irq_disable();
   gen   = bus->generation;
   n     = bus->num_devices < max ? bus->num_devices : max;
   memcpy(out, bus->devices, (size_t)n * sizeof(*out));
   gk_irq_restore(level);
   if (bus->acked != gen)
   {
      /* IOS holds further changes back until this one is
       * acknowledged. */
      gk_ios_ioctl(bus->fd, USB_ATTACH_FINISH, NULL, 0, NULL, 0);
      bus->acked = gen;
      arm_change(bus);
   }
   gk_mutex_unlock(&bus->lock);
   return n;
}

int gk_usb_supported(void)
{
   uint32_t *ver = (uint32_t*)gk_iobuf_get(32);
   int32_t   fd  = gk_ios_open(hid.path, 0);
   int       ok  = fd >= 0
      && gk_ios_ioctl(fd, USB_GET_VERSION, NULL, 0, ver, 32) >= 0
      && ver[0] == USB_VERSION;
   if (fd >= 0)
      gk_ios_close(fd);
   gk_iobuf_put(ver, 32);
   return ok;
}

int gk_usb_list(gk_usb_dev_t *out, int max)
{
   return list(&ven, out, max);
}

int gk_usb_hid_list(gk_usb_dev_t *out, int max)
{
   return list(&hid, out, max);
}

/* The interface listing id now; hid first, for an emulator that lists
 * HID devices on both. */
static struct bus *bus_of(uint32_t id)
{
   struct bus *b[2];
   unsigned i;
   int k;
   b[0] = &hid;
   b[1] = &ven;
   for (i = 0; i < 2; i++)
   {
      uint32_t level;
      int found = 0;
      if (b[i]->fd < 0)
         continue;
      level = gk_irq_disable();
      for (k = 0; k < b[i]->num_devices; k++)
         if (b[i]->devices[k].id == id)
            found = 1;
      gk_irq_restore(level);
      if (found)
         return b[i];
   }
   return NULL;
}

/* ---- one device ---- */

/* An ioctl on one device: its id, then arg at byte 8 (a byte argument
 * goes in arg's top byte, a word one is the whole of it). */
static int dev_ioctl(struct bus *bus, uint32_t id, uint32_t cmd, uint32_t arg,
      void *out, uint32_t out_len)
{
   uint32_t *in = (uint32_t*)gk_iobuf_get(32);
   int32_t ret;
   in[0] = id;
   in[2] = arg;
   ret = gk_ios_ioctl(bus->fd, cmd, in, 32, out, out_len);
   gk_iobuf_put(in, 32);
   return ret < 0 ? -EIO : 0;
}

static void endpoint(gk_usb_ep_t *ep, const uint8_t *e)
{
   ep->address    = e[2];
   ep->attributes = e[3];
   ep->max_packet = (uint16_t)((e[4] << 8) | e[5]);
   ep->interval   = e[6];
}

/* Descriptors as IOS lays them out: the device's, then (ven) the
 * interface's and its endpoints', or (hid) the configuration's, the
 * interface's, and the interrupt IN and OUT endpoints'. */
static int get_params(struct bus *bus, uint32_t id, uint8_t alt,
      gk_usb_info_t *info)
{
   uint32_t size = bus->hid ? HID_PARAMS_SIZE : VEN_PARAMS_SIZE;
   uint8_t *p    = (uint8_t*)gk_iobuf_get(size);
   unsigned i;
   int ret;
   if (!(ret = dev_ioctl(bus, id, USB_GET_PARAMS, (uint32_t)alt << 24, p,
               size)))
   {
      const uint8_t *d   = p + (bus->hid ? 36 : 20);
      const uint8_t *ifc = p + (bus->hid ? 68 : 52);
      memset(info, 0, sizeof(*info));
      info->vid          = (uint16_t)((d[8] << 8) | d[9]);
      info->pid          = (uint16_t)((d[10] << 8) | d[11]);
      info->dev_class    = d[4];
      info->dev_subclass = d[5];
      info->dev_protocol = d[6];
      info->if_number    = ifc[2];
      info->if_class     = ifc[5];
      info->if_subclass  = ifc[6];
      info->if_protocol  = ifc[7];
      if (bus->hid)
      {
         for (i = 0; i < 2; i++)
            if (p[80 + i * 8 + 2])
               endpoint(&info->ep[info->num_ep++], p + 80 + i * 8);
      }
      else
      {
         info->num_ep = ifc[4] < GK_USB_MAX_ENDPOINTS ? ifc[4]
            : GK_USB_MAX_ENDPOINTS;
         for (i = 0; i < info->num_ep && 64 + i * 8 + 8 <= size; i++)
            endpoint(&info->ep[i], p + 64 + i * 8);
      }
   }
   gk_iobuf_put(p, size);
   return ret;
}

/* Remembers (info) or forgets (NULL) an open device's endpoints. */
static void track(uint32_t id, const gk_usb_info_t *info)
{
   int i, slot = -1;
   gk_mutex_lock(&opened_lock);
   for (i = 0; i < GK_USB_MAX_DEVICES; i++)
   {
      if (opened[i].used && opened[i].id == id)
         slot = i;
      else if (slot < 0 && !opened[i].used && info)
         slot = i;
   }
   if (slot >= 0)
   {
      opened[slot].used = info != NULL;
      opened[slot].id   = id;
      if (info)
      {
         opened[slot].num_ep = info->num_ep;
         for (i = 0; i < info->num_ep; i++)
            opened[slot].ep[i] = info->ep[i].address;
      }
   }
   gk_mutex_unlock(&opened_lock);
}

int gk_usb_open(uint32_t id, gk_usb_info_t *info)
{
   struct bus *bus = bus_of(id);
   int ret;
   if (!bus)
      return -ENODEV;
   /* IOS wants the device resumed before anything else. */
   if ((ret = dev_ioctl(bus, id, USB_SUSPEND_RESUME, 1, NULL, 0)))
      return ret;
   if (!(ret = get_params(bus, id, 0, info)))
      track(id, info);
   return ret;
}

int gk_usb_set_alt(uint32_t id, uint8_t alt, gk_usb_info_t *info)
{
   struct bus *bus = bus_of(id);
   int ret;
   if (!bus || bus->hid)
      return -ENODEV;
   if ((ret = dev_ioctl(bus, id, USB_SET_ALTERNATE, (uint32_t)alt << 24,
               NULL, 0)))
      return ret;
   if (!(ret = get_params(bus, id, alt, info)))
      track(id, info);
   return ret;
}

void gk_usb_close(uint32_t id)
{
   struct bus *bus = bus_of(id);
   track(id, NULL);
   if (bus)
      dev_ioctl(bus, id, USB_SUSPEND_RESUME, 0, NULL, 0);
}

int gk_usb_cancel(uint32_t id, uint8_t ep)
{
   struct bus *bus = bus_of(id);
   if (!bus)
      return -ENODEV;
   /* hid names the endpoint by kind: control, interrupt IN or OUT. */
   if (bus->hid)
      ep = !ep ? 0 : (ep & 0x80) ? 1 : 2;
   return dev_ioctl(bus, id, USB_CANCEL, (uint32_t)ep << 24, NULL, 0);
}

/* A transfer: the message, then the data as IOS reads or writes it;
 * the message holds the data's address at ptr_at too, if ptr_at. */
static int transfer(struct bus *bus, uint32_t cmd, uint8_t *msg,
      unsigned ptr_at, void *data, uint32_t len, int to_host)
{
   gk_ios_vec_t vec[2];
   uint8_t *buf = NULL;
   int32_t ret;
   if (len)
   {
      if (!(buf = (uint8_t*)gk_iobuf_get(len)))
      {
         gk_iobuf_put(msg, MSG_SIZE);
         return -ENOMEM;
      }
      if (!to_host)
         memcpy(buf, data, len);
      if (ptr_at)
      {
         msg[ptr_at]     = (uint8_t)(GK_PHYS(buf) >> 24);
         msg[ptr_at + 1] = (uint8_t)(GK_PHYS(buf) >> 16);
         msg[ptr_at + 2] = (uint8_t)(GK_PHYS(buf) >> 8);
         msg[ptr_at + 3] = (uint8_t)GK_PHYS(buf);
      }
   }
   vec[0].data = msg;
   vec[0].len  = MSG_SIZE;
   vec[1].data = buf;
   vec[1].len  = len;
   ret = gk_ios_ioctlv(bus->fd, cmd, to_host ? 1 : 2, to_host ? 1 : 0, vec);
   /* Emulators answer some requests with 0 rather than the length:
    * then the whole buffer is the reply. */
   if (to_host && ret >= 0)
      memcpy(data, buf, ret && (uint32_t)ret < len ? (uint32_t)ret : len);
   if (buf)
      gk_iobuf_put(buf, len);
   gk_iobuf_put(msg, MSG_SIZE);
   return ret < 0 ? -EPIPE : (int)ret;
}

static uint8_t *message(uint32_t id)
{
   uint8_t *m = (uint8_t*)gk_iobuf_get(MSG_SIZE);
   m[0] = (uint8_t)(id >> 24);
   m[1] = (uint8_t)(id >> 16);
   m[2] = (uint8_t)(id >> 8);
   m[3] = (uint8_t)id;
   return m;
}

int gk_usb_ctrl(uint32_t id, uint8_t type, uint8_t request, uint16_t value,
      uint16_t index, void *data, uint16_t len)
{
   struct bus *bus = bus_of(id);
   uint8_t *m;
   if (!bus)
      return -ENODEV;
   m = message(id);
   m[8]  = type;
   m[9]  = request;
   m[10] = (uint8_t)(value >> 8);
   m[11] = (uint8_t)value;
   m[12] = (uint8_t)(index >> 8);
   m[13] = (uint8_t)index;
   m[14] = (uint8_t)(len >> 8);
   m[15] = (uint8_t)len;
   return transfer(bus, USB_CTRL, m, 16, data, len, (type & 0x80) != 0);
}

static int data_transfer(uint32_t cmd, unsigned ep_at, uint32_t id,
      uint8_t ep, void *data, uint32_t len)
{
   struct bus *bus = bus_of(id);
   uint8_t *m;
   if (!bus || (bus->hid && cmd != USB_INTR))
      return -ENODEV;
   if (len > 0xffff)
      return -EINVAL;
   m = message(id);
   if (bus->hid)
   {
      /* Which interrupt endpoint: nonzero for OUT. */
      m[11] = (ep & 0x80) ? 0 : 1;
      return transfer(bus, cmd, m, 0, data, len, (ep & 0x80) != 0);
   }
   m[12]    = (uint8_t)(len >> 8);
   m[13]    = (uint8_t)len;
   m[ep_at] = ep;
   return transfer(bus, cmd, m, 8, data, len, (ep & 0x80) != 0);
}

int gk_usb_bulk(uint32_t id, uint8_t ep, void *data, uint32_t len)
{
   return data_transfer(USB_BULK, 18, id, ep, data, len);
}

int gk_usb_intr(uint32_t id, uint8_t ep, void *data, uint32_t len)
{
   return data_transfer(USB_INTR, 14, id, ep, data, len);
}

/* Leaving: transfers waiting on open devices return, IOS answers the
 * queued device-change requests, and the interfaces close, so nothing
 * of this program's is still with IOS. */
static void shutdown_hook(void)
{
   struct bus *b[2];
   unsigned i, k;
   shut = 1;
   for (i = 0; i < GK_USB_MAX_DEVICES; i++)
   {
      if (!opened[i].used)
         continue;
      gk_usb_cancel(opened[i].id, 0);
      for (k = 0; k < opened[i].num_ep; k++)
         gk_usb_cancel(opened[i].id, opened[i].ep[k]);
   }
   b[0] = &ven;
   b[1] = &hid;
   for (i = 0; i < 2; i++)
   {
      uint32_t gen = b[i]->generation;
      if (b[i]->fd < 0)
         continue;
      gk_ios_ioctl(b[i]->fd, USB_SHUTDOWN, NULL, 0, NULL, 0);
      if (b[i]->generation == gen)
         gk_futex_wait(&b[i]->generation, gen, GK_US_TO_TICKS(100000));
      gk_mutex_lock(&b[i]->lock);
      gk_ios_close(b[i]->fd);
      b[i]->fd          = -1;
      b[i]->num_devices = 0;
      gk_mutex_unlock(&b[i]->lock);
   }
}
