/* audio/drivers/oss.c's write_avail() and buffer_size() on a device that
 * cannot answer, and its wait_writable() on a device whose select()
 * does not wait for room.
 *
 * Both reports ask SNDCTL_DSP_GETOSPACE, and write_avail() is asked
 * every frame: a device that has gone away failed it every frame, and
 * each failure was a logged error. And buffer_size() answered 1 rather
 * than 0, which is how a driver says it has no size - audio_driver.h:
 * zero turns rate control off for the session; a one-byte buffer gave
 * it a setpoint of half a byte.
 *
 * The wait is RetroFW's jz4760 OSS driver, which has no poll method:
 * select() reports the descriptor ready at once, full or not, while
 * GETOSPACE counts whole free fragments. The threaded pipeline's
 * consumer asks wait_writable() for room, and the 0 that eight instant
 * wakes without room returned sent it straight back to ask again. At
 * real-time priority on that single core nothing else ran. The wait is
 * the blocking write's, which sleeps in the driver until a fragment is
 * played.
 *
 * free() on a device whose SNDCTL_DSP_RESET fails returned before
 * closing it, leaking the descriptor, the stage and the driver.
 *
 * The driver is this translation unit; a closed descriptor makes the
 * real ioctl fail, and ioctl, select, write and close are wrapped
 * (--wrap) so one descriptor is the modelled device. */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../../../audio/drivers/oss.c"

static unsigned failures  = 0;
static unsigned log_lines = 0;

#define CHECK(cond, msg, a, b) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s (%ld, %ld)\n", __FILE__, __LINE__, \
               msg, (long)(a), (long)(b)); \
         failures++; \
      } \
   } while (0)

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; log_lines++; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; log_lines++; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; log_lines++; }
uint32_t audio_driver_requested_layout(void) { return 0x3; }
unsigned audio_layout_channels(uint32_t layout) { (void)layout; return 2; }

/* The modelled device: jz4760_i2s.c's playback endpoint. */
#define DEV_FD        1000
#define DEV_FRAGSIZE  16384
#define DEV_FRAGS     16
/* A publish of 16-bit stereo at 32 kHz and 60 fps, in samples */
#define DEV_PUBLISH   1066

static int      dev_free_nodes;
static unsigned dev_selects;
static unsigned dev_write_sleeps;
/* select() as a driver without a poll method answers it (1), or as one
 * that waits for a fragment and times out on a full device (0) */
static int      dev_select_blind;
static unsigned dev_closes;

int __real_ioctl(int fd, unsigned long req, ...);
int __real_select(int n, fd_set *r, fd_set *w, fd_set *e,
      struct timeval *tv);
ssize_t __real_write(int fd, const void *buf, size_t len);
int __real_close(int fd);

int __wrap_close(int fd)
{
   if (fd != DEV_FD)
      return __real_close(fd);
   dev_closes++;
   return 0;
}

int __wrap_ioctl(int fd, unsigned long req, ...)
{
   va_list ap;
   void *arg;
   va_start(ap, req);
   arg = va_arg(ap, void*);
   va_end(ap);
   if (fd != DEV_FD)
      return __real_ioctl(fd, req, arg);
   if (req == SNDCTL_DSP_GETOSPACE)
   {
      audio_buf_info *info = (audio_buf_info*)arg;
      info->fragments      = dev_free_nodes;
      info->fragstotal     = DEV_FRAGS;
      info->fragsize       = DEV_FRAGSIZE;
      info->bytes          = dev_free_nodes * DEV_FRAGSIZE;
      return 0;
   }
   return -1;
}

int __wrap_select(int n, fd_set *r, fd_set *w, fd_set *e,
      struct timeval *tv)
{
   if (!w || !FD_ISSET(DEV_FD, w))
      return __real_select(n, r, w, e, tv);
   dev_selects++;
   if (dev_select_blind)
      return 1;
   FD_ZERO(w);
   return 0;
}

/* A node per fragment's worth, however short, as endpoint_put_userdata()
 * takes them; a blocking write on a full device sleeps until the DMA
 * interrupt frees one. */
ssize_t __wrap_write(int fd, const void *buf, size_t len)
{
   size_t done = 0;
   if (fd != DEV_FD)
      return __real_write(fd, buf, len);
   while (done < len)
   {
      size_t n = len - done;
      if (n > DEV_FRAGSIZE)
         n = DEV_FRAGSIZE;
      if (!dev_free_nodes)
      {
         dev_write_sleeps++;
         dev_free_nodes++;
      }
      dev_free_nodes--;
      done += n;
   }
   return (ssize_t)done;
}

static void dev_open(oss_audio_t *oss, int blind)
{
   memset(oss, 0, sizeof(*oss));
   oss->fd          = DEV_FD;
   oss->rate        = 32000;
   oss->channels    = 2;
   dev_free_nodes   = 0;
   dev_selects      = 0;
   dev_write_sleeps = 0;
   dev_select_blind = blind;
}

int main(void)
{
   static int16_t pcm[DEV_PUBLISH];
   oss_audio_t oss;
   unsigned frame;
   unsigned selects;
   size_t room;

   memset(&oss, 0, sizeof(oss));
   oss.fd = -1;   /* a device that has gone away */

   for (frame = 0; frame < 60; frame++)
      CHECK(oss_write_avail(&oss) == 0, "no room from a device that cannot say",
            oss_write_avail(&oss), 0);
   CHECK(oss_buffer_size(&oss) == 0, "no size from a device that cannot say",
         oss_buffer_size(&oss), 0);
   CHECK(log_lines == 0, "a second of frames logs nothing", log_lines, 0);

   /* A full device whose select() reports ready at once: each pass asks
    * for a publish's worth of room, and the room is claimed so the
    * write sleeps in the driver - and once that is found, select() is
    * not asked again. */
   dev_open(&oss, 1);
   for (frame = 0; frame < 60; frame++)
   {
      room = oss_wait_writable(&oss, sizeof(pcm));
      CHECK(room > 0, "a full poll-less device hands the wait to the write",
            room, frame);
      if (!room)
         break;
      CHECK(oss_write(&oss, pcm, sizeof(pcm)) == (ssize_t)sizeof(pcm),
            "the claimed write is taken whole", frame, 0);
   }
   CHECK(dev_write_sleeps == 60, "every write waited in the driver",
         dev_write_sleeps, 60);
   selects = dev_selects;
   CHECK(selects <= OSS_WAIT_WRITABLE_LAPS + 1,
         "select() is not asked again once it has woken without room",
         selects, OSS_WAIT_WRITABLE_LAPS + 1);
   CHECK(log_lines == 0, "and nothing is logged while it plays", log_lines, 0);

   /* Non-blocking, the write would refuse: no room is claimed, and
    * select() is still not asked. */
   oss.nonblock = true;
   room = oss_wait_writable(&oss, sizeof(pcm));
   CHECK(room == 0, "a non-blocking poll-less device claims no room", room, 0);
   CHECK(dev_selects == selects, "nor asks select() again",
         dev_selects, selects);
   oss.nonblock = false;

   /* With room, the free space comes back, as it always did. */
   dev_free_nodes = 3;
   room = oss_wait_writable(&oss, sizeof(pcm));
   CHECK(room == 3 * DEV_FRAGSIZE, "room on a poll-less device is the free space",
         room, 3 * DEV_FRAGSIZE);

   /* A full device whose select() does wait, and times out: the pass is
    * skipped after one bounded wait, and the device is not taken for a
    * poll-less one. */
   dev_open(&oss, 0);
   room = oss_wait_writable(&oss, sizeof(pcm));
   CHECK(room == 0, "a stalled device with a working select() skips the pass",
         room, 0);
   CHECK(dev_selects == 1, "after one bounded wait", dev_selects, 1);
   CHECK(!oss.select_blind, "and is not latched as poll-less", 1, 0);

   /* SNDCTL_DSP_RESET fails on the modelled device: free() still closes
    * it, and LeakSanitizer sees the stage and the driver freed. */
   {
      oss_audio_t *heap = (oss_audio_t*)calloc(1, sizeof(*heap));
      heap->fd          = DEV_FD;
      heap->stage       = (int16_t*)malloc(64);
      dev_closes        = 0;
      oss_free(heap);
      CHECK(dev_closes == 1, "a device that cannot reset is still closed",
            dev_closes, 1);
   }

   if (failures)
   {
      printf("[fail] oss_report_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] oss_report_test\n");
   return 0;
}
