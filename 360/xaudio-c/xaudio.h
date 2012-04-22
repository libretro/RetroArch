/*
   xaudio.h (2010-08-14) / (2012-12-04)
   authors: OV2, Themaister
*/

// Kinda stripped down. Only contains the bare essentials used in RetroArch.

#ifndef XAUDIO2_XDK360_H
#define XAUDIO2_XDK360_H

#include <comdecl.h>        // For DEFINE_CLSID and DEFINE_IID

DEFINE_CLSID(XAudio2, 3eda9b49, 2085, 498b, 9b, b2, 39, a6, 77, 84, 93, de);
DEFINE_CLSID(XAudio2_Debug, 47199894, 7cc2, 444d, 98, 73, ce, d2, 56, 2c, c6, 0e);
DEFINE_IID(IXAudio2, 8bcf1f58, 9fe7, 4583, 8a, c6, e2, ad, c4, 65, c8, bb);


// Ignore the rest of this header if only the GUID definitions were requested
#ifndef GUID_DEFS_ONLY

#include "objbase.h"   // Xbox COM declarations (IUnknown, etc)

#include <audiodefs.h>      // Basic audio data types and constants
#include <xma2defs.h>       // Data types and constants for XMA2 audio

// All structures defined in this file use tight field packing
#pragma pack(push, 1)

// Numeric boundary values
#define XAUDIO2_MAX_BUFFER_BYTES        0x80000000    // Maximum bytes allowed in a source buffer
#define XAUDIO2_MAX_QUEUED_BUFFERS      64            // Maximum buffers allowed in a voice queue
#define XAUDIO2_MAX_BUFFERS_SYSTEM      2             // Maximum buffers allowed for system threads (Xbox 360 only)
#define XAUDIO2_MAX_AUDIO_CHANNELS      64            // Maximum channels in an audio stream
#define XAUDIO2_MIN_SAMPLE_RATE         1000          // Minimum audio sample rate supported
#define XAUDIO2_MAX_SAMPLE_RATE         200000        // Maximum audio sample rate supported
#define XAUDIO2_MAX_VOLUME_LEVEL        16777216.0f   // Maximum acceptable volume level (2^24)
#define XAUDIO2_MIN_FREQ_RATIO          (1/1024.0f)   // Minimum SetFrequencyRatio argument
#define XAUDIO2_MAX_FREQ_RATIO          1024.0f       // Maximum MaxFrequencyRatio argument
#define XAUDIO2_DEFAULT_FREQ_RATIO      2.0f          // Default MaxFrequencyRatio argument
#define XAUDIO2_MAX_FILTER_ONEOVERQ     1.5f          // Maximum XAUDIO2_FILTER_PARAMETERS.OneOverQ
#define XAUDIO2_MAX_FILTER_FREQUENCY    1.0f          // Maximum XAUDIO2_FILTER_PARAMETERS.Frequency
#define XAUDIO2_MAX_LOOP_COUNT          254           // Maximum non-infinite XAUDIO2_BUFFER.LoopCount
#define XAUDIO2_MAX_INSTANCES           8             // Maximum simultaneous XAudio2 objects on Xbox 360

#define XAUDIO2_MAX_RATIO_TIMES_RATE_XMA_MONO         600000
#define XAUDIO2_MAX_RATIO_TIMES_RATE_XMA_MULTICHANNEL 300000

// Numeric values with special meanings
#define XAUDIO2_COMMIT_NOW              0             // Used as an OperationSet argument
#define XAUDIO2_COMMIT_ALL              0             // Used in IXAudio2::CommitChanges
#define XAUDIO2_INVALID_OPSET           (UINT32)(-1)  // Not allowed for OperationSet arguments
#define XAUDIO2_NO_LOOP_REGION          0             // Used in XAUDIO2_BUFFER.LoopCount
#define XAUDIO2_LOOP_INFINITE           255           // Used in XAUDIO2_BUFFER.LoopCount
#define XAUDIO2_DEFAULT_CHANNELS        0             // Used in CreateMasteringVoice
#define XAUDIO2_DEFAULT_SAMPLERATE      0             // Used in CreateMasteringVoice

// Flags
#define XAUDIO2_DEBUG_ENGINE            0x0001        // Used in XAudio2Create on Windows only
#define XAUDIO2_VOICE_NOPITCH           0x0002        // Used in IXAudio2::CreateSourceVoice
#define XAUDIO2_VOICE_NOSRC             0x0004        // Used in IXAudio2::CreateSourceVoice
#define XAUDIO2_VOICE_USEFILTER         0x0008        // Used in IXAudio2::CreateSource/SubmixVoice
#define XAUDIO2_VOICE_MUSIC             0x0010        // Used in IXAudio2::CreateSourceVoice
#define XAUDIO2_PLAY_TAILS              0x0020        // Used in IXAudio2SourceVoice::Stop
#define XAUDIO2_END_OF_STREAM           0x0040        // Used in XAUDIO2_BUFFER.Flags
#define XAUDIO2_SEND_USEFILTER          0x0080        // Used in XAUDIO2_SEND_DESCRIPTOR.Flags

// Default parameters for the built-in filter
#define XAUDIO2_DEFAULT_FILTER_TYPE     LowPassFilter
#define XAUDIO2_DEFAULT_FILTER_FREQUENCY XAUDIO2_MAX_FILTER_FREQUENCY
#define XAUDIO2_DEFAULT_FILTER_ONEOVERQ 1.0f

// Internal XAudio2 constants
#define XAUDIO2_QUANTUM_NUMERATOR   2             // On Xbox 360, XAudio2 processes audio
#define XAUDIO2_QUANTUM_DENOMINATOR 375           //  in 5.333ms chunks (= 2/375 seconds)
#define XAUDIO2_QUANTUM_MS (1000.0f * XAUDIO2_QUANTUM_NUMERATOR / XAUDIO2_QUANTUM_DENOMINATOR)

// XAudio2 error codes
#define FACILITY_XAUDIO2 0x896
#define XAUDIO2_E_INVALID_CALL          0x88960001    // An API call or one of its arguments was illegal
#define XAUDIO2_E_XMA_DECODER_ERROR     0x88960002    // The XMA hardware suffered an unrecoverable error
#define XAUDIO2_E_XAPO_CREATION_FAILED  0x88960003    // XAudio2 failed to initialize an XAPO effect
#define XAUDIO2_E_DEVICE_INVALIDATED    0x88960004    // An audio device became unusable (unplugged, etc)

#define FWD_DECLARE(x) typedef interface x x

FWD_DECLARE(IXAudio2);
FWD_DECLARE(IXAudio2Voice);
FWD_DECLARE(IXAudio2SourceVoice);
FWD_DECLARE(IXAudio2SubmixVoice);
FWD_DECLARE(IXAudio2MasteringVoice);
FWD_DECLARE(IXAudio2EngineCallback);
FWD_DECLARE(IXAudio2VoiceCallback);

typedef enum XAUDIO2_XBOX_HWTHREAD_SPECIFIER
{
    XboxThread0 = 0x01,
    XboxThread1 = 0x02,
    XboxThread2 = 0x04,
    XboxThread3 = 0x08,
    XboxThread4 = 0x10,
    XboxThread5 = 0x20,
    XAUDIO2_ANY_PROCESSOR = XboxThread4,
    XAUDIO2_DEFAULT_PROCESSOR = XAUDIO2_ANY_PROCESSOR
} XAUDIO2_XBOX_HWTHREAD_SPECIFIER, XAUDIO2_PROCESSOR;

typedef enum XAUDIO2_DEVICE_ROLE
{
    NotDefaultDevice            = 0x0,
    DefaultConsoleDevice        = 0x1,
    DefaultMultimediaDevice     = 0x2,
    DefaultCommunicationsDevice = 0x4,
    DefaultGameDevice           = 0x8,
    GlobalDefaultDevice         = 0xf,
    InvalidDeviceRole = ~GlobalDefaultDevice
} XAUDIO2_DEVICE_ROLE;

typedef struct XAUDIO2_DEVICE_DETAILS
{
    WCHAR DeviceID[256];                // String identifier for the audio device.
    WCHAR DisplayName[256];             // Friendly name suitable for display to a human.
    XAUDIO2_DEVICE_ROLE Role;           // Roles that the device should be used for.
    WAVEFORMATEXTENSIBLE OutputFormat;  // The device's native PCM audio output format.
} XAUDIO2_DEVICE_DETAILS;

typedef struct XAUDIO2_VOICE_DETAILS
{
    UINT32 CreationFlags;               // Flags the voice was created with.
    UINT32 InputChannels;               // Channels in the voice's input audio.
    UINT32 InputSampleRate;             // Sample rate of the voice's input audio.
} XAUDIO2_VOICE_DETAILS;

// Used in XAUDIO2_VOICE_SENDS below
typedef struct XAUDIO2_SEND_DESCRIPTOR
{
    UINT32 Flags;                       // Either 0 or XAUDIO2_SEND_USEFILTER.
    IXAudio2Voice* pOutputVoice;        // This send's destination voice.
} XAUDIO2_SEND_DESCRIPTOR;

// Used in the voice creation functions and in IXAudio2Voice::SetOutputVoices
typedef struct XAUDIO2_VOICE_SENDS
{
    UINT32 SendCount;                   // Number of sends from this voice.
    XAUDIO2_SEND_DESCRIPTOR* pSends;    // Array of SendCount send descriptors.
} XAUDIO2_VOICE_SENDS;

// Used in XAUDIO2_EFFECT_CHAIN below
typedef struct XAUDIO2_EFFECT_DESCRIPTOR
{
    IUnknown* pEffect;                  // Pointer to the effect object's IUnknown interface.
    BOOL InitialState;                  // TRUE if the effect should begin in the enabled state.
    UINT32 OutputChannels;              // How many output channels the effect should produce.
} XAUDIO2_EFFECT_DESCRIPTOR;

// Used in the voice creation functions and in IXAudio2Voice::SetEffectChain
typedef struct XAUDIO2_EFFECT_CHAIN
{
    UINT32 EffectCount;                 // Number of effects in this voice's effect chain.
    XAUDIO2_EFFECT_DESCRIPTOR* pEffectDescriptors; // Array of effect descriptors.
} XAUDIO2_EFFECT_CHAIN;

// Used in XAUDIO2_FILTER_PARAMETERS below
typedef enum XAUDIO2_FILTER_TYPE
{
    LowPassFilter,                      // Attenuates frequencies above the cutoff frequency.
    BandPassFilter,                     // Attenuates frequencies outside a given range.
    HighPassFilter,                     // Attenuates frequencies below the cutoff frequency.
    NotchFilter                         // Attenuates frequencies inside a given range.
} XAUDIO2_FILTER_TYPE;

// Used in IXAudio2Voice::Set/GetFilterParameters and Set/GetOutputFilterParameters
typedef struct XAUDIO2_FILTER_PARAMETERS
{
    XAUDIO2_FILTER_TYPE Type;           // Low-pass, band-pass or high-pass.
    float Frequency;                    // Radian frequency (2 * sin(pi*CutoffFrequency/SampleRate));
                                        //  must be >= 0 and <= XAUDIO2_MAX_FILTER_FREQUENCY
                                        //  (giving a maximum CutoffFrequency of SampleRate/6).
    float OneOverQ;                     // Reciprocal of the filter's quality factor Q;
                                        //  must be > 0 and <= XAUDIO2_MAX_FILTER_ONEOVERQ.
} XAUDIO2_FILTER_PARAMETERS;

// Used in IXAudio2SourceVoice::SubmitSourceBuffer
typedef struct XAUDIO2_BUFFER
{
    UINT32 Flags;                       // Either 0 or XAUDIO2_END_OF_STREAM.
    UINT32 AudioBytes;                  // Size of the audio data buffer in bytes.
    const BYTE* pAudioData;             // Pointer to the audio data buffer.
    UINT32 PlayBegin;                   // First sample in this buffer to be played.
    UINT32 PlayLength;                  // Length of the region to be played in samples,
                                        //  or 0 to play the whole buffer.
    UINT32 LoopBegin;                   // First sample of the region to be looped.
    UINT32 LoopLength;                  // Length of the desired loop region in samples,
                                        //  or 0 to loop the entire buffer.
    UINT32 LoopCount;                   // Number of times to repeat the loop region,
                                        //  or XAUDIO2_LOOP_INFINITE to loop forever.
    void* pContext;                     // Context value to be passed back in callbacks.
} XAUDIO2_BUFFER;

typedef struct XAUDIO2_BUFFER_WMA
{
    const UINT32* pDecodedPacketCumulativeBytes; // Decoded packet's cumulative size array.
                                                 //  Each element is the number of bytes accumulated
                                                 //  when the corresponding XWMA packet is decoded in
                                                 //  order.  The array must have PacketCount elements.
    UINT32 PacketCount;                          // Number of XWMA packets submitted. Must be >= 1 and
                                                 //  divide evenly into XAUDIO2_BUFFER.AudioBytes.
} XAUDIO2_BUFFER_WMA;

// Returned by IXAudio2SourceVoice::GetState
typedef struct XAUDIO2_VOICE_STATE
{
    void* pCurrentBufferContext;        // The pContext value provided in the XAUDIO2_BUFFER
                                        //  that is currently being processed, or NULL if
                                        //  there are no buffers in the queue.
    UINT32 BuffersQueued;               // Number of buffers currently queued on the voice
                                        //  (including the one that is being processed).
    UINT64 SamplesPlayed;               // Total number of samples produced by the voice since
                                        //  it began processing the current audio stream.
} XAUDIO2_VOICE_STATE;

#define XAUDIO2_LOG_ERRORS     0x0001   // For handled errors with serious effects.
#define XAUDIO2_LOG_WARNINGS   0x0002   // For handled errors that may be recoverable.
#define XAUDIO2_LOG_INFO       0x0004   // Informational chit-chat (e.g. state changes).
#define XAUDIO2_LOG_DETAIL     0x0008   // More detailed chit-chat.
#define XAUDIO2_LOG_API_CALLS  0x0010   // Public API function entries and exits.
#define XAUDIO2_LOG_FUNC_CALLS 0x0020   // Internal function entries and exits.
#define XAUDIO2_LOG_TIMING     0x0040   // Delays detected and other timing data.
#define XAUDIO2_LOG_LOCKS      0x0080   // Usage of critical sections and mutexes.
#define XAUDIO2_LOG_MEMORY     0x0100   // Memory heap usage information.
#define XAUDIO2_LOG_STREAMING  0x1000   // Audio streaming information.

#define X2DEFAULT(x)

#undef INTERFACE
#define INTERFACE IXAudio2
DECLARE_INTERFACE_(IXAudio2, IUnknown)
{
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, __deref_out void** ppvInterface) PURE;
    STDMETHOD_(ULONG, AddRef) (THIS) PURE;
    STDMETHOD_(ULONG, Release) (THIS) PURE;
    STDMETHOD(GetDeviceCount) (THIS_ __out UINT32* pCount) PURE;
    STDMETHOD(GetDeviceDetails) (THIS_ UINT32 Index, __out XAUDIO2_DEVICE_DETAILS* pDeviceDetails) PURE;
    STDMETHOD(Initialize) (THIS_ UINT32 Flags X2DEFAULT(0),
                           XAUDIO2_PROCESSOR XAudio2Processor X2DEFAULT(XAUDIO2_DEFAULT_PROCESSOR)) PURE;
    STDMETHOD(RegisterForCallbacks) (__in IXAudio2EngineCallback* pCallback) PURE;
    STDMETHOD_(void, UnregisterForCallbacks) (__in IXAudio2EngineCallback* pCallback) PURE;
    STDMETHOD(CreateSourceVoice) (THIS_ __deref_out IXAudio2SourceVoice** ppSourceVoice,
                                  __in const WAVEFORMATEX* pSourceFormat,
                                  UINT32 Flags X2DEFAULT(0),
                                  float MaxFrequencyRatio X2DEFAULT(XAUDIO2_DEFAULT_FREQ_RATIO),
                                  __in_opt IXAudio2VoiceCallback* pCallback X2DEFAULT(NULL),
                                  __in_opt const XAUDIO2_VOICE_SENDS* pSendList X2DEFAULT(NULL),
                                  __in_opt const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) PURE;
    STDMETHOD(CreateSubmixVoice) (THIS_ __deref_out IXAudio2SubmixVoice** ppSubmixVoice,
                                  UINT32 InputChannels, UINT32 InputSampleRate,
                                  UINT32 Flags X2DEFAULT(0), UINT32 ProcessingStage X2DEFAULT(0),
                                  __in_opt const XAUDIO2_VOICE_SENDS* pSendList X2DEFAULT(NULL),
                                  __in_opt const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) PURE;
    STDMETHOD(CreateMasteringVoice) (THIS_ __deref_out IXAudio2MasteringVoice** ppMasteringVoice,
                                     UINT32 InputChannels X2DEFAULT(XAUDIO2_DEFAULT_CHANNELS),
                                     UINT32 InputSampleRate X2DEFAULT(XAUDIO2_DEFAULT_SAMPLERATE),
                                     UINT32 Flags X2DEFAULT(0), UINT32 DeviceIndex X2DEFAULT(0),
                                     __in_opt const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) PURE;


    STDMETHOD(StartEngine) (THIS) PURE;
    STDMETHOD_(void, StopEngine) (THIS) PURE;
    STDMETHOD(CommitChanges) (THIS_ UINT32 OperationSet) PURE;
};

#undef INTERFACE
#define INTERFACE IXAudio2Voice
DECLARE_INTERFACE(IXAudio2Voice)
{
    #define Declare_IXAudio2Voice_Methods() \
    STDMETHOD_(void, GetVoiceDetails) (THIS_ __out XAUDIO2_VOICE_DETAILS* pVoiceDetails) PURE; \
    \
    STDMETHOD(SetOutputVoices) (THIS_ __in_opt const XAUDIO2_VOICE_SENDS* pSendList) PURE; \
    \
    STDMETHOD(SetEffectChain) (THIS_ __in_opt const XAUDIO2_EFFECT_CHAIN* pEffectChain) PURE; \
    \
    STDMETHOD(EnableEffect) (THIS_ UINT32 EffectIndex, \
                             UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD(DisableEffect) (THIS_ UINT32 EffectIndex, \
                              UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD_(void, GetEffectState) (THIS_ UINT32 EffectIndex, __out BOOL* pEnabled) PURE; \
    \
    STDMETHOD(SetEffectParameters) (THIS_ UINT32 EffectIndex, \
                                    __in_bcount(ParametersByteSize) const void* pParameters, \
                                    UINT32 ParametersByteSize, \
                                    UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD(GetEffectParameters) (THIS_ UINT32 EffectIndex, \
                                    __out_bcount(ParametersByteSize) void* pParameters, \
                                    UINT32 ParametersByteSize) PURE; \
    \
    STDMETHOD(SetFilterParameters) (THIS_ __in const XAUDIO2_FILTER_PARAMETERS* pParameters, \
                                    UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD_(void, GetFilterParameters) (THIS_ __out XAUDIO2_FILTER_PARAMETERS* pParameters) PURE; \
    \
    STDMETHOD(SetOutputFilterParameters) (THIS_ __in_opt IXAudio2Voice* pDestinationVoice, \
                                          __in const XAUDIO2_FILTER_PARAMETERS* pParameters, \
                                          UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD_(void, GetOutputFilterParameters) (THIS_ __in_opt IXAudio2Voice* pDestinationVoice, \
                                                 __out XAUDIO2_FILTER_PARAMETERS* pParameters) PURE; \
    \
    STDMETHOD(SetVolume) (THIS_ float Volume, \
                          UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD_(void, GetVolume) (THIS_ __out float* pVolume) PURE; \
    \
    STDMETHOD(SetChannelVolumes) (THIS_ UINT32 Channels, __in_ecount(Channels) const float* pVolumes, \
                                  UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD_(void, GetChannelVolumes) (THIS_ UINT32 Channels, __out_ecount(Channels) float* pVolumes) PURE; \
    \
    STDMETHOD(SetOutputMatrix) (THIS_ __in_opt IXAudio2Voice* pDestinationVoice, \
                                UINT32 SourceChannels, UINT32 DestinationChannels, \
                                __in_ecount(SourceChannels * DestinationChannels) const float* pLevelMatrix, \
                                UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
    \
    STDMETHOD_(void, GetOutputMatrix) (THIS_ __in_opt IXAudio2Voice* pDestinationVoice, \
                                       UINT32 SourceChannels, UINT32 DestinationChannels, \
                                       __out_ecount(SourceChannels * DestinationChannels) float* pLevelMatrix) PURE; \
    \
    STDMETHOD_(void, DestroyVoice) (THIS) PURE

    Declare_IXAudio2Voice_Methods();
};

#undef INTERFACE
#define INTERFACE IXAudio2SourceVoice
DECLARE_INTERFACE_(IXAudio2SourceVoice, IXAudio2Voice)
{
    Declare_IXAudio2Voice_Methods();
    STDMETHOD(Start) (THIS_ UINT32 Flags X2DEFAULT(0), UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
    STDMETHOD(Stop) (THIS_ UINT32 Flags X2DEFAULT(0), UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
    STDMETHOD(SubmitSourceBuffer) (THIS_ __in const XAUDIO2_BUFFER* pBuffer, __in_opt const XAUDIO2_BUFFER_WMA* pBufferWMA X2DEFAULT(NULL)) PURE;
    STDMETHOD(FlushSourceBuffers) (THIS) PURE;
    STDMETHOD(Discontinuity) (THIS) PURE;
    STDMETHOD(ExitLoop) (THIS_ UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
    STDMETHOD_(void, GetState) (THIS_ __out XAUDIO2_VOICE_STATE* pVoiceState) PURE;
    STDMETHOD(SetFrequencyRatio) (THIS_ float Ratio,
                                  UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
    STDMETHOD_(void, GetFrequencyRatio) (THIS_ __out float* pRatio) PURE;
    STDMETHOD(SetSourceSampleRate) (THIS_ UINT32 NewSourceSampleRate) PURE;
};

#undef INTERFACE
#define INTERFACE IXAudio2SubmixVoice
DECLARE_INTERFACE_(IXAudio2SubmixVoice, IXAudio2Voice)
{
    Declare_IXAudio2Voice_Methods();
};

#undef INTERFACE
#define INTERFACE IXAudio2MasteringVoice
DECLARE_INTERFACE_(IXAudio2MasteringVoice, IXAudio2Voice)
{
    Declare_IXAudio2Voice_Methods();
};

#undef INTERFACE
#define INTERFACE IXAudio2EngineCallback
DECLARE_INTERFACE(IXAudio2EngineCallback)
{
    STDMETHOD_(void, OnProcessingPassStart) (THIS) PURE;
    STDMETHOD_(void, OnProcessingPassEnd) (THIS) PURE;
    STDMETHOD_(void, OnCriticalError) (THIS_ HRESULT Error) PURE;
};

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

// IXAudio2
#define IXAudio2_QueryInterface(This,riid,ppvInterface) ((This)->lpVtbl->QueryInterface(This,riid,ppvInterface))
#define IXAudio2_AddRef(This) ((This)->lpVtbl->AddRef(This))
#define IXAudio2_Release(This) ((This)->lpVtbl->Release(This))
#define IXAudio2_GetDeviceCount(This,puCount) ((This)->lpVtbl->GetDeviceCount(This,puCount))
#define IXAudio2_GetDeviceDetails(This,Index,pDeviceDetails) ((This)->lpVtbl->GetDeviceDetails(This,Index,pDeviceDetails))
#define IXAudio2_Initialize(This,Flags,XAudio2Processor) ((This)->lpVtbl->Initialize(This,Flags,XAudio2Processor))
#define IXAudio2_CreateSourceVoice(This,ppSourceVoice,pSourceFormat,Flags,MaxFrequencyRatio,pCallback,pSendList,pEffectChain) ((This)->lpVtbl->CreateSourceVoice(This,ppSourceVoice,pSourceFormat,Flags,MaxFrequencyRatio,pCallback,pSendList,pEffectChain))
#define IXAudio2_CreateSubmixVoice(This,ppSubmixVoice,InputChannels,InputSampleRate,Flags,ProcessingStage,pSendList,pEffectChain) ((This)->lpVtbl->CreateSubmixVoice(This,ppSubmixVoice,InputChannels,InputSampleRate,Flags,ProcessingStage,pSendList,pEffectChain))
#define IXAudio2_CreateMasteringVoice(This,ppMasteringVoice,InputChannels,InputSampleRate,Flags,DeviceIndex,pEffectChain) ((This)->lpVtbl->CreateMasteringVoice(This,ppMasteringVoice,InputChannels,InputSampleRate,Flags,DeviceIndex,pEffectChain))
#define IXAudio2_StartEngine(This) ((This)->lpVtbl->StartEngine(This))
#define IXAudio2_StopEngine(This) ((This)->lpVtbl->StopEngine(This))
#define IXAudio2_CommitChanges(This,OperationSet) ((This)->lpVtbl->CommitChanges(This,OperationSet))

// IXAudio2Voice
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

// IXAudio2SourceVoice
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
#define IXAudio2SourceVoice_GetState(This,pVoiceState) ((This)->lpVtbl->GetState(This,pVoiceState))
#define IXAudio2SourceVoice_SetFrequencyRatio(This,Ratio,OperationSet) ((This)->lpVtbl->SetFrequencyRatio(This,Ratio,OperationSet))
#define IXAudio2SourceVoice_GetFrequencyRatio(This,pRatio) ((This)->lpVtbl->GetFrequencyRatio(This,pRatio))
#define IXAudio2SourceVoice_SetSourceSampleRate(This,NewSourceSampleRate) ((This)->lpVtbl->SetSourceSampleRate(This,NewSourceSampleRate))

// IXAudio2SubmixVoice
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

// IXAudio2MasteringVoice
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

STDAPI XAudio2Create(__deref_out IXAudio2** ppXAudio2, UINT32 Flags X2DEFAULT(0),
                     XAUDIO2_PROCESSOR XAudio2Processor X2DEFAULT(XAUDIO2_DEFAULT_PROCESSOR));


// Undo the #pragma pack(push, 1) directive at the top of this file
#pragma pack(pop)

#endif // #ifndef GUID_DEFS_ONLY

#endif

