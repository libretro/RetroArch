#ifndef OPENSLES_ANDROID_STUB_H
#define OPENSLES_ANDROID_STUB_H
#include <SLES/OpenSLES.h>
#define SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE 0x800007BD
#define SL_ANDROID_DATAFORMAT_PCM_EX 0x00000004
#define SL_ANDROID_PCM_REPRESENTATION_FLOAT 3
typedef struct { SLuint32 locatorType, numBuffers; } SLDataLocator_AndroidSimpleBufferQueue;
typedef struct { SLuint32 formatType, numChannels, sampleRate, bitsPerSample,
   containerSize, channelMask, endianness, representation; } SLAndroidDataFormat_PCM_EX;
struct SLAndroidSimpleBufferQueueItf_;
typedef const struct SLAndroidSimpleBufferQueueItf_ * const * SLAndroidSimpleBufferQueueItf;
struct SLAndroidSimpleBufferQueueItf_ {
   SLresult (*Enqueue)(SLAndroidSimpleBufferQueueItf, const void*, SLuint32);
   SLresult (*Clear)(SLAndroidSimpleBufferQueueItf);
   SLresult (*RegisterCallback)(SLAndroidSimpleBufferQueueItf,
         void (*)(SLAndroidSimpleBufferQueueItf, void*), void*);
};
extern SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
#endif
