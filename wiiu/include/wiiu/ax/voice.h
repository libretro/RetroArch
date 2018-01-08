#pragma once
#include <wiiu/types.h>
#include "device.h"
#include "result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*AXVoiceCallbackFn)(void *);
typedef void(*AXVoiceCallbackExFn)(void *, uint32_t, uint32_t);

enum AX_VOICE_FORMAT
{
   AX_VOICE_FORMAT_ADPCM   = 0,
   AX_VOICE_FORMAT_LPCM16  = 10,
   AX_VOICE_FORMAT_LPCM8   = 25,
};
typedef uint16_t AXVoiceFormat;

enum AX_VOICE_LOOP
{
   AX_VOICE_LOOP_DISABLED  = 0,
   AX_VOICE_LOOP_ENABLED   = 1,
};
typedef uint16_t AXVoiceLoop;

enum AX_VOICE_RENDERER
{
   AX_VOICE_RENDERER_DSP   = 0,
   AX_VOICE_RENDERER_CPU   = 1,
   AX_VOICE_RENDERER_AUTO  = 2,
};
typedef uint32_t AXVoiceRenderer;

enum AX_VOICE_RATIO_RESULT
{
   AX_VOICE_RATIO_RESULT_SUCCESS                   = 0,
   AX_VOICE_RATIO_RESULT_LESS_THAN_ZERO            = -1,
   AX_VOICE_RATIO_RESULT_GREATER_THAN_SOMETHING    = -2,
};
typedef int32_t AXVoiceSrcRatioResult;

enum AX_VOICE_SRC_TYPE
{
   AX_VOICE_SRC_TYPE_NONE     = 0,
   AX_VOICE_SRC_TYPE_LINEAR   = 1,
   AX_VOICE_SRC_TYPE_UNK0     = 2,
   AX_VOICE_SRC_TYPE_UNK1     = 3,
   AX_VOICE_SRC_TYPE_UNK2     = 4,
};
typedef uint32_t AXVoiceSrcType;

enum AX_VOICE_STATE
{
   AX_VOICE_STATE_STOPPED  = 0,
   AX_VOICE_STATE_PLAYING  = 1,
};
typedef uint32_t AXVoiceState;

enum AX_VOICE_TYPE
{
   /* Unknown */
   AX_VOICE_TYPE_UNKNOWN
};
typedef uint32_t AXVoiceType;

typedef struct
{
   AXVoiceFormat dataType;
   AXVoiceLoop loopingEnabled;
   uint32_t loopOffset;
   uint32_t endOffset;
   uint32_t currentOffset;
   const void *data;
} AXVoiceOffsets;

typedef struct AXVoice AXVoice;

typedef struct
{
   AXVoice *next;
   AXVoice *prev;
} AXVoiceLink;

struct AXVoice
{
   uint32_t index;
   AXVoiceState state;
   uint32_t volume;
   AXVoiceRenderer renderer;
   AXVoiceLink link;
   AXVoice *cbNext;
   uint32_t priority;
   AXVoiceCallbackFn callback;
   void *userContext;
   uint32_t syncBits;
   uint32_t _unknown[0x2];
   AXVoiceOffsets offsets;
   AXVoiceCallbackExFn callbackEx;
   uint32_t callbackReason;
   float unk0;
   float unk1;
};

typedef struct
{
   uint16_t volume;
   int16_t delta;
} AXVoiceDeviceBusMixData;

typedef struct
{
   AXVoiceDeviceBusMixData bus[4];
} AXVoiceDeviceMixData;

typedef struct
{
   uint16_t volume;
   int16_t delta;
} AXVoiceVeData;

typedef struct
{
   uint16_t predScale;
   int16_t prevSample[2];
} AXVoiceAdpcmLoopData;

typedef struct
{
   int16_t coefficients[16];
   uint16_t gain;
   uint16_t predScale;
   int16_t prevSample[2];
} AXVoiceAdpcm;

/* AXVoice Sample Rate Converter */
typedef struct
{
   uint16_t ratio_int;
   uint16_t ratio_fraction;
   uint16_t currentOffsetFrac;
   int16_t lastSample[4];
} AXVoiceSrc;

AXVoice *AXAcquireVoice(uint32_t priority, AXVoiceCallbackFn callback, void *userContext);
AXVoice *AXAcquireVoiceEx(uint32_t priority,  AXVoiceCallbackExFn callback, void *userContext);
BOOL AXCheckVoiceOffsets(AXVoiceOffsets *offsets);
void AXFreeVoice(AXVoice *voice);
uint32_t AXGetMaxVoices();
uint32_t AXGetVoiceCurrentOffsetEx(AXVoice *voice, const void *samples);
uint32_t AXGetVoiceLoopCount(AXVoice *voice);
void AXGetVoiceOffsets(AXVoice *voice, AXVoiceOffsets *offsets);
BOOL AXIsVoiceRunning(AXVoice *voice);
void AXSetVoiceAdpcm(AXVoice *voice, AXVoiceAdpcm *adpcm);
void AXSetVoiceAdpcmLoop(AXVoice *voice, AXVoiceAdpcmLoopData *loopData);
void AXSetVoiceCurrentOffset(AXVoice *voice, uint32_t offset);
AXResult AXSetVoiceDeviceMix(AXVoice *voice, AXDeviceType type, uint32_t id, AXVoiceDeviceMixData *mixData);
void AXSetVoiceEndOffset(AXVoice *voice, uint32_t offset);
void AXSetVoiceEndOffsetEx(AXVoice *voice, uint32_t offset, const void *samples);
AXResult AXSetVoiceInitialTimeDelay(AXVoice *voice, uint16_t delay);
void AXSetVoiceLoopOffset(AXVoice *voice, uint32_t offset);
void AXSetVoiceLoopOffsetEx(AXVoice *voice, uint32_t offset, const void *samples);
void AXSetVoiceLoop(AXVoice *voice, AXVoiceLoop loop);
void AXSetVoiceOffsets(AXVoice *voice, AXVoiceOffsets *offsets);
void AXSetVoicePriority(AXVoice *voice, uint32_t priority);
void AXSetVoiceRmtIIRCoefs(AXVoice *voice, uint16_t filter, ...);
void AXSetVoiceSrc(AXVoice *voice, AXVoiceSrc *src);
AXVoiceSrcRatioResult AXSetVoiceSrcRatio(AXVoice *voice, float ratio);
void AXSetVoiceSrcType(AXVoice *voice, AXVoiceSrcType type);
void AXSetVoiceState(AXVoice *voice, AXVoiceState state);
void AXSetVoiceType(AXVoice *voice, AXVoiceType type);
void AXSetVoiceVe(AXVoice *voice, AXVoiceVeData *veData);
void AXSetVoiceVeDelta(AXVoice *voice, int16_t delta);

#ifdef __cplusplus
}
#endif
