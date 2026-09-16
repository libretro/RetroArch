/* A scripted PCM character device behind the embedded ALSA driver's
 * syscall seam (EALSA_SYSCALLS in audio/drivers/alsa.c).
 *
 * The driver talks to the kernel's PCM ABI and nothing else, so a
 * device that answers its ioctls the way a card does is enough to run
 * it on a machine with no card - and, unlike a card, it can be told
 * to refuse a rate, a format, a channel count or a period size, which
 * is where a negotiation goes wrong. */
#ifndef EALSA_MOCK_H
#define EALSA_MOCK_H

#include <stddef.h>
#include <sys/types.h>

/* what the device can do; set before the driver opens it */
typedef struct
{
   unsigned rate_min, rate_max;
   unsigned channels_min, channels_max;
   int      allow_float;             /* the device takes FLOAT_LE */
   int      allow_s16;
   unsigned period_min, period_max;  /* frames; the refine clamps into this */
   unsigned period_granularity;      /* rounded up to a multiple of this */
   unsigned periods_fixed;           /* if set, the device insists on this many */
   int      can_pause;
   int      open_fails;
   int      refine_fails;
   int      params_fail_until_refine;/* HW_PARAMS refuses what a refine did not settle */
} ealsa_mock_caps_t;

void ealsa_mock_reset(ealsa_mock_caps_t *caps);   /* NULL: a cooperative card */
void ealsa_mock_drain(size_t frames);             /* the device plays this much */
void ealsa_mock_inject_xrun(void);
void ealsa_mock_freeze(int on);        /* the device takes nothing and never signals */
void ealsa_mock_inject_eintr(unsigned n);  /* the next n polls are cut short by a signal */
unsigned ealsa_mock_polls(void);                /* the next write returns EPIPE */
size_t ealsa_mock_queued(void);
size_t ealsa_mock_written(void);
unsigned ealsa_mock_rate(void);
unsigned ealsa_mock_channels(void);
int      ealsa_mock_format(void);                 /* the EALSA_FMT_ value taken */
unsigned ealsa_mock_period(void);
unsigned ealsa_mock_buffer(void);
int      ealsa_mock_open_fds(void);
int      ealsa_mock_started(void);
int      ealsa_mock_paused(void);
int      ealsa_mock_prepares(void);
unsigned long ealsa_mock_boundary(void);
unsigned long ealsa_mock_stop_threshold(void);
unsigned long ealsa_mock_start_threshold(void);

int  ealsa_mock_open(const char *path, int flags);
int  ealsa_mock_ioctl(int fd, unsigned long req, void *arg);
int  ealsa_mock_close(int fd);
int  ealsa_mock_poll(void *fds, unsigned n, int timeout);
const char *ealsa_mock_refine_why(void);

#endif
