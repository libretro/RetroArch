/* Minimal OpenSL ES stub: only what audio/drivers/opensl.c names,
 * for a Linux syntax pass. Never runs. */
#ifndef OPENSLES_STUB_H
#define OPENSLES_STUB_H
#include <stdint.h>
typedef uint32_t SLresult; typedef uint32_t SLuint32; typedef uint32_t SLboolean;
typedef int32_t SLint32; typedef uint16_t SLuint16; typedef uint32_t SLmillisecond;
typedef void *SLInterfaceID;
#define SL_RESULT_SUCCESS 0
#define SL_BOOLEAN_TRUE 1
#define SL_BOOLEAN_FALSE 0
#define SL_PLAYSTATE_STOPPED 1
#define SL_PLAYSTATE_PLAYING 3
#define SL_DATAFORMAT_PCM 2
#define SL_BYTEORDER_LITTLEENDIAN 2
#define SL_SPEAKER_FRONT_LEFT 1
#define SL_SPEAKER_FRONT_RIGHT 2
#define SL_DATALOCATOR_OUTPUTMIX 3
typedef struct { SLuint32 formatType, numChannels, samplesPerSec, bitsPerSample,
   containerSize, channelMask, endianness; } SLDataFormat_PCM;
typedef struct { void *pLocator, *pFormat; } SLDataSource;
typedef struct { void *pLocator, *pFormat; } SLDataSink;
struct SLObjectItf_; typedef const struct SLObjectItf_ * const * SLObjectItf;
typedef struct { SLuint32 locatorType; SLObjectItf outputMix; } SLDataLocator_OutputMix;
struct SLEngineItf_; typedef const struct SLEngineItf_ * const * SLEngineItf;
struct SLPlayItf_;   typedef const struct SLPlayItf_ * const * SLPlayItf;
struct SLObjectItf_ {
   SLresult (*Realize)(SLObjectItf, SLboolean);
   SLresult (*GetInterface)(SLObjectItf, SLInterfaceID, void*);
   void (*Destroy)(SLObjectItf);
};
struct SLEngineItf_ {
   SLresult (*CreateOutputMix)(SLEngineItf, SLObjectItf*, SLuint32,
         const SLInterfaceID*, const SLboolean*);
   SLresult (*CreateAudioPlayer)(SLEngineItf, SLObjectItf*, SLDataSource*,
         SLDataSink*, SLuint32, const SLInterfaceID*, const SLboolean*);
};
struct SLPlayItf_ {
   SLresult (*SetPlayState)(SLPlayItf, SLuint32);
   SLresult (*GetPlayState)(SLPlayItf, SLuint32*);
};
extern SLInterfaceID SL_IID_ENGINE, SL_IID_PLAY;
SLresult slCreateEngine(SLObjectItf*, SLuint32, const void*, SLuint32,
      const SLInterfaceID*, const SLboolean*);
#endif
