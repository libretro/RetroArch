#pragma once

#include <retro_inline.h>

#ifdef __MINGW32__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
/* Pointer parameters */
#define _In_
#define _Out_
#define _Inout_
#define _In_z_
#define _Inout_z_
#define _In_reads_(s)
#define _In_reads_bytes_(s)
#define _In_reads_z_(s)
#define _In_reads_or_z_(s)
#define _Out_writes_(s)
#define _Out_writes_bytes_(s)
#define _Out_writes_z_(s)
#define _Inout_updates_(s)
#define _Inout_updates_bytes_(s)
#define _Inout_updates_z_(s)
#define _Out_writes_to_(s,c)
#define _Out_writes_bytes_to_(s, c)
#define _Out_writes_all_(s)
#define _Out_writes_bytes_all_(s)
#define _Inout_updates_to_(s, c)
#define _Inout_updates_bytes_to_(s, c)
#define _Inout_updates_all_(s)
#define _Inout_updates_bytes_all_(s)
#define _In_reads_to_ptr_(p)
#define _In_reads_to_ptr_z_(p)
#define _Out_writes_to_ptr_(p)
#define _Out_writes_to_ptr_z(p)

/* Optional pointer parameters */
#define __in_opt
#define __out_opt
#define __inout_opt
#define _In_opt_
#define _Out_opt_
#define _Inout_opt_
#define _In_opt_z_
#define _Inout_opt_z_
#define _In_reads_opt_(s)
#define _In_reads_bytes_opt_(s)
#define _In_reads_opt_z_(s)

#define _Out_writes_opt_(s)
#define _Out_writes_opt_z_(s)
#define _Inout_updates_opt_(s)
#define _Inout_updates_bytes_opt_(s)
#define _Inout_updates_opt_z_(s)
#define _Out_writes_to_opt_(s, c)
#define _Out_writes_bytes_to_opt_(s, c)
#define _Out_writes_all_opt_(s)
#define _Out_writes_bytes_all_opt_(s)

#define _Inout_updates_to_opt_(s, c)
#define _Inout_updates_bytes_to_opt_(s, c)
#define _Inout_updates_all_opt_(s)
#define _Inout_updates_bytes_all_opt_(s)
#define _In_reads_to_ptr_opt_(p)
#define _In_reads_to_ptr_opt_z_(p)
#define _Out_writes_to_ptr_opt_(p)
#define _Out_writes_to_ptr_opt_z_(p)

/* Output pointer parameters */
#define _Outptr_
#define _Outptr_opt_
#define _Outptr_result_maybenull_
#define _Outptr_opt_result_maybenull_
#define _Outptr_result_z_
#define _Outptr_opt_result_z_
#define _Outptr_result_maybenull_z_
#define _Outptr_opt_result_maybenull_z_
#define _COM_Outptr_
#define _COM_Outptr_opt_
#define _COM_Outptr_result_maybenull_
#define _COM_Outptr_opt_result_maybenull_
#define _Outptr_result_buffer_(s)
#define _Outptr_result_bytebuffer_(s)
#define _Outptr_opt_result_buffer_(s)
#define _Outptr_opt_result_bytebuffer_(s)
#define _Outptr_result_buffer_to_(s, c)
#define _Outptr_result_bytebuffer_to_(s, c)
#define _Outptr_result_bytebuffer_maybenull_(s)
#define _Outptr_opt_result_buffer_to_(s, c)
#define _Outptr_opt_result_bytebuffer_to_(s, c)
#define _Result_nullonfailure_
#define _Result_zeroonfailure_
#define _Outptr_result_nullonfailure_
#define _Outptr_opt_result_nullonfailure_
#define _Outref_result_nullonfailure_

/* Output reference parameters */
#define _Outref_
#define _Outref_result_maybenull_
#define _Outref_result_buffer_(s)
#define _Outref_result_bytebuffer_(s)
#define _Outref_result_buffer_to_(s, c)
#define _Outref_result_bytebuffer_to_(s, c)
#define _Outref_result_buffer_all_(s)
#define _Outref_result_bytebuffer_all_(s)
#define _Outref_result_buffer_maybenull_(s)
#define _Outref_result_bytebuffer_maybenull_(s)
#define _Outref_result_buffer_to_maybenull_(s, c)
#define _Outref_result_bytebuffer_to_maybenull_(s, c)
#define _Outref_result_buffer_all_maybenull_(s)
#define _Outref_result_bytebuffer_all_maybenull_(s)

/* Return values */
#define _Ret_z_
#define _Ret_writes_(s)
#define _Ret_writes_bytes_(s)
#define _Ret_writes_z_(s)
#define _Ret_writes_bytes_to_(s, c)
#define _Ret_writes_maybenull_(s)
#define _Ret_writes_to_maybenull_(s, c)
#define _Ret_writes_maybenull_z_(s)
#define _Ret_maybenull_
#define _Ret_maybenull_z_
#define _Ret_null_
#define _Ret_notnull_
#define _Ret_writes_bytes_to_(s, c)
#define _Ret_writes_bytes_maybenull_(s)
#define _Ret_writes_bytes_to_maybenull_(s, c)

/* Other common annotations */
#define _In_range_(low, hi)
#define _Out_range_(low, hi)
#define _Ret_range_(low, hi)
#define _Deref_in_range_(low, hi)
#define _Deref_out_range_(low, hi)
#define _Deref_inout_range_(low, hi)
#define _Pre_equal_to_(expr)
#define _Post_equal_to_(expr)
#define _Struct_size_bytes_(size)

/* Function annotations */
#define _Called_from_function_class_(name)
#define _Check_return_ __checkReturn
#define _Function_class_(name)
#define _Raises_SEH_exception_
#define _Maybe_raises_SEH_exception_
#define _Must_inspect_result_
#define _Use_decl_annotations_

/* Success/failure annotations */
#define _Always_(anno_list)
#define _On_failure_(anno_list)
#define _Return_type_success_(expr)
#define _Success_(expr)

#define _Reserved_
#define _Const_

/* Buffer properties */
#define _Readable_bytes_(s)
#define _Readable_elements_(s)
#define _Writable_bytes_(s)
#define _Writable_elements_(s)
#define _Null_terminated_
#define _NullNull_terminated_
#define _Pre_readable_size_(s)
#define _Pre_writable_size_(s)
#define _Pre_readable_byte_size_(s)
#define _Pre_writable_byte_size_(s)
#define _Post_readable_size_(s)
#define _Post_writable_size_(s)
#define _Post_readable_byte_size_(s)
#define _Post_writable_byte_size_(s)

/* Field properties */
#define _Field_size_(s)
#define _Field_size_full_(s)
#define _Field_size_full_opt_(s)
#define _Field_size_opt_(s)
#define _Field_size_part_(s, c)
#define _Field_size_part_opt_(s, c)
#define _Field_size_bytes_(size)
#define _Field_size_bytes_full_(size)
#define _Field_size_bytes_full_opt_(s)
#define _Field_size_bytes_opt_(s)
#define _Field_size_bytes_part_(s, c)
#define _Field_size_bytes_part_opt_(s, c)
#define _Field_z_
#define _Field_range_(min, max)

/* Structural annotations */
#define _At_(e, a)
#define _At_buffer_(e, i, c, a)
#define _Group_(a)
#define _When_(e, a)

/* printf/scanf annotations */
#define _Printf_format_string_
#define _Scanf_format_string_
#define _Scanf_s_format_string_
#define _Format_string_impl_(kind,where)
#define _Printf_format_string_params_(x)
#define _Scanf_format_string_params_(x)
#define _Scanf_s_format_string_params_(x)

/* Analysis */
#define _Analysis_assume_(expr)
#define _Analysis_assume_nullterminated_(expr)

#define __in
#define __out

#define __in_bcount(size)
#define __in_ecount(size)
#define __out_bcount(size)
#define __out_bcount_part(size, length)
#define __out_ecount(size)
#define __inout
#define __deref_out_ecount(size)
#define __in_ecount_opt(s)

#define _In_
#define _In_opt_
#define _Null_

#define _Out_writes_bytes_opt_(s)
#define _Out_writes_bytes_(s)
#define _In_reads_bytes_(s)
#define _Inout_opt_bytecount_(s)

#ifndef __cplusplus
#define static_assert _Static_assert
#endif
#endif

#include <assert.h>
#include <dxgi1_5.h>

#ifndef countof
#define countof(a) (sizeof(a) / sizeof(*a))
#endif

#ifndef uuidof
#if defined(__cplusplus)
#define uuidof(type) IID_##type
#else
#define uuidof(type) &IID_##type
#endif
#endif

#if !defined(__cplusplus) || defined(CINTERFACE)
#ifndef COM_RELEASE_DECLARED
#define COM_RELEASE_DECLARED
static INLINE ULONG Release(void* object)
{
   if (object)
      return ((IUnknown*)object)->lpVtbl->Release((IUnknown*)object);

   return 0;
}
#endif
#endif

#if !defined(__cplusplus) || defined(CINTERFACE)
#ifndef COM_ADDREF_DECLARED
#define COM_ADDREF_DECLARED
static INLINE ULONG AddRef(void* object)
{
   if (object)
      return ((IUnknown*)object)->lpVtbl->AddRef((IUnknown*)object);

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
#ifdef __WINRT__
typedef IDXGIFactory2*          DXGIFactory2;
#endif
typedef IDXGIAdapter1*          DXGIAdapter;
typedef IDXGIDisplayControl*    DXGIDisplayControl;
typedef IDXGIOutputDuplication* DXGIOutputDuplication;
typedef IDXGIDecodeSwapChain*   DXGIDecodeSwapChain;
typedef IDXGIFactoryMedia*      DXGIFactoryMedia;
typedef IDXGISwapChainMedia*    DXGISwapChainMedia;
typedef IDXGISwapChain3*        DXGISwapChain;

#if !defined(__cplusplus) || defined(CINTERFACE)
static INLINE ULONG DXGIReleaseDeviceSubObject(DXGIDeviceSubObject device_sub_object)
{
   return device_sub_object->lpVtbl->Release(device_sub_object);
}
static INLINE HRESULT DXGIGetSharedHandle(void* resource, HANDLE* shared_handle)
{
   return ((IDXGIResource*)resource)->lpVtbl->GetSharedHandle((IDXGIResource*)resource, shared_handle);
}
static INLINE HRESULT DXGIGetUsage(void* resource, DXGI_USAGE* usage)
{
   return ((IDXGIResource*)resource)->lpVtbl->GetUsage((IDXGIResource*)resource, usage);
}
static INLINE HRESULT DXGISetEvictionPriority(void* resource, UINT eviction_priority)
{
   return ((IDXGIResource*)resource)->lpVtbl->SetEvictionPriority((IDXGIResource*)resource, eviction_priority);
}
static INLINE HRESULT DXGIGetEvictionPriority(void* resource, UINT* eviction_priority)
{
   return ((IDXGIResource*)resource)->lpVtbl->GetEvictionPriority((IDXGIResource*)resource, eviction_priority);
}
static INLINE ULONG DXGIReleaseKeyedMutex(DXGIKeyedMutex keyed_mutex)
{
   return keyed_mutex->lpVtbl->Release(keyed_mutex);
}
static INLINE HRESULT DXGIAcquireSync(DXGIKeyedMutex keyed_mutex, UINT64 key, DWORD dw_milliseconds)
{
   return keyed_mutex->lpVtbl->AcquireSync(keyed_mutex, key, dw_milliseconds);
}
static INLINE HRESULT DXGIReleaseSync(DXGIKeyedMutex keyed_mutex, UINT64 key)
{
   return keyed_mutex->lpVtbl->ReleaseSync(keyed_mutex, key);
}
static INLINE ULONG DXGIReleaseSurface(DXGISurface surface)
{
   return surface->lpVtbl->Release(surface);
}
static INLINE HRESULT DXGIMap(DXGISurface surface, DXGI_MAPPED_RECT* locked_rect, UINT map_flags)
{
   return surface->lpVtbl->Map(surface, locked_rect, map_flags);
}
static INLINE HRESULT DXGIUnmap(DXGISurface surface) { return surface->lpVtbl->Unmap(surface); }
static INLINE HRESULT DXGIGetDC(DXGISurface surface, BOOL discard, HDC* hdc)
{
   return surface->lpVtbl->GetDC(surface, discard, hdc);
}
static INLINE HRESULT DXGIReleaseDC(DXGISurface surface, RECT* dirty_rect)
{
   return surface->lpVtbl->ReleaseDC(surface, dirty_rect);
}
static INLINE ULONG DXGIReleaseOutput(DXGIOutput output) { return output->lpVtbl->Release(output); }
static INLINE HRESULT DXGIGetDisplayModeList(
      DXGIOutput output, DXGI_FORMAT enum_format, UINT flags, UINT* num_modes, DXGI_MODE_DESC* desc)
{
   return output->lpVtbl->GetDisplayModeList(output, enum_format, flags, num_modes, desc);
}
static INLINE HRESULT DXGIFindClosestMatchingMode(
      DXGIOutput      output,
      DXGI_MODE_DESC* mode_to_match,
      DXGI_MODE_DESC* closest_match,
      void*           concerned_device)
{
   return output->lpVtbl->FindClosestMatchingMode(
         output, mode_to_match, closest_match, (IUnknown*)concerned_device);
}
static INLINE HRESULT DXGIWaitForVBlank(DXGIOutput output)
{
   return output->lpVtbl->WaitForVBlank(output);
}
static INLINE HRESULT DXGITakeOwnership(DXGIOutput output, void* device, BOOL exclusive)
{
   return output->lpVtbl->TakeOwnership(output, (IUnknown*)device, exclusive);
}
static INLINE void DXGIReleaseOwnership(DXGIOutput output)
{
   output->lpVtbl->ReleaseOwnership(output);
}
static INLINE HRESULT
DXGIGetGammaControlCapabilities(DXGIOutput output, DXGI_GAMMA_CONTROL_CAPABILITIES* gamma_caps)
{
   return output->lpVtbl->GetGammaControlCapabilities(output, gamma_caps);
}
static INLINE HRESULT DXGISetGammaControl(DXGIOutput output, DXGI_GAMMA_CONTROL* array)
{
   return output->lpVtbl->SetGammaControl(output, array);
}
static INLINE HRESULT DXGIGetGammaControl(DXGIOutput output, DXGI_GAMMA_CONTROL* array)
{
   return output->lpVtbl->GetGammaControl(output, array);
}
static INLINE HRESULT DXGISetDisplaySurface(DXGIOutput output, DXGISurface scanout_surface)
{
   return output->lpVtbl->SetDisplaySurface(output, (IDXGISurface*)scanout_surface);
}
static INLINE HRESULT DXGIGetDisplaySurfaceData(DXGIOutput output, DXGISurface destination)
{
   return output->lpVtbl->GetDisplaySurfaceData(output, (IDXGISurface*)destination);
}
static INLINE ULONG DXGIReleaseDevice(DXGIDevice device) { return device->lpVtbl->Release(device); }
static INLINE HRESULT DXGICreateSurface(
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
static INLINE HRESULT DXGISetGPUThreadPriority(DXGIDevice device, INT priority)
{
   return device->lpVtbl->SetGPUThreadPriority(device, priority);
}
static INLINE HRESULT DXGIGetGPUThreadPriority(DXGIDevice device, INT* priority)
{
   return device->lpVtbl->GetGPUThreadPriority(device, priority);
}
static INLINE ULONG DXGIReleaseFactory(DXGIFactory factory)
{
   return factory->lpVtbl->Release(factory);
}
static INLINE HRESULT DXGIMakeWindowAssociation(DXGIFactory factory, HWND window_handle, UINT flags)
{
   return factory->lpVtbl->MakeWindowAssociation(factory, window_handle, flags);
}
static INLINE HRESULT DXGIGetWindowAssociation(DXGIFactory factory, HWND* window_handle)
{
   return factory->lpVtbl->GetWindowAssociation(factory, window_handle);
}
static INLINE HRESULT DXGICreateSwapChain(
      DXGIFactory factory, void* device, DXGI_SWAP_CHAIN_DESC* desc, DXGISwapChain* swap_chain)
{
   return factory->lpVtbl->CreateSwapChain(
         factory, (IUnknown*)device, desc, (IDXGISwapChain**)swap_chain);
}
#ifdef __WINRT__
static INLINE HRESULT DXGICreateSwapChainForCoreWindow(
      DXGIFactory2 factory, void* device, void* corewindow, DXGI_SWAP_CHAIN_DESC1* desc, DXGIOutput restrict_to, DXGISwapChain* swap_chain)
{
   return factory->lpVtbl->CreateSwapChainForCoreWindow(
         factory, (IUnknown*)device, (IUnknown*)corewindow, desc, restrict_to, (IDXGISwapChain1**)swap_chain);
}
#endif
static INLINE HRESULT
DXGICreateSoftwareAdapter(DXGIFactory factory, HMODULE module, DXGIAdapter* adapter)
{
   return factory->lpVtbl->CreateSoftwareAdapter(factory, module, (IDXGIAdapter**)adapter);
}
static INLINE HRESULT DXGIEnumAdapters(DXGIFactory factory, UINT id, DXGIAdapter* adapter)
{
   return factory->lpVtbl->EnumAdapters1(factory, id, adapter);
}
#ifdef __WINRT__
static INLINE HRESULT DXGIEnumAdapters2(DXGIFactory2 factory, UINT id, DXGIAdapter* adapter)
{
   return factory->lpVtbl->EnumAdapters1(factory, id, adapter);
}
#endif
static INLINE BOOL DXGIIsCurrent(DXGIFactory factory)
{
   return factory->lpVtbl->IsCurrent(factory);
}
static INLINE ULONG DXGIReleaseAdapter(DXGIAdapter adapter)
{
   return adapter->lpVtbl->Release(adapter);
}
static INLINE HRESULT DXGIEnumOutputs(DXGIAdapter adapter, UINT id, DXGIOutput* output)
{
   return adapter->lpVtbl->EnumOutputs(adapter, id, output);
}
static INLINE HRESULT
DXGICheckInterfaceSupport(DXGIAdapter adapter, REFGUID interface_name, LARGE_INTEGER* u_m_d_version)
{
   return adapter->lpVtbl->CheckInterfaceSupport(adapter, interface_name, u_m_d_version);
}
static INLINE HRESULT DXGIGetAdapterDesc1(DXGIAdapter adapter, DXGI_ADAPTER_DESC1* desc)
{
   return adapter->lpVtbl->GetDesc1(adapter, desc);
}
#ifndef __WINRT__
static INLINE ULONG DXGIReleaseDisplayControl(DXGIDisplayControl display_control)
{
   return display_control->lpVtbl->Release(display_control);
}
static INLINE BOOL DXGIIsStereoEnabled(DXGIDisplayControl display_control)
{
   return display_control->lpVtbl->IsStereoEnabled(display_control);
}
static INLINE void DXGISetStereoEnabled(DXGIDisplayControl display_control, BOOL enabled)
{
   display_control->lpVtbl->SetStereoEnabled(display_control, enabled);
}
static INLINE ULONG DXGIReleaseOutputDuplication(DXGIOutputDuplication output_duplication)
{
   return output_duplication->lpVtbl->Release(output_duplication);
}
static INLINE HRESULT DXGIAcquireNextFrame(
      DXGIOutputDuplication    output_duplication,
      UINT                     timeout_in_milliseconds,
      DXGI_OUTDUPL_FRAME_INFO* frame_info,
      void*                    desktop_resource)
{
   return output_duplication->lpVtbl->AcquireNextFrame(
         output_duplication, timeout_in_milliseconds, frame_info,
         (IDXGIResource**)desktop_resource);
}
static INLINE HRESULT DXGIGetFrameDirtyRects(
      DXGIOutputDuplication output_duplication,
      UINT                  dirty_rects_buffer_size,
      RECT*                 dirty_rects_buffer,
      UINT*                 dirty_rects_buffer_size_required)
{
   return output_duplication->lpVtbl->GetFrameDirtyRects(
         output_duplication, dirty_rects_buffer_size, dirty_rects_buffer,
         dirty_rects_buffer_size_required);
}
static INLINE HRESULT DXGIGetFrameMoveRects(
      DXGIOutputDuplication   output_duplication,
      UINT                    move_rects_buffer_size,
      DXGI_OUTDUPL_MOVE_RECT* move_rect_buffer,
      UINT*                   move_rects_buffer_size_required)
{
   return output_duplication->lpVtbl->GetFrameMoveRects(
         output_duplication, move_rects_buffer_size, move_rect_buffer,
         move_rects_buffer_size_required);
}
static INLINE HRESULT DXGIGetFramePointerShape(
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
static INLINE HRESULT
DXGIMapDesktopSurface(DXGIOutputDuplication output_duplication, DXGI_MAPPED_RECT* locked_rect)
{
   return output_duplication->lpVtbl->MapDesktopSurface(output_duplication, locked_rect);
}
static INLINE HRESULT DXGIUnMapDesktopSurface(DXGIOutputDuplication output_duplication)
{
   return output_duplication->lpVtbl->UnMapDesktopSurface(output_duplication);
}
static INLINE HRESULT DXGIReleaseFrame(DXGIOutputDuplication output_duplication)
{
   return output_duplication->lpVtbl->ReleaseFrame(output_duplication);
}
static INLINE ULONG DXGIReleaseDecodeSwapChain(DXGIDecodeSwapChain decode_swap_chain)
{
   return decode_swap_chain->lpVtbl->Release(decode_swap_chain);
}
static INLINE HRESULT DXGIPresentBuffer(
      DXGIDecodeSwapChain decode_swap_chain, UINT buffer_to_present, UINT sync_interval, UINT flags)
{
   return decode_swap_chain->lpVtbl->PresentBuffer(
         decode_swap_chain, buffer_to_present, sync_interval, flags);
}
static INLINE HRESULT DXGISetSourceRect(DXGIDecodeSwapChain decode_swap_chain, RECT* rect)
{
   return decode_swap_chain->lpVtbl->SetSourceRect(decode_swap_chain, rect);
}
static INLINE HRESULT DXGISetTargetRect(DXGIDecodeSwapChain decode_swap_chain, RECT* rect)
{
   return decode_swap_chain->lpVtbl->SetTargetRect(decode_swap_chain, rect);
}
static INLINE HRESULT
DXGISetDestSize(DXGIDecodeSwapChain decode_swap_chain, UINT width, UINT height)
{
   return decode_swap_chain->lpVtbl->SetDestSize(decode_swap_chain, width, height);
}
static INLINE HRESULT DXGIGetSourceRect(DXGIDecodeSwapChain decode_swap_chain, RECT* rect)
{
   return decode_swap_chain->lpVtbl->GetSourceRect(decode_swap_chain, rect);
}
static INLINE HRESULT DXGIGetTargetRect(DXGIDecodeSwapChain decode_swap_chain, RECT* rect)
{
   return decode_swap_chain->lpVtbl->GetTargetRect(decode_swap_chain, rect);
}
static INLINE HRESULT
DXGIGetDestSize(DXGIDecodeSwapChain decode_swap_chain, UINT* width, UINT* height)
{
   return decode_swap_chain->lpVtbl->GetDestSize(decode_swap_chain, width, height);
}
static INLINE HRESULT DXGISetColorSpace(
      DXGIDecodeSwapChain decode_swap_chain, DXGI_MULTIPLANE_OVERLAY_YCbCr_FLAGS color_space)
{
   return decode_swap_chain->lpVtbl->SetColorSpace(decode_swap_chain, color_space);
}
static INLINE DXGI_MULTIPLANE_OVERLAY_YCbCr_FLAGS
DXGIGetColorSpace(DXGIDecodeSwapChain decode_swap_chain)
{
   return decode_swap_chain->lpVtbl->GetColorSpace(decode_swap_chain);
}
static INLINE ULONG DXGIReleaseFactoryMedia(DXGIFactoryMedia factory_media)
{
   return factory_media->lpVtbl->Release(factory_media);
}
static INLINE HRESULT DXGICreateSwapChainForCompositionSurfaceHandle(
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
static INLINE HRESULT DXGICreateDecodeSwapChainForCompositionSurfaceHandle(
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
static INLINE ULONG DXGIReleaseSwapChainMedia(DXGISwapChainMedia swap_chain_media)
{
   return swap_chain_media->lpVtbl->Release(swap_chain_media);
}
static INLINE HRESULT
DXGIGetFrameStatisticsMedia(DXGISwapChainMedia swap_chain_media, DXGI_FRAME_STATISTICS_MEDIA* stats)
{
   return swap_chain_media->lpVtbl->GetFrameStatisticsMedia(swap_chain_media, stats);
}
static INLINE HRESULT DXGISetPresentDuration(DXGISwapChainMedia swap_chain_media, UINT duration)
{
   return swap_chain_media->lpVtbl->SetPresentDuration(swap_chain_media, duration);
}
static INLINE HRESULT DXGICheckPresentDurationSupport(
      DXGISwapChainMedia swap_chain_media,
      UINT               desired_present_duration,
      UINT*              closest_smaller_present_duration,
      UINT*              closest_larger_present_duration)
{
   return swap_chain_media->lpVtbl->CheckPresentDurationSupport(
         swap_chain_media, desired_present_duration, closest_smaller_present_duration,
         closest_larger_present_duration);
}
#endif
static INLINE ULONG DXGIReleaseSwapChain(DXGISwapChain swap_chain)
{
   return swap_chain->lpVtbl->Release(swap_chain);
}
static INLINE HRESULT DXGIPresent(DXGISwapChain swap_chain, UINT sync_interval, UINT flags)
{
   return swap_chain->lpVtbl->Present(swap_chain, sync_interval, flags);
}
static INLINE HRESULT DXGIGetBuffer(DXGISwapChain swap_chain, UINT buffer, IDXGISurface** out)
{
   return swap_chain->lpVtbl->GetBuffer(swap_chain, buffer, uuidof(IDXGISurface), (void**)out);
}
static INLINE HRESULT
DXGISetFullscreenState(DXGISwapChain swap_chain, BOOL fullscreen, DXGIOutput target)
{
   return swap_chain->lpVtbl->SetFullscreenState(swap_chain, fullscreen, target);
}
static INLINE HRESULT
DXGIGetFullscreenState(DXGISwapChain swap_chain, BOOL* fullscreen, DXGIOutput* target)
{
   return swap_chain->lpVtbl->GetFullscreenState(swap_chain, fullscreen, target);
}
static INLINE HRESULT DXGIResizeBuffers(
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
static INLINE HRESULT
DXGIResizeTarget(DXGISwapChain swap_chain, DXGI_MODE_DESC* new_target_parameters)
{
   return swap_chain->lpVtbl->ResizeTarget(swap_chain, new_target_parameters);
}
static INLINE HRESULT DXGIGetContainingOutput(DXGISwapChain swap_chain, DXGIOutput* output)
{
   return swap_chain->lpVtbl->GetContainingOutput(swap_chain, output);
}
static INLINE HRESULT DXGIGetFrameStatistics(DXGISwapChain swap_chain, DXGI_FRAME_STATISTICS* stats)
{
   return swap_chain->lpVtbl->GetFrameStatistics(swap_chain, stats);
}
static INLINE HRESULT DXGIGetLastPresentCount(DXGISwapChain swap_chain, UINT* last_present_count)
{
   return swap_chain->lpVtbl->GetLastPresentCount(swap_chain, last_present_count);
}
static INLINE HRESULT DXGIGetSwapChainDesc1(DXGISwapChain swap_chain, DXGI_SWAP_CHAIN_DESC1* desc)
{
   return swap_chain->lpVtbl->GetDesc1(swap_chain, desc);
}
static INLINE HRESULT
DXGIGetFullscreenDesc(DXGISwapChain swap_chain, DXGI_SWAP_CHAIN_FULLSCREEN_DESC* desc)
{
   return swap_chain->lpVtbl->GetFullscreenDesc(swap_chain, desc);
}
static INLINE HRESULT DXGIGetHwnd(DXGISwapChain swap_chain, HWND* hwnd)
{
   return swap_chain->lpVtbl->GetHwnd(swap_chain, hwnd);
}
static INLINE HRESULT DXGIPresent1(
      DXGISwapChain            swap_chain,
      UINT                     sync_interval,
      UINT                     present_flags,
      DXGI_PRESENT_PARAMETERS* present_parameters)
{
   return swap_chain->lpVtbl->Present1(
         swap_chain, sync_interval, present_flags, present_parameters);
}
static INLINE BOOL DXGIIsTemporaryMonoSupported(DXGISwapChain swap_chain)
{
   return swap_chain->lpVtbl->IsTemporaryMonoSupported(swap_chain);
}
static INLINE HRESULT
DXGIGetRestrictToOutput(DXGISwapChain swap_chain, DXGIOutput* restrict_to_output)
{
   return swap_chain->lpVtbl->GetRestrictToOutput(swap_chain, restrict_to_output);
}
static INLINE HRESULT DXGISetBackgroundColor(DXGISwapChain swap_chain, DXGI_RGBA* color)
{
   return swap_chain->lpVtbl->SetBackgroundColor(swap_chain, color);
}
static INLINE HRESULT DXGIGetBackgroundColor(DXGISwapChain swap_chain, DXGI_RGBA* color)
{
   return swap_chain->lpVtbl->GetBackgroundColor(swap_chain, color);
}
static INLINE HRESULT DXGISetRotation(DXGISwapChain swap_chain, DXGI_MODE_ROTATION rotation)
{
   return swap_chain->lpVtbl->SetRotation(swap_chain, rotation);
}
static INLINE HRESULT DXGIGetRotation(DXGISwapChain swap_chain, DXGI_MODE_ROTATION* rotation)
{
   return swap_chain->lpVtbl->GetRotation(swap_chain, rotation);
}
static INLINE HRESULT DXGISetSourceSize(DXGISwapChain swap_chain, UINT width, UINT height)
{
   return swap_chain->lpVtbl->SetSourceSize(swap_chain, width, height);
}
static INLINE HRESULT DXGIGetSourceSize(DXGISwapChain swap_chain, UINT* width, UINT* height)
{
   return swap_chain->lpVtbl->GetSourceSize(swap_chain, width, height);
}
static INLINE HRESULT DXGISetMaximumFrameLatency(DXGISwapChain swap_chain, UINT max_latency)
{
   return swap_chain->lpVtbl->SetMaximumFrameLatency(swap_chain, max_latency);
}
static INLINE HRESULT DXGIGetMaximumFrameLatency(DXGISwapChain swap_chain, UINT* max_latency)
{
   return swap_chain->lpVtbl->GetMaximumFrameLatency(swap_chain, max_latency);
}
static INLINE HANDLE DXGIGetFrameLatencyWaitableObject(DXGISwapChain swap_chain)
{
   return swap_chain->lpVtbl->GetFrameLatencyWaitableObject(swap_chain);
}
static INLINE HRESULT DXGISetMatrixTransform(DXGISwapChain swap_chain, DXGI_MATRIX_3X2_F* matrix)
{
   return swap_chain->lpVtbl->SetMatrixTransform(swap_chain, matrix);
}
static INLINE HRESULT DXGIGetMatrixTransform(DXGISwapChain swap_chain, DXGI_MATRIX_3X2_F* matrix)
{
   return swap_chain->lpVtbl->GetMatrixTransform(swap_chain, matrix);
}
static INLINE UINT DXGIGetCurrentBackBufferIndex(DXGISwapChain swap_chain)
{
   return swap_chain->lpVtbl->GetCurrentBackBufferIndex(swap_chain);
}
static INLINE HRESULT DXGICheckColorSpaceSupport(
      DXGISwapChain swap_chain, DXGI_COLOR_SPACE_TYPE color_space, UINT* color_space_support)
{
   return swap_chain->lpVtbl->CheckColorSpaceSupport(swap_chain, color_space, color_space_support);
}
static INLINE HRESULT
DXGISetColorSpace1(DXGISwapChain swap_chain, DXGI_COLOR_SPACE_TYPE color_space)
{
   return swap_chain->lpVtbl->SetColorSpace1(swap_chain, color_space);
}
#endif
/* end of auto-generated */

static INLINE HRESULT DXGICreateFactory(DXGIFactory* factory)
{
   return CreateDXGIFactory1(uuidof(IDXGIFactory1), (void**)factory);
}
#ifdef __WINRT__
static INLINE HRESULT DXGICreateFactory2(DXGIFactory2* factory)
{
   return CreateDXGIFactory1(uuidof(IDXGIFactory2), (void**)factory);
}
#endif

/* internal */

#include "../../retroarch.h"
#include "../drivers_shader/glslang_util.h"

#define DXGI_COLOR_RGBA(r, g, b, a) (((UINT32)(a) << 24) | ((UINT32)(b) << 16) | ((UINT32)(g) << 8) | ((UINT32)(r) << 0))

typedef enum {
   DXGI_FORMAT_EX_A4R4G4B4_UNORM = 1000
} DXGI_FORMAT_EX;

typedef struct
{
   float x;
   float y;
   float z;
   float w;
} float4_t;

RETRO_BEGIN_DECLS

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

void dxgi_update_title(video_frame_info_t* video_info);

DXGI_FORMAT glslang_format_to_dxgi(glslang_format fmt);

RETRO_END_DECLS

#if 1
#include "../../performance_counters.h"

#ifndef PERF_START
#define PERF_START() \
   { \
   static struct retro_perf_counter perfcounter = { __FUNCTION__ }; \
   LARGE_INTEGER                    start, stop; \
   rarch_perf_register(&perfcounter); \
   perfcounter.call_cnt++; \
   QueryPerformanceCounter(&start)

#define PERF_STOP() \
   QueryPerformanceCounter(&stop); \
   perfcounter.total += stop.QuadPart - start.QuadPart; \
   }
#endif
#else
#define PERF_START()
#define PERF_STOP()
#endif
