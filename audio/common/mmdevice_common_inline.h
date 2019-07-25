/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef _MMDEVICE_COMMON_INLINE_H
#define _MMDEVICE_COMMON_INLINE_H

#include <stdlib.h>

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winerror.h>
#include <propidl.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <audioclient.h>

#include <retro_common_api.h>

#ifdef _MSC_VER
DEFINE_GUID(IID_IAudioClient, 0x1CB9AD4C, 0xDBFA, 0x4C32, 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2);
DEFINE_GUID(IID_IAudioRenderClient, 0xF294ACFC, 0x3146, 0x4483, 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
#undef KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);
#endif

DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14); /* DEVPROP_TYPE_STRING */

#ifdef __cplusplus
#define _IMMDeviceCollection_Item(This,nDevice,ppdevice) (This)->Item(nDevice,ppdevice)
#define _IAudioClient_Start(This)	( (This)->Start() )
#define _IAudioClient_Stop(This)	( (This)->Stop() )
#define _IAudioClient_GetCurrentPadding(This,pNumPaddingFrames)	\
    ( (This)->GetCurrentPadding(pNumPaddingFrames) )
#define _IAudioRenderClient_GetBuffer(This,NumFramesRequested,ppData)	\
    ( (This)->GetBuffer(NumFramesRequested,ppData) )
#define _IAudioRenderClient_ReleaseBuffer(This,NumFramesWritten,dwFlags)	\
    ( (This)->ReleaseBuffer(NumFramesWritten,dwFlags) )
#define _IAudioClient_GetService(This,riid,ppv) ( (This)->GetService(riid,ppv) )
#define _IAudioClient_SetEventHandle(This,eventHandle)	( (This)->SetEventHandle(eventHandle) )
#define _IAudioClient_GetBufferSize(This,pNumBufferFrames) ( (This)->GetBufferSize(pNumBufferFrames) )
#define _IAudioClient_GetStreamLatency(This,phnsLatency)	( (This)->GetStreamLatency(phnsLatency) )
#define _IAudioClient_GetDevicePeriod(This,phnsDefaultDevicePeriod,phnsMinimumDevicePeriod)	( (This)->GetDevicePeriod(phnsDefaultDevicePeriod,phnsMinimumDevicePeriod) )
#define _IMMDevice_Activate(This,iid,dwClsCtx,pActivationParams,ppv) ((This)->Activate(iid,(dwClsCtx),pActivationParams,ppv))
#define _IMMDeviceEnumerator_EnumAudioEndpoints(This,dataFlow,dwStateMask,ppDevices) (This)->EnumAudioEndpoints(dataFlow,dwStateMask,ppDevices)
#define _IMMDeviceEnumerator_GetDefaultAudioEndpoint(This,dataFlow,role,ppEndpoint) (This)->GetDefaultAudioEndpoint(dataFlow,role,ppEndpoint)
#define _IMMDevice_OpenPropertyStore(This,stgmAccess,ppProperties) (This)->OpenPropertyStore(stgmAccess,ppProperties)
#define _IMMDevice_GetId(This,ppstrId) ((This)->GetId(ppstrId))
#define _IPropertyStore_GetValue(This,key,pv) ( (This)->GetValue(key,pv) )
#define _IMMDeviceCollection_GetCount(This,cProps) ( (This)->GetCount(cProps) )
#else
#define _IMMDeviceCollection_Item(This,nDevice,ppdevice) (This)->lpVtbl->Item(This,nDevice,ppdevice)
#define _IAudioClient_Start(This)	( (This)->lpVtbl -> Start(This) )
#define _IAudioClient_Stop(This)	( (This)->lpVtbl -> Stop(This) )
#define _IAudioClient_GetCurrentPadding(This,pNumPaddingFrames)	\
    ( (This)->lpVtbl -> GetCurrentPadding(This,pNumPaddingFrames) )
#define _IAudioRenderClient_GetBuffer(This,NumFramesRequested,ppData)	\
    ( (This)->lpVtbl -> GetBuffer(This,NumFramesRequested,ppData) )
#define _IAudioRenderClient_ReleaseBuffer(This,NumFramesWritten,dwFlags)	\
    ( (This)->lpVtbl -> ReleaseBuffer(This,NumFramesWritten,dwFlags) )
#define _IAudioClient_GetService(This,riid,ppv)	( (This)->lpVtbl -> GetService(This,&(riid),ppv) )
#define _IAudioClient_SetEventHandle(This,eventHandle)	( (This)->lpVtbl -> SetEventHandle(This,eventHandle) )
#define _IAudioClient_GetBufferSize(This,pNumBufferFrames) ( (This)->lpVtbl -> GetBufferSize(This,pNumBufferFrames) )
#define _IAudioClient_GetStreamLatency(This,phnsLatency)	( (This)->lpVtbl -> GetStreamLatency(This,phnsLatency) )
#define _IAudioClient_GetDevicePeriod(This,phnsDefaultDevicePeriod,phnsMinimumDevicePeriod)	( (This)->lpVtbl -> GetDevicePeriod(This,phnsDefaultDevicePeriod,phnsMinimumDevicePeriod) )
#define _IMMDevice_Activate(This,iid,dwClsCtx,pActivationParams,ppv) ((This)->lpVtbl->Activate(This,&(iid),dwClsCtx,pActivationParams,ppv))
#define _IMMDeviceEnumerator_EnumAudioEndpoints(This,dataFlow,dwStateMask,ppDevices) (This)->lpVtbl->EnumAudioEndpoints(This,dataFlow,dwStateMask,ppDevices)
#define _IMMDeviceEnumerator_GetDefaultAudioEndpoint(This,dataFlow,role,ppEndpoint) (This)->lpVtbl->GetDefaultAudioEndpoint(This,dataFlow,role,ppEndpoint)
#define _IMMDevice_OpenPropertyStore(This,stgmAccess,ppProperties) (This)->lpVtbl->OpenPropertyStore(This,stgmAccess,ppProperties)
#define _IMMDevice_GetId(This,ppstrId) (This)->lpVtbl->GetId(This,ppstrId)
#define _IPropertyStore_GetValue(This,key,pv) ( (This)->lpVtbl -> GetValue(This,&(key),pv) )
#define _IMMDeviceCollection_GetCount(This,cProps) ( (This)->lpVtbl -> GetCount(This,cProps) )
#endif

#ifdef __cplusplus
#ifndef IFACE_RELEASE
#define IFACE_RELEASE(iface) \
      if (iface) \
      { \
         iface->Release(); \
         iface = NULL; \
      }
#endif
#else
#ifndef IFACE_RELEASE
#define IFACE_RELEASE(iface) \
      if (iface) \
      { \
         iface->lpVtbl->Release(iface);\
         iface = NULL; \
      }
#endif
#endif

#endif
