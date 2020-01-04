/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018 - Krzysztof Haładyn
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

 /**************************************************************************
  *
  * Copyright (c) Microsoft Corporation.  All rights reserved.
  *
  * File:    xaudio2.h
  * Content: Declarations for the XAudio2 game audio API.
  *
  **************************************************************************/

  /* Modified slightly to build without requiring the WinRT compiler since that is only available in C++ sources */

#ifdef _MSC_VER
#pragma once
#endif

#ifndef __XAUDIO2_INCLUDED__
#define __XAUDIO2_INCLUDED__

#include <sdkddkver.h>

#include <retro_inline.h>

#if(_WIN32_WINNT < _WIN32_WINNT_WIN8)
#error "This version of XAudio2 is available only in Windows 8 or later. Use the XAudio2 headers and libraries from the DirectX SDK with applications that target Windows 7 and earlier versions."
#endif /* (_WIN32_WINNT < _WIN32_WINNT_WIN8) */

#include <winapifamily.h>

#pragma region Application Family
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_TV_APP | WINAPI_PARTITION_TV_TITLE)

/* Current name of the DLL shipped in the same SDK as this header.
 * The name reflects the current version */
#if(_WIN32_WINNT >= _WIN32_WINNT_WIN10)
#define XAUDIO2_DLL_A  "xaudio2_9.dll"
#define XAUDIO2_DLL_W L"xaudio2_9.dll"
#define XAUDIO2D_DLL_A  "xaudio2_9d.dll"
#define XAUDIO2D_DLL_W L"xaudio2_9d.dll"
#else
#define XAUDIO2_DLL_A  "xaudio2_8.dll"
#define XAUDIO2_DLL_W L"xaudio2_8.dll"
#define XAUDIO2D_DLL_A  "xaudio2_8.dll"
#define XAUDIO2D_DLL_W L"xaudio2_8.dll"
#endif

#ifdef UNICODE
#define XAUDIO2_DLL XAUDIO2_DLL_W
#define XAUDIO2D_DLL XAUDIO2D_DLL_W
#else
#define XAUDIO2_DLL XAUDIO2_DLL_A
#define XAUDIO2D_DLL XAUDIO2D_DLL_A
#endif

/**************************************************************************
 *
 * XAudio2 COM object class and interface IDs.
 *
 **************************************************************************/

#include <basetyps.h>

#if defined(__cplusplus__) && defined(__WINRT__)

#if(_WIN32_WINNT >= _WIN32_WINNT_WIN10)
 /* XAudio 2.9 */
interface __declspec(uuid("2B02E3CF-2E0B-4ec3-BE45-1B2A3FE7210D")) IXAudio2;
#else
 /* XAudio 2.8 */
interface __declspec(uuid("60d8dac8-5aa1-4e8e-b597-2f5e2883d484")) IXAudio2;
#endif

#else

 /* Modified for C support */
#define DEFINE_GUID_X(n, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
   static const GUID n = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }
#define DEFINE_CLSID_X(className, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
   DEFINE_GUID_X(CLSID_##className, 0x##l, 0x##w1, 0x##w2, 0x##b1, 0x##b2, 0x##b3, 0x##b4, 0x##b5, 0x##b6, 0x##b7, 0x##b8)
#define DEFINE_IID_X(interfaceName, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
   DEFINE_GUID_X(IID_##interfaceName, 0x##l, 0x##w1, 0x##w2, 0x##b1, 0x##b2, 0x##b3, 0x##b4, 0x##b5, 0x##b6, 0x##b7, 0x##b8)

#if(_WIN32_WINNT >= _WIN32_WINNT_WIN10)
DEFINE_IID_X(IXAudio2, 2B02E3CF, 2E0B, 4ec3, BE, 45, 1B, 2A, 3F, E7, 21, 0D);
#else
DEFINE_IID_X(IXAudio2, 60d8dac8, 5aa1, 4e8e, b5, 97, 2f, 5e, 28, 83, d4, 84);
#endif

#endif

/* Ignore the rest of this header if only the GUID definitions were requested */
#ifndef GUID_DEFS_ONLY

#include <objbase.h>           /* Windows COM declarations */
#include <sal.h>               /* Markers for documenting API semantics */
#include <mmreg.h>             /* Basic data types and constants for audio work */
#include <audiosessiontypes.h> /* For AUDIO_STREAM_CATEGORY */

/* All structures defined in this file use tight field packing */
#pragma pack(push, 1)

/**************************************************************************
 *
 * XAudio2 constants, flags and error codes.
 *
 **************************************************************************/

 /* Numeric boundary values */
#define XAUDIO2_MAX_BUFFER_BYTES        0x80000000    /* Maximum bytes allowed in a source buffer */
#define XAUDIO2_MAX_QUEUED_BUFFERS      64            /* Maximum buffers allowed in a voice queue */
#define XAUDIO2_MAX_BUFFERS_SYSTEM      2             /* Maximum buffers allowed for system threads (Xbox 360 only) */
#define XAUDIO2_MAX_AUDIO_CHANNELS      64            /* Maximum channels in an audio stream */
#define XAUDIO2_MIN_SAMPLE_RATE         1000          /* Minimum audio sample rate supported */
#define XAUDIO2_MAX_SAMPLE_RATE         200000        /* Maximum audio sample rate supported */
#define XAUDIO2_MAX_VOLUME_LEVEL        16777216.0f   /* Maximum acceptable volume level (2^24) */
#define XAUDIO2_MIN_FREQ_RATIO          (1/1024.0f)   /* Minimum SetFrequencyRatio argument */
#define XAUDIO2_MAX_FREQ_RATIO          1024.0f       /* Maximum MaxFrequencyRatio argument */
#define XAUDIO2_DEFAULT_FREQ_RATIO      2.0f          /* Default MaxFrequencyRatio argument */
#define XAUDIO2_MAX_FILTER_ONEOVERQ     1.5f          /* Maximum XAUDIO2_FILTER_PARAMETERS.OneOverQ */
#define XAUDIO2_MAX_FILTER_FREQUENCY    1.0f          /* Maximum XAUDIO2_FILTER_PARAMETERS.Frequency */
#define XAUDIO2_MAX_LOOP_COUNT          254           /* Maximum non-infinite XAUDIO2_BUFFER.LoopCount */
#define XAUDIO2_MAX_INSTANCES           8             /* Maximum simultaneous XAudio2 objects on Xbox 360 */

/* For XMA voices on Xbox 360 there is an additional restriction on the MaxFrequencyRatio
 * argument and the voice's sample rate: the product of these numbers cannot exceed 600000
 * for one-channel voices or 300000 for voices with more than one channel.
 */
#define XAUDIO2_MAX_RATIO_TIMES_RATE_XMA_MONO         600000
#define XAUDIO2_MAX_RATIO_TIMES_RATE_XMA_MULTICHANNEL 300000

/* Numeric values with special meanings */
#define XAUDIO2_COMMIT_NOW              0             /* Used as an OperationSet argument */
#define XAUDIO2_COMMIT_ALL              0             /* Used in IXAudio2::CommitChanges */
#define XAUDIO2_INVALID_OPSET           (UINT32)(-1)  /* Not allowed for OperationSet arguments */
#define XAUDIO2_NO_LOOP_REGION          0             /* Used in XAUDIO2_BUFFER.LoopCount */
#define XAUDIO2_LOOP_INFINITE           255           /* Used in XAUDIO2_BUFFER.LoopCount */
#define XAUDIO2_DEFAULT_CHANNELS        0             /* Used in CreateMasteringVoice */
#define XAUDIO2_DEFAULT_SAMPLERATE      0             /* Used in CreateMasteringVoice */

/* Flags */
#define XAUDIO2_DEBUG_ENGINE                  0x0001    /* Used in XAudio2Create */
#define XAUDIO2_VOICE_NOPITCH                 0x0002    /* Used in IXAudio2::CreateSourceVoice */
#define XAUDIO2_VOICE_NOSRC                   0x0004    /* Used in IXAudio2::CreateSourceVoice */
#define XAUDIO2_VOICE_USEFILTER               0x0008    /* Used in IXAudio2::CreateSource/SubmixVoice */
#define XAUDIO2_PLAY_TAILS                    0x0020    /* Used in IXAudio2SourceVoice::Stop */
#define XAUDIO2_END_OF_STREAM                 0x0040    /* Used in XAUDIO2_BUFFER.Flags */
#define XAUDIO2_SEND_USEFILTER                0x0080    /* Used in XAUDIO2_SEND_DESCRIPTOR.Flags */
#define XAUDIO2_VOICE_NOSAMPLESPLAYED         0x0100    /* Used in IXAudio2SourceVoice::GetState */
#define XAUDIO2_STOP_ENGINE_WHEN_IDLE         0x2000    /* Used in XAudio2Create to force the engine to Stop when no source voices are Started, and Start when a voice is Started */
#define XAUDIO2_1024_QUANTUM                  0x8000    /* Used in XAudio2Create to specify nondefault processing quantum of 21.33 ms (1024 samples at 48KHz) */
#define XAUDIO2_NO_VIRTUAL_AUDIO_CLIENT          0x10000   /* Used in CreateMasteringVoice to create a virtual audio client */

/* Default parameters for the built-in filter */
#define XAUDIO2_DEFAULT_FILTER_TYPE     LowPassFilter
#define XAUDIO2_DEFAULT_FILTER_FREQUENCY XAUDIO2_MAX_FILTER_FREQUENCY
#define XAUDIO2_DEFAULT_FILTER_ONEOVERQ 1.0f

/* Internal XAudio2 constants */
/* The audio frame quantum can be calculated by reducing the fraction:
 *     SamplesPerAudioFrame / SamplesPerSecond */
#define XAUDIO2_QUANTUM_NUMERATOR   1                 /* On Windows, XAudio2 processes audio */
#define XAUDIO2_QUANTUM_DENOMINATOR 100               /*  in 10ms chunks (= 1/100 seconds) */
#define XAUDIO2_QUANTUM_MS (1000.0f * XAUDIO2_QUANTUM_NUMERATOR / XAUDIO2_QUANTUM_DENOMINATOR)

/* XAudio2 error codes */
#define FACILITY_XAUDIO2 0x896
#define XAUDIO2_E_INVALID_CALL                       0x88960001    /* An API call or one of its arguments was illegal */
#define XAUDIO2_E_XMA_DECODER_ERROR                  0x88960002    /* The XMA hardware suffered an unrecoverable error */
#define XAUDIO2_E_XAPO_CREATION_FAILED               0x88960003    /* XAudio2 failed to initialize an XAPO effect */
#define XAUDIO2_E_DEVICE_INVALIDATED                 0x88960004    /* An audio device became unusable (unplugged, etc) */

/**************************************************************************
 *
 * Forward declarations for the XAudio2 interfaces.
 *
 **************************************************************************/

#ifdef __cplusplus
#define FWD_DECLARE(x) interface x
#else
#define FWD_DECLARE(x) typedef interface x x
#endif

FWD_DECLARE(IXAudio2);
FWD_DECLARE(IXAudio2Voice);
FWD_DECLARE(IXAudio2SourceVoice);
FWD_DECLARE(IXAudio2SubmixVoice);
FWD_DECLARE(IXAudio2MasteringVoice);
FWD_DECLARE(IXAudio2EngineCallback);
FWD_DECLARE(IXAudio2VoiceCallback);

/**************************************************************************
 *
 * XAudio2 structures and enumerations.
 *
 **************************************************************************/

 /* Used in XAudio2Create, specifies which CPU(s) to use. */
typedef UINT32 XAUDIO2_PROCESSOR;
#define Processor1  0x00000001
#define Processor2  0x00000002
#define Processor3  0x00000004
#define Processor4  0x00000008
#define Processor5  0x00000010
#define Processor6  0x00000020
#define Processor7  0x00000040
#define Processor8  0x00000080
#define Processor9  0x00000100
#define Processor10 0x00000200
#define Processor11 0x00000400
#define Processor12 0x00000800
#define Processor13 0x00001000
#define Processor14 0x00002000
#define Processor15 0x00004000
#define Processor16 0x00008000
#define Processor17 0x00010000
#define Processor18 0x00020000
#define Processor19 0x00040000
#define Processor20 0x00080000
#define Processor21 0x00100000
#define Processor22 0x00200000
#define Processor23 0x00400000
#define Processor24 0x00800000
#define Processor25 0x01000000
#define Processor26 0x02000000
#define Processor27 0x04000000
#define Processor28 0x08000000
#define Processor29 0x10000000
#define Processor30 0x20000000
#define Processor31 0x40000000
#define Processor32 0x80000000
#define XAUDIO2_ANY_PROCESSOR 0xffffffff
#define XAUDIO2_DEFAULT_PROCESSOR Processor1

/* Returned by IXAudio2Voice::GetVoiceDetails */
typedef struct XAUDIO2_VOICE_DETAILS
{
   UINT32 CreationFlags;               /* Flags the voice was created with. */
   UINT32 ActiveFlags;                 /* Flags currently active. */
   UINT32 InputChannels;               /* Channels in the voice's input audio. */
   UINT32 InputSampleRate;             /* Sample rate of the voice's input audio. */
} XAUDIO2_VOICE_DETAILS;

/* Used in XAUDIO2_VOICE_SENDS below */
typedef struct XAUDIO2_SEND_DESCRIPTOR
{
   UINT32 Flags;                       /* Either 0 or XAUDIO2_SEND_USEFILTER. */
   IXAudio2Voice* pOutputVoice;        /* This send's destination voice. */
} XAUDIO2_SEND_DESCRIPTOR;

/* Used in the voice creation functions and in IXAudio2Voice::SetOutputVoices */
typedef struct XAUDIO2_VOICE_SENDS
{
   UINT32 SendCount;                   /* Number of sends from this voice. */
   XAUDIO2_SEND_DESCRIPTOR* pSends;    /* Array of SendCount send descriptors. */
} XAUDIO2_VOICE_SENDS;

/* Used in XAUDIO2_EFFECT_CHAIN below */
typedef struct XAUDIO2_EFFECT_DESCRIPTOR
{
   IUnknown* pEffect;                  /* Pointer to the effect object's IUnknown interface. */
   BOOL InitialState;                  /* TRUE if the effect should begin in the enabled state. */
   UINT32 OutputChannels;              /* How many output channels the effect should produce. */
} XAUDIO2_EFFECT_DESCRIPTOR;

/* Used in the voice creation functions and in IXAudio2Voice::SetEffectChain */
typedef struct XAUDIO2_EFFECT_CHAIN
{
   UINT32 EffectCount;                 /* Number of effects in this voice's effect chain. */
   XAUDIO2_EFFECT_DESCRIPTOR* pEffectDescriptors; /* Array of effect descriptors. */
} XAUDIO2_EFFECT_CHAIN;

/* Used in XAUDIO2_FILTER_PARAMETERS below */
typedef enum XAUDIO2_FILTER_TYPE
{
   LowPassFilter,                      /* Attenuates frequencies above the cutoff frequency (state-variable filter). */
   BandPassFilter,                     /* Attenuates frequencies outside a given range      (state-variable filter). */
   HighPassFilter,                     /* Attenuates frequencies below the cutoff frequency (state-variable filter). */
   NotchFilter,                        /* Attenuates frequencies inside a given range       (state-variable filter). */
   LowPassOnePoleFilter,               /* Attenuates frequencies above the cutoff frequency (one-pole filter, XAUDIO2_FILTER_PARAMETERS.OneOverQ has no effect) */
   HighPassOnePoleFilter               /* Attenuates frequencies below the cutoff frequency (one-pole filter, XAUDIO2_FILTER_PARAMETERS.OneOverQ has no effect) */
} XAUDIO2_FILTER_TYPE;

/* Used in IXAudio2Voice::Set/GetFilterParameters and Set/GetOutputFilterParameters */
typedef struct XAUDIO2_FILTER_PARAMETERS
{
   XAUDIO2_FILTER_TYPE Type;           /* Filter type. */
   float Frequency;                    /* Filter coefficient. */
                                       /*  must be >= 0 and <= XAUDIO2_MAX_FILTER_FREQUENCY */
                                       /*  See XAudio2CutoffFrequencyToRadians() for state-variable filter types and */
                                       /*  XAudio2CutoffFrequencyToOnePoleCoefficient() for one-pole filter types. */
   float OneOverQ;                     /* Reciprocal of the filter's quality factor Q; */
                                       /*  must be > 0 and <= XAUDIO2_MAX_FILTER_ONEOVERQ. */
                                       /*  Has no effect for one-pole filters. */
} XAUDIO2_FILTER_PARAMETERS;

/* Used in IXAudio2SourceVoice::SubmitSourceBuffer */
typedef struct XAUDIO2_BUFFER
{
   UINT32 Flags;                       /* Either 0 or XAUDIO2_END_OF_STREAM. */
   UINT32 AudioBytes;                  /* Size of the audio data buffer in bytes. */
   const BYTE* pAudioData;             /* Pointer to the audio data buffer. */
   UINT32 PlayBegin;                   /* First sample in this buffer to be played. */
   UINT32 PlayLength;                  /* Length of the region to be played in samples, */
                                       /*  or 0 to play the whole buffer. */
   UINT32 LoopBegin;                   /* First sample of the region to be looped. */
   UINT32 LoopLength;                  /* Length of the desired loop region in samples, */
                                       /*  or 0 to loop the entire buffer. */
   UINT32 LoopCount;                   /* Number of times to repeat the loop region, */
                                       /*  or XAUDIO2_LOOP_INFINITE to loop forever. */
   void* pContext;                     /* Context value to be passed back in callbacks. */
} XAUDIO2_BUFFER;

/* Used in IXAudio2SourceVoice::SubmitSourceBuffer when submitting XWMA data.
 * NOTE: If an XWMA sound is submitted in more than one buffer, each buffer's
 * pDecodedPacketCumulativeBytes[PacketCount-1] value must be subtracted from
 * all the entries in the next buffer's pDecodedPacketCumulativeBytes array.
 * And whether a sound is submitted in more than one buffer or not, the final
 * buffer of the sound should use the XAUDIO2_END_OF_STREAM flag, or else the
 * client must call IXAudio2SourceVoice::Discontinuity after submitting it.
 */
typedef struct XAUDIO2_BUFFER_WMA
{
   const UINT32* pDecodedPacketCumulativeBytes; /* Decoded packet's cumulative size array.
                                                 *  Each element is the number of bytes accumulated
                                                 *  when the corresponding XWMA packet is decoded in
                                                 *  order.  The array must have PacketCount elements. */
   UINT32 PacketCount;                          /* Number of XWMA packets submitted. Must be >= 1 and */
                                                /*  divide evenly into XAUDIO2_BUFFER.AudioBytes. */
} XAUDIO2_BUFFER_WMA;

/* Returned by IXAudio2SourceVoice::GetState */
typedef struct XAUDIO2_VOICE_STATE
{
   void* pCurrentBufferContext;        /* The pContext value provided in the XAUDIO2_BUFFER */
                                       /*  that is currently being processed, or NULL if */
                                       /*  there are no buffers in the queue. */
   UINT32 BuffersQueued;               /* Number of buffers currently queued on the voice */
                                       /*  (including the one that is being processed). */
   UINT64 SamplesPlayed;               /* Total number of samples produced by the voice since 
                                        *  it began processing the current audio stream.
                                        *  If XAUDIO2_VOICE_NOSAMPLESPLAYED is specified
                                        *  in the call to IXAudio2SourceVoice::GetState,
                                        *  this member will not be calculated, saving CPU. */
} XAUDIO2_VOICE_STATE;

/* Returned by IXAudio2::GetPerformanceData */
typedef struct XAUDIO2_PERFORMANCE_DATA
{
   /* CPU usage information */
   UINT64 AudioCyclesSinceLastQuery;   /* CPU cycles spent on audio processing since the
                                        *  last call to StartEngine or GetPerformanceData. */
   UINT64 TotalCyclesSinceLastQuery;   /* Total CPU cycles elapsed since the last call
                                        *  (only counts the CPU XAudio2 is running on). */
   UINT32 MinimumCyclesPerQuantum;     /* Fewest CPU cycles spent processing any one
                                        *  audio quantum since the last call. */
   UINT32 MaximumCyclesPerQuantum;     /* Most CPU cycles spent processing any one
                                        *  audio quantum since the last call. */

   /* Memory usage information */
   UINT32 MemoryUsageInBytes;          /* Total heap space currently in use. */

   /* Audio latency and glitching information */
   UINT32 CurrentLatencyInSamples;     /* Minimum delay from when a sample is read from a */
                                       /*  source buffer to when it reaches the speakers. */
   UINT32 GlitchesSinceEngineStarted;  /* Audio dropouts since the engine was started. */

   /* Data about XAudio2's current workload */
   UINT32 ActiveSourceVoiceCount;      /* Source voices currently playing. */
   UINT32 TotalSourceVoiceCount;       /* Source voices currently existing. */
   UINT32 ActiveSubmixVoiceCount;      /* Submix voices currently playing/existing. */

   UINT32 ActiveResamplerCount;        /* Resample xAPOs currently active. */
   UINT32 ActiveMatrixMixCount;        /* MatrixMix xAPOs currently active. */

   /* Usage of the hardware XMA decoder (Xbox 360 only) */
   UINT32 ActiveXmaSourceVoices;       /* Number of source voices decoding XMA data. */
   UINT32 ActiveXmaStreams;            /* A voice can use more than one XMA stream. */
} XAUDIO2_PERFORMANCE_DATA;

/* Used in IXAudio2::SetDebugConfiguration */
typedef struct XAUDIO2_DEBUG_CONFIGURATION
{
   UINT32 TraceMask;                   /* Bitmap of enabled debug message types. */
   UINT32 BreakMask;                   /* Message types that will break into the debugger. */
   BOOL LogThreadID;                   /* Whether to log the thread ID with each message. */
   BOOL LogFileline;                   /* Whether to log the source file and line number. */
   BOOL LogFunctionName;               /* Whether to log the function name. */
   BOOL LogTiming;                     /* Whether to log message timestamps. */
} XAUDIO2_DEBUG_CONFIGURATION;

/* Values for the TraceMask and BreakMask bitmaps.  Only ERRORS and WARNINGS
 * are valid in BreakMask.  WARNINGS implies ERRORS, DETAIL implies INFO, and
 * FUNC_CALLS implies API_CALLS.  By default, TraceMask is ERRORS and WARNINGS
 * and all the other settings are zero. */
#define XAUDIO2_LOG_ERRORS     0x0001   /* For handled errors with serious effects. */
#define XAUDIO2_LOG_WARNINGS   0x0002   /* For handled errors that may be recoverable. */
#define XAUDIO2_LOG_INFO       0x0004   /* Informational chit-chat (e.g. state changes). */
#define XAUDIO2_LOG_DETAIL     0x0008   /* More detailed chit-chat. */
#define XAUDIO2_LOG_API_CALLS  0x0010   /* Public API function entries and exits. */
#define XAUDIO2_LOG_FUNC_CALLS 0x0020   /* Internal function entries and exits. */
#define XAUDIO2_LOG_TIMING     0x0040   /* Delays detected and other timing data. */
#define XAUDIO2_LOG_LOCKS      0x0080   /* Usage of critical sections and mutexes. */
#define XAUDIO2_LOG_MEMORY     0x0100   /* Memory heap usage information. */
#define XAUDIO2_LOG_STREAMING  0x1000   /* Audio streaming information. */

/**************************************************************************
 *
 * IXAudio2: Top-level XAudio2 COM interface.
 *
 **************************************************************************/

 /* Use default arguments if compiling as C++ */
#ifdef __cplusplus
#define X2DEFAULT(x) =x
#else
#define X2DEFAULT(x)
#endif

#undef INTERFACE
#define INTERFACE IXAudio2
DECLARE_INTERFACE_(IXAudio2, IUnknown)
{
   STDMETHOD(QueryInterface) (THIS_ REFIID riid, _Outptr_ void** ppvInterface) PURE;
   STDMETHOD_(ULONG, AddRef) (THIS) PURE;
   STDMETHOD_(ULONG, Release) (THIS) PURE;
   STDMETHOD(RegisterForCallbacks) (_In_ IXAudio2EngineCallback* pCallback) PURE;
   STDMETHOD_(void, UnregisterForCallbacks) (_In_ IXAudio2EngineCallback* pCallback) PURE;
   STDMETHOD(CreateSourceVoice) (THIS_ _Outptr_ IXAudio2SourceVoice** ppSourceVoice,
      _In_ const WAVEFORMATEX* pSourceFormat,
      UINT32 Flags X2DEFAULT(0),
      float MaxFrequencyRatio X2DEFAULT(XAUDIO2_DEFAULT_FREQ_RATIO),
      _In_opt_ IXAudio2VoiceCallback* pCallback X2DEFAULT(NULL),
      _In_opt_ const XAUDIO2_VOICE_SENDS* pSendList X2DEFAULT(NULL),
      _In_opt_ const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) PURE;
   STDMETHOD(CreateSubmixVoice) (THIS_ _Outptr_ IXAudio2SubmixVoice** ppSubmixVoice,
      UINT32 InputChannels, UINT32 InputSampleRate,
      UINT32 Flags X2DEFAULT(0), UINT32 ProcessingStage X2DEFAULT(0),
      _In_opt_ const XAUDIO2_VOICE_SENDS* pSendList X2DEFAULT(NULL),
      _In_opt_ const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) PURE;
   STDMETHOD(CreateMasteringVoice) (THIS_ _Outptr_ IXAudio2MasteringVoice** ppMasteringVoice,
      UINT32 InputChannels X2DEFAULT(XAUDIO2_DEFAULT_CHANNELS),
      UINT32 InputSampleRate X2DEFAULT(XAUDIO2_DEFAULT_SAMPLERATE),
      UINT32 Flags X2DEFAULT(0), _In_opt_z_ LPCWSTR szDeviceId X2DEFAULT(NULL),
      _In_opt_ const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL),
      _In_ AUDIO_STREAM_CATEGORY StreamCategory X2DEFAULT(AudioCategory_GameEffects)) PURE;
   STDMETHOD(StartEngine) (THIS) PURE;
   STDMETHOD_(void, StopEngine) (THIS) PURE;
   STDMETHOD(CommitChanges) (THIS_ UINT32 OperationSet) PURE;
   STDMETHOD_(void, GetPerformanceData) (THIS_ _Out_ XAUDIO2_PERFORMANCE_DATA* pPerfData) PURE;
   STDMETHOD_(void, SetDebugConfiguration) (THIS_ _In_opt_ const XAUDIO2_DEBUG_CONFIGURATION* pDebugConfiguration,
      _Reserved_ void* pReserved X2DEFAULT(NULL)) PURE;
};

/**************************************************************************
 *
 * IXAudio2Voice: Base voice management interface.
 *
 **************************************************************************/

#undef INTERFACE
#define INTERFACE IXAudio2Voice

DECLARE_INTERFACE(IXAudio2Voice)
{
#define Declare_IXAudio2Voice_Methods() \
   STDMETHOD_(void, GetVoiceDetails) (THIS_ _Out_ XAUDIO2_VOICE_DETAILS* pVoiceDetails) PURE; \
   STDMETHOD(SetOutputVoices) (THIS_ _In_opt_ const XAUDIO2_VOICE_SENDS* pSendList) PURE; \
   STDMETHOD(SetEffectChain) (THIS_ _In_opt_ const XAUDIO2_EFFECT_CHAIN* pEffectChain) PURE; \
   STDMETHOD(EnableEffect) (THIS_ UINT32 EffectIndex, \
      UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD(DisableEffect) (THIS_ UINT32 EffectIndex, \
      UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD_(void, GetEffectState) (THIS_ UINT32 EffectIndex, _Out_ BOOL* pEnabled) PURE; \
   STDMETHOD(SetEffectParameters) (THIS_ UINT32 EffectIndex, \
      _In_reads_bytes_(ParametersByteSize) const void* pParameters, \
      UINT32 ParametersByteSize, \
      UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD(GetEffectParameters) (THIS_ UINT32 EffectIndex, \
      _Out_writes_bytes_(ParametersByteSize) void* pParameters, \
      UINT32 ParametersByteSize) PURE; \
   STDMETHOD(SetFilterParameters) (THIS_ _In_ const XAUDIO2_FILTER_PARAMETERS* pParameters, \
      UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD_(void, GetFilterParameters) (THIS_ _Out_ XAUDIO2_FILTER_PARAMETERS* pParameters) PURE; \
   STDMETHOD(SetOutputFilterParameters) (THIS_ _In_opt_ IXAudio2Voice* pDestinationVoice, \
      _In_ const XAUDIO2_FILTER_PARAMETERS* pParameters, \
      UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD_(void, GetOutputFilterParameters) (THIS_ _In_opt_ IXAudio2Voice* pDestinationVoice, \
      _Out_ XAUDIO2_FILTER_PARAMETERS* pParameters) PURE; \
   STDMETHOD(SetVolume) (THIS_ float Volume, \
      UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD_(void, GetVolume) (THIS_ _Out_ float* pVolume) PURE; \
   STDMETHOD(SetChannelVolumes) (THIS_ UINT32 Channels, _In_reads_(Channels) const float* pVolumes, \
      UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD_(void, GetChannelVolumes) (THIS_ UINT32 Channels, _Out_writes_(Channels) float* pVolumes) PURE; \
   STDMETHOD(SetOutputMatrix) (THIS_ _In_opt_ IXAudio2Voice* pDestinationVoice, \
      UINT32 SourceChannels, UINT32 DestinationChannels, \
      _In_reads_(SourceChannels * DestinationChannels) const float* pLevelMatrix, \
      UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD_(void, GetOutputMatrix) (THIS_ _In_opt_ IXAudio2Voice* pDestinationVoice, \
      UINT32 SourceChannels, UINT32 DestinationChannels, \
      _Out_writes_(SourceChannels * DestinationChannels) float* pLevelMatrix) PURE; \
   STDMETHOD_(void, DestroyVoice) (THIS) PURE

   Declare_IXAudio2Voice_Methods();
};

/**************************************************************************
 *
 * IXAudio2SourceVoice: Source voice management interface.
 *
 **************************************************************************/

#undef INTERFACE
#define INTERFACE IXAudio2SourceVoice
DECLARE_INTERFACE_(IXAudio2SourceVoice, IXAudio2Voice)
{
   Declare_IXAudio2Voice_Methods();
   STDMETHOD(Start) (THIS_ UINT32 Flags X2DEFAULT(0), UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
   STDMETHOD(Stop) (THIS_ UINT32 Flags X2DEFAULT(0), UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
   STDMETHOD(SubmitSourceBuffer) (THIS_ _In_ const XAUDIO2_BUFFER* pBuffer, _In_opt_ const XAUDIO2_BUFFER_WMA* pBufferWMA X2DEFAULT(NULL)) PURE;
   STDMETHOD(FlushSourceBuffers) (THIS) PURE;
   STDMETHOD(Discontinuity) (THIS) PURE;
   STDMETHOD(ExitLoop) (THIS_ UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
   STDMETHOD_(void, GetState) (THIS_ _Out_ XAUDIO2_VOICE_STATE* pVoiceState, UINT32 Flags X2DEFAULT(0)) PURE;
   STDMETHOD(SetFrequencyRatio) (THIS_ float Ratio,
      UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
   STDMETHOD_(void, GetFrequencyRatio) (THIS_ _Out_ float* pRatio) PURE;
   STDMETHOD(SetSourceSampleRate) (THIS_ UINT32 NewSourceSampleRate) PURE;
};

/**************************************************************************
 *
 * IXAudio2SubmixVoice: Submixing voice management interface.
 *
 **************************************************************************/

#undef INTERFACE
#define INTERFACE IXAudio2SubmixVoice
DECLARE_INTERFACE_(IXAudio2SubmixVoice, IXAudio2Voice)
{
   Declare_IXAudio2Voice_Methods();
};

/**************************************************************************
 *
 * IXAudio2MasteringVoice: Mastering voice management interface.
 *
 **************************************************************************/

#undef INTERFACE
#define INTERFACE IXAudio2MasteringVoice
DECLARE_INTERFACE_(IXAudio2MasteringVoice, IXAudio2Voice)
{
   Declare_IXAudio2Voice_Methods();
   STDMETHOD(GetChannelMask) (THIS_ _Out_ DWORD* pChannelmask) PURE;
};

/**************************************************************************
 *
 * IXAudio2EngineCallback: Client notification interface for engine events.
 *
 * REMARKS: Contains methods to notify the client when certain events happen
 *          in the XAudio2 engine.  This interface should be implemented by
 *          the client.  XAudio2 will call these methods via the interface
 *          pointer provided by the client when it calls
 *          IXAudio2::RegisterForCallbacks.
 *
 **************************************************************************/

#undef INTERFACE
#define INTERFACE IXAudio2EngineCallback
DECLARE_INTERFACE(IXAudio2EngineCallback)
{
   STDMETHOD_(void, OnProcessingPassStart) (THIS) PURE;
   STDMETHOD_(void, OnProcessingPassEnd) (THIS) PURE;
   STDMETHOD_(void, OnCriticalError) (THIS_ HRESULT Error) PURE;
};

/**************************************************************************
 *
 * IXAudio2VoiceCallback: Client notification interface for voice events.
 *
 * REMARKS: Contains methods to notify the client when certain events happen
 *          in an XAudio2 voice.  This interface should be implemented by the
 *          client.  XAudio2 will call these methods via an interface pointer
 *          provided by the client in the IXAudio2::CreateSourceVoice call.
 *
 **************************************************************************/

#undef INTERFACE
#define INTERFACE IXAudio2VoiceCallback
DECLARE_INTERFACE(IXAudio2VoiceCallback)
{
   STDMETHOD_(void, OnVoiceProcessingPassStart) (THIS_ UINT32 BytesRequired) PURE;
   STDMETHOD_(void, OnVoiceProcessingPassEnd) (THIS) PURE;
   STDMETHOD_(void, OnStreamEnd) (THIS) PURE;
   STDMETHOD_(void, OnBufferStart) (THIS_ void* pBufferContext) PURE;
   STDMETHOD_(void, OnBufferEnd) (THIS_ void* pBufferContext) PURE;
   STDMETHOD_(void, OnLoopEnd) (THIS_ void* pBufferContext) PURE;
   STDMETHOD_(void, OnVoiceError) (THIS_ void* pBufferContext, HRESULT Error) PURE;
};

/**************************************************************************
 *
 * Macros to make it easier to use the XAudio2 COM interfaces in C code.
 *
 **************************************************************************/

#ifndef __cplusplus

 /* IXAudio2 */
#define IXAudio2_QueryInterface(This,riid,ppvInterface) ((This)->lpVtbl->QueryInterface(This,riid,ppvInterface))
#define IXAudio2_AddRef(This) ((This)->lpVtbl->AddRef(This))
#define IXAudio2_Release(This) ((This)->lpVtbl->Release(This))
#define IXAudio2_CreateSourceVoice(This,ppSourceVoice,pSourceFormat,Flags,MaxFrequencyRatio,pCallback,pSendList,pEffectChain) ((This)->lpVtbl->CreateSourceVoice(This,ppSourceVoice,pSourceFormat,Flags,MaxFrequencyRatio,pCallback,pSendList,pEffectChain))
#define IXAudio2_CreateSubmixVoice(This,ppSubmixVoice,InputChannels,InputSampleRate,Flags,ProcessingStage,pSendList,pEffectChain) ((This)->lpVtbl->CreateSubmixVoice(This,ppSubmixVoice,InputChannels,InputSampleRate,Flags,ProcessingStage,pSendList,pEffectChain))
#define IXAudio2_CreateMasteringVoice(This,ppMasteringVoice,InputChannels,InputSampleRate,Flags,DeviceId,pEffectChain,StreamCategory) ((This)->lpVtbl->CreateMasteringVoice(This,ppMasteringVoice,InputChannels,InputSampleRate,Flags,DeviceId,pEffectChain,StreamCategory))
#define IXAudio2_StartEngine(This) ((This)->lpVtbl->StartEngine(This))
#define IXAudio2_StopEngine(This) ((This)->lpVtbl->StopEngine(This))
#define IXAudio2_CommitChanges(This,OperationSet) ((This)->lpVtbl->CommitChanges(This,OperationSet))
#define IXAudio2_GetPerformanceData(This,pPerfData) ((This)->lpVtbl->GetPerformanceData(This,pPerfData))
#define IXAudio2_SetDebugConfiguration(This,pDebugConfiguration,pReserved) ((This)->lpVtbl->SetDebugConfiguration(This,pDebugConfiguration,pReserved))

/* IXAudio2Voice */
#define IXAudio2Voice_GetVoiceDetails(This,pVoiceDetails) ((This)->lpVtbl->GetVoiceDetails(This,pVoiceDetails))
#define IXAudio2Voice_SetOutputVoices(This,pSendList) ((This)->lpVtbl->SetOutputVoices(This,pSendList))
#define IXAudio2Voice_SetEffectChain(This,pEffectChain) ((This)->lpVtbl->SetEffectChain(This,pEffectChain))
#define IXAudio2Voice_EnableEffect(This,EffectIndex,OperationSet) ((This)->lpVtbl->EnableEffect(This,EffectIndex,OperationSet))
#define IXAudio2Voice_DisableEffect(This,EffectIndex,OperationSet) ((This)->lpVtbl->DisableEffect(This,EffectIndex,OperationSet))
#define IXAudio2Voice_GetEffectState(This,EffectIndex,pEnabled) ((This)->lpVtbl->GetEffectState(This,EffectIndex,pEnabled))
#define IXAudio2Voice_SetEffectParameters(This,EffectIndex,pParameters,ParametersByteSize, OperationSet) ((This)->lpVtbl->SetEffectParameters(This,EffectIndex,pParameters,ParametersByteSize,OperationSet))
#define IXAudio2Voice_GetEffectParameters(This,EffectIndex,pParameters,ParametersByteSize) ((This)->lpVtbl->GetEffectParameters(This,EffectIndex,pParameters,ParametersByteSize))
#define IXAudio2Voice_SetFilterParameters(This,pParameters,OperationSet) ((This)->lpVtbl->SetFilterParameters(This,pParameters,OperationSet))
#define IXAudio2Voice_GetFilterParameters(This,pParameters) ((This)->lpVtbl->GetFilterParameters(This,pParameters))
#define IXAudio2Voice_SetOutputFilterParameters(This,pDestinationVoice,pParameters,OperationSet) ((This)->lpVtbl->SetOutputFilterParameters(This,pDestinationVoice,pParameters,OperationSet))
#define IXAudio2Voice_GetOutputFilterParameters(This,pDestinationVoice,pParameters) ((This)->lpVtbl->GetOutputFilterParameters(This,pDestinationVoice,pParameters))
#define IXAudio2Voice_SetVolume(This,Volume,OperationSet) ((This)->lpVtbl->SetVolume(This,Volume,OperationSet))
#define IXAudio2Voice_GetVolume(This,pVolume) ((This)->lpVtbl->GetVolume(This,pVolume))
#define IXAudio2Voice_SetChannelVolumes(This,Channels,pVolumes,OperationSet) ((This)->lpVtbl->SetChannelVolumes(This,Channels,pVolumes,OperationSet))
#define IXAudio2Voice_GetChannelVolumes(This,Channels,pVolumes) ((This)->lpVtbl->GetChannelVolumes(This,Channels,pVolumes))
#define IXAudio2Voice_SetOutputMatrix(This,pDestinationVoice,SourceChannels,DestinationChannels,pLevelMatrix,OperationSet) ((This)->lpVtbl->SetOutputMatrix(This,pDestinationVoice,SourceChannels,DestinationChannels,pLevelMatrix,OperationSet))
#define IXAudio2Voice_GetOutputMatrix(This,pDestinationVoice,SourceChannels,DestinationChannels,pLevelMatrix) ((This)->lpVtbl->GetOutputMatrix(This,pDestinationVoice,SourceChannels,DestinationChannels,pLevelMatrix))
#define IXAudio2Voice_DestroyVoice(This) ((This)->lpVtbl->DestroyVoice(This))

/* IXAudio2SourceVoice */
#define IXAudio2SourceVoice_GetVoiceDetails IXAudio2Voice_GetVoiceDetails
#define IXAudio2SourceVoice_SetOutputVoices IXAudio2Voice_SetOutputVoices
#define IXAudio2SourceVoice_SetEffectChain IXAudio2Voice_SetEffectChain
#define IXAudio2SourceVoice_EnableEffect IXAudio2Voice_EnableEffect
#define IXAudio2SourceVoice_DisableEffect IXAudio2Voice_DisableEffect
#define IXAudio2SourceVoice_GetEffectState IXAudio2Voice_GetEffectState
#define IXAudio2SourceVoice_SetEffectParameters IXAudio2Voice_SetEffectParameters
#define IXAudio2SourceVoice_GetEffectParameters IXAudio2Voice_GetEffectParameters
#define IXAudio2SourceVoice_SetFilterParameters IXAudio2Voice_SetFilterParameters
#define IXAudio2SourceVoice_GetFilterParameters IXAudio2Voice_GetFilterParameters
#define IXAudio2SourceVoice_SetOutputFilterParameters IXAudio2Voice_SetOutputFilterParameters
#define IXAudio2SourceVoice_GetOutputFilterParameters IXAudio2Voice_GetOutputFilterParameters
#define IXAudio2SourceVoice_SetVolume IXAudio2Voice_SetVolume
#define IXAudio2SourceVoice_GetVolume IXAudio2Voice_GetVolume
#define IXAudio2SourceVoice_SetChannelVolumes IXAudio2Voice_SetChannelVolumes
#define IXAudio2SourceVoice_GetChannelVolumes IXAudio2Voice_GetChannelVolumes
#define IXAudio2SourceVoice_SetOutputMatrix IXAudio2Voice_SetOutputMatrix
#define IXAudio2SourceVoice_GetOutputMatrix IXAudio2Voice_GetOutputMatrix
#define IXAudio2SourceVoice_DestroyVoice IXAudio2Voice_DestroyVoice
#define IXAudio2SourceVoice_Start(This,Flags,OperationSet) ((This)->lpVtbl->Start(This,Flags,OperationSet))
#define IXAudio2SourceVoice_Stop(This,Flags,OperationSet) ((This)->lpVtbl->Stop(This,Flags,OperationSet))
#define IXAudio2SourceVoice_SubmitSourceBuffer(This,pBuffer,pBufferWMA) ((This)->lpVtbl->SubmitSourceBuffer(This,pBuffer,pBufferWMA))
#define IXAudio2SourceVoice_FlushSourceBuffers(This) ((This)->lpVtbl->FlushSourceBuffers(This))
#define IXAudio2SourceVoice_Discontinuity(This) ((This)->lpVtbl->Discontinuity(This))
#define IXAudio2SourceVoice_ExitLoop(This,OperationSet) ((This)->lpVtbl->ExitLoop(This,OperationSet))
#define IXAudio2SourceVoice_GetState(This,pVoiceState,Flags) ((This)->lpVtbl->GetState(This,pVoiceState,Flags))
#define IXAudio2SourceVoice_SetFrequencyRatio(This,Ratio,OperationSet) ((This)->lpVtbl->SetFrequencyRatio(This,Ratio,OperationSet))
#define IXAudio2SourceVoice_GetFrequencyRatio(This,pRatio) ((This)->lpVtbl->GetFrequencyRatio(This,pRatio))
#define IXAudio2SourceVoice_SetSourceSampleRate(This,NewSourceSampleRate) ((This)->lpVtbl->SetSourceSampleRate(This,NewSourceSampleRate))

/* IXAudio2SubmixVoice */
#define IXAudio2SubmixVoice_GetVoiceDetails IXAudio2Voice_GetVoiceDetails
#define IXAudio2SubmixVoice_SetOutputVoices IXAudio2Voice_SetOutputVoices
#define IXAudio2SubmixVoice_SetEffectChain IXAudio2Voice_SetEffectChain
#define IXAudio2SubmixVoice_EnableEffect IXAudio2Voice_EnableEffect
#define IXAudio2SubmixVoice_DisableEffect IXAudio2Voice_DisableEffect
#define IXAudio2SubmixVoice_GetEffectState IXAudio2Voice_GetEffectState
#define IXAudio2SubmixVoice_SetEffectParameters IXAudio2Voice_SetEffectParameters
#define IXAudio2SubmixVoice_GetEffectParameters IXAudio2Voice_GetEffectParameters
#define IXAudio2SubmixVoice_SetFilterParameters IXAudio2Voice_SetFilterParameters
#define IXAudio2SubmixVoice_GetFilterParameters IXAudio2Voice_GetFilterParameters
#define IXAudio2SubmixVoice_SetOutputFilterParameters IXAudio2Voice_SetOutputFilterParameters
#define IXAudio2SubmixVoice_GetOutputFilterParameters IXAudio2Voice_GetOutputFilterParameters
#define IXAudio2SubmixVoice_SetVolume IXAudio2Voice_SetVolume
#define IXAudio2SubmixVoice_GetVolume IXAudio2Voice_GetVolume
#define IXAudio2SubmixVoice_SetChannelVolumes IXAudio2Voice_SetChannelVolumes
#define IXAudio2SubmixVoice_GetChannelVolumes IXAudio2Voice_GetChannelVolumes
#define IXAudio2SubmixVoice_SetOutputMatrix IXAudio2Voice_SetOutputMatrix
#define IXAudio2SubmixVoice_GetOutputMatrix IXAudio2Voice_GetOutputMatrix
#define IXAudio2SubmixVoice_DestroyVoice IXAudio2Voice_DestroyVoice

/* IXAudio2MasteringVoice */
#define IXAudio2MasteringVoice_GetVoiceDetails IXAudio2Voice_GetVoiceDetails
#define IXAudio2MasteringVoice_SetOutputVoices IXAudio2Voice_SetOutputVoices
#define IXAudio2MasteringVoice_SetEffectChain IXAudio2Voice_SetEffectChain
#define IXAudio2MasteringVoice_EnableEffect IXAudio2Voice_EnableEffect
#define IXAudio2MasteringVoice_DisableEffect IXAudio2Voice_DisableEffect
#define IXAudio2MasteringVoice_GetEffectState IXAudio2Voice_GetEffectState
#define IXAudio2MasteringVoice_SetEffectParameters IXAudio2Voice_SetEffectParameters
#define IXAudio2MasteringVoice_GetEffectParameters IXAudio2Voice_GetEffectParameters
#define IXAudio2MasteringVoice_SetFilterParameters IXAudio2Voice_SetFilterParameters
#define IXAudio2MasteringVoice_GetFilterParameters IXAudio2Voice_GetFilterParameters
#define IXAudio2MasteringVoice_SetOutputFilterParameters IXAudio2Voice_SetOutputFilterParameters
#define IXAudio2MasteringVoice_GetOutputFilterParameters IXAudio2Voice_GetOutputFilterParameters
#define IXAudio2MasteringVoice_SetVolume IXAudio2Voice_SetVolume
#define IXAudio2MasteringVoice_GetVolume IXAudio2Voice_GetVolume
#define IXAudio2MasteringVoice_SetChannelVolumes IXAudio2Voice_SetChannelVolumes
#define IXAudio2MasteringVoice_GetChannelVolumes IXAudio2Voice_GetChannelVolumes
#define IXAudio2MasteringVoice_SetOutputMatrix IXAudio2Voice_SetOutputMatrix
#define IXAudio2MasteringVoice_GetOutputMatrix IXAudio2Voice_GetOutputMatrix
#define IXAudio2MasteringVoice_DestroyVoice IXAudio2Voice_DestroyVoice
#define IXAudio2MasteringVoice_GetChannelMask(This,pChannelMask) ((This)->lpVtbl->GetChannelMask(This,pChannelMask))

#endif /* #ifndef __cplusplus */

/**************************************************************************
 *
 * Utility functions used to convert from pitch in semitones and volume
 * in decibels to the frequency and amplitude ratio units used by XAudio2.
 * These are only defined if the client #defines XAUDIO2_HELPER_FUNCTIONS
 * prior to #including xaudio2.h.
 *
 **************************************************************************/

#ifdef XAUDIO2_HELPER_FUNCTIONS

#define _USE_MATH_DEFINES   /* Make math.h define M_PI */
#include <math.h>           /* For powf, log10f, sinf and asinf */

/* Calculate the argument to SetVolume from a decibel value */
static INLINE float XAudio2DecibelsToAmplitudeRatio(float Decibels)
{
   return powf(10.0f, Decibels / 20.0f);
}

/* Recover a volume in decibels from an amplitude factor */
static INLINE float XAudio2AmplitudeRatioToDecibels(float Volume)
{
   if (Volume == 0)
      return -3.402823466e+38f; /* Smallest float value (-FLT_MAX) */
   return 20.0f * log10f(Volume);
}

/* Calculate the argument to SetFrequencyRatio from a semitone value */
static INLINE float XAudio2SemitonesToFrequencyRatio(float Semitones)
{
   /* FrequencyRatio = 2 ^ Octaves
    *                = 2 ^ (Semitones / 12)
    */
   return powf(2.0f, Semitones / 12.0f);
}

/* Recover a pitch in semitones from a frequency ratio */
static INLINE float XAudio2FrequencyRatioToSemitones(float FrequencyRatio)
{
   /* Semitones = 12 * log2(FrequencyRatio)
    *           = 12 * log2(10) * log10(FrequencyRatio)
    */
   return 39.86313713864835f * log10f(FrequencyRatio);
}

/* Convert from filter cutoff frequencies expressed in Hertz to the radian
 * frequency values used in XAUDIO2_FILTER_PARAMETERS.Frequency, state-variable
 * filter types only.  Use XAudio2CutoffFrequencyToOnePoleCoefficient() for one-pole filter types.
 * Note that the highest CutoffFrequency supported is SampleRate/6.
 * Higher values of CutoffFrequency will return XAUDIO2_MAX_FILTER_FREQUENCY.
 */
static INLINE float XAudio2CutoffFrequencyToRadians(float CutoffFrequency, UINT32 SampleRate)
{
   if ((UINT32)(CutoffFrequency * 6.0f) >= SampleRate)
      return XAUDIO2_MAX_FILTER_FREQUENCY;
   return 2.0f * sinf((float)M_PI * CutoffFrequency / SampleRate);
}

/* Convert from radian frequencies back to absolute frequencies in Hertz */
static INLINE float XAudio2RadiansToCutoffFrequency(float Radians, float SampleRate)
{
   return SampleRate * asinf(Radians / 2.0f) / (float)M_PI;
}

/* Convert from filter cutoff frequencies expressed in Hertz to the filter
 * coefficients used with XAUDIO2_FILTER_PARAMETERS.Frequency,
 * LowPassOnePoleFilter and HighPassOnePoleFilter filter types only.
 * Use XAudio2CutoffFrequencyToRadians() for state-variable filter types.
 */
static INLINE float XAudio2CutoffFrequencyToOnePoleCoefficient(float CutoffFrequency, UINT32 SampleRate)
{
   if ((UINT32)CutoffFrequency >= SampleRate)
      return XAUDIO2_MAX_FILTER_FREQUENCY;
   return (1.0f - powf(1.0f - 2.0f * CutoffFrequency / SampleRate, 2.0f));
}

#endif /* #ifdef XAUDIO2_HELPER_FUNCTIONS */

/**************************************************************************
 *
 * XAudio2Create: Top-level function that creates an XAudio2 instance.
 *
 * ARGUMENTS:
 *
 *  Flags - Flags specifying the XAudio2 object's behavior.
 *
 *  XAudio2Processor - An XAUDIO2_PROCESSOR value that specifies the
 *          hardware threads (Xbox) or processors (Windows) that XAudio2
 *          will use.  Note that XAudio2 supports concurrent processing on
 *          multiple threads, using any combination of XAUDIO2_PROCESSOR
 *          flags.  The values are platform-specific; platform-independent
 *          code can use XAUDIO2_DEFAULT_PROCESSOR to use the default on
 *          each platform.
 *
 **************************************************************************/

#ifdef __cplusplus__

#if (defined XAUDIO2_EXPORT)
 /* We're building xaudio2.dll */
#define XAUDIO2_STDAPI extern "C" __declspec(dllexport) HRESULT __stdcall
#else
 /* We're an xaudio2 client */
#define XAUDIO2_STDAPI extern "C" __declspec(dllimport) HRESULT __stdcall
#endif

#else

 /* Modified for C support */

#if (defined XAUDIO2_EXPORT)
    /* We're building xaudio2.dll */
#define XAUDIO2_STDAPI __declspec(dllexport) HRESULT __stdcall
#else
    /* We're an xaudio2 client */
#define XAUDIO2_STDAPI __declspec(dllimport) HRESULT __stdcall
#endif

#endif

#if (NTDDI_VERSION >= NTDDI_WIN10_RS5) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
XAUDIO2_STDAPI XAudio2CreateWithVersionInfo(_Outptr_ IXAudio2** ppXAudio2,
   UINT32 Flags X2DEFAULT(0),
   XAUDIO2_PROCESSOR XAudio2Processor X2DEFAULT(XAUDIO2_DEFAULT_PROCESSOR),
   DWORD ntddiVersion X2DEFAULT(NTDDI_VERSION));

static INLINE HRESULT XAudio2Create(_Outptr_ IXAudio2** ppXAudio2,
   UINT32 Flags X2DEFAULT(0),
   XAUDIO2_PROCESSOR XAudio2Processor X2DEFAULT(XAUDIO2_DEFAULT_PROCESSOR))
{
   /* When compiled for RS5 or later, try to invoke XAudio2CreateWithVersionInfo.
    * Need to use LoadLibrary in case the app is running on an older OS. */
   typedef HRESULT(__stdcall *XAudio2CreateWithVersionInfoFunc)(_Outptr_ IXAudio2**, UINT32, XAUDIO2_PROCESSOR, DWORD);
   typedef HRESULT(__stdcall *XAudio2CreateInfoFunc)(_Outptr_ IXAudio2**, UINT32, XAUDIO2_PROCESSOR);

   static XAudio2CreateWithVersionInfoFunc 
      s_pfnAudio2CreateWithVersion                = NULL;
   static XAudio2CreateInfoFunc s_pfnAudio2Create = NULL;
   static HMODULE s_dllInstance                   = NULL;

   if (!s_dllInstance)
   {
      s_dllInstance = LoadLibraryEx(XAUDIO2_DLL, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
      if (!s_dllInstance)
         return HRESULT_FROM_WIN32(GetLastError());

      s_pfnAudio2CreateWithVersion = (XAudio2CreateWithVersionInfoFunc)(void*)GetProcAddress(s_dllInstance, "XAudio2CreateWithVersionInfo");
      if (!s_pfnAudio2CreateWithVersion)
      {
         s_pfnAudio2Create = (XAudio2CreateInfoFunc)(void*)GetProcAddress(s_dllInstance, "XAudio2Create");
         if (!s_pfnAudio2Create)
            return HRESULT_FROM_WIN32(GetLastError());
      }
   }

   if (s_pfnAudio2CreateWithVersion)
      return (*s_pfnAudio2CreateWithVersion)(ppXAudio2, Flags, XAudio2Processor, NTDDI_VERSION);
   return (*s_pfnAudio2Create)(ppXAudio2, Flags, XAudio2Processor);
}
#else
/* RS4 or older, or not a desktop app */
XAUDIO2_STDAPI XAudio2Create(_Outptr_ IXAudio2** ppXAudio2, UINT32 Flags X2DEFAULT(0),
   XAUDIO2_PROCESSOR XAudio2Processor X2DEFAULT(XAUDIO2_DEFAULT_PROCESSOR));
#endif

/* Undo the #pragma pack(push, 1) directive at the top of this file */
#pragma pack(pop)

#endif /* #ifndef GUID_DEFS_ONLY */

#endif /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_TV_APP | WINAPI_PARTITION_TV_TITLE) */
#pragma endregion

#endif /* #ifndef __XAUDIO2_INCLUDED__ */
