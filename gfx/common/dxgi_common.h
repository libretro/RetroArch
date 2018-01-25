#pragma once

#ifdef __MINGW32__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#define _In_
#define _In_opt_
#define _Null_

#define _Out_writes_bytes_opt_(s)
#endif

#define CINTERFACE
#include <assert.h>
#include <dxgi1_5.h>

#ifndef countof
#define countof(a) (sizeof(a) / sizeof(*a))
#endif

#ifndef __uuidof
#define __uuidof(type) & IID_##type
#endif

#ifndef COM_RELEASE_DECLARED
#define COM_RELEASE_DECLARED
#if defined(__cplusplus) && !defined(CINTERFACE)
static inline ULONG Release(IUnknown* object)
{
   if (object)
      return object->Release();

   return 0;
}
#else
static inline ULONG Release(void* object)
{
   if (object)
      return ((IUnknown*)object)->lpVtbl->Release(object);

   return 0;
}
#endif
#endif

/* auto-generated */

typedef IDXGIObject*            DXGIObject;
typedef IDXGIDeviceSubObject*   DXGIDeviceSubObject;
typedef IDXGIResource*          DXGIResource;
typedef IDXGIKeyedMutex*        DXGIKeyedMutex;
typedef IDXGISurface1*          DXGISurface;
typedef IDXGIOutput*            DXGIOutput;
typedef IDXGIDevice*            DXGIDevice;
typedef IDXGIFactory1*          DXGIFactory;
typedef IDXGIAdapter1*          DXGIAdapter;
typedef IDXGIDisplayControl*    DXGIDisplayControl;
typedef IDXGIOutputDuplication* DXGIOutputDuplication;
typedef IDXGIDecodeSwapChain*   DXGIDecodeSwapChain;
typedef IDXGIFactoryMedia*      DXGIFactoryMedia;
typedef IDXGISwapChainMedia*    DXGISwapChainMedia;
typedef IDXGISwapChain3*        DXGISwapChain;

static inline ULONG DXGIReleaseDeviceSubObject(DXGIDeviceSubObject device_sub_object)
{
   return device_sub_object->lpVtbl->Release(device_sub_object);
}
static inline HRESULT DXGIGetSharedHandle(void* resource, HANDLE* shared_handle)
{
   return ((IDXGIResource*)resource)->lpVtbl->GetSharedHandle(resource, shared_handle);
}
static inline HRESULT DXGIGetUsage(void* resource, DXGI_USAGE* usage)
{
   return ((IDXGIResource*)resource)->lpVtbl->GetUsage(resource, usage);
}
static inline HRESULT DXGISetEvictionPriority(void* resource, UINT eviction_priority)
{
   return ((IDXGIResource*)resource)->lpVtbl->SetEvictionPriority(resource, eviction_priority);
}
static inline HRESULT DXGIGetEvictionPriority(void* resource, UINT* eviction_priority)
{
   return ((IDXGIResource*)resource)->lpVtbl->GetEvictionPriority(resource, eviction_priority);
}
static inline ULONG DXGIReleaseKeyedMutex(DXGIKeyedMutex keyed_mutex)
{
   return keyed_mutex->lpVtbl->Release(keyed_mutex);
}
static inline HRESULT DXGIAcquireSync(DXGIKeyedMutex keyed_mutex, UINT64 key, DWORD dw_milliseconds)
{
   return keyed_mutex->lpVtbl->AcquireSync(keyed_mutex, key, dw_milliseconds);
}
static inline HRESULT DXGIReleaseSync(DXGIKeyedMutex keyed_mutex, UINT64 key)
{
   return keyed_mutex->lpVtbl->ReleaseSync(keyed_mutex, key);
}
static inline ULONG DXGIReleaseSurface(DXGISurface surface)
{
   return surface->lpVtbl->Release(surface);
}
static inline HRESULT DXGIMap(DXGISurface surface, DXGI_MAPPED_RECT* locked_rect, UINT map_flags)
{
   return surface->lpVtbl->Map(surface, locked_rect, map_flags);
}
static inline HRESULT DXGIUnmap(DXGISurface surface) { return surface->lpVtbl->Unmap(surface); }
static inline HRESULT DXGIGetDC(DXGISurface surface, BOOL discard, HDC* hdc)
{
   return surface->lpVtbl->GetDC(surface, discard, hdc);
}
static inline HRESULT DXGIReleaseDC(DXGISurface surface, RECT* dirty_rect)
{
   return surface->lpVtbl->ReleaseDC(surface, dirty_rect);
}
static inline ULONG DXGIReleaseOutput(DXGIOutput output) { return output->lpVtbl->Release(output); }
static inline HRESULT DXGIGetDisplayModeList(
      DXGIOutput output, DXGI_FORMAT enum_format, UINT flags, UINT* num_modes, DXGI_MODE_DESC* desc)
{
   return output->lpVtbl->GetDisplayModeList(output, enum_format, flags, num_modes, desc);
}
static inline HRESULT DXGIFindClosestMatchingMode(
      DXGIOutput      output,
      DXGI_MODE_DESC* mode_to_match,
      DXGI_MODE_DESC* closest_match,
      void*           concerned_device)
{
   return output->lpVtbl->FindClosestMatchingMode(
         output, mode_to_match, closest_match, (IUnknown*)concerned_device);
}
static inline HRESULT DXGIWaitForVBlank(DXGIOutput output)
{
   return output->lpVtbl->WaitForVBlank(output);
}
static inline HRESULT DXGITakeOwnership(DXGIOutput output, void* device, BOOL exclusive)
{
   return output->lpVtbl->TakeOwnership(output, (IUnknown*)device, exclusive);
}
static inline void DXGIReleaseOwnership(DXGIOutput output)
{
   output->lpVtbl->ReleaseOwnership(output);
}
static inline HRESULT
DXGIGetGammaControlCapabilities(DXGIOutput output, DXGI_GAMMA_CONTROL_CAPABILITIES* gamma_caps)
{
   return output->lpVtbl->GetGammaControlCapabilities(output, gamma_caps);
}
static inline HRESULT DXGISetGammaControl(DXGIOutput output, DXGI_GAMMA_CONTROL* array)
{
   return output->lpVtbl->SetGammaControl(output, array);
}
static inline HRESULT DXGIGetGammaControl(DXGIOutput output, DXGI_GAMMA_CONTROL* array)
{
   return output->lpVtbl->GetGammaControl(output, array);
}
static inline HRESULT DXGISetDisplaySurface(DXGIOutput output, DXGISurface scanout_surface)
{
   return output->lpVtbl->SetDisplaySurface(output, (IDXGISurface*)scanout_surface);
}
static inline HRESULT DXGIGetDisplaySurfaceData(DXGIOutput output, DXGISurface destination)
{
   return output->lpVtbl->GetDisplaySurfaceData(output, (IDXGISurface*)destination);
}
static inline ULONG DXGIReleaseDevice(DXGIDevice device) { return device->lpVtbl->Release(device); }
static inline HRESULT DXGICreateSurface(
      DXGIDevice            device,
      DXGI_SURFACE_DESC*    desc,
      UINT                  num_surfaces,
      DXGI_USAGE            usage,
      DXGI_SHARED_RESOURCE* shared_resource,
      DXGISurface*          surface)
{
   return device->lpVtbl->CreateSurface(
         device, desc, num_surfaces, usage, shared_resource, (IDXGISurface**)surface);
}
static inline HRESULT DXGISetGPUThreadPriority(DXGIDevice device, INT priority)
{
   return device->lpVtbl->SetGPUThreadPriority(device, priority);
}
static inline HRESULT DXGIGetGPUThreadPriority(DXGIDevice device, INT* priority)
{
   return device->lpVtbl->GetGPUThreadPriority(device, priority);
}
static inline ULONG DXGIReleaseFactory(DXGIFactory factory)
{
   return factory->lpVtbl->Release(factory);
}
static inline HRESULT DXGIMakeWindowAssociation(DXGIFactory factory, HWND window_handle, UINT flags)
{
   return factory->lpVtbl->MakeWindowAssociation(factory, window_handle, flags);
}
static inline HRESULT DXGIGetWindowAssociation(DXGIFactory factory, HWND* window_handle)
{
   return factory->lpVtbl->GetWindowAssociation(factory, window_handle);
}
static inline HRESULT DXGICreateSwapChain(
      DXGIFactory factory, void* device, DXGI_SWAP_CHAIN_DESC* desc, DXGISwapChain* swap_chain)
{
   return factory->lpVtbl->CreateSwapChain(
         factory, (IUnknown*)device, desc, (IDXGISwapChain**)swap_chain);
}
static inline HRESULT
DXGICreateSoftwareAdapter(DXGIFactory factory, HMODULE module, DXGIAdapter* adapter)
{
   return factory->lpVtbl->CreateSoftwareAdapter(factory, module, (IDXGIAdapter**)adapter);
}
static inline HRESULT DXGIEnumAdapters(DXGIFactory factory, UINT id, DXGIAdapter* adapter)
{
   return factory->lpVtbl->EnumAdapters1(factory, id, adapter);
}
static inline BOOL DXGIIsCurrent(DXGIFactory factory)
{
   return factory->lpVtbl->IsCurrent(factory);
}
static inline ULONG DXGIReleaseAdapter(DXGIAdapter adapter)
{
   return adapter->lpVtbl->Release(adapter);
}
static inline HRESULT DXGIEnumOutputs(DXGIAdapter adapter, UINT id, DXGIOutput* output)
{
   return adapter->lpVtbl->EnumOutputs(adapter, id, output);
}
static inline HRESULT
DXGICheckInterfaceSupport(DXGIAdapter adapter, REFGUID interface_name, LARGE_INTEGER* u_m_d_version)
{
   return adapter->lpVtbl->CheckInterfaceSupport(adapter, interface_name, u_m_d_version);
}
static inline HRESULT DXGIGetAdapterDesc1(DXGIAdapter adapter, DXGI_ADAPTER_DESC1* desc)
{
   return adapter->lpVtbl->GetDesc1(adapter, desc);
}
static inline ULONG DXGIReleaseDisplayControl(DXGIDisplayControl display_control)
{
   return display_control->lpVtbl->Release(display_control);
}
static inline BOOL DXGIIsStereoEnabled(DXGIDisplayControl display_control)
{
   return display_control->lpVtbl->IsStereoEnabled(display_control);
}
static inline void DXGISetStereoEnabled(DXGIDisplayControl display_control, BOOL enabled)
{
   display_control->lpVtbl->SetStereoEnabled(display_control, enabled);
}
static inline ULONG DXGIReleaseOutputDuplication(DXGIOutputDuplication output_duplication)
{
   return output_duplication->lpVtbl->Release(output_duplication);
}
static inline HRESULT DXGIAcquireNextFrame(
      DXGIOutputDuplication    output_duplication,
      UINT                     timeout_in_milliseconds,
      DXGI_OUTDUPL_FRAME_INFO* frame_info,
      void*                    desktop_resource)
{
   return output_duplication->lpVtbl->AcquireNextFrame(
         output_duplication, timeout_in_milliseconds, frame_info,
         (IDXGIResource**)desktop_resource);
}
static inline HRESULT DXGIGetFrameDirtyRects(
      DXGIOutputDuplication output_duplication,
      UINT                  dirty_rects_buffer_size,
      RECT*                 dirty_rects_buffer,
      UINT*                 dirty_rects_buffer_size_required)
{
   return output_duplication->lpVtbl->GetFrameDirtyRects(
         output_duplication, dirty_rects_buffer_size, dirty_rects_buffer,
         dirty_rects_buffer_size_required);
}
static inline HRESULT DXGIGetFrameMoveRects(
      DXGIOutputDuplication   output_duplication,
      UINT                    move_rects_buffer_size,
      DXGI_OUTDUPL_MOVE_RECT* move_rect_buffer,
      UINT*                   move_rects_buffer_size_required)
{
   return output_duplication->lpVtbl->GetFrameMoveRects(
         output_duplication, move_rects_buffer_size, move_rect_buffer,
         move_rects_buffer_size_required);
}
static inline HRESULT DXGIGetFramePointerShape(
      DXGIOutputDuplication            output_duplication,
      UINT                             pointer_shape_buffer_size,
      void*                            pointer_shape_buffer,
      UINT*                            pointer_shape_buffer_size_required,
      DXGI_OUTDUPL_POINTER_SHAPE_INFO* pointer_shape_info)
{
   return output_duplication->lpVtbl->GetFramePointerShape(
         output_duplication, pointer_shape_buffer_size, pointer_shape_buffer,
         pointer_shape_buffer_size_required, pointer_shape_info);
}
static inline HRESULT
DXGIMapDesktopSurface(DXGIOutputDuplication output_duplication, DXGI_MAPPED_RECT* locked_rect)
{
   return output_duplication->lpVtbl->MapDesktopSurface(output_duplication, locked_rect);
}
static inline HRESULT DXGIUnMapDesktopSurface(DXGIOutputDuplication output_duplication)
{
   return output_duplication->lpVtbl->UnMapDesktopSurface(output_duplication);
}
static inline HRESULT DXGIReleaseFrame(DXGIOutputDuplication output_duplication)
{
   return output_duplication->lpVtbl->ReleaseFrame(output_duplication);
}
static inline ULONG DXGIReleaseDecodeSwapChain(DXGIDecodeSwapChain decode_swap_chain)
{
   return decode_swap_chain->lpVtbl->Release(decode_swap_chain);
}
static inline HRESULT DXGIPresentBuffer(
      DXGIDecodeSwapChain decode_swap_chain, UINT buffer_to_present, UINT sync_interval, UINT flags)
{
   return decode_swap_chain->lpVtbl->PresentBuffer(
         decode_swap_chain, buffer_to_present, sync_interval, flags);
}
static inline HRESULT DXGISetSourceRect(DXGIDecodeSwapChain decode_swap_chain, RECT* rect)
{
   return decode_swap_chain->lpVtbl->SetSourceRect(decode_swap_chain, rect);
}
static inline HRESULT DXGISetTargetRect(DXGIDecodeSwapChain decode_swap_chain, RECT* rect)
{
   return decode_swap_chain->lpVtbl->SetTargetRect(decode_swap_chain, rect);
}
static inline HRESULT
DXGISetDestSize(DXGIDecodeSwapChain decode_swap_chain, UINT width, UINT height)
{
   return decode_swap_chain->lpVtbl->SetDestSize(decode_swap_chain, width, height);
}
static inline HRESULT DXGIGetSourceRect(DXGIDecodeSwapChain decode_swap_chain, RECT* rect)
{
   return decode_swap_chain->lpVtbl->GetSourceRect(decode_swap_chain, rect);
}
static inline HRESULT DXGIGetTargetRect(DXGIDecodeSwapChain decode_swap_chain, RECT* rect)
{
   return decode_swap_chain->lpVtbl->GetTargetRect(decode_swap_chain, rect);
}
static inline HRESULT
DXGIGetDestSize(DXGIDecodeSwapChain decode_swap_chain, UINT* width, UINT* height)
{
   return decode_swap_chain->lpVtbl->GetDestSize(decode_swap_chain, width, height);
}
static inline HRESULT DXGISetColorSpace(
      DXGIDecodeSwapChain decode_swap_chain, DXGI_MULTIPLANE_OVERLAY_YCbCr_FLAGS color_space)
{
   return decode_swap_chain->lpVtbl->SetColorSpace(decode_swap_chain, color_space);
}
static inline DXGI_MULTIPLANE_OVERLAY_YCbCr_FLAGS
DXGIGetColorSpace(DXGIDecodeSwapChain decode_swap_chain)
{
   return decode_swap_chain->lpVtbl->GetColorSpace(decode_swap_chain);
}
static inline ULONG DXGIReleaseFactoryMedia(DXGIFactoryMedia factory_media)
{
   return factory_media->lpVtbl->Release(factory_media);
}
static inline HRESULT DXGICreateSwapChainForCompositionSurfaceHandle(
      DXGIFactoryMedia       factory_media,
      void*                  device,
      HANDLE                 h_surface,
      DXGI_SWAP_CHAIN_DESC1* desc,
      DXGIOutput             restrict_to_output,
      DXGISwapChain*         swap_chain)
{
   return factory_media->lpVtbl->CreateSwapChainForCompositionSurfaceHandle(
         factory_media, (IUnknown*)device, h_surface, desc, restrict_to_output,
         (IDXGISwapChain1**)swap_chain);
}
static inline HRESULT DXGICreateDecodeSwapChainForCompositionSurfaceHandle(
      DXGIFactoryMedia             factory_media,
      void*                        device,
      HANDLE                       h_surface,
      DXGI_DECODE_SWAP_CHAIN_DESC* desc,
      void*                        yuv_decode_buffers,
      DXGIOutput                   restrict_to_output,
      DXGIDecodeSwapChain*         swap_chain)
{
   return factory_media->lpVtbl->CreateDecodeSwapChainForCompositionSurfaceHandle(
         factory_media, (IUnknown*)device, h_surface, desc, (IDXGIResource*)yuv_decode_buffers,
         restrict_to_output, swap_chain);
}
static inline ULONG DXGIReleaseSwapChainMedia(DXGISwapChainMedia swap_chain_media)
{
   return swap_chain_media->lpVtbl->Release(swap_chain_media);
}
static inline HRESULT
DXGIGetFrameStatisticsMedia(DXGISwapChainMedia swap_chain_media, DXGI_FRAME_STATISTICS_MEDIA* stats)
{
   return swap_chain_media->lpVtbl->GetFrameStatisticsMedia(swap_chain_media, stats);
}
static inline HRESULT DXGISetPresentDuration(DXGISwapChainMedia swap_chain_media, UINT duration)
{
   return swap_chain_media->lpVtbl->SetPresentDuration(swap_chain_media, duration);
}
static inline HRESULT DXGICheckPresentDurationSupport(
      DXGISwapChainMedia swap_chain_media,
      UINT               desired_present_duration,
      UINT*              closest_smaller_present_duration,
      UINT*              closest_larger_present_duration)
{
   return swap_chain_media->lpVtbl->CheckPresentDurationSupport(
         swap_chain_media, desired_present_duration, closest_smaller_present_duration,
         closest_larger_present_duration);
}
static inline ULONG DXGIReleaseSwapChain(DXGISwapChain swap_chain)
{
   return swap_chain->lpVtbl->Release(swap_chain);
}
static inline HRESULT DXGIPresent(DXGISwapChain swap_chain, UINT sync_interval, UINT flags)
{
   return swap_chain->lpVtbl->Present(swap_chain, sync_interval, flags);
}
static inline HRESULT DXGIGetBuffer(DXGISwapChain swap_chain, UINT buffer, IDXGISurface** out)
{
   return swap_chain->lpVtbl->GetBuffer(swap_chain, buffer, __uuidof(IDXGISurface), (void**)out);
}
static inline HRESULT
DXGISetFullscreenState(DXGISwapChain swap_chain, BOOL fullscreen, DXGIOutput target)
{
   return swap_chain->lpVtbl->SetFullscreenState(swap_chain, fullscreen, target);
}
static inline HRESULT
DXGIGetFullscreenState(DXGISwapChain swap_chain, BOOL* fullscreen, DXGIOutput* target)
{
   return swap_chain->lpVtbl->GetFullscreenState(swap_chain, fullscreen, target);
}
static inline HRESULT DXGIResizeBuffers(
      DXGISwapChain swap_chain,
      UINT          buffer_count,
      UINT          width,
      UINT          height,
      DXGI_FORMAT   new_format,
      UINT          swap_chain_flags)
{
   return swap_chain->lpVtbl->ResizeBuffers(
         swap_chain, buffer_count, width, height, new_format, swap_chain_flags);
}
static inline HRESULT
DXGIResizeTarget(DXGISwapChain swap_chain, DXGI_MODE_DESC* new_target_parameters)
{
   return swap_chain->lpVtbl->ResizeTarget(swap_chain, new_target_parameters);
}
static inline HRESULT DXGIGetContainingOutput(DXGISwapChain swap_chain, DXGIOutput* output)
{
   return swap_chain->lpVtbl->GetContainingOutput(swap_chain, output);
}
static inline HRESULT DXGIGetFrameStatistics(DXGISwapChain swap_chain, DXGI_FRAME_STATISTICS* stats)
{
   return swap_chain->lpVtbl->GetFrameStatistics(swap_chain, stats);
}
static inline HRESULT DXGIGetLastPresentCount(DXGISwapChain swap_chain, UINT* last_present_count)
{
   return swap_chain->lpVtbl->GetLastPresentCount(swap_chain, last_present_count);
}
static inline HRESULT DXGIGetSwapChainDesc1(DXGISwapChain swap_chain, DXGI_SWAP_CHAIN_DESC1* desc)
{
   return swap_chain->lpVtbl->GetDesc1(swap_chain, desc);
}
static inline HRESULT
DXGIGetFullscreenDesc(DXGISwapChain swap_chain, DXGI_SWAP_CHAIN_FULLSCREEN_DESC* desc)
{
   return swap_chain->lpVtbl->GetFullscreenDesc(swap_chain, desc);
}
static inline HRESULT DXGIGetHwnd(DXGISwapChain swap_chain, HWND* hwnd)
{
   return swap_chain->lpVtbl->GetHwnd(swap_chain, hwnd);
}
static inline HRESULT DXGIPresent1(
      DXGISwapChain            swap_chain,
      UINT                     sync_interval,
      UINT                     present_flags,
      DXGI_PRESENT_PARAMETERS* present_parameters)
{
   return swap_chain->lpVtbl->Present1(
         swap_chain, sync_interval, present_flags, present_parameters);
}
static inline BOOL DXGIIsTemporaryMonoSupported(DXGISwapChain swap_chain)
{
   return swap_chain->lpVtbl->IsTemporaryMonoSupported(swap_chain);
}
static inline HRESULT
DXGIGetRestrictToOutput(DXGISwapChain swap_chain, DXGIOutput* restrict_to_output)
{
   return swap_chain->lpVtbl->GetRestrictToOutput(swap_chain, restrict_to_output);
}
static inline HRESULT DXGISetBackgroundColor(DXGISwapChain swap_chain, DXGI_RGBA* color)
{
   return swap_chain->lpVtbl->SetBackgroundColor(swap_chain, color);
}
static inline HRESULT DXGIGetBackgroundColor(DXGISwapChain swap_chain, DXGI_RGBA* color)
{
   return swap_chain->lpVtbl->GetBackgroundColor(swap_chain, color);
}
static inline HRESULT DXGISetRotation(DXGISwapChain swap_chain, DXGI_MODE_ROTATION rotation)
{
   return swap_chain->lpVtbl->SetRotation(swap_chain, rotation);
}
static inline HRESULT DXGIGetRotation(DXGISwapChain swap_chain, DXGI_MODE_ROTATION* rotation)
{
   return swap_chain->lpVtbl->GetRotation(swap_chain, rotation);
}
static inline HRESULT DXGISetSourceSize(DXGISwapChain swap_chain, UINT width, UINT height)
{
   return swap_chain->lpVtbl->SetSourceSize(swap_chain, width, height);
}
static inline HRESULT DXGIGetSourceSize(DXGISwapChain swap_chain, UINT* width, UINT* height)
{
   return swap_chain->lpVtbl->GetSourceSize(swap_chain, width, height);
}
static inline HRESULT DXGISetMaximumFrameLatency(DXGISwapChain swap_chain, UINT max_latency)
{
   return swap_chain->lpVtbl->SetMaximumFrameLatency(swap_chain, max_latency);
}
static inline HRESULT DXGIGetMaximumFrameLatency(DXGISwapChain swap_chain, UINT* max_latency)
{
   return swap_chain->lpVtbl->GetMaximumFrameLatency(swap_chain, max_latency);
}
static inline HANDLE DXGIGetFrameLatencyWaitableObject(DXGISwapChain swap_chain)
{
   return swap_chain->lpVtbl->GetFrameLatencyWaitableObject(swap_chain);
}
static inline HRESULT DXGISetMatrixTransform(DXGISwapChain swap_chain, DXGI_MATRIX_3X2_F* matrix)
{
   return swap_chain->lpVtbl->SetMatrixTransform(swap_chain, matrix);
}
static inline HRESULT DXGIGetMatrixTransform(DXGISwapChain swap_chain, DXGI_MATRIX_3X2_F* matrix)
{
   return swap_chain->lpVtbl->GetMatrixTransform(swap_chain, matrix);
}
static inline UINT DXGIGetCurrentBackBufferIndex(DXGISwapChain swap_chain)
{
   return swap_chain->lpVtbl->GetCurrentBackBufferIndex(swap_chain);
}
static inline HRESULT DXGICheckColorSpaceSupport(
      DXGISwapChain swap_chain, DXGI_COLOR_SPACE_TYPE color_space, UINT* color_space_support)
{
   return swap_chain->lpVtbl->CheckColorSpaceSupport(swap_chain, color_space, color_space_support);
}
static inline HRESULT
DXGISetColorSpace1(DXGISwapChain swap_chain, DXGI_COLOR_SPACE_TYPE color_space)
{
   return swap_chain->lpVtbl->SetColorSpace1(swap_chain, color_space);
}

/* end of auto-generated */

static inline HRESULT DXGICreateFactory(DXGIFactory* factory)
{
   return CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)factory);
}

/* internal */

#define DXGI_COLOR_RGBA(r, g, b, a) (((UINT32)(a) << 24) | ((UINT32)(b) << 16) | ((UINT32)(g) << 8) | ((UINT32)(r) << 0))

typedef enum {
   DXGI_FORMAT_EX_A4R4G4B4_UNORM = 1000,
} DXGI_FORMAT_EX;

DXGI_FORMAT* dxgi_get_format_fallback_list(DXGI_FORMAT format);

void dxgi_copy(
      int         width,
      int         height,
      DXGI_FORMAT src_format,
      int         src_pitch,
      const void* src_data,
      DXGI_FORMAT dst_format,
      int         dst_pitch,
      void*       dst_data);

#if 1
#include <performance_counters.h>
#ifndef PERF_START
#define PERF_START() \
   static struct retro_perf_counter perfcounter = { __FUNCTION__ }; \
   LARGE_INTEGER                    start, stop; \
   rarch_perf_register(&perfcounter); \
   perfcounter.call_cnt++; \
   QueryPerformanceCounter(&start)

#define PERF_STOP() \
   QueryPerformanceCounter(&stop); \
   perfcounter.total += stop.QuadPart - start.QuadPart
#endif
#else
#define PERF_START()
#define PERF_STOP()
#endif
