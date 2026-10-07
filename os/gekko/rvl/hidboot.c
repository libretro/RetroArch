/* Wii: USB keyboards and mice, the boot protocol HID interfaces
 * (class 3, subclass 1, protocol 1 and 2) among the devices
 * /dev/usb/hid lists.  A thread looks for new ones twice a second;
 * each gets a reader thread.  A keyboard's 8-byte reports (modifiers,
 * a reserved byte, six usages) become events as they change, which
 * queue up for gk_kbd_read(); a mouse's (buttons, x, y and on some a
 * wheel) add up for gk_mouse_read().
 *
 * A device that is neither stays as it was found: the HID pad driver
 * may have it open. */

#include <string.h>

#include <gekko/keyboard.h>
#include <gekko/mouse.h>
#include <gekko/thread.h>
#include <gekko/usb.h>

#include "../kernel/kernel.h"

#define QUEUE    64              /* a power of two */
#define MAX_DEV  8

#define PROTOCOL_KEYBOARD 1
#define PROTOCOL_MOUSE    2

#define HID_SET_IDLE     0x0a
#define HID_SET_PROTOCOL 0x0b
#define CLASS_INTERFACE  0x21    /* host to device, class, interface */

struct boot_dev
{
   gk_thread_t      *reader;
   uint32_t          id;
   uint8_t           ep;
   uint8_t           protocol;
   uint8_t           buttons;     /* a mouse's, held */
   volatile uint8_t  used;
};

static struct
{
   gk_kbd_event_t  ev[QUEUE];
   struct boot_dev dev[MAX_DEV];
   int32_t         dx, dy, wheel; /* the mice, since the last look */
   uint32_t        seen[GK_USB_MAX_DEVICES];  /* looked at, while listed */
   gk_mutex_t      lock;
   uint32_t        head;
   uint32_t        tail;
   int             num_seen;
   int             started;
   volatile int    shut;
} kb;

static void shutdown_hook(void);
static struct gk_exit_hook exit_hook = { shutdown_hook, NULL };

static void push(const gk_kbd_event_t *ev)
{
   gk_mutex_lock(&kb.lock);
   if (kb.head - kb.tail < QUEUE)
      kb.ev[kb.head++ & (QUEUE - 1)] = *ev;
   gk_mutex_unlock(&kb.lock);
}

static void *mouse_reader(struct boot_dev *d)
{
   uint8_t report[8];
   while (!kb.shut)
   {
      int n = gk_usb_intr(d->id, d->ep, report, sizeof(report));
      if (n < 0)
         break;
      if (n < 3)
         continue;
      gk_mutex_lock(&kb.lock);
      d->buttons = report[0] & 0x1f;
      kb.dx     += (int8_t)report[1];
      kb.dy     += (int8_t)report[2];
      if (n >= 4)
         kb.wheel += (int8_t)report[3];
      gk_mutex_unlock(&kb.lock);
   }
   gk_mutex_lock(&kb.lock);
   d->buttons = 0;
   gk_mutex_unlock(&kb.lock);
   gk_usb_close(d->id);
   d->used = 0;
   return NULL;
}

static void *reader(void *arg)
{
   struct boot_dev *d = (struct boot_dev*)arg;
   gk_kbd_event_t ev;
   uint8_t report[8], last[8];
   if (d->protocol == PROTOCOL_MOUSE)
      return mouse_reader(d);
   memset(&ev, 0, sizeof(ev));
   memset(last, 0, sizeof(last));
   ev.id = d->id;
   while (!kb.shut)
   {
      int n = gk_usb_intr(d->id, d->ep, report, sizeof(report));
      if (n < 0)
         break;
      /* Too many keys at once reads as usage 1 everywhere; a keyboard
       * that repeats itself says nothing new. */
      if (n < 8 || report[2] == 0x01 || !memcmp(report, last, 8))
         continue;
      memcpy(last, report, 8);
      ev.type      = GK_KBD_KEYS;
      ev.modifiers = report[0];
      memcpy(ev.keys, report + 2, 6);
      push(&ev);
   }
   memset(&ev, 0, sizeof(ev));
   ev.id   = d->id;
   ev.type = GK_KBD_DISCONNECT;
   push(&ev);
   gk_usb_close(d->id);
   d->used = 0;
   return NULL;
}

/* Boot protocol, and reports only on a change. */
static void attach(const gk_usb_info_t *info, uint32_t id)
{
   struct boot_dev *d = NULL;
   int i;
   for (i = 0; i < MAX_DEV && !d; i++)
      if (!kb.dev[i].used)
         d = &kb.dev[i];
   if (!d)
      return;
   d->id       = id;
   d->ep       = 0;
   d->protocol = info->if_protocol;
   d->buttons  = 0;
   for (i = 0; i < info->num_ep; i++)
      if (     (info->ep[i].address & 0x80)
            && (info->ep[i].attributes & 3) == 3)
         d->ep = info->ep[i].address;
   if (!d->ep)
      return;
   gk_usb_ctrl(id, CLASS_INTERFACE, HID_SET_PROTOCOL, 0, info->if_number,
         NULL, 0);
   gk_usb_ctrl(id, CLASS_INTERFACE, HID_SET_IDLE, 0, info->if_number,
         NULL, 0);
   if (d->protocol == PROTOCOL_KEYBOARD)
   {
      gk_kbd_event_t ev;
      memset(&ev, 0, sizeof(ev));
      ev.id   = id;
      ev.type = GK_KBD_CONNECT;
      push(&ev);
   }
   d->used = 1;
   if (!(d->reader = gk_thread_create(reader, d, NULL, 8192,
               GK_PRIO_DEFAULT + 4)))
      d->used = 0;
   else
      gk_thread_detach(d->reader);
}

static int seen(uint32_t id)
{
   int i;
   for (i = 0; i < kb.num_seen; i++)
      if (kb.seen[i] == id)
         return 1;
   return 0;
}

static void *finder(void *arg)
{
   (void)arg;
   while (!kb.shut)
   {
      gk_usb_dev_t devs[GK_USB_MAX_DEVICES];
      int n = gk_usb_hid_list(devs, GK_USB_MAX_DEVICES), i, k;
      /* Forget what was unplugged; an id can come back as another. */
      for (i = 0; i < kb.num_seen; i++)
      {
         for (k = 0; k < n && devs[k].id != kb.seen[i]; k++)
            ;
         if (k == n)
            kb.seen[i--] = kb.seen[--kb.num_seen];
      }
      for (i = 0; i < n && !kb.shut; i++)
      {
         gk_usb_info_t info;
         if (seen(devs[i].id) || kb.num_seen >= GK_USB_MAX_DEVICES)
            continue;
         kb.seen[kb.num_seen++] = devs[i].id;
         if (     !gk_usb_open(devs[i].id, &info)
               && info.if_class == 3 && info.if_subclass == 1
               && (     info.if_protocol == PROTOCOL_KEYBOARD
                     || info.if_protocol == PROTOCOL_MOUSE))
            attach(&info, devs[i].id);
      }
      gk_sleep_us(500000);
   }
   return NULL;
}

static void start(void)
{
   gk_thread_t *t;
   if (kb.started)
      return;
   kb.started = 1;
   gk_exit_hook_add(&exit_hook);
   if ((t = gk_thread_create(finder, NULL, NULL, 16384,
               GK_PRIO_DEFAULT + 2)))
      gk_thread_detach(t);
}

int gk_kbd_read(gk_kbd_event_t *ev)
{
   int got = 0;
   start();
   gk_mutex_lock(&kb.lock);
   if (kb.tail != kb.head)
   {
      *ev = kb.ev[kb.tail++ & (QUEUE - 1)];
      got = 1;
   }
   gk_mutex_unlock(&kb.lock);
   return got;
}

void gk_mouse_read(gk_mouse_t *m)
{
   int i;
   start();
   memset(m, 0, sizeof(*m));
   gk_mutex_lock(&kb.lock);
   m->dx    = kb.dx;
   m->dy    = kb.dy;
   m->wheel = kb.wheel;
   kb.dx    = 0;
   kb.dy    = 0;
   kb.wheel = 0;
   for (i = 0; i < MAX_DEV; i++)
      if (kb.dev[i].used && kb.dev[i].protocol == PROTOCOL_MOUSE)
      {
         m->buttons |= kb.dev[i].buttons;
         m->count++;
      }
   gk_mutex_unlock(&kb.lock);
}

/* Leaving: nothing new is looked for; the USB layer's own hook ends
 * the transfers the readers wait in. */
static void shutdown_hook(void)
{
   kb.shut = 1;
}
