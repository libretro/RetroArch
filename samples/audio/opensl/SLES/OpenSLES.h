/* A stand-in for Android's <SLES/OpenSLES.h>, enough of it to build
 * and run audio/drivers/opensl.c on a host: the object and interface
 * shapes the driver uses, over a scripted player that consumes the
 * buffer queue on a thread the way a device does.
 *
 * The real header is Khronos'/Android's; this exists so the driver has
 * coverage on a machine that is not a phone. */
#ifndef OPENSL_MOCK_H
#define OPENSL_MOCK_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t SLresult;
typedef uint32_t SLuint32;
typedef uint16_t SLuint16;
typedef uint8_t  SLuint8;
typedef int32_t  SLint32;
typedef uint32_t SLboolean;
typedef const void *SLInterfaceID;

#define SL_RESULT_SUCCESS            0
#define SL_RESULT_PARAMETER_INVALID  13
#define SL_RESULT_MEMORY_FAILURE     3
#define SL_BOOLEAN_FALSE             0
#define SL_BOOLEAN_TRUE              1

#define SL_DATALOCATOR_OUTPUTMIX     0x00000003
#define SL_DATAFORMAT_PCM            0x00000002
#define SL_BYTEORDER_LITTLEENDIAN    0x00000002
#define SL_SPEAKER_FRONT_LEFT        0x00000001
#define SL_SPEAKER_FRONT_RIGHT       0x00000002
#define SL_SAMPLINGRATE_44_1         44100000
#define SL_PCMSAMPLEFORMAT_FIXED_16  16

#define SL_PLAYSTATE_STOPPED         0x00000001
#define SL_PLAYSTATE_PAUSED          0x00000002
#define SL_PLAYSTATE_PLAYING         0x00000003

extern const SLInterfaceID SL_IID_ENGINE;
extern const SLInterfaceID SL_IID_PLAY;

struct SLObjectItf_;
typedef const struct SLObjectItf_ * const * SLObjectItf;

typedef struct { SLuint32 locatorType; SLObjectItf outputMix; } SLDataLocator_OutputMix;

typedef struct
{
   SLuint32 formatType, numChannels, samplesPerSec, bitsPerSample;
   SLuint32 containerSize, channelMask, endianness;
} SLDataFormat_PCM;

typedef struct { void *pLocator; void *pFormat; } SLDataSource;
typedef struct { void *pLocator; void *pFormat; } SLDataSink;

struct SLObjectItf_
{
   SLresult (*Realize)(SLObjectItf self, SLboolean async);
   SLresult (*GetInterface)(SLObjectItf self, SLInterfaceID iid, void *pItf);
   void     (*Destroy)(SLObjectItf self);
};

struct SLEngineItf_;
typedef const struct SLEngineItf_ * const * SLEngineItf;
struct SLEngineItf_
{
   SLresult (*CreateOutputMix)(SLEngineItf self, SLObjectItf *mix,
         SLuint32 n, const SLInterfaceID *ids, const SLboolean *req);
   SLresult (*CreateAudioPlayer)(SLEngineItf self, SLObjectItf *player,
         SLDataSource *src, SLDataSink *sink,
         SLuint32 n, const SLInterfaceID *ids, const SLboolean *req);
};

struct SLPlayItf_;
typedef const struct SLPlayItf_ * const * SLPlayItf;
struct SLPlayItf_
{
   SLresult (*SetPlayState)(SLPlayItf self, SLuint32 state);
};

SLresult slCreateEngine(SLObjectItf *engine, SLuint32 n, const void *opts,
      SLuint32 nids, const SLInterfaceID *ids, const SLboolean *req);

#endif
