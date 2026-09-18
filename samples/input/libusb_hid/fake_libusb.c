/* A fake libusb for libusb_hid_test: one context, devices the test
 * plugs and unplugs, interrupt transfers the test completes, and an
 * event loop that hands completions and hotplug events to callbacks
 * the way libusb does - from inside libusb_handle_events*(), on the
 * calling thread. Built against the real <libusb.h> for its types and
 * inline fill helpers. */

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#include <libusb-1.0/libusb.h>

#include "fake_libusb.h"

#define MAX_EVENTS 256

struct fake_event
{
   int kind;                      /* 0 transfer, 1 hotplug */
   struct libusb_transfer *xfer;
   enum libusb_transfer_status status;
   struct libusb_device *dev;
   libusb_hotplug_event hp_event;
};

struct libusb_device        { struct fake_dev *fd; };
struct libusb_device_handle { struct libusb_device *dev; };

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;
static struct fake_event events[MAX_EVENTS];
static int nevents;
static int interrupted;

static libusb_hotplug_callback_fn hp_cb;
static void *hp_user;
static int hp_registered;

static struct libusb_transfer *flying[64];
static int nflying;

struct fake_stats fake_stats;

struct fake_stats fake_snapshot(void)
{
   struct fake_stats s;
   pthread_mutex_lock(&mtx);
   s = fake_stats;
   pthread_mutex_unlock(&mtx);
   return s;
}

static void queue_event(struct fake_event e)
{
   events[nevents++] = e;
   pthread_cond_broadcast(&cond);
}

static int flying_index(struct libusb_transfer *t)
{
   int i;
   for (i = 0; i < nflying; i++)
      if (flying[i] == t)
         return i;
   return -1;
}

static void flying_remove(int i) { flying[i] = flying[--nflying]; }

/* ---- test controls ---- */

static struct libusb_device devs[4];
static struct fake_dev fds[4];

struct libusb_device *fake_new_device(void)
{
   int i;
   for (i = 0; i < 4; i++)
      if (!devs[i].fd)
      {
         memset(&fds[i], 0, sizeof(fds[i]));
         devs[i].fd = &fds[i];
         return &devs[i];
      }
   return NULL;
}

void fake_plug(struct libusb_device *dev)
{
   struct fake_event e;
   pthread_mutex_lock(&mtx);
   dev->fd->present = 1;
   if (hp_registered)
   {
      memset(&e, 0, sizeof(e));
      e.kind = 1; e.dev = dev; e.hp_event = LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED;
      queue_event(e);
   }
   pthread_mutex_unlock(&mtx);
}

void fake_unplug(struct libusb_device *dev)
{
   int i;
   struct fake_event e;
   pthread_mutex_lock(&mtx);
   dev->fd->present = 0;
   for (i = nflying - 1; i >= 0; i--)
      if (flying[i]->dev_handle->dev == dev)
      {
         memset(&e, 0, sizeof(e));
         e.xfer = flying[i]; e.status = LIBUSB_TRANSFER_NO_DEVICE;
         flying_remove(i);
         queue_event(e);
      }
   if (hp_registered)
   {
      memset(&e, 0, sizeof(e));
      e.kind = 1; e.dev = dev; e.hp_event = LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT;
      queue_event(e);
   }
   pthread_mutex_unlock(&mtx);
}

/* Completes the device's pending IN read with a report. */
int fake_report(struct libusb_device *dev, const unsigned char *d, int len)
{
   int i;
   struct fake_event e;
   pthread_mutex_lock(&mtx);
   for (i = 0; i < nflying; i++)
   {
      struct libusb_transfer *t = flying[i];
      if (t->dev_handle->dev == dev && (t->endpoint & LIBUSB_ENDPOINT_IN))
      {
         memcpy(t->buffer, d, len);
         t->actual_length = len;
         memset(&e, 0, sizeof(e));
         e.xfer = t; e.status = LIBUSB_TRANSFER_COMPLETED;
         flying_remove(i);
         queue_event(e);
         pthread_mutex_unlock(&mtx);
         return 1;
      }
   }
   pthread_mutex_unlock(&mtx);
   return 0;
}

/* Completes the device's pending OUT send, if any. */
int fake_ack_out(struct libusb_device *dev)
{
   int i;
   struct fake_event e;
   pthread_mutex_lock(&mtx);
   for (i = 0; i < nflying; i++)
   {
      struct libusb_transfer *t = flying[i];
      if (t->dev_handle->dev == dev && !(t->endpoint & LIBUSB_ENDPOINT_IN))
      {
         t->actual_length = t->length;
         memset(&e, 0, sizeof(e));
         e.xfer = t; e.status = LIBUSB_TRANSFER_COMPLETED;
         flying_remove(i);
         queue_event(e);
         pthread_mutex_unlock(&mtx);
         return 1;
      }
   }
   pthread_mutex_unlock(&mtx);
   return 0;
}

int fake_in_flight(struct libusb_device *dev, int in)
{
   int i, n = 0;
   pthread_mutex_lock(&mtx);
   for (i = 0; i < nflying; i++)
      if (     flying[i]->dev_handle->dev == dev
            && !!(flying[i]->endpoint & LIBUSB_ENDPOINT_IN) == !!in)
         n++;
   pthread_mutex_unlock(&mtx);
   return n;
}

void fake_forget_devices(void) { memset(devs, 0, sizeof(devs)); }

/* ---- libusb ---- */

int LIBUSB_CALL libusb_init(libusb_context **ctx) { *ctx = (libusb_context*)&mtx; return 0; }
void LIBUSB_CALL libusb_exit(libusb_context *ctx) { fake_stats.exits++; }
int LIBUSB_CALL libusb_has_capability(uint32_t cap) { return 1; }

int LIBUSB_CALL libusb_hotplug_register_callback(libusb_context *ctx,
      int events_mask, int flags, int vid, int pid, int dev_class,
      libusb_hotplug_callback_fn cb, void *user, libusb_hotplug_callback_handle *h)
{
   int i;
   hp_cb = cb; hp_user = user; hp_registered = 1; *h = 1;
   /* LIBUSB_HOTPLUG_ENUMERATE: ARRIVED for attached devices, now. */
   for (i = 0; i < 4; i++)
      if (devs[i].fd && devs[i].fd->present)
         cb(ctx, &devs[i], LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, user);
   return 0;
}

void LIBUSB_CALL libusb_hotplug_deregister_callback(libusb_context *ctx,
      libusb_hotplug_callback_handle h)
{
   pthread_mutex_lock(&mtx);
   hp_registered = 0;
   pthread_mutex_unlock(&mtx);
}

ssize_t LIBUSB_CALL libusb_get_device_list(libusb_context *ctx, libusb_device ***list) { *list = NULL; return 0; }
void LIBUSB_CALL libusb_free_device_list(libusb_device **list, int unref) { }

int LIBUSB_CALL libusb_get_device_descriptor(libusb_device *dev,
      struct libusb_device_descriptor *desc)
{
   memset(desc, 0, sizeof(*desc));
   desc->idVendor = 0x1234; desc->idProduct = 0x5678; desc->iProduct = 1;
   return 0;
}

static struct libusb_endpoint_descriptor eps[2];
static struct libusb_interface_descriptor alt;
static struct libusb_interface iface;
static struct libusb_config_descriptor cfg;

int LIBUSB_CALL libusb_get_config_descriptor(libusb_device *dev, uint8_t idx,
      struct libusb_config_descriptor **config)
{
   eps[0].bEndpointAddress = 0x81; eps[0].bmAttributes = LIBUSB_TRANSFER_TYPE_INTERRUPT; eps[0].wMaxPacketSize = 64;
   eps[1].bEndpointAddress = 0x02; eps[1].bmAttributes = LIBUSB_TRANSFER_TYPE_INTERRUPT; eps[1].wMaxPacketSize = 64;
   alt.bInterfaceNumber = 0; alt.bNumEndpoints = 2; alt.endpoint = eps;
   iface.altsetting = &alt; iface.num_altsetting = 1;
   cfg.bNumInterfaces = 1; cfg.interface = &iface;
   *config = &cfg;
   return 0;
}
void LIBUSB_CALL libusb_free_config_descriptor(struct libusb_config_descriptor *c) { }

int LIBUSB_CALL libusb_open(libusb_device *dev, libusb_device_handle **h)
{
   if (!dev->fd->present)
      return LIBUSB_ERROR_NO_DEVICE;
   *h = (libusb_device_handle*)calloc(1, sizeof(**h));
   (*h)->dev = dev;
   pthread_mutex_lock(&mtx); fake_stats.opens++; pthread_mutex_unlock(&mtx);
   return 0;
}
void LIBUSB_CALL libusb_close(libusb_device_handle *h)
{
   pthread_mutex_lock(&mtx); fake_stats.closes++; pthread_mutex_unlock(&mtx);
   free(h);
}
int LIBUSB_CALL libusb_get_string_descriptor_ascii(libusb_device_handle *h,
      uint8_t idx, unsigned char *data, int length)
{
   strncpy((char*)data, "FakePad", length);
   return 7;
}
int LIBUSB_CALL libusb_kernel_driver_active(libusb_device_handle *h, int i) { return 0; }
int LIBUSB_CALL libusb_detach_kernel_driver(libusb_device_handle *h, int i) { return 0; }
int LIBUSB_CALL libusb_claim_interface(libusb_device_handle *h, int i) { return 0; }
int LIBUSB_CALL libusb_release_interface(libusb_device_handle *h, int i) { return 0; }

struct libusb_transfer * LIBUSB_CALL libusb_alloc_transfer(int iso)
{
   pthread_mutex_lock(&mtx); fake_stats.allocs++; pthread_mutex_unlock(&mtx);
   return (struct libusb_transfer*)calloc(1, sizeof(struct libusb_transfer));
}
void LIBUSB_CALL libusb_free_transfer(struct libusb_transfer *t)
{
   pthread_mutex_lock(&mtx);
   fake_stats.frees++;
   if (flying_index(t) >= 0)
      fake_stats.freed_in_flight++;
   pthread_mutex_unlock(&mtx);
   free(t);
}

int LIBUSB_CALL libusb_submit_transfer(struct libusb_transfer *t)
{
   pthread_mutex_lock(&mtx);
   if (!t->dev_handle->dev->fd || !t->dev_handle->dev->fd->present)
   {
      pthread_mutex_unlock(&mtx);
      return LIBUSB_ERROR_NO_DEVICE;
   }
   if (flying_index(t) >= 0)
      fake_stats.double_submits++;
   flying[nflying++] = t;
   if (!(t->endpoint & LIBUSB_ENDPOINT_IN))
   {
      int n = fake_stats.nsent < 16 ? fake_stats.nsent++ : 15;
      memcpy(fake_stats.sent[n], t->buffer, t->length < 8 ? t->length : 8);
      if (nflying > 0)
      {
         int i, outs = 0;
         for (i = 0; i < nflying; i++)
            if (     flying[i]->dev_handle == t->dev_handle
                  && !(flying[i]->endpoint & LIBUSB_ENDPOINT_IN))
               outs++;
         if (outs > fake_stats.max_outs_in_flight)
            fake_stats.max_outs_in_flight = outs;
      }
   }
   pthread_mutex_unlock(&mtx);
   return 0;
}

int LIBUSB_CALL libusb_cancel_transfer(struct libusb_transfer *t)
{
   int i;
   struct fake_event e;
   pthread_mutex_lock(&mtx);
   if ((i = flying_index(t)) < 0)
   {
      pthread_mutex_unlock(&mtx);
      return LIBUSB_ERROR_NOT_FOUND;
   }
   flying_remove(i);
   memset(&e, 0, sizeof(e));
   e.xfer = t; e.status = LIBUSB_TRANSFER_CANCELLED;
   queue_event(e);
   pthread_mutex_unlock(&mtx);
   return 0;
}

void LIBUSB_CALL libusb_interrupt_event_handler(libusb_context *ctx)
{
   pthread_mutex_lock(&mtx);
   interrupted = 1;
   pthread_cond_broadcast(&cond);
   pthread_mutex_unlock(&mtx);
}

int LIBUSB_CALL libusb_handle_events_timeout_completed(libusb_context *ctx,
      struct timeval *tv, int *completed)
{
   struct fake_event batch[MAX_EVENTS];
   int i, n;
   struct timespec until;

   clock_gettime(CLOCK_REALTIME, &until);
   until.tv_sec  += tv ? tv->tv_sec : 60;
   until.tv_nsec += tv ? tv->tv_usec * 1000L : 0;
   if (until.tv_nsec >= 1000000000L) { until.tv_sec++; until.tv_nsec -= 1000000000L; }

   pthread_mutex_lock(&mtx);
   while (!nevents && !interrupted && !(completed && *completed))
      if (pthread_cond_timedwait(&cond, &mtx, &until) == ETIMEDOUT)
         break;
   interrupted = 0;
   n = nevents;
   memcpy(batch, events, n * sizeof(batch[0]));
   nevents = 0;
   pthread_mutex_unlock(&mtx);

   for (i = 0; i < n; i++)
   {
      if (batch[i].kind == 1)
      {
         if (hp_registered)
            hp_cb(ctx, batch[i].dev, batch[i].hp_event, hp_user);
      }
      else
      {
         batch[i].xfer->status = batch[i].status;
         batch[i].xfer->callback(batch[i].xfer);
      }
   }
   return 0;
}

int LIBUSB_CALL libusb_handle_events_completed(libusb_context *ctx, int *completed)
{
   return libusb_handle_events_timeout_completed(ctx, NULL, completed);
}
