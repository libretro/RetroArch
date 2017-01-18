#pragma once
#include <wut.h>
#include "device.h"
#include "result.h"

/**
 * \defgroup sndcore2_voice Voice
 * \ingroup sndcore2
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AXVoice AXVoice;
typedef struct AXVoiceAdpcmLoopData AXVoiceAdpcmLoopData;
typedef struct AXVoiceAdpcm AXVoiceAdpcm;
typedef struct AXVoiceDeviceBusMixData AXVoiceDeviceBusMixData;
typedef struct AXVoiceDeviceMixData AXVoiceDeviceMixData;
typedef struct AXVoiceLink AXVoiceLink;
typedef struct AXVoiceOffsets AXVoiceOffsets;
typedef struct AXVoiceSrc AXVoiceSrc;
typedef struct AXVoiceVeData AXVoiceVeData;

//! A value from enum AX_VOICE_FORMAT.
typedef uint16_t AXVoiceFormat;

//! A value from enum AX_VOICE_LOOP.
typedef uint16_t AXVoiceLoop;

//! A value from enum AX_VOICE_SRC_TYPE.
typedef uint32_t AXVoiceSrcType;

//! A value from enum AX_VOICE_STATE.
typedef uint32_t AXVoiceState;

//! A value from enum AX_VOICE_RENDERER.
typedef uint32_t AXVoiceRenderer;

//! A value from enum AX_VOICE_RATIO_RESULT.
typedef int32_t AXVoiceSrcRatioResult;

//! A value from enum AX_VOICE_TYPE.
typedef uint32_t AXVoiceType;

typedef void(*AXVoiceCallbackFn)(void *);
typedef void(*AXVoiceCallbackExFn)(void *, uint32_t, uint32_t);

enum AX_VOICE_FORMAT
{
   AX_VOICE_FORMAT_ADPCM   = 0,
   AX_VOICE_FORMAT_LPCM16  = 10,
   AX_VOICE_FORMAT_LPCM8   = 25,
};

enum AX_VOICE_LOOP
{
   AX_VOICE_LOOP_DISABLED  = 0,
   AX_VOICE_LOOP_ENABLED   = 1,
};

enum AX_VOICE_RENDERER
{
   AX_VOICE_RENDERER_DSP   = 0,
   AX_VOICE_RENDERER_CPU   = 1,
   AX_VOICE_RENDERER_AUTO  = 2,
};

enum AX_VOICE_RATIO_RESULT
{
   AX_VOICE_RATIO_RESULT_SUCCESS                   = 0,
   AX_VOICE_RATIO_RESULT_LESS_THAN_ZERO            = -1,
   AX_VOICE_RATIO_RESULT_GREATER_THAN_SOMETHING    = -2,
};

enum AX_VOICE_SRC_TYPE
{
   AX_VOICE_SRC_TYPE_NONE     = 0,
   AX_VOICE_SRC_TYPE_LINEAR   = 1,
   AX_VOICE_SRC_TYPE_UNK0     = 2,
   AX_VOICE_SRC_TYPE_UNK1     = 3,
   AX_VOICE_SRC_TYPE_UNK2     = 4,
};

enum AX_VOICE_STATE
{
   AX_VOICE_STATE_STOPPED  = 0,
   AX_VOICE_STATE_PLAYING  = 1,
};

enum AX_VOICE_TYPE
{
   // Unknown
   AX_VOICE_TYPE_UNKNOWN
};

struct AXVoiceLink
{
   AXVoice *next;
   AXVoice *prev;
};
CHECK_OFFSET(AXVoiceLink, 0x0, next);
CHECK_OFFSET(AXVoiceLink, 0x4, prev);
CHECK_SIZE(AXVoiceLink, 0x8);

struct AXVoiceOffsets
{
   AXVoiceFormat dataType;
   AXVoiceLoop loopingEnabled;
   uint32_t loopOffset;
   uint32_t endOffset;
   uint32_t currentOffset;
   const void *data;
};
CHECK_OFFSET(AXVoiceOffsets, 0x0, dataType);
CHECK_OFFSET(AXVoiceOffsets, 0x2, loopingEnabled);
CHECK_OFFSET(AXVoiceOffsets, 0x4, loopOffset);
CHECK_OFFSET(AXVoiceOffsets, 0x8, endOffset);
CHECK_OFFSET(AXVoiceOffsets, 0xc, currentOffset);
CHECK_OFFSET(AXVoiceOffsets, 0x10, data);
CHECK_SIZE(AXVoiceOffsets, 0x14);

struct AXVoice
{
   //! The index of this voice out of the total voices
   uint32_t index;

   //! Current play state of this voice
   AXVoiceState state;

   //! Current volume of this voice
   uint32_t volume;

   //! The renderer to use for this voice
   AXVoiceRenderer renderer;

   //! this is a link used in the stack, we do this in host-memory currently
   AXVoiceLink link;

   //! A link to the next callback to invoke
   AXVoice *cbNext;

   //! The priority of this voice used for force-acquiring a voice
   uint32_t priority;

   //! The callback to call if this is force-free'd by another acquire
   AXVoiceCallbackFn callback;

   //! The user context to send to the callbacks
   void *userContext;

   //! A bitfield representing different things needing to be synced.
   uint32_t syncBits;

   UNKNOWN(0x8);

   //! The current offset data!
   AXVoiceOffsets offsets;

   //! An extended version of the callback above
   AXVoiceCallbackExFn callbackEx;

   //! The reason for the callback being invoked
   uint32_t callbackReason;

   float unk0;
   float unk1;
};
CHECK_OFFSET(AXVoice, 0x0, index);
CHECK_OFFSET(AXVoice, 0x4, state);
CHECK_OFFSET(AXVoice, 0x8, volume);
CHECK_OFFSET(AXVoice, 0xc, renderer);
CHECK_OFFSET(AXVoice, 0x10, link);
CHECK_OFFSET(AXVoice, 0x18, cbNext);
CHECK_OFFSET(AXVoice, 0x1c, priority);
CHECK_OFFSET(AXVoice, 0x20, callback);
CHECK_OFFSET(AXVoice, 0x24, userContext);
CHECK_OFFSET(AXVoice, 0x28, syncBits);
CHECK_OFFSET(AXVoice, 0x34, offsets);
CHECK_OFFSET(AXVoice, 0x48, callbackEx);
CHECK_OFFSET(AXVoice, 0x4c, callbackReason);
CHECK_OFFSET(AXVoice, 0x50, unk0);
CHECK_OFFSET(AXVoice, 0x54, unk1);
CHECK_SIZE(AXVoice, 0x58);

struct AXVoiceDeviceBusMixData
{
   uint16_t volume;
   int16_t delta;
};
CHECK_OFFSET(AXVoiceDeviceBusMixData, 0x0, volume);
CHECK_OFFSET(AXVoiceDeviceBusMixData, 0x2, delta);
CHECK_SIZE(AXVoiceDeviceBusMixData, 0x4);

struct AXVoiceDeviceMixData
{
   AXVoiceDeviceBusMixData bus[4];
};
CHECK_OFFSET(AXVoiceDeviceMixData, 0x0, bus);
CHECK_SIZE(AXVoiceDeviceMixData, 0x10);

struct AXVoiceVeData
{
   uint16_t volume;
   int16_t delta;
};
CHECK_OFFSET(AXVoiceVeData, 0x0, volume);
CHECK_OFFSET(AXVoiceVeData, 0x2, delta);
CHECK_SIZE(AXVoiceVeData, 0x4);

struct AXVoiceAdpcmLoopData
{
   uint16_t predScale;
   int16_t prevSample[2];
};
CHECK_OFFSET(AXVoiceAdpcmLoopData, 0x0, predScale);
CHECK_OFFSET(AXVoiceAdpcmLoopData, 0x2, prevSample);
CHECK_SIZE(AXVoiceAdpcmLoopData, 0x6);

struct AXVoiceAdpcm
{
   int16_t coefficients[16];
   uint16_t gain;
   uint16_t predScale;
   int16_t prevSample[2];
};
CHECK_OFFSET(AXVoiceAdpcm, 0x0, coefficients);
CHECK_OFFSET(AXVoiceAdpcm, 0x20, gain);
CHECK_OFFSET(AXVoiceAdpcm, 0x22, predScale);
CHECK_OFFSET(AXVoiceAdpcm, 0x24, prevSample);
CHECK_SIZE(AXVoiceAdpcm, 0x28);

#pragma pack(push, 1)

/**
 * AXVoice Sample Rate Converter
 */
struct AXVoiceSrc
{
   //! Playback rate, fixed 16.16
   uint32_t ratio;

   //! Used by the resampler, fixed 0.16
   uint16_t currentOffsetFrac;
   int16_t lastSample[4];
};
CHECK_OFFSET(AXVoiceSrc, 0x0, ratio);
CHECK_OFFSET(AXVoiceSrc, 0x4, currentOffsetFrac);
CHECK_OFFSET(AXVoiceSrc, 0x6, lastSample);
CHECK_SIZE(AXVoiceSrc, 0xe);

#pragma pack(pop)

AXVoice *
AXAcquireVoice(uint32_t priority,
               AXVoiceCallbackFn callback,
               void *userContext);

AXVoice *
AXAcquireVoiceEx(uint32_t priority,
                 AXVoiceCallbackExFn callback,
                 void *userContext);

BOOL
AXCheckVoiceOffsets(AXVoiceOffsets *offsets);

void
AXFreeVoice(AXVoice *voice);

uint32_t
AXGetMaxVoices();

uint32_t
AXGetVoiceCurrentOffsetEx(AXVoice *voice,
                          const void *samples);

uint32_t
AXGetVoiceLoopCount(AXVoice *voice);

void
AXGetVoiceOffsets(AXVoice *voice,
                  AXVoiceOffsets *offsets);

BOOL
AXIsVoiceRunning(AXVoice *voice);

void
AXSetVoiceAdpcm(AXVoice *voice,
                AXVoiceAdpcm *adpcm);

void
AXSetVoiceAdpcmLoop(AXVoice *voice,
                    AXVoiceAdpcmLoopData *loopData);

void
AXSetVoiceCurrentOffset(AXVoice *voice,
                        uint32_t offset);

AXResult
AXSetVoiceDeviceMix(AXVoice *voice,
                    AXDeviceType type,
                    uint32_t id,
                    AXVoiceDeviceMixData *mixData);

void
AXSetVoiceEndOffset(AXVoice *voice,
                    uint32_t offset);

void
AXSetVoiceEndOffsetEx(AXVoice *voice,
                      uint32_t offset,
                      const void *samples);

AXResult
AXSetVoiceInitialTimeDelay(AXVoice *voice,
                           uint16_t delay);

void
AXSetVoiceLoopOffset(AXVoice *voice,
                     uint32_t offset);

void
AXSetVoiceLoopOffsetEx(AXVoice *voice,
                       uint32_t offset,
                       const void *samples);

void
AXSetVoiceLoop(AXVoice *voice,
               AXVoiceLoop loop);

void
AXSetVoiceOffsets(AXVoice *voice,
                  AXVoiceOffsets *offsets);

void
AXSetVoicePriority(AXVoice *voice,
                   uint32_t priority);

void
AXSetVoiceRmtIIRCoefs(AXVoice *voice,
                      uint16_t filter,
                      ...);

void
AXSetVoiceSrc(AXVoice *voice,
              AXVoiceSrc *src);

AXVoiceSrcRatioResult
AXSetVoiceSrcRatio(AXVoice *voice,
                   float ratio);

void
AXSetVoiceSrcType(AXVoice *voice,
                  AXVoiceSrcType type);

void
AXSetVoiceState(AXVoice *voice,
                AXVoiceState state);

void
AXSetVoiceType(AXVoice *voice,
               AXVoiceType type);

void
AXSetVoiceVe(AXVoice *voice,
             AXVoiceVeData *veData);

void
AXSetVoiceVeDelta(AXVoice *voice,
                  int16_t delta);

#ifdef __cplusplus
}
#endif

/** @} */
