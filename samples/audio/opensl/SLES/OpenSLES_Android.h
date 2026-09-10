/* The Android additions the driver uses: the simple buffer queue, and
 * the extended PCM format that carries a float representation. */
#ifndef OPENSL_MOCK_ANDROID_H
#define OPENSL_MOCK_ANDROID_H

#include "OpenSLES.h"

#define SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE 0x800007BD
#define SL_ANDROID_DATAFORMAT_PCM_EX            0x800007C0
#define SL_ANDROID_PCM_REPRESENTATION_FLOAT     0x00000003

extern const SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE;

typedef struct { SLuint32 locatorType; SLuint32 numBuffers; }
   SLDataLocator_AndroidSimpleBufferQueue;

typedef struct
{
   SLuint32 formatType, numChannels, sampleRate, bitsPerSample;
   SLuint32 containerSize, channelMask, endianness, representation;
} SLAndroidDataFormat_PCM_EX;

typedef struct { SLuint32 count, index; } SLAndroidSimpleBufferQueueState;

struct SLAndroidSimpleBufferQueueItf_;
typedef const struct SLAndroidSimpleBufferQueueItf_ * const * SLAndroidSimpleBufferQueueItf;
struct SLAndroidSimpleBufferQueueItf_
{
   SLresult (*Enqueue)(SLAndroidSimpleBufferQueueItf self, const void *buf, SLuint32 size);
   SLresult (*Clear)(SLAndroidSimpleBufferQueueItf self);
   SLresult (*GetState)(SLAndroidSimpleBufferQueueItf self, SLAndroidSimpleBufferQueueState *state);
   SLresult (*RegisterCallback)(SLAndroidSimpleBufferQueueItf self,
         void (*cb)(SLAndroidSimpleBufferQueueItf, void*), void *ctx);
};

/* the harness's controls */
void   opensl_mock_reset(void);
void   opensl_mock_set_float_supported(int on);
void   opensl_mock_set_queue_limit(unsigned max_buffers);   /* 0: no limit */
void   opensl_mock_freeze(int on);            /* the device stops consuming */
unsigned opensl_mock_num_buffers(void);       /* numBuffers the player was created with */
unsigned opensl_mock_buffer_bytes(void);      /* the size of the blocks enqueued */
unsigned opensl_mock_rate_milli(void);
int      opensl_mock_is_float(void);
size_t   opensl_mock_consumed(void);          /* blocks the device has played */
unsigned opensl_mock_enqueue_failures(void);
int      opensl_mock_playing(void);
int      opensl_mock_objects(void);           /* created less destroyed */

#endif
