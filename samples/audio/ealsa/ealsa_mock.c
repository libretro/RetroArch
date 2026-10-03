#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <sys/ioctl.h>
#include "ealsa_mock.h"

/* The ABI the driver uses, as it spells it - the mock has to agree
 * with it byte for byte, which is the point. */
typedef unsigned long m_uframes_t;
typedef signed long   m_sframes_t;
#define M_MASK_MAX          256
#define M_P_ACCESS          0
#define M_P_FORMAT          1
#define M_P_SUBFORMAT       2
#define M_P_FIRST_MASK      M_P_ACCESS
#define M_P_LAST_MASK       M_P_SUBFORMAT
#define M_P_SAMPLE_BITS     8
#define M_P_CHANNELS        10
#define M_P_RATE            11
#define M_P_PERIOD_SIZE     13
#define M_P_PERIODS         15
#define M_P_BUFFER_SIZE     17
#define M_P_TICK_TIME       19
#define M_P_FIRST_INTERVAL  M_P_SAMPLE_BITS
#define M_P_LAST_INTERVAL   M_P_TICK_TIME
#define M_ACCESS_RW_INTERLEAVED 3
#define M_FMT_S16_LE   2
#define M_FMT_FLOAT_LE 14
#define M_INFO_PAUSE   0x00080000

struct m_interval { unsigned int min, max; unsigned int openmin:1, openmax:1, integer:1, empty:1; };
struct m_mask { uint32_t bits[(M_MASK_MAX + 31) / 32]; };
struct m_hw_params
{
   unsigned int flags;
   struct m_mask     masks[M_P_LAST_MASK - M_P_FIRST_MASK + 1];
   struct m_mask     mres[5];
   struct m_interval intervals[M_P_LAST_INTERVAL - M_P_FIRST_INTERVAL + 1];
   struct m_interval ires[9];
   unsigned int rmask, cmask, info, msbits, rate_num, rate_den;
   m_uframes_t fifo_size;
   unsigned char reserved[64];
};
struct m_sw_params
{
   int tstamp_mode; unsigned int period_step, sleep_min;
   m_uframes_t avail_min, xfer_align, start_threshold, stop_threshold;
   m_uframes_t silence_threshold, silence_size, boundary;
   unsigned int proto, tstamp_type; unsigned char reserved[56];
};
struct m_xferi { m_sframes_t result; void *buf; m_uframes_t frames; };

#define M_IOCTL_HW_REFINE     _IOWR('A', 0x10, struct m_hw_params)
#define M_IOCTL_HW_PARAMS     _IOWR('A', 0x11, struct m_hw_params)
#define M_IOCTL_SW_PARAMS     _IOWR('A', 0x13, struct m_sw_params)
#define M_IOCTL_DELAY         _IOR('A', 0x21, m_sframes_t)
#define M_IOCTL_PREPARE       _IO('A', 0x40)
#define M_IOCTL_START         _IO('A', 0x42)
#define M_IOCTL_DROP          _IO('A', 0x43)
#define M_IOCTL_PAUSE         _IOW('A', 0x45, int)
#define M_IOCTL_WRITEI_FRAMES _IOW('A', 0x50, struct m_xferi)

static char refine_why_buf[128];
static ealsa_mock_caps_t caps;
static int      fds, next_fd = 5, started, paused, prepares, xrun_pending, nonblock_open;
static int      frozen;
static unsigned eintr_pending, polls;
static unsigned took_rate, took_channels, took_period, took_buffer;
static int      took_format = -1, committed;
static size_t   queued, written;
static struct m_sw_params sw_taken;

static void caps_default(void)
{
   memset(&caps, 0, sizeof(caps));
   caps.rate_min = 8000;  caps.rate_max = 192000;
   caps.channels_min = 1; caps.channels_max = 8;
   caps.allow_float = 1;  caps.allow_s16 = 1;
   caps.period_min = 32;  caps.period_max = 8192;
   caps.period_granularity = 1;
   caps.can_pause = 1;
}

void ealsa_mock_reset(ealsa_mock_caps_t *c)
{
   if (c) caps = *c; else caps_default();
   fds = started = paused = prepares = xrun_pending = nonblock_open = 0;
   frozen = 0; eintr_pending = 0; polls = 0;
   took_rate = took_channels = took_period = took_buffer = 0;
   took_format = -1; committed = 0; queued = written = 0;
   memset(&sw_taken, 0, sizeof(sw_taken));
}
void ealsa_mock_drain(size_t frames) { queued = frames >= queued ? 0 : queued - frames; }
void ealsa_mock_inject_xrun(void) { xrun_pending = 1; }
void ealsa_mock_freeze(int on) { frozen = on; }
void ealsa_mock_inject_eintr(unsigned n) { eintr_pending = n; }
unsigned ealsa_mock_polls(void) { return polls; }
size_t   ealsa_mock_queued(void)   { return queued; }
size_t   ealsa_mock_written(void)  { return written; }
unsigned ealsa_mock_rate(void)     { return took_rate; }
unsigned ealsa_mock_channels(void) { return took_channels; }
int      ealsa_mock_format(void)   { return took_format; }
unsigned ealsa_mock_period(void)   { return took_period; }
unsigned ealsa_mock_buffer(void)   { return took_buffer; }
int      ealsa_mock_open_fds(void) { return fds; }
int      ealsa_mock_started(void)  { return started; }
int      ealsa_mock_paused(void)   { return paused; }
int      ealsa_mock_prepares(void) { return prepares; }
unsigned long ealsa_mock_boundary(void)        { return sw_taken.boundary; }
unsigned long ealsa_mock_stop_threshold(void)  { return sw_taken.stop_threshold; }
unsigned long ealsa_mock_start_threshold(void) { return sw_taken.start_threshold; }

int ealsa_mock_open(const char *path, int flags)
{
   /* only card 0, device 0 exists */
   if (caps.open_fails || !strstr(path, "pcmC0D0p"))
   {
      errno = ENOENT;
      return -1;
   }
   nonblock_open = (flags & O_NONBLOCK) ? 1 : 0;
   fds++;
   return next_fd++;
}
int ealsa_mock_close(int fd) { (void)fd; fds--; return 0; }

static struct m_mask *mmask(struct m_hw_params *p, unsigned n)
{ return &p->masks[n - M_P_FIRST_MASK]; }
static struct m_interval *miv(struct m_hw_params *p, unsigned n)
{ return &p->intervals[n - M_P_FIRST_INTERVAL]; }
static int mask_has(const struct m_mask *m, unsigned bit)
{ return (m->bits[bit >> 5] >> (bit & 31)) & 1; }

/* Narrow a set the way a card does: the caller's request intersected
 * with what the device can do, refused outright when the intersection
 * is empty. */
static int refine(struct m_hw_params *p)
{
   struct m_interval *rate = miv(p, M_P_RATE);
   struct m_interval *ch   = miv(p, M_P_CHANNELS);
   struct m_interval *per  = miv(p, M_P_PERIOD_SIZE);
   struct m_interval *pers = miv(p, M_P_PERIODS);
   struct m_interval *buf  = miv(p, M_P_BUFFER_SIZE);
   unsigned lo, hi;

   if (caps.refine_fails) { snprintf(refine_why_buf, sizeof(refine_why_buf), "refine_fails"); errno = EINVAL; return -1; }
   if (!mask_has(mmask(p, M_P_ACCESS), M_ACCESS_RW_INTERLEAVED)) { snprintf(refine_why_buf, sizeof(refine_why_buf), "access"); errno = EINVAL; return -1; }
   {
      int want_float = mask_has(mmask(p, M_P_FORMAT), M_FMT_FLOAT_LE);
      int want_s16   = mask_has(mmask(p, M_P_FORMAT), M_FMT_S16_LE);
      int ok_float   = want_float && caps.allow_float;
      int ok_s16     = want_s16   && caps.allow_s16;
      if (!ok_float && !ok_s16) { snprintf(refine_why_buf, sizeof(refine_why_buf), "format"); errno = EINVAL; return -1; }
      /* the device settles on one */
      memset(mmask(p, M_P_FORMAT)->bits, 0, sizeof(mmask(p, M_P_FORMAT)->bits));
      if (ok_float) mmask(p, M_P_FORMAT)->bits[M_FMT_FLOAT_LE >> 5] |= 1u << (M_FMT_FLOAT_LE & 31);
      else          mmask(p, M_P_FORMAT)->bits[M_FMT_S16_LE   >> 5] |= 1u << (M_FMT_S16_LE   & 31);
   }
   lo = rate->min > caps.rate_min ? rate->min : caps.rate_min;
   hi = rate->max < caps.rate_max ? rate->max : caps.rate_max;
   if (lo > hi) { snprintf(refine_why_buf, sizeof(refine_why_buf), "rate"); errno = EINVAL; return -1; }
   rate->min = lo; rate->max = hi;

   lo = ch->min > caps.channels_min ? ch->min : caps.channels_min;
   hi = ch->max < caps.channels_max ? ch->max : caps.channels_max;
   if (lo > hi) { snprintf(refine_why_buf, sizeof(refine_why_buf), "channels"); errno = EINVAL; return -1; }
   ch->min = lo; ch->max = hi;

   /* The period, its count and the buffer as ranges the card can
    * meet: min rounded up to what it counts in, max rounded down.
    * A refine narrows; it does not choose. */
   lo = per->min > caps.period_min ? per->min : caps.period_min;
   hi = per->max < caps.period_max ? per->max : caps.period_max;
   if (caps.period_granularity > 1)
   {
      unsigned g = caps.period_granularity;
      lo = ((lo + g - 1) / g) * g;
      hi = (hi / g) * g;
   }
   if (lo > hi) { snprintf(refine_why_buf, sizeof(refine_why_buf), "period range"); errno = EINVAL; return -1; }
   per->min = lo; per->max = hi;

   {
      unsigned nlo = pers->min > 2 ? pers->min : 2;
      unsigned nhi = pers->max < 16 ? pers->max : 16;
      if (caps.periods_fixed)
      {
         if (nlo > caps.periods_fixed || nhi < caps.periods_fixed)
         { snprintf(refine_why_buf, sizeof(refine_why_buf), "periods"); errno = EINVAL; return -1; }
         nlo = nhi = caps.periods_fixed;
      }
      if (nlo > nhi) { snprintf(refine_why_buf, sizeof(refine_why_buf), "periods range"); errno = EINVAL; return -1; }
      pers->min = nlo; pers->max = nhi;
   }

   /* The buffer is the product. A caller that has fixed it picks the
    * pair that makes it, which is how a buffer near a latency is
    * asked for without knowing the card's grid. */
   {
      unsigned blo = per->min * pers->min;
      unsigned bhi = per->max * pers->max;
      if (buf->min > blo) blo = buf->min;
      if (buf->max < bhi) bhi = buf->max;
      if (blo > bhi) { snprintf(refine_why_buf, sizeof(refine_why_buf), "buffer range"); errno = EINVAL; return -1; }
      if (buf->min == buf->max)
      {
         unsigned p_, n_, got = 0;
         for (p_ = per->min; p_ <= per->max && !got; p_ += (caps.period_granularity > 1 ? caps.period_granularity : 1))
            for (n_ = pers->min; n_ <= pers->max; n_++)
               if (p_ * n_ == buf->min) { per->min = per->max = p_; pers->min = pers->max = n_; got = 1; break; }
         if (!got) { snprintf(refine_why_buf, sizeof(refine_why_buf), "buffer not a period multiple"); errno = EINVAL; return -1; }
         blo = bhi = buf->min;
      }
      buf->min = blo; buf->max = bhi;
   }
   p->info  = caps.can_pause ? M_INFO_PAUSE : 0;
   return 0;
}

int ealsa_mock_ioctl(int fd, unsigned long req, void *arg)
{
   (void)fd;
   switch (req)
   {
      case M_IOCTL_HW_REFINE:
         return refine((struct m_hw_params*)arg);
      case M_IOCTL_HW_PARAMS:
      {
         struct m_hw_params *p = (struct m_hw_params*)arg;
         if (refine(p) < 0)
            return -1;
         /* the kernel settles each interval at its minimum */
         took_rate     = miv(p, M_P_RATE)->min;
         took_channels = miv(p, M_P_CHANNELS)->min;
         took_period   = miv(p, M_P_PERIOD_SIZE)->min;
         took_buffer   = took_period * miv(p, M_P_PERIODS)->min;
         miv(p, M_P_PERIOD_SIZE)->max = took_period;
         miv(p, M_P_BUFFER_SIZE)->min = miv(p, M_P_BUFFER_SIZE)->max = took_buffer;
         took_format   = mask_has(mmask(p, M_P_FORMAT), M_FMT_FLOAT_LE)
               ? M_FMT_FLOAT_LE : M_FMT_S16_LE;
         committed     = 1;
         return 0;
      }
      case M_IOCTL_SW_PARAMS:
         sw_taken = *(struct m_sw_params*)arg;
         return 0;
      case M_IOCTL_DELAY:
         *(m_sframes_t*)arg = (m_sframes_t)queued;
         return 0;
      case M_IOCTL_PREPARE:
         prepares++; started = 0; queued = 0;
         return 0;
      case M_IOCTL_START:
         if (!committed) { errno = EBADFD; return -1; }
         started = 1;
         return 0;
      case M_IOCTL_DROP:
         started = 0; queued = 0;
         return 0;
      case M_IOCTL_PAUSE:
         if (!caps.can_pause) { errno = ENOSYS; return -1; }
         paused = *(int*)arg ? 1 : 0;
         return 0;
      case M_IOCTL_WRITEI_FRAMES:
      {
         struct m_xferi *x = (struct m_xferi*)arg;
         size_t room;
         if (!committed) { errno = EBADFD; return -1; }
         if (xrun_pending) { xrun_pending = 0; errno = EPIPE; return -1; }
         if (paused || frozen) { errno = EAGAIN; return -1; }
         room = queued >= took_buffer ? 0 : took_buffer - queued;
         if (!room)        { errno = EAGAIN; return -1; }
         if (x->frames < room)
            room = x->frames;
         queued    += room;
         written   += room;
         x->result  = (m_sframes_t)room;
         return 0;
      }
      default:
         errno = ENOTTY;
         return -1;
   }
}

/* Writable when the device has room; a device that is full and not
 * draining times out, which is what the driver's bounded waits are
 * for. */
int ealsa_mock_poll(void *fds_, unsigned n, int timeout)
{
   struct pollfd *pfd = (struct pollfd*)fds_;
   (void)n; (void)timeout;
   polls++;
   if (eintr_pending)
   {
      eintr_pending--;
      errno = EINTR;
      return -1;
   }
   if (!paused && !frozen && queued < took_buffer)
   {
      pfd->revents = POLLOUT;
      return 1;
   }
   pfd->revents = 0;
   return 0;
}

/* for the harness: why the last refine refused */
const char *ealsa_mock_refine_why(void) { return refine_why_buf; }
