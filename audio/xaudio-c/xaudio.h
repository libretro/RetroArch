/*
   xaudio.h (2010-08-14) / (2012-12-04)
   authors: OV2, Themaister
*/

// Kinda stripped down. Only contains the bare essentials used in RetroArch.

#ifndef XAUDIO2_MINGW_H
#define XAUDIO2_MINGW_H

#ifdef __cplusplus
#define CINTERFACE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <basetyps.h>
#include <objbase.h>
#include <mmreg.h>

#define DEFINE_GUID_X(n, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
   static const GUID n = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }
#define DEFINE_CLSID_X(className, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
   DEFINE_GUID_X(CLSID_##className, 0x##l, 0x##w1, 0x##w2, 0x##b1, 0x##b2, 0x##b3, 0x##b4, 0x##b5, 0x##b6, 0x##b7, 0x##b8)
#define DEFINE_IID_X(interfaceName, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
   DEFINE_GUID_X(IID_##interfaceName, 0x##l, 0x##w1, 0x##w2, 0x##b1, 0x##b2, 0x##b3, 0x##b4, 0x##b5, 0x##b6, 0x##b7, 0x##b8)
#define X2DEFAULT(x)

DEFINE_CLSID_X(XAudio2, e21a7345, eb21, 468e, be, 50, 80, 4d, b9, 7c, f7, 08);
DEFINE_IID_X(IXAudio2, 8bcf1f58, 9fe7, 4583, 8a, c6, e2, ad, c4, 65, c8, bb);

#ifndef INTERFACE
#define INTERFACE void
#endif

#define XAUDIO2_COMMIT_NOW              0
#define XAUDIO2_DEFAULT_CHANNELS        0
#define XAUDIO2_DEFAULT_SAMPLERATE      0
#define XAUDIO2_DEFAULT_FREQ_RATIO      4.0f
#define XAUDIO2_DEBUG_ENGINE            0x0001
#define XAUDIO2_VOICE_NOSRC             0x0004

typedef enum XAUDIO2_DEVICE_ROLE
{
   NotDefaultDevice            = 0x0,
   DefaultConsoleDevice        = 0x1,
   DefaultMultimediaDevice     = 0x2,
   DefaultCommunicationsDevice = 0x4,
   DefaultGameDevice           = 0x8,
   GlobalDefaultDevice         = 0xf,
   InvalidDeviceRole           = ~GlobalDefaultDevice
} XAUDIO2_DEVICE_ROLE;

typedef enum XAUDIO2_WINDOWS_PROCESSOR_SPECIFIER
{
   XAUDIO2_ANY_PROCESSOR       = 0xffffffff,
   XAUDIO2_DEFAULT_PROCESSOR   = XAUDIO2_ANY_PROCESSOR
} XAUDIO2_WINDOWS_PROCESSOR_SPECIFIER, XAUDIO2_PROCESSOR;

typedef enum XAUDIO2_FILTER_TYPE {
   LowPassFilter,
   BandPassFilter,
   HighPassFilter 
} XAUDIO2_FILTER_TYPE;

typedef struct XAUDIO2_DEVICE_DETAILS
{
   WCHAR DeviceID[256];
   WCHAR DisplayName[256];
   XAUDIO2_DEVICE_ROLE Role;
   WAVEFORMATEXTENSIBLE OutputFormat;
} XAUDIO2_DEVICE_DETAILS;

typedef struct XAUDIO2_VOICE_DETAILS XAUDIO2_VOICE_DETAILS;
typedef struct XAUDIO2_VOICE_SENDS XAUDIO2_VOICE_SENDS;
typedef struct XAUDIO2_EFFECT_DESCRIPTOR XAUDIO2_EFFECT_DESCRIPTOR;
typedef struct XAUDIO2_EFFECT_CHAIN XAUDIO2_EFFECT_CHAIN;
typedef struct XAUDIO2_FILTER_PARAMETERS XAUDIO2_FILTER_PARAMETERS;
typedef struct XAUDIO2_BUFFER_WMA XAUDIO2_BUFFER_WMA;
typedef struct XAUDIO2_VOICE_STATE XAUDIO2_VOICE_STATE;
typedef struct XAUDIO2_PERFORMANCE_DATA XAUDIO2_PERFORMANCE_DATA;
typedef struct XAUDIO2_DEBUG_CONFIGURATION XAUDIO2_DEBUG_CONFIGURATION;
typedef struct IXAudio2EngineCallback IXAudio2EngineCallback;
typedef struct IXAudio2SubmixVoice IXAudio2SubmixVoice;

typedef struct XAUDIO2_BUFFER
{
   UINT32 Flags;
   UINT32 AudioBytes;
   const BYTE* pAudioData;
   UINT32 PlayBegin;
   UINT32 PlayLength;
   UINT32 LoopBegin;
   UINT32 LoopLength;
   UINT32 LoopCount;
   void *pContext;
} XAUDIO2_BUFFER;

DECLARE_INTERFACE(IXAudio2VoiceCallback)
{
   STDMETHOD_(void, OnVoiceProcessingPassStart) (THIS_ UINT32 BytesRequired) PURE;
   STDMETHOD_(void, OnVoiceProcessingPassEnd) (THIS) PURE;
   STDMETHOD_(void, OnStreamEnd) (THIS) PURE;
   STDMETHOD_(void, OnBufferStart) (THIS_ void *pBufferContext) PURE;
   STDMETHOD_(void, OnBufferEnd) (THIS_ void *pBufferContext) PURE;
   STDMETHOD_(void, OnLoopEnd) (THIS_ void *pBufferContext) PURE;
   STDMETHOD_(void, OnVoiceError) (THIS_ void *pBufferContext, HRESULT Error) PURE;
};

DECLARE_INTERFACE(IXAudio2Voice)
{
#define Declare_IXAudio2Voice_Methods() \
   STDMETHOD_(void, GetVoiceDetails) (THIS_ XAUDIO2_VOICE_DETAILS* pVoiceDetails) PURE; \
   STDMETHOD(SetOutputVoices) (THIS_ const XAUDIO2_VOICE_SENDS* pSendList) PURE; \
   STDMETHOD(SetEffectChain) (THIS_ const XAUDIO2_EFFECT_CHAIN* pEffectChain) PURE; \
   STDMETHOD(EnableEffect) (THIS_ UINT32 EffectIndex, \
         UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD(DisableEffect) (THIS_ UINT32 EffectIndex, \
         UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD_(void, GetEffectState) (THIS_ UINT32 EffectIndex, BOOL* pEnabled) PURE; \
   STDMETHOD(SetEffectParameters) (THIS_ UINT32 EffectIndex, \
         const void *pParameters, \
         UINT32 ParametersByteSize, \
         UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD(GetEffectParameters) (THIS_ UINT32 EffectIndex, void *pParameters, \
         UINT32 ParametersByteSize) PURE; \
   STDMETHOD(SetFilterParameters) (THIS_ const XAUDIO2_FILTER_PARAMETERS* pParameters, \
         UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD_(void, GetFilterParameters) (THIS_ XAUDIO2_FILTER_PARAMETERS* pParameters) PURE; \
   STDMETHOD(SetVolume) (THIS_ float Volume, \
         UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD_(void, GetVolume) (THIS_ float* pVolume) PURE; \
   STDMETHOD(SetChannelVolumes) (THIS_ UINT32 Channels, const float* pVolumes, \
         UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD_(void, GetChannelVolumes) (THIS_ UINT32 Channels, float* pVolumes) PURE; \
   STDMETHOD(SetOutputMatrix) (THIS_ IXAudio2Voice* pDestinationVoice, \
         UINT32 SourceChannels, UINT32 DestinationChannels, \
         const float* pLevelMatrix, \
         UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE; \
   STDMETHOD_(void, GetOutputMatrix) (THIS_ IXAudio2Voice* pDestinationVoice, \
         UINT32 SourceChannels, UINT32 DestinationChannels, \
         float* pLevelMatrix) PURE; \
   STDMETHOD_(void, DestroyVoice) (THIS) PURE

   Declare_IXAudio2Voice_Methods();
};

DECLARE_INTERFACE_(IXAudio2MasteringVoice, IXAudio2Voice)
{
   Declare_IXAudio2Voice_Methods();
};

DECLARE_INTERFACE_(IXAudio2SourceVoice, IXAudio2Voice)
{
   Declare_IXAudio2Voice_Methods();
   STDMETHOD(Start) (THIS_ UINT32 Flags, UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
   STDMETHOD(Stop) (THIS_ UINT32 Flags, UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
   STDMETHOD(SubmitSourceBuffer) (THIS_ const XAUDIO2_BUFFER* pBuffer, const XAUDIO2_BUFFER_WMA* pBufferWMA X2DEFAULT(NULL)) PURE;
   STDMETHOD(FlushSourceBuffers) (THIS) PURE;
   STDMETHOD(Discontinuity) (THIS) PURE;
   STDMETHOD(ExitLoop) (THIS_ UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
   STDMETHOD_(void, GetState) (THIS_ XAUDIO2_VOICE_STATE* pVoiceState) PURE;
   STDMETHOD(SetFrequencyRatio) (THIS_ float Ratio,
         UINT32 OperationSet X2DEFAULT(XAUDIO2_COMMIT_NOW)) PURE;
   STDMETHOD_(void, GetFrequencyRatio) (THIS_ float* pRatio) PURE;
};

DECLARE_INTERFACE_(IXAudio2, IUnknown)
{
   STDMETHOD(QueryInterface) (THIS_ REFIID riid, void** ppvInterface) PURE;
   STDMETHOD_(ULONG, AddRef) (THIS) PURE;
   STDMETHOD_(ULONG, Release) (THIS) PURE;
   STDMETHOD(GetDeviceCount) (THIS_ UINT32* pCount) PURE;
   STDMETHOD(GetDeviceDetails) (THIS_ UINT32 Index, XAUDIO2_DEVICE_DETAILS* pDeviceDetails) PURE;
   STDMETHOD(Initialize) (THIS_ UINT32 Flags X2DEFAULT(0),
         XAUDIO2_PROCESSOR XAudio2Processor X2DEFAULT(XAUDIO2_DEFAULT_PROCESSOR)) PURE;
   STDMETHOD(RegisterForCallbacks) (IXAudio2EngineCallback* pCallback) PURE;
   STDMETHOD_(void, UnregisterForCallbacks) (IXAudio2EngineCallback* pCallback) PURE;
   STDMETHOD(CreateSourceVoice) (THIS_ IXAudio2SourceVoice** ppSourceVoice,
         const WAVEFORMATEX* pSourceFormat,
         UINT32 Flags X2DEFAULT(0),
         float MaxFrequencyRatio X2DEFAULT(XAUDIO2_DEFAULT_FREQ_RATIO),
         IXAudio2VoiceCallback* pCallback X2DEFAULT(NULL),
         const XAUDIO2_VOICE_SENDS* pSendList X2DEFAULT(NULL),
         const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) PURE;
   STDMETHOD(CreateSubmixVoice) (THIS_ IXAudio2SubmixVoice** ppSubmixVoice,
         UINT32 InputChannels, UINT32 InputSampleRate,
         UINT32 Flags X2DEFAULT(0), UINT32 ProcessingStage X2DEFAULT(0),
         const XAUDIO2_VOICE_SENDS* pSendList X2DEFAULT(NULL),
         const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) PURE;
   STDMETHOD(CreateMasteringVoice) (THIS_ IXAudio2MasteringVoice** ppMasteringVoice,
         UINT32 InputChannels X2DEFAULT(XAUDIO2_DEFAULT_CHANNELS),
         UINT32 InputSampleRate X2DEFAULT(XAUDIO2_DEFAULT_SAMPLERATE),
         UINT32 Flags X2DEFAULT(0), UINT32 DeviceIndex X2DEFAULT(0),
         const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) PURE;
   STDMETHOD(StartEngine) (THIS) PURE;
   STDMETHOD_(void, StopEngine) (THIS) PURE;
   STDMETHOD(CommitChanges) (THIS_ UINT32 OperationSet) PURE;
   STDMETHOD_(void, GetPerformanceData) (THIS_ XAUDIO2_PERFORMANCE_DATA* pPerfData) PURE;
   STDMETHOD_(void, SetDebugConfiguration) (THIS_ const XAUDIO2_DEBUG_CONFIGURATION* pDebugConfiguration,
         void *pReserved X2DEFAULT(NULL)) PURE;
};

// C hooks.
#define IXAudio2_Initialize(THIS, ...) (THIS)->lpVtbl->Initialize(THIS, __VA_ARGS__)
#define IXAudio2_Release(THIS) (THIS)->lpVtbl->Release(THIS)
#define IXAudio2_CreateSourceVoice(THIS, ...) (THIS)->lpVtbl->CreateSourceVoice(THIS, __VA_ARGS__)
#define IXAudio2_CreateMasteringVoice(THIS, ...) (THIS)->lpVtbl->CreateMasteringVoice(THIS, __VA_ARGS__)
#define IXAudio2_GetDeviceCount(THIS, ...) (THIS)->lpVtbl->GetDeviceCount(THIS, __VA_ARGS__)
#define IXAudio2_GetDeviceDetails(THIS, ...) (THIS)->lpVtbl->GetDeviceDetails(THIS, __VA_ARGS__)

#define IXAudio2SourceVoice_Start(THIS, ...) (THIS)->lpVtbl->Start(THIS, __VA_ARGS__)
#define IXAudio2SourceVoice_Stop(THIS, ...) (THIS)->lpVtbl->Stop(THIS, __VA_ARGS__)
#define IXAudio2SourceVoice_SubmitSourceBuffer(THIS, ...) (THIS)->lpVtbl->SubmitSourceBuffer(THIS, __VA_ARGS__)
#define IXAudio2SourceVoice_DestroyVoice(THIS) (THIS)->lpVtbl->DestroyVoice(THIS)
#define IXAudio2MasteringVoice_DestroyVoice(THIS) (THIS)->lpVtbl->DestroyVoice(THIS)

static inline HRESULT XAudio2Create(IXAudio2 **ppXAudio2, int, int)
{
   IXAudio2 *pXAudio2;

#ifdef __cplusplus
   HRESULT hr = CoCreateInstance(CLSID_XAudio2, NULL, CLSCTX_INPROC_SERVER, IID_IXAudio2, (void**)&pXAudio2);
#else
   HRESULT hr = CoCreateInstance(&CLSID_XAudio2, NULL, CLSCTX_INPROC_SERVER, &IID_IXAudio2, (void**)&pXAudio2);
#endif

   if (SUCCEEDED(hr))
   {
      hr = IXAudio2_Initialize(pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
      if (SUCCEEDED(hr))
         *ppXAudio2 = pXAudio2;
      else
         IXAudio2_Release(pXAudio2);
   }
   return hr;
}

#endif

