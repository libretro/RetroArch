/* The slice of the BSD audioio interface audio/drivers/audioio.c
 * compiles against, for a Linux build of the driver in this test. No
 * ioctl here is ever issued: the test stops the driver at open(). */
#ifndef SHIM_SYS_AUDIOIO_H
#define SHIM_SYS_AUDIOIO_H

#include <string.h>

struct audio_prinfo
{
   unsigned sample_rate, channels, precision, encoding;
   unsigned buffer_size, seek;
   unsigned char pause;
};

struct audio_info
{
   struct audio_prinfo play;
   unsigned mode;
   unsigned blocksize;
};

typedef struct audio_device
{
   char name[16];
   char version[16];
   char config[16];
} audio_device_t;

#define AUDIO_INITINFO(p)      memset((p), 0xff, sizeof(*(p)))
#define AUMODE_PLAY            1
#define AUMODE_PLAY_ALL        4
#define AUDIO_ENCODING_SLINEAR 6
#define AUDIO_ENCODING_LINEAR  3
#define AUDIO_SETINFO          0x1001
#define AUDIO_GETINFO          0x1002
#define AUDIO_GETBUFINFO       0x1003
#define AUDIO_GETDEV           0x1004
#define AUDIO_FLUSH            0x1005

#endif
