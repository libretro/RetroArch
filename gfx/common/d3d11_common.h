/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
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

#pragma once

#include <retro_inline.h>

#include <lists/string_list.h>

#include "dxgi_common.h"
#ifdef CINTERFACE
#define D3D11_NO_HELPERS
#endif
#include <d3d11.h>

#define D3D11_MAX_GPU_COUNT 16

typedef const ID3D11ShaderResourceView* D3D11ShaderResourceViewRef;
typedef const ID3D11SamplerState*       D3D11SamplerStateRef;
typedef const ID3D11BlendState*         D3D11BlendStateRef;

typedef ID3D11InputLayout*              D3D11InputLayout;
typedef ID3D11RasterizerState*          D3D11RasterizerState;
typedef ID3D11DepthStencilState*        D3D11DepthStencilState;
typedef ID3D11BlendState*               D3D11BlendState;
typedef ID3D11PixelShader*              D3D11PixelShader;
typedef ID3D11SamplerState*             D3D11SamplerState;
typedef ID3D11VertexShader*             D3D11VertexShader;
typedef ID3D11DomainShader*             D3D11DomainShader;
typedef ID3D11HullShader*               D3D11HullShader;
typedef ID3D11ComputeShader*            D3D11ComputeShader;
typedef ID3D11GeometryShader*           D3D11GeometryShader;

/* auto-generated */

typedef ID3D11Resource*                 D3D11Resource;
typedef ID3D11Buffer*                   D3D11Buffer;
typedef ID3D11Texture1D*                D3D11Texture1D;
typedef ID3D11Texture2D*                D3D11Texture2D;
typedef ID3D11Texture3D*                D3D11Texture3D;
typedef ID3D11View*                     D3D11View;
typedef ID3D11ShaderResourceView*       D3D11ShaderResourceView;
typedef ID3D11RenderTargetView*         D3D11RenderTargetView;
typedef ID3D11DepthStencilView*         D3D11DepthStencilView;
typedef ID3D11UnorderedAccessView*      D3D11UnorderedAccessView;
typedef ID3D11Asynchronous*             D3D11Asynchronous;
typedef ID3D11Query*                    D3D11Query;
typedef ID3D11Predicate*                D3D11Predicate;
typedef ID3D11Counter*                  D3D11Counter;
typedef ID3D11ClassInstance*            D3D11ClassInstance;
typedef ID3D11ClassLinkage*             D3D11ClassLinkage;
typedef ID3D11CommandList*              D3D11CommandList;
typedef ID3D11DeviceContext*            D3D11DeviceContext;
typedef ID3D11VideoDecoder*             D3D11VideoDecoder;
typedef ID3D11VideoProcessorEnumerator* D3D11VideoProcessorEnumerator;
typedef ID3D11VideoProcessor*           D3D11VideoProcessor;
typedef ID3D11AuthenticatedChannel*     D3D11AuthenticatedChannel;
typedef ID3D11CryptoSession*            D3D11CryptoSession;
typedef ID3D11VideoDecoderOutputView*   D3D11VideoDecoderOutputView;
typedef ID3D11VideoProcessorInputView*  D3D11VideoProcessorInputView;
typedef ID3D11VideoProcessorOutputView* D3D11VideoProcessorOutputView;
typedef ID3D11VideoContext*             D3D11VideoContext;
typedef ID3D11VideoDevice*              D3D11VideoDevice;
typedef ID3D11Device*                   D3D11Device;
#ifdef DEBUG
typedef ID3D11Debug*                    D3D11Debug;
#endif
typedef ID3D11SwitchToRef*              D3D11SwitchToRef;
typedef ID3D11TracingDevice*            D3D11TracingDevice;
typedef ID3D11InfoQueue*                D3D11InfoQueue;

#if !defined(__cplusplus) || defined(CINTERFACE)
static INLINE void D3D11SetResourceEvictionPriority(D3D11Resource resource, UINT eviction_priority)
{
   resource->lpVtbl->SetEvictionPriority(resource, eviction_priority);
}
static INLINE UINT D3D11GetResourceEvictionPriority(D3D11Resource resource)
{
   return resource->lpVtbl->GetEvictionPriority(resource);
}
static INLINE void D3D11GetTexture2DDesc(D3D11Texture2D texture2d, D3D11_TEXTURE2D_DESC* desc)
{
    texture2d->lpVtbl->GetDesc(texture2d, desc);
}
static INLINE void D3D11GetViewResource(D3D11View view, D3D11Resource* resource)
{
   view->lpVtbl->GetResource(view, resource);
}
static INLINE void D3D11GetShaderResourceViewResource(
      D3D11ShaderResourceView shader_resource_view, D3D11Resource* resource)
{
   shader_resource_view->lpVtbl->GetResource(shader_resource_view, resource);
}
static INLINE void D3D11GetShaderResourceViewTexture2D(
    D3D11ShaderResourceView shader_resource_view, D3D11Texture2D* texture2d)
{
    shader_resource_view->lpVtbl->GetResource(shader_resource_view, (D3D11Resource*)texture2d);
}
static INLINE void D3D11GetShaderResourceViewDesc(
    D3D11ShaderResourceView shader_resource_view, D3D11_SHADER_RESOURCE_VIEW_DESC* desc)
{
    shader_resource_view->lpVtbl->GetDesc(shader_resource_view, desc);
}
static INLINE void
D3D11GetRenderTargetViewResource(D3D11RenderTargetView render_target_view, D3D11Resource* resource)
{
   render_target_view->lpVtbl->GetResource(render_target_view, resource);
}
static INLINE void
D3D11GetDepthStencilViewResource(D3D11DepthStencilView depth_stencil_view, D3D11Resource* resource)
{
   depth_stencil_view->lpVtbl->GetResource(depth_stencil_view, resource);
}
static INLINE void D3D11GetUnorderedAccessViewResource(
      D3D11UnorderedAccessView unordered_access_view, D3D11Resource* resource)
{
   unordered_access_view->lpVtbl->GetResource(unordered_access_view, resource);
}
static INLINE UINT D3D11GetAsynchronousDataSize(D3D11Asynchronous asynchronous)
{
   return asynchronous->lpVtbl->GetDataSize(asynchronous);
}
static INLINE UINT D3D11GetQueryDataSize(D3D11Query query)
{
   return query->lpVtbl->GetDataSize(query);
}
static INLINE UINT D3D11GetPredicateDataSize(D3D11Predicate predicate)
{
   return predicate->lpVtbl->GetDataSize(predicate);
}
static INLINE UINT D3D11GetCounterDataSize(D3D11Counter counter)
{
   return counter->lpVtbl->GetDataSize(counter);
}

static INLINE void D3D11SetPShaderResources(
      D3D11DeviceContext               device_context,
      UINT                             start_slot,
      UINT                             num_views,
      ID3D11ShaderResourceView* const* shader_resource_views)
{
   device_context->lpVtbl->PSSetShaderResources(
         device_context, start_slot, num_views,
         shader_resource_views);
}

static INLINE void D3D11SetGShaderConstantBuffers(
      D3D11DeviceContext device_context,
      UINT               start_slot,
      UINT               num_buffers,
      D3D11Buffer* const constant_buffers)
{
   device_context->lpVtbl->GSSetConstantBuffers(
         device_context, start_slot, num_buffers, constant_buffers);
}

static INLINE HRESULT D3D11GetData(
      D3D11DeviceContext device_context,
      D3D11Asynchronous  async,
      void*              data,
      UINT               data_size,
      UINT               get_data_flags)
{
   return device_context->lpVtbl->GetData(device_context, async, data, data_size, get_data_flags);
}
static INLINE void D3D11SetPredication(
      D3D11DeviceContext device_context, D3D11Predicate predicate, BOOL predicate_value)
{
   device_context->lpVtbl->SetPredication(device_context, predicate, predicate_value);
}

static INLINE void
D3D11GenerateMips(D3D11DeviceContext device_context, D3D11ShaderResourceView shader_resource_view)
{
   device_context->lpVtbl->GenerateMips(device_context, shader_resource_view);
}
static INLINE void
D3D11SetResourceMinLOD(D3D11DeviceContext device_context, D3D11Resource resource, FLOAT min_lod)
{
   device_context->lpVtbl->SetResourceMinLOD(device_context, resource, min_lod);
}
static INLINE FLOAT
D3D11GetResourceMinLOD(D3D11DeviceContext device_context, D3D11Resource resource)
{
   return device_context->lpVtbl->GetResourceMinLOD(device_context, resource);
}

static INLINE void
D3D11GetState(D3D11DeviceContext device_context, D3D11RasterizerState* rasterizer_state)
{
   device_context->lpVtbl->RSGetState(device_context, rasterizer_state);
}

static INLINE UINT D3D11GetDeviceContextContextFlags(D3D11DeviceContext device_context)
{
   return device_context->lpVtbl->GetContextFlags(device_context);
}

static INLINE HRESULT D3D11GetAuthenticatedChannelCertificateSize(
      D3D11AuthenticatedChannel authenticated_channel, UINT* certificate_size)
{
   return authenticated_channel->lpVtbl->GetCertificateSize(
         authenticated_channel, certificate_size);
}
static INLINE HRESULT D3D11GetAuthenticatedChannelCertificate(
      D3D11AuthenticatedChannel authenticated_channel, UINT certificate_size, BYTE* certificate)
{
   return authenticated_channel->lpVtbl->GetCertificate(
         authenticated_channel, certificate_size, certificate);
}
static INLINE void
D3D11GetChannelHandle(D3D11AuthenticatedChannel authenticated_channel, HANDLE* channel_handle)
{
   authenticated_channel->lpVtbl->GetChannelHandle(authenticated_channel, channel_handle);
}
static INLINE void D3D11GetCryptoType(D3D11CryptoSession crypto_session, GUID* crypto_type)
{
   crypto_session->lpVtbl->GetCryptoType(crypto_session, crypto_type);
}
static INLINE void D3D11GetDecoderProfile(D3D11CryptoSession crypto_session, GUID* decoder_profile)
{
   crypto_session->lpVtbl->GetDecoderProfile(crypto_session, decoder_profile);
}
static INLINE HRESULT
D3D11GetCryptoSessionCertificateSize(D3D11CryptoSession crypto_session, UINT* certificate_size)
{
   return crypto_session->lpVtbl->GetCertificateSize(crypto_session, certificate_size);
}
static INLINE HRESULT D3D11GetCryptoSessionCertificate(
      D3D11CryptoSession crypto_session, UINT certificate_size, BYTE* certificate)
{
   return crypto_session->lpVtbl->GetCertificate(crypto_session, certificate_size, certificate);
}
static INLINE void
D3D11GetCryptoSessionHandle(D3D11CryptoSession crypto_session, HANDLE* crypto_session_handle)
{
   crypto_session->lpVtbl->GetCryptoSessionHandle(crypto_session, crypto_session_handle);
}

static INLINE HRESULT D3D11NegotiateCryptoSessionKeyExchange(
      D3D11VideoContext  video_context,
      D3D11CryptoSession crypto_session,
      UINT               data_size,
      void*              data)
{
   return video_context->lpVtbl->NegotiateCryptoSessionKeyExchange(
         video_context, crypto_session, data_size, data);
}
static INLINE void D3D11EncryptionBlt(
      D3D11VideoContext  video_context,
      D3D11CryptoSession crypto_session,
      D3D11Texture2D     src_surface,
      D3D11Texture2D     dst_surface,
      UINT               ivsize,
      void*              iv)
{
   video_context->lpVtbl->EncryptionBlt(
         video_context, crypto_session, src_surface, dst_surface, ivsize, iv);
}
static INLINE void D3D11DecryptionBlt(
      D3D11VideoContext           video_context,
      D3D11CryptoSession          crypto_session,
      D3D11Texture2D              src_surface,
      D3D11Texture2D              dst_surface,
      D3D11_ENCRYPTED_BLOCK_INFO* encrypted_block_info,
      UINT                        content_key_size,
      void*                       content_key,
      UINT                        ivsize,
      void*                       iv)
{
   video_context->lpVtbl->DecryptionBlt(
         video_context, crypto_session, src_surface, dst_surface, encrypted_block_info,
         content_key_size, content_key, ivsize, iv);
}
static INLINE void D3D11StartSessionKeyRefresh(
      D3D11VideoContext  video_context,
      D3D11CryptoSession crypto_session,
      UINT               random_number_size,
      void*              random_number)
{
   video_context->lpVtbl->StartSessionKeyRefresh(
         video_context, crypto_session, random_number_size, random_number);
}
static INLINE void
D3D11FinishSessionKeyRefresh(D3D11VideoContext video_context, D3D11CryptoSession crypto_session)
{
   video_context->lpVtbl->FinishSessionKeyRefresh(video_context, crypto_session);
}
static INLINE HRESULT D3D11GetEncryptionBltKey(
      D3D11VideoContext  video_context,
      D3D11CryptoSession crypto_session,
      UINT               key_size,
      void*              readback_key)
{
   return video_context->lpVtbl->GetEncryptionBltKey(
         video_context, crypto_session, key_size, readback_key);
}
static INLINE HRESULT D3D11NegotiateAuthenticatedChannelKeyExchange(
      D3D11VideoContext         video_context,
      D3D11AuthenticatedChannel channel,
      UINT                      data_size,
      void*                     data)
{
   return video_context->lpVtbl->NegotiateAuthenticatedChannelKeyExchange(
         video_context, channel, data_size, data);
}
static INLINE HRESULT D3D11QueryAuthenticatedChannel(
      D3D11VideoContext         video_context,
      D3D11AuthenticatedChannel channel,
      UINT                      input_size,
      void*                     input,
      UINT                      output_size,
      void*                     output)
{
   return video_context->lpVtbl->QueryAuthenticatedChannel(
         video_context, channel, input_size, input, output_size, output);
}
static INLINE HRESULT D3D11ConfigureAuthenticatedChannel(
      D3D11VideoContext                     video_context,
      D3D11AuthenticatedChannel             channel,
      UINT                                  input_size,
      void*                                 input,
      D3D11_AUTHENTICATED_CONFIGURE_OUTPUT* output)
{
   return video_context->lpVtbl->ConfigureAuthenticatedChannel(
         video_context, channel, input_size, input, output);
}
static INLINE void D3D11VideoProcessorSetStreamRotation(
      D3D11VideoContext              video_context,
      D3D11VideoProcessor            video_processor,
      UINT                           stream_index,
      BOOL                           enable,
      D3D11_VIDEO_PROCESSOR_ROTATION rotation)
{
   video_context->lpVtbl->VideoProcessorSetStreamRotation(
         video_context, video_processor, stream_index, enable, rotation);
}
static INLINE void D3D11VideoProcessorGetStreamRotation(
      D3D11VideoContext               video_context,
      D3D11VideoProcessor             video_processor,
      UINT                            stream_index,
      BOOL*                           enable,
      D3D11_VIDEO_PROCESSOR_ROTATION* rotation)
{
   video_context->lpVtbl->VideoProcessorGetStreamRotation(
         video_context, video_processor, stream_index, enable, rotation);
}
static INLINE HRESULT D3D11CreateAuthenticatedChannel(
      D3D11VideoDevice                 video_device,
      D3D11_AUTHENTICATED_CHANNEL_TYPE channel_type,
      D3D11AuthenticatedChannel*       authenticated_channel)
{
   return video_device->lpVtbl->CreateAuthenticatedChannel(
         video_device, channel_type, authenticated_channel);
}
static INLINE HRESULT D3D11CreateCryptoSession(
      D3D11VideoDevice    video_device,
      GUID*               crypto_type,
      GUID*               decoder_profile,
      GUID*               key_exchange_type,
      D3D11CryptoSession* crypto_session)
{
   return video_device->lpVtbl->CreateCryptoSession(
         video_device, crypto_type, decoder_profile, key_exchange_type, crypto_session);
}
static INLINE HRESULT D3D11CreateBuffer(
      D3D11Device             device,
      D3D11_BUFFER_DESC*      desc,
      D3D11_SUBRESOURCE_DATA* initial_data,
      D3D11Buffer*            buffer)
{
   return device->lpVtbl->CreateBuffer(device, desc, initial_data, buffer);
}
static INLINE HRESULT D3D11CreateTexture1D(
      D3D11Device             device,
      D3D11_TEXTURE1D_DESC*   desc,
      D3D11_SUBRESOURCE_DATA* initial_data,
      D3D11Texture1D*         texture1d)
{
   return device->lpVtbl->CreateTexture1D(device, desc, initial_data, texture1d);
}
static INLINE HRESULT D3D11CreateTexture2D(
      D3D11Device             device,
      D3D11_TEXTURE2D_DESC*   desc,
      D3D11_SUBRESOURCE_DATA* initial_data,
      D3D11Texture2D*         texture2d)
{
   return device->lpVtbl->CreateTexture2D(device, desc, initial_data, texture2d);
}
static INLINE HRESULT D3D11CreateTexture3D(
      D3D11Device             device,
      D3D11_TEXTURE3D_DESC*   desc,
      D3D11_SUBRESOURCE_DATA* initial_data,
      D3D11Texture3D*         texture3d)
{
   return device->lpVtbl->CreateTexture3D(device, desc, initial_data, texture3d);
}
static INLINE HRESULT D3D11CreateShaderResourceView(
      D3D11Device                      device,
      D3D11Resource                    resource,
      D3D11_SHADER_RESOURCE_VIEW_DESC* desc,
      D3D11ShaderResourceView*         srview)
{
   return device->lpVtbl->CreateShaderResourceView(device, resource, desc, srview);
}
static INLINE HRESULT D3D11CreateUnorderedAccessView(
      D3D11Device                       device,
      D3D11Resource                     resource,
      D3D11_UNORDERED_ACCESS_VIEW_DESC* desc,
      D3D11UnorderedAccessView*         uaview)
{
   return device->lpVtbl->CreateUnorderedAccessView(device, resource, desc, uaview);
}
static INLINE HRESULT D3D11CreateRenderTargetView(
      D3D11Device                    device,
      D3D11Resource                  resource,
      D3D11_RENDER_TARGET_VIEW_DESC* desc,
      D3D11RenderTargetView*         rtview)
{
   return device->lpVtbl->CreateRenderTargetView(device, resource, desc, rtview);
}
static INLINE HRESULT D3D11CreateDepthStencilView(
      D3D11Device                    device,
      D3D11Resource                  resource,
      D3D11_DEPTH_STENCIL_VIEW_DESC* desc,
      D3D11DepthStencilView*         depth_stencil_view)
{
   return device->lpVtbl->CreateDepthStencilView(device, resource, desc, depth_stencil_view);
}
static INLINE HRESULT D3D11CreateInputLayout(
      D3D11Device                     device,
      const D3D11_INPUT_ELEMENT_DESC* input_element_descs,
      UINT                            num_elements,
      void*                           shader_bytecode_with_input_signature,
      SIZE_T                          bytecode_length,
      D3D11InputLayout*               input_layout)
{
   return device->lpVtbl->CreateInputLayout(
         device, input_element_descs, num_elements, shader_bytecode_with_input_signature,
         bytecode_length, input_layout);
}
static INLINE HRESULT D3D11CreateVertexShader(
      D3D11Device        device,
      void*              shader_bytecode,
      SIZE_T             bytecode_length,
      D3D11ClassLinkage  class_linkage,
      D3D11VertexShader* vertex_shader)
{
   return device->lpVtbl->CreateVertexShader(
         device, shader_bytecode, bytecode_length, class_linkage, vertex_shader);
}
static INLINE HRESULT D3D11CreateGeometryShader(
      D3D11Device          device,
      void*                shader_bytecode,
      SIZE_T               bytecode_length,
      D3D11ClassLinkage    class_linkage,
      D3D11GeometryShader* geometry_shader)
{
   return device->lpVtbl->CreateGeometryShader(
         device, shader_bytecode, bytecode_length, class_linkage, geometry_shader);
}
static INLINE HRESULT D3D11CreateGeometryShaderWithStreamOutput(
      D3D11Device                 device,
      void*                       shader_bytecode,
      SIZE_T                      bytecode_length,
      D3D11_SO_DECLARATION_ENTRY* sodeclaration,
      UINT                        num_entries,
      UINT*                       buffer_strides,
      UINT                        num_strides,
      UINT                        rasterized_stream,
      D3D11ClassLinkage           class_linkage,
      D3D11GeometryShader*        geometry_shader)
{
   return device->lpVtbl->CreateGeometryShaderWithStreamOutput(
         device, shader_bytecode, bytecode_length, sodeclaration, num_entries, buffer_strides,
         num_strides, rasterized_stream, class_linkage, geometry_shader);
}
static INLINE HRESULT D3D11CreatePixelShader(
      D3D11Device       device,
      void*             shader_bytecode,
      SIZE_T            bytecode_length,
      D3D11ClassLinkage class_linkage,
      D3D11PixelShader* pixel_shader)
{
   return device->lpVtbl->CreatePixelShader(
         device, shader_bytecode, bytecode_length, class_linkage, pixel_shader);
}
static INLINE HRESULT D3D11CreateHullShader(
      D3D11Device       device,
      void*             shader_bytecode,
      SIZE_T            bytecode_length,
      D3D11ClassLinkage class_linkage,
      D3D11HullShader*  hull_shader)
{
   return device->lpVtbl->CreateHullShader(
         device, shader_bytecode, bytecode_length, class_linkage, hull_shader);
}
static INLINE HRESULT D3D11CreateDomainShader(
      D3D11Device        device,
      void*              shader_bytecode,
      SIZE_T             bytecode_length,
      D3D11ClassLinkage  class_linkage,
      D3D11DomainShader* domain_shader)
{
   return device->lpVtbl->CreateDomainShader(
         device, shader_bytecode, bytecode_length, class_linkage, domain_shader);
}
static INLINE HRESULT D3D11CreateComputeShader(
      D3D11Device         device,
      void*               shader_bytecode,
      SIZE_T              bytecode_length,
      D3D11ClassLinkage   class_linkage,
      D3D11ComputeShader* compute_shader)
{
   return device->lpVtbl->CreateComputeShader(
         device, shader_bytecode, bytecode_length, class_linkage, compute_shader);
}
static INLINE HRESULT D3D11CreateClassLinkage(D3D11Device device, D3D11ClassLinkage* linkage)
{
   return device->lpVtbl->CreateClassLinkage(device, linkage);
}

static INLINE HRESULT
D3D11CreateQuery(D3D11Device device, D3D11_QUERY_DESC* query_desc, D3D11Query* query)
{
   return device->lpVtbl->CreateQuery(device, query_desc, query);
}
static INLINE HRESULT D3D11CreatePredicate(
      D3D11Device device, D3D11_QUERY_DESC* predicate_desc, D3D11Predicate* predicate)
{
   return device->lpVtbl->CreatePredicate(device, predicate_desc, predicate);
}
static INLINE HRESULT
D3D11CreateCounter(D3D11Device device, D3D11_COUNTER_DESC* counter_desc, D3D11Counter* counter)
{
   return device->lpVtbl->CreateCounter(device, counter_desc, counter);
}
static INLINE HRESULT D3D11CreateDeferredContext(
      D3D11Device device, UINT context_flags, D3D11DeviceContext* deferred_context)
{
   return device->lpVtbl->CreateDeferredContext(device, context_flags, deferred_context);
}
static INLINE HRESULT
D3D11OpenSharedResource(D3D11Device device, HANDLE h_resource, ID3D11Resource** out)
{
   return device->lpVtbl->OpenSharedResource(
         device, h_resource, uuidof(ID3D11Resource), (void**)out);
}
static INLINE HRESULT
D3D11CheckFormatSupport(D3D11Device device, DXGI_FORMAT format, UINT* format_support)
{
   return device->lpVtbl->CheckFormatSupport(device, format, format_support);
}
static INLINE HRESULT D3D11CheckMultisampleQualityLevels(
      D3D11Device device, DXGI_FORMAT format, UINT sample_count, UINT* num_quality_levels)
{
   return device->lpVtbl->CheckMultisampleQualityLevels(
         device, format, sample_count, num_quality_levels);
}

static INLINE HRESULT D3D11CheckFeatureSupport(
      D3D11Device   device,
      D3D11_FEATURE feature,
      void*         feature_support_data,
      UINT          feature_support_data_size)
{
   return device->lpVtbl->CheckFeatureSupport(
         device, feature, feature_support_data, feature_support_data_size);
}
static INLINE D3D_FEATURE_LEVEL D3D11GetFeatureLevel(D3D11Device device)
{
   return device->lpVtbl->GetFeatureLevel(device);
}
static INLINE UINT D3D11GetCreationFlags(D3D11Device device)
{
   return device->lpVtbl->GetCreationFlags(device);
}

/* end of auto-generated */

static INLINE HRESULT D3D11CreateTexture2DRenderTargetView(
      D3D11Device                    device,
      D3D11Texture2D                 texture,
      D3D11_RENDER_TARGET_VIEW_DESC* desc,
      D3D11RenderTargetView*         rtview)
{
   return device->lpVtbl->CreateRenderTargetView(device, (D3D11Resource)texture, desc, rtview);
}
static INLINE HRESULT D3D11CreateTexture2DShaderResourceView(
      D3D11Device                      device,
      D3D11Texture2D                   texture,
      D3D11_SHADER_RESOURCE_VIEW_DESC* desc,
      D3D11ShaderResourceView*         srview)
{
   return device->lpVtbl->CreateShaderResourceView(device, (D3D11Resource)texture, desc, srview);
}
#endif

   /* internal */

#include <assert.h>
#include <boolean.h>
#include <retro_math.h>
#include <gfx/math/matrix_4x4.h>
#include <libretro_d3d.h>
#include "../../retroarch.h"
#include "../drivers_shader/slang_process.h"

typedef struct d3d11_vertex_t
{
   float position[2];
   float texcoord[2];
   float color[4];
} d3d11_vertex_t;

typedef struct
{
   D3D11Texture2D          handle;
   D3D11Texture2D          staging;
   D3D11_TEXTURE2D_DESC    desc;
   D3D11RenderTargetView   rt_view;
   D3D11ShaderResourceView view;
   D3D11SamplerStateRef    sampler;
   float4_t                size_data;
} d3d11_texture_t;

typedef struct
{
   UINT32 colors[4];
   struct
   {
      float x, y, w, h;
   } pos;
   struct
   {
      float u, v, w, h;
   } coords;
   struct
   {
      float scaling;
      float rotation;
   } params;
} d3d11_sprite_t;

#ifndef ALIGN
#ifdef _MSC_VER
#define ALIGN(x) __declspec(align(x))
#else
#define ALIGN(x) __attribute__((aligned(x)))
#endif
#endif

typedef struct ALIGN(16)
{
   math_matrix_4x4 mvp;
   struct
   {
      float width;
      float height;
   } OutputSize;
   float time;
} d3d11_uniform_t;

typedef struct d3d11_shader_t
{
   D3D11VertexShader   vs;
   D3D11PixelShader    ps;
   D3D11GeometryShader gs;
   D3D11InputLayout    layout;
} d3d11_shader_t;

typedef struct
{
   unsigned              cur_mon_id;
   DXGISwapChain         swapChain;
   D3D11Device           device;
   D3D_FEATURE_LEVEL     supportedFeatureLevel;
   D3D11DeviceContext    context;
   D3D11RasterizerState  scissor_enabled;
   D3D11RasterizerState  scissor_disabled;
   D3D11Buffer           ubo;
   d3d11_uniform_t       ubo_values;
#ifdef HAVE_DXGI_HDR
   d3d11_texture_t       back_buffer;
#endif
   D3D11SamplerState     samplers[RARCH_FILTER_MAX][RARCH_WRAP_MAX];
   D3D11BlendState       blend_enable;
   D3D11BlendState       blend_disable;
   D3D11BlendState       blend_pipeline;
   D3D11Buffer           menu_pipeline_vbo;
   math_matrix_4x4       mvp, mvp_no_rot;
   struct video_viewport vp;
   D3D11_VIEWPORT        viewport;
   D3D11_RECT            scissor;
   DXGI_FORMAT           format;
   float                 clearcolor[4];
   unsigned              swap_interval;
   bool                  vsync;
   bool                  resize_chain;
   bool                  keep_aspect;
   bool                  resize_viewport;
   bool                  resize_render_targets;
   bool                  init_history;
   bool                  has_flip_model;
   bool                  has_allow_tearing;
   d3d11_shader_t        shaders[GFX_MAX_SHADERS];
#ifdef HAVE_DXGI_HDR
   enum dxgi_swapchain_bit_depth 
                         chain_bit_depth;
   DXGI_COLOR_SPACE_TYPE chain_color_space;
   DXGI_FORMAT           chain_formats[DXGI_SWAPCHAIN_BIT_DEPTH_COUNT];
#endif
#ifdef __WINRT__
   DXGIFactory2 factory;
#else
   DXGIFactory factory;
#endif
   DXGIAdapter adapter;

	struct
   {
      bool enable;
      struct retro_hw_render_interface_d3d11 iface;
   } hw;

#ifdef HAVE_DXGI_HDR
   struct
   {
      dxgi_hdr_uniform_t               ubo_values;
      D3D11Buffer                      ubo;
      float                            max_output_nits;
      float                            min_output_nits;
      float                            max_cll;
      float                            max_fall;
      bool                             support;
      bool                             enable;
   } hdr;
#endif

	struct
   {
      d3d11_shader_t shader;
      d3d11_shader_t shader_font;
      D3D11Buffer    vbo;
      int            offset;
      int            capacity;
      bool           enabled;
   } sprites;

#ifdef HAVE_OVERLAY
   struct
   {
      D3D11Buffer      vbo;
      d3d11_texture_t* textures;
      bool             enabled;
      bool             fullscreen;
      int              count;
   } overlays;
#endif

   struct
   {
      d3d11_texture_t texture;
      D3D11Buffer     vbo;
      bool            enabled;
      bool            fullscreen;
   } menu;

   struct
   {
      d3d11_texture_t texture[GFX_MAX_FRAME_HISTORY + 1];
      D3D11Buffer     vbo;
      D3D11Buffer     ubo;
      D3D11_VIEWPORT  viewport;
      float4_t        output_size;
      int             rotation;
   } frame;

   struct
   {
      d3d11_shader_t             shader;
      D3D11Buffer                buffers[SLANG_CBUFFER_MAX];
      d3d11_texture_t            rt;
      d3d11_texture_t            feedback;
      D3D11_VIEWPORT             viewport;
      pass_semantics_t           semantics;
      uint32_t                   frame_count;
      int32_t                    frame_direction;
   } pass[GFX_MAX_SHADERS];

   struct video_shader* shader_preset;
   struct string_list *gpu_list;
   IDXGIAdapter1 *current_adapter;
   IDXGIAdapter1 *adapters[D3D11_MAX_GPU_COUNT];
   d3d11_texture_t      luts[GFX_MAX_TEXTURES];
} d3d11_video_t;

void d3d11_init_texture(D3D11Device device, d3d11_texture_t* texture);
static INLINE void d3d11_release_texture(d3d11_texture_t* texture)
{
   Release(texture->handle);
   Release(texture->staging);
   Release(texture->view);
   Release(texture->rt_view);
}

void d3d11_update_texture(
      D3D11DeviceContext ctx,
      unsigned           width,
      unsigned           height,
      unsigned           pitch,
      DXGI_FORMAT        format,
      const void*        data,
      d3d11_texture_t*   texture);

DXGI_FORMAT d3d11_get_closest_match(
      D3D11Device device, DXGI_FORMAT desired_format, UINT desired_format_support);

enum d3d11_feature_level_hint
{
   D3D11_FEATURE_LEVEL_HINT_DONTCARE,
   D3D11_FEATURE_LEVEL_HINT_1_0_CORE,
   D3D11_FEATURE_LEVEL_HINT_9_1,
   D3D11_FEATURE_LEVEL_HINT_9_2,
   D3D11_FEATURE_LEVEL_HINT_9_3,
   D3D11_FEATURE_LEVEL_HINT_10_0,
   D3D11_FEATURE_LEVEL_HINT_10_1,
   D3D11_FEATURE_LEVEL_HINT_11_0,
   D3D11_FEATURE_LEVEL_HINT_11_1,
   D3D11_FEATURE_LEVEL_HINT_12_0,
   D3D11_FEATURE_LEVEL_HINT_12_1,
   D3D11_FEATURE_LEVEL_HINT_12_2
};

bool d3d11_init_shader(
      D3D11Device                     device,
      const char*                     src,
      size_t                          size,
      const void*                     src_name,
      LPCSTR                          vs_entry,
      LPCSTR                          ps_entry,
      LPCSTR                          gs_entry,
      const D3D11_INPUT_ELEMENT_DESC* input_element_descs,
      UINT                            num_elements,
      d3d11_shader_t*                 out,
      enum d3d11_feature_level_hint   hint);

static INLINE void d3d11_release_shader(d3d11_shader_t* shader)
{
   Release(shader->layout);
   Release(shader->vs);
   Release(shader->ps);
   Release(shader->gs);
}
