/* The embedded ALSA driver ("tinyalsa" in audio/drivers/alsa.c)
 * against a mock kernel PCM device.
 *
 * The driver reaches the kernel through four calls, and the harness
 * replaces them: a scripted device that refines parameters the way
 * the kernel does - intersecting what it can do with what was asked -
 * takes frames into a buffer, drains on demand, reports its delay,
 * and can be made to underrun. So the negotiation, the write path,
 * xrun recovery, pause and the telemetry all run here, on a machine
 * with no sound card. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>

/* the seam: defined before the driver is included */
#define EALSA_SYSCALLS
static int mock_open(const char *path, int flags);
static int mock_ioctl(int fd, unsigned long req, void *arg);
static int mock_close(int fd);
static int mock_poll(void *fds, unsigned n, int timeout);
#define ealsa_open_dev(path, flags) mock_open((path), (flags))
#define ealsa_ioctl(fd, req, arg)   mock_ioctl((fd), (unsigned long)(req), (void*)(arg))
#define ealsa_close(fd)             mock_close((fd))
#define ealsa_poll(fds, n, timeout) mock_poll((void*)(fds), (n), (timeout))

#include "../../../audio/drivers/alsa.c"

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* --- the device ------------------------------------------------- */
static struct {
   int      open;
   unsigned min_rate, max_rate, max_channels;
   bool     takes_float, can_pause, prepared, running, paused;
   unsigned rate, channels, format, period, periods;
   size_t   queued, capacity, taken;
   int      xrun_after;      /* writes until the device underruns */
   int      writes;
   int      prepares, starts, drops, pauses;
   int      poll_calls;
} dev;

static void dev_reset(void)
{
   memset(&dev, 0, sizeof(dev));
   dev.min_rate = 8000; dev.max_rate = 192000; dev.max_channels = 8;
   dev.takes_float = true; dev.can_pause = true;
   dev.xrun_after = -1;
}

static int mock_open(const char *path, int flags)
{
   (void)flags;
   if (strncmp(path, "/dev/snd/pcmC", 13))
      return -1;
   /* only card 0 device 0 exists */
   if (strcmp(path, "/dev/snd/pcmC0D0p"))
      return -1;
   dev.open++;
   return 42;
}
static int mock_close(int fd) { (void)fd; dev.open--; return 0; }
static int mock_poll(void *fds, unsigned n, int timeout)
{
   (void)fds; (void)n; (void)timeout;
   dev.poll_calls++;
   /* the device plays a period while the caller waits */
   if (dev.queued >= dev.period)
      dev.queued -= dev.period;
   else
      dev.queued = 0;
   return 1;
}

static void iv_set(struct ealsa_interval *iv, unsigned lo, unsigned hi)
{
   if (iv->min < lo) iv->min = lo;
   if (iv->max > hi) iv->max = hi;
}
static bool mask_has(const struct ealsa_mask *m, unsigned bit)
{
   return (m->bits[bit >> 5] >> (bit & 31)) & 1;
}

/* The kernel's refine: each parameter narrowed to what the hardware
 * can do, and an empty result is an error. */
static int mock_refine(struct ealsa_hw_params *p)
{
   struct ealsa_interval *rate = &p->intervals[EALSA_P_RATE - EALSA_P_FIRST_INTERVAL];
   struct ealsa_interval *ch   = &p->intervals[EALSA_P_CHANNELS - EALSA_P_FIRST_INTERVAL];
   struct ealsa_interval *per  = &p->intervals[EALSA_P_PERIOD_SIZE - EALSA_P_FIRST_INTERVAL];
   struct ealsa_interval *cnt  = &p->intervals[EALSA_P_PERIODS - EALSA_P_FIRST_INTERVAL];
   struct ealsa_interval *buf  = &p->intervals[EALSA_P_BUFFER_SIZE - EALSA_P_FIRST_INTERVAL];
   struct ealsa_mask     *fmt  = &p->masks[EALSA_P_FORMAT - EALSA_P_FIRST_MASK];
   struct ealsa_mask     *acc  = &p->masks[EALSA_P_ACCESS - EALSA_P_FIRST_MASK];

   if (!mask_has(acc, EALSA_ACCESS_RW_INTERLEAVED))
      return -1;
   if (!mask_has(fmt, EALSA_FMT_S16_LE)
         && !(dev.takes_float && mask_has(fmt, EALSA_FMT_FLOAT_LE)))
      return -1;
   iv_set(rate, dev.min_rate, dev.max_rate);
   iv_set(ch,   1,            dev.max_channels);
   iv_set(per,  64,           8192);
   iv_set(cnt,  2,            32);
   if (rate->min > rate->max || ch->min > ch->max || per->min > per->max)
      return -1;
   /* a determined set: the buffer follows the period and the count */
   buf->min = buf->max = per->min * cnt->min;
   p->info  = dev.can_pause ? EALSA_INFO_PAUSE : 0;
   return 0;
}

static int mock_ioctl(int fd, unsigned long req, void *arg)
{
   (void)fd;
   switch (req)
   {
      case EALSA_IOCTL_HW_REFINE:
         return mock_refine((struct ealsa_hw_params*)arg);
      case EALSA_IOCTL_HW_PARAMS:
      {
         struct ealsa_hw_params *p = (struct ealsa_hw_params*)arg;
         if (mock_refine(p) < 0)
            return -1;
         dev.rate     = p->intervals[EALSA_P_RATE - EALSA_P_FIRST_INTERVAL].min;
         dev.channels = p->intervals[EALSA_P_CHANNELS - EALSA_P_FIRST_INTERVAL].min;
         dev.period   = p->intervals[EALSA_P_PERIOD_SIZE - EALSA_P_FIRST_INTERVAL].min;
         dev.periods  = p->intervals[EALSA_P_PERIODS - EALSA_P_FIRST_INTERVAL].min;
         dev.format   = mask_has(&p->masks[EALSA_P_FORMAT - EALSA_P_FIRST_MASK], EALSA_FMT_FLOAT_LE)
                      ? EALSA_FMT_FLOAT_LE : EALSA_FMT_S16_LE;
         dev.capacity = (size_t)dev.period * dev.periods;
         return 0;
      }
      case EALSA_IOCTL_SW_PARAMS:
         return 0;
      case EALSA_IOCTL_PREPARE:
         dev.prepares++; dev.prepared = true; dev.running = false; dev.queued = 0;
         return 0;
      case EALSA_IOCTL_START:
         if (!dev.prepared) return -1;
         dev.starts++; dev.running = true;
         return 0;
      case EALSA_IOCTL_DROP:
         dev.drops++; dev.running = false; dev.prepared = false; dev.queued = 0;
         return 0;
      case EALSA_IOCTL_PAUSE:
         if (!dev.can_pause) { errno = EINVAL; return -1; }
         dev.pauses++; dev.paused = *(int*)arg != 0;
         return 0;
      case EALSA_IOCTL_DELAY:
         *(ealsa_sframes_t*)arg = (ealsa_sframes_t)dev.queued;
         return 0;
      case EALSA_IOCTL_WRITEI_FRAMES:
      {
         struct ealsa_xferi *x = (struct ealsa_xferi*)arg;
         size_t room;
         if (dev.xrun_after >= 0 && dev.writes >= dev.xrun_after)
         {
            dev.xrun_after = -1;
            dev.prepared   = false;
            errno = EPIPE;
            return -1;
         }
         dev.writes++;
         if (dev.paused) { errno = EAGAIN; return -1; }
         room = (dev.queued < dev.capacity) ? dev.capacity - dev.queued : 0;
         if (!room) { errno = EAGAIN; return -1; }
         if (x->frames > room)
            x->frames = room;
         dev.queued += x->frames;
         dev.taken  += x->frames;
         x->result   = (ealsa_sframes_t)x->frames;
         return 0;
      }
      default:
         errno = ENOTTY;
         return -1;
   }
}

/* --- the tests --------------------------------------------------- */
static uint8_t buf[8192];

int main(void)
{
   audio_driver_t *drv = &audio_tinyalsa;
   void *h;
   unsigned new_rate;
   size_t i;

   printf("embedded alsa:\n");

   printf("   a device that is not there is not opened\n");
   dev_reset();
   new_rate = 0;
   h = drv->init("3,1", 48000, 64, &new_rate);
   CHECK(h == NULL, "a missing device returned %p", h);
   CHECK(dev.open == 0, "%d handle(s) left open", dev.open);

   printf("   the negotiated rate and format are what the device took\n");
   dev_reset();
   new_rate = 0;
   h = drv->init(NULL, 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (!h) { printf("%u failure(s)\n", failures); return 1; }
   printf("      %u Hz, %u channels, %s; period %u, buffer %u frames\n",
         dev.rate, dev.channels, dev.format == EALSA_FMT_FLOAT_LE ? "float" : "s16",
         dev.period, (unsigned)dev.capacity);
   CHECK(new_rate == dev.rate, "the device took %u Hz and the frontend was told %u", dev.rate, new_rate);
   CHECK(drv->use_float(h) == (dev.format == EALSA_FMT_FLOAT_LE),
         "use_float disagrees with the format the device took");
   CHECK(drv->buffer_size(h) == dev.capacity * (dev.format == EALSA_FMT_FLOAT_LE ? 4 : 2) * dev.channels,
         "buffer_size is %u bytes for %u frames", (unsigned)drv->buffer_size(h), (unsigned)dev.capacity);
   drv->free(h);
   CHECK(dev.open == 0, "free left the device open");

   printf("   a rate outside the device's range is pulled into it and reported\n");
   dev_reset();
   dev.min_rate = 44100; dev.max_rate = 48000;
   new_rate = 0;
   h = drv->init(NULL, 96000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (h)
   {
      printf("      asked 96000, opened %u, frontend told %u\n", dev.rate, new_rate);
      CHECK(new_rate == 48000, "the frontend was told %u, not the 48000 the device took", new_rate);
      drv->free(h);
   }

   printf("   an s16-only device is opened in s16\n");
   dev_reset();
   dev.takes_float = false;
   h = drv->init(NULL, 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (h)
   {
      CHECK(!drv->use_float(h), "float was reported for an s16-only device");
      CHECK(dev.format == EALSA_FMT_S16_LE, "the device was given a float stream");
      drv->free(h);
   }

   printf("   writes reach the device, and it is started once it has audio\n");
   dev_reset();
   h = drv->init(NULL, 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (h)
   {
      size_t frame_bytes = (dev.format == EALSA_FMT_FLOAT_LE ? 4 : 2) * dev.channels;
      size_t chunk = dev.period * frame_bytes;
      size_t sent  = 0;
      if (chunk > sizeof(buf)) chunk = sizeof(buf);
      drv->set_nonblock_state(h, false);
      for (i = 0; i < 32; i++)
      {
         ssize_t w = drv->write(h, buf, chunk);
         CHECK(w == (ssize_t)chunk, "write %u returned %ld of %u", (unsigned)i, (long)w, (unsigned)chunk);
         if (w > 0) sent += w / frame_bytes;
         if (dev.queued >= dev.period) dev.queued -= dev.period;   /* the device plays */
      }
      printf("      %u frames sent, %u taken by the device, started %d time(s)\n",
            (unsigned)sent, (unsigned)dev.taken, dev.starts);
      CHECK(dev.taken == sent, "%u frames were accepted but %u reached the device",
            (unsigned)sent, (unsigned)dev.taken);
      CHECK(dev.starts == 1, "the device was started %d times", dev.starts);

      printf("   frames_consumed is what the device has played\n");
      {
         size_t consumed = drv->frames_consumed(h);
         printf("      %u sent, %u still queued, %u consumed\n",
               (unsigned)sent, (unsigned)dev.queued, (unsigned)consumed);
         CHECK(consumed == sent - dev.queued, "consumed is %u, expected %u",
               (unsigned)consumed, (unsigned)(sent - dev.queued));
      }

      printf("   a full device refuses a non-blocking write and takes a blocking one\n");
      drv->set_nonblock_state(h, true);
      {
         size_t total = 0; ssize_t w; unsigned tries = 0;
         do { w = drv->write(h, buf, chunk); if (w > 0) total += w; tries++; }
         while (w == (ssize_t)chunk && tries < 256);
         printf("      non-blocking: %u writes, %u bytes, then short\n", tries, (unsigned)total);
         CHECK(tries < 256, "a non-blocking write never came back short");
      }
      drv->set_nonblock_state(h, false);
      {
         int polls = dev.poll_calls;
         ssize_t w = drv->write(h, buf, chunk);
         printf("      blocking: %ld bytes after %d wait(s)\n", (long)w, dev.poll_calls - polls);
         CHECK(w == (ssize_t)chunk, "a blocking write returned %ld of %u", (long)w, (unsigned)chunk);
         CHECK(dev.poll_calls > polls, "the blocking write did not wait for the device");
      }

      printf("   an underrun is recovered and the frames go out\n");
      {
         int prepares = dev.prepares;
         size_t taken = dev.taken;
         dev.xrun_after = dev.writes;   /* the next write underruns */
         drv->write(h, buf, chunk);
         printf("      prepares %d -> %d, %u frames after\n", prepares, dev.prepares,
               (unsigned)(dev.taken - taken));
         CHECK(dev.prepares > prepares, "the device was not prepared after the underrun");
         CHECK(dev.taken > taken, "no frames reached the device after the underrun");
      }

      printf("   stop pauses, start resumes\n");
      CHECK(drv->stop(h), "stop");
      CHECK(dev.paused, "the device was not paused");
      CHECK(!drv->alive(h), "alive after stop");
      CHECK(drv->start(h, false), "start");
      CHECK(!dev.paused, "the device was not resumed");
      CHECK(drv->alive(h), "not alive after start");
      drv->free(h);
      CHECK(dev.drops > 0, "the device was not dropped on free");
   }

   printf("   a device that cannot pause is dropped and prepared instead\n");
   dev_reset();
   dev.can_pause = false;
   h = drv->init(NULL, 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (h)
   {
      int drops = dev.drops;
      drv->write(h, buf, 512);
      CHECK(drv->stop(h), "stop on a device that cannot pause");
      CHECK(dev.drops > drops, "stop neither paused nor dropped");
      CHECK(dev.pauses == 0, "a device that cannot pause was asked to");
      CHECK(drv->start(h, false), "start after a drop");
      CHECK(drv->alive(h), "not alive after start");
      drv->free(h);
   }

   printf("   the device list is what the kernel exposes\n");
   dev_reset();
   {
      struct string_list *l = (struct string_list*)drv->device_list_new(NULL);
      CHECK(l != NULL, "no list");
      if (l)
      {
         printf("      %u device(s): %s\n", (unsigned)l->size, l->size ? l->elems[0].data : "-");
         CHECK(l->size == 1, "%u devices for a machine with one", (unsigned)l->size);
         CHECK(l->size && !strcmp(l->elems[0].data, "0,0"), "the device is not named 0,0");
         drv->device_list_free(NULL, l);
      }
      CHECK(dev.open == 0, "the listing left %d handle(s) open", dev.open);
   }

   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("embedded alsa: the driver negotiates, writes, recovers and reports\n");
   return 0;
}
