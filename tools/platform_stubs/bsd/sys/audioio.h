/* Compile-only stand-in for NetBSD's <sys/audioio.h>, the audio(4)
 * interface. Enough of it for audio/drivers/audioio.c to be held to a
 * syntax gate on a host: the header ships with the systems that have
 * that driver - NetBSD and Solaris - and nothing else in this tree
 * compiles it.
 *
 * Shaped after NetBSD's, which is the branch that has play.seek;
 * Solaris audio_prinfo carries buffer_size and samples but no seek,
 * and the driver's __NetBSD__ gate is what tells them apart. Values
 * are arbitrary where the driver only assigns or compares them. */
#ifndef BSDSTUB_SYS_AUDIOIO_H
#define BSDSTUB_SYS_AUDIOIO_H

#include <string.h>
#include <sys/types.h>

struct audio_prinfo
{
   unsigned int  sample_rate;
   unsigned int  channels;
   unsigned int  precision;
   unsigned int  encoding;
   unsigned int  gain;
   unsigned int  port;
   unsigned int  seek;         /* BSD extension: bytes pending */
   unsigned int  avail_ports;
   unsigned int  buffer_size;  /* total size of the audio buffer */
   unsigned int  samples;
   unsigned int  eof;
   unsigned char pause;
   unsigned char error;
   unsigned char waiting;
   unsigned char balance;
   unsigned char open;
   unsigned char active;
};

struct audio_info
{
   struct audio_prinfo play;
   struct audio_prinfo record;
   unsigned int monitor_gain;
   unsigned int blocksize;
   unsigned int hiwat;
   unsigned int lowat;
   unsigned int mode;
};

#define AUMODE_PLAY     0x01
#define AUMODE_RECORD   0x02
#define AUMODE_PLAY_ALL 0x04

#define AUDIO_INITINFO(p) memset((void *)(p), 0xff, sizeof(struct audio_info))

#define AUDIO_ENCODING_NONE       0
#define AUDIO_ENCODING_ULAW       1
#define AUDIO_ENCODING_ALAW       2
#define AUDIO_ENCODING_LINEAR     3
#define AUDIO_ENCODING_SLINEAR    10
#define AUDIO_ENCODING_ULINEAR    11

#define AUDIO_GETINFO    0x4121
#define AUDIO_SETINFO    0x4122
#define AUDIO_DRAIN      0x4123
#define AUDIO_FLUSH      0x4124
#define AUDIO_PERROR     0x4131

/* NetBSD's, and not Solaris's, which is what the driver keys the
 * play.seek branch off. Define BSDSTUB_NO_GETBUFINFO to compile the
 * other side of that gate on a host. */
#ifndef BSDSTUB_NO_GETBUFINFO
#define AUDIO_GETBUFINFO 0x4135
#endif

#endif
