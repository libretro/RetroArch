#ifndef _DXGI_COMMON_H
#define _DXGI_COMMON_H

#include <retro_inline.h>
#include <retro_common_api.h>

#ifndef HAVE_DXGI_HDR
#define HAVE_DXGI_HDR
#endif

#ifdef HAVE_DXGI_HDR
#ifndef ALIGN
#ifdef _MSC_VER
#define ALIGN(x) __declspec(align(x))
#else
#define ALIGN(x) __attribute__((aligned(x)))
#endif
#endif

#include <gfx/math/matrix_4x4.h>

RETRO_BEGIN_DECLS

typedef struct ALIGN(16)
{
   math_matrix_4x4   mvp;
   float             contrast;         /* 2.0f    */
   float             paper_white_nits; /* 200.0f  */
   float             max_nits;         /* 1000.0f */
   float             expand_gamut;     /* 1.0f    */
   float             inverse_tonemap;  /* 1.0f    */
   float             hdr10;            /* 1.0f    */
} dxgi_hdr_uniform_t;

enum dxgi_swapchain_bit_depth
{
   DXGI_SWAPCHAIN_BIT_DEPTH_8 = 0,
   DXGI_SWAPCHAIN_BIT_DEPTH_10,
   DXGI_SWAPCHAIN_BIT_DEPTH_16,
   DXGI_SWAPCHAIN_BIT_DEPTH_COUNT
};
#endif

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

#ifndef RARCH_INTERNAL
#define __in
#define __out
#endif

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
#include <dxgi1_6.h>

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
typedef IDXGIOutput6*           DXGIOutput6;
typedef IDXGIDevice*            DXGIDevice;
typedef IDXGIFactory1*          DXGIFactory1;
#ifdef __WINRT__
typedef IDXGIFactory2*          DXGIFactory2;
#endif
typedef IDXGIAdapter1*          DXGIAdapter;
typedef IDXGIDisplayControl*    DXGIDisplayControl;
typedef IDXGIOutputDuplication* DXGIOutputDuplication;
typedef IDXGIDecodeSwapChain*   DXGIDecodeSwapChain;
typedef IDXGIFactoryMedia*      DXGIFactoryMedia;
typedef IDXGISwapChainMedia*    DXGISwapChainMedia;
typedef IDXGISwapChain4*        DXGISwapChain;

#if !defined(__cplusplus) || defined(CINTERFACE)
static INLINE HRESULT DXGIWaitForVBlank(DXGIOutput output)
{
   return output->lpVtbl->WaitForVBlank(output);
}

static INLINE HRESULT DXGIMakeWindowAssociation(DXGIFactory1 factory, HWND window_handle, UINT flags)
{
   return factory->lpVtbl->MakeWindowAssociation(factory, window_handle, flags);
}

static INLINE HRESULT DXGICreateSwapChain(
      DXGIFactory1 factory, void* device, DXGI_SWAP_CHAIN_DESC* desc, DXGISwapChain* swap_chain)
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

static INLINE HRESULT DXGIEnumAdapters1(DXGIFactory1 factory, UINT id, DXGIAdapter* adapter)
{
   return factory->lpVtbl->EnumAdapters1(factory, id, adapter);
}

#ifdef __WINRT__
static INLINE HRESULT DXGIEnumAdapters2(DXGIFactory2 factory, UINT id, DXGIAdapter* adapter)
{
   return factory->lpVtbl->EnumAdapters1(factory, id, adapter);
}
#endif

#define DXGIPresent(swap_chain, sync_interval, flags) ((swap_chain)->lpVtbl->Present((swap_chain), (UINT)(sync_interval), flags))

#define DXGIResizeBuffers(swap_chain, buffer_count, width, height, new_format, swap_chain_flags) ((swap_chain)->lpVtbl->ResizeBuffers((swap_chain), buffer_count, width, height, new_format, swap_chain_flags))

static INLINE HRESULT DXGIGetContainingOutput(DXGISwapChain swap_chain, DXGIOutput* output)
{
   return swap_chain->lpVtbl->GetContainingOutput(swap_chain, output);
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
static INLINE UINT DXGIGetCurrentBackBufferIndex(DXGISwapChain swap_chain)
{
   return swap_chain->lpVtbl->GetCurrentBackBufferIndex(swap_chain);
}
#endif
/* end of auto-generated */

static INLINE HRESULT DXGICreateFactory1(DXGIFactory1* factory)
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

#ifdef HAVE_DXGI_HDR
#ifdef __WINRT__
bool dxgi_check_display_hdr_support(DXGIFactory2 factory, HWND hwnd);
#else
bool dxgi_check_display_hdr_support(DXGIFactory1 factory, HWND hwnd);
#endif
void dxgi_swapchain_color_space(DXGISwapChain handle, DXGI_COLOR_SPACE_TYPE
*chain_color_space, DXGI_COLOR_SPACE_TYPE color_space);
void dxgi_set_hdr_metadata(
      DXGISwapChain                 handle,
      bool                          hdr_supported,
      enum dxgi_swapchain_bit_depth chain_bit_depth,
      DXGI_COLOR_SPACE_TYPE         chain_color_space,
      float                         max_output_nits,
      float                         min_output_nits,
      float                         max_cll,
      float                         max_fall
);
#endif

DXGI_FORMAT glslang_format_to_dxgi(glslang_format fmt);

RETRO_END_DECLS

#endif
