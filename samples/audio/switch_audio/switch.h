/* A stand-in <switch.h>: the libnx audout surface
 * audio/drivers/switch_audio.c uses, so the driver builds and runs on
 * the host. audout_mock.c is the service: a queue of appended buffers
 * that plays out, oldest first, when the test says so. */

#ifndef SWITCH_AUDIO_MOCK_SWITCH_H
#define SWITCH_AUDIO_MOCK_SWITCH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t  s64;
typedef u32      Result;

typedef struct AudioOutBuffer AudioOutBuffer;
struct AudioOutBuffer
{
   AudioOutBuffer *next;
   void *buffer;
   u64 buffer_size;
   u64 data_size;
   u64 data_offset;
};

Result audoutInitialize(void);
void   audoutExit(void);
Result audoutStartAudioOut(void);
Result audoutStopAudioOut(void);
Result audoutAppendAudioOutBuffer(AudioOutBuffer *buffer);
Result audoutGetReleasedAudioOutBuffer(AudioOutBuffer **buffer, u32 *released_count);
Result audoutWaitPlayFinish(AudioOutBuffer **released, u32 *released_count, u64 timeout);
u32    audoutGetSampleRate(void);

#endif
