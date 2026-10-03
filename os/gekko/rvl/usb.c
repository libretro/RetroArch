/* Wii: USB through /dev/usb/ven, the USBv5 interface of IOS 57 on.
 *
 * Every buffer IOS sees comes from a pool in MEM2, which is where it
 * wants them, 32-byte aligned.  A device-change request stays queued
 * with IOS; its completion refreshes the device list, and the next
 * caller acknowledges it (AttachFinish) and queues the next one. */

#include <errno.h>
#include <string.h>

#include <gekko/ios.h>
#include <gekko/thread.h>
#include <gekko/usb.h>

#define VEN_GET_VERSION    0x00
#define VEN_DEVICE_CHANGE  0x01
#define VEN_GET_PARAMS     0x03
#define VEN_ATTACH_FINISH  0x06
#define VEN_SET_ALTERNATE  0x07
#define VEN_SUSPEND_RESUME 0x10
#define VEN_CANCEL         0x11
#define VEN_CTRL           0x12
#define VEN_INTR           0x13
#define VEN_BULK           0x15

#define VEN_VERSION        0x00050001u

#define CHANGE_SIZE        0x180
#define PARAMS_SIZE        0xc0
#define MSG_SIZE           64

#define POOL_SIZE          (256 * 1024)
#define POOL_UNIT          64
#define POOL_UNITS         (POOL_SIZE / POOL_UNIT)

/* ---- buffers in MEM2 ---- */

static uint8_t   *pool;
static uint8_t    pool_busy[POOL_UNITS / 8];
static gk_mutex_t pool_lock = GK_MUTEX_INIT;
static gk_cond_t  pool_freed = GK_COND_INIT;

/* A zeroed buffer; waits while the pool is full. */
static void *pool_get(uint32_t size)
{
   uint32_t n = (size + POOL_UNIT - 1) / POOL_UNIT, i, run = 0;
   void *p = NULL;
   if (!n || n > POOL_UNITS)
      return NULL;
   gk_mutex_lock(&pool_lock);
   while (!p)
   {
      for (i = 0, run = 0; i < POOL_UNITS; i++)
      {
         run = (pool_busy[i / 8] & (1u << (i % 8))) ? 0 : run + 1;
         if (run == n)
            break;
      }
      if (run == n)
      {
         uint32_t start = i + 1 - n, k;
         for (k = start; k <= i; k++)
            pool_busy[k / 8] |= (uint8_t)(1u << (k % 8));
         p = pool + (size_t)start * POOL_UNIT;
      }
      else
         gk_cond_wait(&pool_freed, &pool_lock, GK_WAIT_FOREVER);
   }
   gk_mutex_unlock(&pool_lock);
   memset(p, 0, size);
   return p;
}

static void pool_put(void *p, uint32_t size)
{
   uint32_t start = (uint32_t)((uint8_t*)p - pool) / POOL_UNIT, k;
   uint32_t n = (size + POOL_UNIT - 1) / POOL_UNIT;
   gk_mutex_lock(&pool_lock);
   for (k = start; k < start + n; k++)
      pool_busy[k / 8] &= (uint8_t)~(1u << (k % 8));
   gk_cond_broadcast(&pool_freed);
   gk_mutex_unlock(&pool_lock);
}

/* ---- the device list ---- */

static int32_t           fd = -1;
static gk_mutex_t        init_lock = GK_MUTEX_INIT;
static uint8_t          *change_buf;
static gk_usb_dev_t      devices[GK_USB_MAX_DEVICES];
static int               num_devices;
static volatile uint32_t generation;
static volatile uint32_t acked;

/* Interrupt context: IOS has written the list. */
static void change_done(int32_t result, void *data)
{
   int i, n = result < 0 ? 0 : result;
   (void)data;
   if (n > GK_USB_MAX_DEVICES)
      n = GK_USB_MAX_DEVICES;
   for (i = 0; i < n; i++)
   {
      const uint8_t *e = change_buf + i * 12;
      devices[i].id           = ((uint32_t)e[0] << 24) | (e[1] << 16)
         | (e[2] << 8) | e[3];
      devices[i].vid          = (uint16_t)((e[4] << 8) | e[5]);
      devices[i].pid          = (uint16_t)((e[6] << 8) | e[7]);
      devices[i].interface    = e[10];
      devices[i].alt_settings = e[11];
   }
   num_devices = n;
   generation++;
   gk_futex_wake(&generation, 0x7fffffff);
}

static int arm_change(void)
{
   return gk_ios_ioctl_async(fd, VEN_DEVICE_CHANGE, NULL, 0, change_buf,
         CHANGE_SIZE, change_done, NULL) < 0 ? -EIO : 0;
}

static int init(void)
{
   uint32_t *ver;
   int ret = 0;
   gk_mutex_lock(&init_lock);
   if (fd >= 0)
      goto out;
   if (!pool && !(pool = (uint8_t*)gk_arena_take_top(&gk_mem2, POOL_SIZE,
               32)))
   {
      ret = -ENOMEM;
      goto out;
   }
   if (!change_buf && !(change_buf = (uint8_t*)pool_get(CHANGE_SIZE)))
   {
      ret = -ENOMEM;
      goto out;
   }
   if ((fd = gk_ios_open("/dev/usb/ven", 0)) < 0)
   {
      ret = -ENODEV;
      goto out;
   }
   ver = (uint32_t*)pool_get(32);
   if (gk_ios_ioctl(fd, VEN_GET_VERSION, NULL, 0, ver, 32) < 0
         || ver[0] != VEN_VERSION || arm_change())
   {
      gk_ios_close(fd);
      fd  = -1;
      ret = -ENODEV;
   }
   pool_put(ver, 32);
out:
   gk_mutex_unlock(&init_lock);
   return ret;
}

int gk_usb_list(gk_usb_dev_t *out, int max)
{
   uint32_t gen, level;
   int n;
   if (init())
      return 0;
   /* The first list comes straight away. */
   if (!generation)
      gk_futex_wait(&generation, 0, GK_US_TO_TICKS(500000));
   gk_mutex_lock(&init_lock);
   /* The list changes in the interrupt of a queued change request. */
   level = gk_irq_disable();
   gen   = generation;
   n     = num_devices < max ? num_devices : max;
   memcpy(out, devices, (size_t)n * sizeof(*out));
   gk_irq_restore(level);
   if (acked != gen)
   {
      /* IOS holds further changes back until this one is
       * acknowledged. */
      gk_ios_ioctl(fd, VEN_ATTACH_FINISH, NULL, 0, NULL, 0);
      acked = gen;
      arm_change();
   }
   gk_mutex_unlock(&init_lock);
   return n;
}

/* ---- one device ---- */

/* An ioctl on one device: its id, then arg at byte 8 (a byte argument
 * goes in arg's top byte, a word one is the whole of it). */
static int dev_ioctl(uint32_t id, uint32_t cmd, uint32_t arg, void *out,
      uint32_t out_len)
{
   uint32_t *in = (uint32_t*)pool_get(32);
   int32_t ret;
   in[0] = id;
   in[2] = arg;
   ret = gk_ios_ioctl(fd, cmd, in, 32, out, out_len);
   pool_put(in, 32);
   return ret < 0 ? -EIO : 0;
}

static int get_params(uint32_t id, uint8_t alt, gk_usb_info_t *info)
{
   uint8_t *p = (uint8_t*)pool_get(PARAMS_SIZE);
   unsigned i;
   int ret;
   if (!(ret = dev_ioctl(id, VEN_GET_PARAMS, (uint32_t)alt << 24, p,
               PARAMS_SIZE)))
   {
      const uint8_t *d = p + 20, *ifc = p + 52;
      memset(info, 0, sizeof(*info));
      info->vid          = (uint16_t)((d[8] << 8) | d[9]);
      info->pid          = (uint16_t)((d[10] << 8) | d[11]);
      info->dev_class    = d[4];
      info->dev_subclass = d[5];
      info->dev_protocol = d[6];
      info->if_number    = ifc[2];
      info->num_ep       = ifc[4] < GK_USB_MAX_ENDPOINTS ? ifc[4]
         : GK_USB_MAX_ENDPOINTS;
      info->if_class     = ifc[5];
      info->if_subclass  = ifc[6];
      info->if_protocol  = ifc[7];
      for (i = 0; i < info->num_ep && 64 + i * 8 + 8 <= PARAMS_SIZE; i++)
      {
         const uint8_t *e = p + 64 + i * 8;
         info->ep[i].address    = e[2];
         info->ep[i].attributes = e[3];
         info->ep[i].max_packet = (uint16_t)((e[4] << 8) | e[5]);
         info->ep[i].interval   = e[6];
      }
   }
   pool_put(p, PARAMS_SIZE);
   return ret;
}

int gk_usb_open(uint32_t id, gk_usb_info_t *info)
{
   int ret;
   if (init())
      return -ENODEV;
   /* IOS wants the device resumed before anything else. */
   if ((ret = dev_ioctl(id, VEN_SUSPEND_RESUME, 1, NULL, 0)))
      return ret;
   return get_params(id, 0, info);
}

int gk_usb_set_alt(uint32_t id, uint8_t alt, gk_usb_info_t *info)
{
   int ret;
   if (fd < 0)
      return -ENODEV;
   if ((ret = dev_ioctl(id, VEN_SET_ALTERNATE, (uint32_t)alt << 24, NULL, 0)))
      return ret;
   return get_params(id, alt, info);
}

void gk_usb_close(uint32_t id)
{
   if (fd >= 0)
      dev_ioctl(id, VEN_SUSPEND_RESUME, 0, NULL, 0);
}

int gk_usb_cancel(uint32_t id, uint8_t ep)
{
   return fd < 0 ? -ENODEV : dev_ioctl(id, VEN_CANCEL, (uint32_t)ep << 24, NULL,
         0);
}

/* A transfer: the message, then the data as IOS reads or writes it;
 * the message holds the data's address at ptr_at too. */
static int transfer(uint32_t cmd, uint8_t *msg, unsigned ptr_at, void *data,
      uint32_t len, int to_host)
{
   gk_ios_vec_t vec[2];
   uint8_t *buf = NULL;
   int32_t ret;
   if (len)
   {
      if (!(buf = (uint8_t*)pool_get(len)))
         return -ENOMEM;
      if (!to_host)
         memcpy(buf, data, len);
      msg[ptr_at]     = (uint8_t)(GK_PHYS(buf) >> 24);
      msg[ptr_at + 1] = (uint8_t)(GK_PHYS(buf) >> 16);
      msg[ptr_at + 2] = (uint8_t)(GK_PHYS(buf) >> 8);
      msg[ptr_at + 3] = (uint8_t)GK_PHYS(buf);
   }
   vec[0].data = msg;
   vec[0].len  = MSG_SIZE;
   vec[1].data = buf;
   vec[1].len  = len;
   ret = gk_ios_ioctlv(fd, cmd, to_host ? 1 : 2, to_host ? 1 : 0, vec);
   /* Emulators answer some requests with 0 rather than the length:
    * then the whole buffer is the reply. */
   if (to_host && ret >= 0)
      memcpy(data, buf, ret && (uint32_t)ret < len ? (uint32_t)ret : len);
   if (buf)
      pool_put(buf, len);
   pool_put(msg, MSG_SIZE);
   return ret < 0 ? -EPIPE : (int)ret;
}

static uint8_t *message(uint32_t id)
{
   uint8_t *m = (uint8_t*)pool_get(MSG_SIZE);
   m[0] = (uint8_t)(id >> 24);
   m[1] = (uint8_t)(id >> 16);
   m[2] = (uint8_t)(id >> 8);
   m[3] = (uint8_t)id;
   return m;
}

int gk_usb_ctrl(uint32_t id, uint8_t type, uint8_t request, uint16_t value,
      uint16_t index, void *data, uint16_t len)
{
   uint8_t *m;
   if (fd < 0)
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
   return transfer(VEN_CTRL, m, 16, data, len, (type & 0x80) != 0);
}

static int data_transfer(uint32_t cmd, unsigned ep_at, uint32_t id,
      uint8_t ep, void *data, uint32_t len)
{
   uint8_t *m;
   if (fd < 0)
      return -ENODEV;
   if (len > 0xffff)
      return -EINVAL;
   m = message(id);
   m[12]    = (uint8_t)(len >> 8);
   m[13]    = (uint8_t)len;
   m[ep_at] = ep;
   return transfer(cmd, m, 8, data, len, (ep & 0x80) != 0);
}

int gk_usb_bulk(uint32_t id, uint8_t ep, void *data, uint32_t len)
{
   return data_transfer(VEN_BULK, 18, id, ep, data, len);
}

int gk_usb_intr(uint32_t id, uint8_t ep, void *data, uint32_t len)
{
   return data_transfer(VEN_INTR, 14, id, ep, data, len);
}
