/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2010-2013 - OV2
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

// Kinda stripped down. Only contains the bare essentials used in RetroArch.

#ifndef XAUDIO2_XDK360_H
#define XAUDIO2_XDK360_H

#define DEFINE_CLSID(className, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
	DEFINE_GUID(CLSID_##className, 0x##l, 0x##w1, 0x##w2, 0x##b1, 0x##b2, 0x##b3, 0x##b4, 0x##b5, 0x##b6, 0x##b7, 0x##b8)
#define DEFINE_IID(interfaceName, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
	DEFINE_GUID(IID_##interfaceName, 0x##l, 0x##w1, 0x##w2, 0x##b1, 0x##b2, 0x##b3, 0x##b4, 0x##b5, 0x##b6, 0x##b7, 0x##b8)
#define X2DEFAULT(x)

DEFINE_CLSID(XAudio2, 3eda9b49, 2085, 498b, 9b, b2, 39, a6, 77, 84, 93, de);
DEFINE_CLSID(XAudio2_Debug, 47199894, 7cc2, 444d, 98, 73, ce, d2, 56, 2c, c6, 0e);
DEFINE_IID(IXAudio2, 8bcf1f58, 9fe7, 4583, 8a, c6, e2, ad, c4, 65, c8, bb);

#include <audiodefs.h>      // Basic audio data types and constants

// All structures defined in this file use tight field packing
#pragma pack(push, 1)

#define XAUDIO2_COMMIT_NOW              0
#define XAUDIO2_DEFAULT_CHANNELS        0
#define XAUDIO2_DEFAULT_SAMPLERATE      0
#define XAUDIO2_DEFAULT_FREQ_RATIO      2.0f
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

typedef enum XAUDIO2_FILTER_TYPE
{
	LowPassFilter,
	BandPassFilter,
	HighPassFilter,
	NotchFilter
} XAUDIO2_FILTER_TYPE;

typedef struct XAUDIO2_DEVICE_DETAILS
{
	WCHAR DeviceID[256];
	WCHAR DisplayName[256];
	XAUDIO2_DEVICE_ROLE Role;
	WAVEFORMATEXTENSIBLE OutputFormat;
} XAUDIO2_DEVICE_DETAILS;

typedef interface XAUDIO2_VOICE_DETAILS XAUDIO2_VOICE_DETAILS;
typedef interface XAUDIO2_VOICE_SENDS XAUDIO2_VOICE_SENDS;
typedef interface XAUDIO2_EFFECT_DESCRIPTOR XAUDIO2_EFFECT_DESCRIPTOR;
typedef interface XAUDIO2_EFFECT_CHAIN XAUDIO2_EFFECT_CHAIN;
typedef interface XAUDIO2_FILTER_PARAMETERS XAUDIO2_FILTER_PARAMETERS;
typedef interface XAUDIO2_BUFFER_WMA XAUDIO2_BUFFER_WMA;
typedef interface XAUDIO2_VOICE_STATE XAUDIO2_VOICE_STATE;
typedef interface XAUDIO2_PERFORMANCE_DATA XAUDIO2_PERFORMANCE_DATA;
typedef interface XAUDIO2_DEBUG_CONFIGURATION XAUDIO2_DEBUG_CONFIGURATION;
typedef interface IXAudio2EngineCallback IXAudio2EngineCallback;
typedef interface IXAudio2SubmixVoice IXAudio2SubmixVoice;

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
	void* pContext;
} XAUDIO2_BUFFER;

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

DECLARE_INTERFACE_(IXAudio2MasteringVoice, IXAudio2Voice)
{
	Declare_IXAudio2Voice_Methods();
};

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

// C++ hooks.
#define IXAudio2_Initialize(This,Flags,XAudio2Processor) ((This)->lpVtbl->Initialize(This,Flags,XAudio2Processor))
#define IXAudio2_Release(This) ((This)->lpVtbl->Release(This))
#define IXAudio2_CreateSourceVoice(This,ppSourceVoice,pSourceFormat,Flags,MaxFrequencyRatio,pCallback,pSendList,pEffectChain) ((This)->lpVtbl->CreateSourceVoice(This,ppSourceVoice,pSourceFormat,Flags,MaxFrequencyRatio,pCallback,pSendList,pEffectChain))
#define IXAudio2_CreateMasteringVoice(This,ppMasteringVoice,InputChannels,InputSampleRate,Flags,DeviceIndex,pEffectChain) ((This)->lpVtbl->CreateMasteringVoice(This,ppMasteringVoice,InputChannels,InputSampleRate,Flags,DeviceIndex,pEffectChain))
#define IXAudio2_GetDeviceCount(This,puCount) ((This)->lpVtbl->GetDeviceCount(This,puCount))
#define IXAudio2_GetDeviceDetails(This,Index,pDeviceDetails) ((This)->lpVtbl->GetDeviceDetails(This,Index,pDeviceDetails))

#define IXAudio2SourceVoice_Start(This,Flags,OperationSet) ((This)->lpVtbl->Start(This,Flags,OperationSet))
#define IXAudio2SourceVoice_Stop(This,Flags,OperationSet) ((This)->lpVtbl->Stop(This,Flags,OperationSet))
#define IXAudio2SourceVoice_SubmitSourceBuffer(This,pBuffer,pBufferWMA) ((This)->lpVtbl->SubmitSourceBuffer(This,pBuffer,pBufferWMA))
#define IXAudio2SourceVoice_DestroyVoice IXAudio2Voice_DestroyVoice
#define IXAudio2MasteringVoice_DestroyVoice IXAudio2Voice_DestroyVoice

STDAPI XAudio2Create(__deref_out IXAudio2** ppXAudio2, UINT32 Flags X2DEFAULT(0),
		XAUDIO2_PROCESSOR XAudio2Processor X2DEFAULT(XAUDIO2_DEFAULT_PROCESSOR));

// Undo the #pragma pack(push, 1) directive at the top of this file
#pragma pack(pop)

#endif

