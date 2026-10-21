/* Hardware profile/configuration discovery; no decode acceptance is implied. */
#include <stdio.h>

#ifdef _WIN32
#define CINTERFACE
#define COBJMACROS
#include <string.h>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

static const GUID av1_profile0 =
   { 0xb8be4ccb, 0xcf53, 0x46ba, { 0x8d, 0x59, 0xd6, 0xb8, 0xa6, 0xda, 0x5d, 0x2a } };

static unsigned probe_format(ID3D11VideoDevice *video, DXGI_FORMAT format)
{
   D3D11_VIDEO_DECODER_DESC desc;
   UINT count = 0, i;
   BOOL supported = FALSE;
   unsigned opened = 0;
   HRESULT hr = ID3D11VideoDevice_CheckVideoDecoderFormat(video,
         &av1_profile0, format, &supported);
   printf("  format=%s supported=%u query=0x%08lx\n",
         format == DXGI_FORMAT_NV12 ? "NV12" : "P010",
         (unsigned)supported, (unsigned long)hr);
   if (FAILED(hr) || !supported) return 0;
   memset(&desc, 0, sizeof(desc));
   desc.Guid = av1_profile0;
   desc.SampleWidth = desc.SampleHeight = 128;
   desc.OutputFormat = format;
   hr = ID3D11VideoDevice_GetVideoDecoderConfigCount(video, &desc, &count);
   if (FAILED(hr)) return 0;
   for (i = 0; i < count; i++)
   {
      D3D11_VIDEO_DECODER_CONFIG config;
      ID3D11VideoDecoder *decoder = NULL;
      hr = ID3D11VideoDevice_GetVideoDecoderConfig(video, &desc, i, &config);
      if (FAILED(hr)) continue;
      hr = ID3D11VideoDevice_CreateVideoDecoder(video, &desc, &config, &decoder);
      printf("    config=%u bitstream_raw=%u decoder_created=%u result=0x%08lx\n",
            (unsigned)i, (unsigned)config.ConfigBitstreamRaw,
            (unsigned)(SUCCEEDED(hr) && decoder), (unsigned long)hr);
      if (SUCCEEDED(hr) && decoder) opened++;
      if (decoder) ID3D11VideoDecoder_Release(decoder);
   }
   return opened != 0;
}

static unsigned probe_adapter(IDXGIAdapter1 *adapter, unsigned ordinal)
{
   static const D3D_FEATURE_LEVEL levels[] =
      { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
   ID3D11Device *device = NULL;
   ID3D11DeviceContext *context = NULL;
   ID3D11VideoDevice *video = NULL;
   DXGI_ADAPTER_DESC1 desc;
   UINT count, i;
   unsigned profile = 0, formats = 0;
   HRESULT hr;
   if (FAILED(IDXGIAdapter1_GetDesc1(adapter, &desc))
         || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) return 0;
   printf("adapter=%u vendor=%04x device=%04x\n", ordinal,
         (unsigned)desc.VendorId, (unsigned)desc.DeviceId);
   hr = D3D11CreateDevice((IDXGIAdapter*)adapter, D3D_DRIVER_TYPE_UNKNOWN,
         NULL, D3D11_CREATE_DEVICE_VIDEO_SUPPORT, levels, 2, D3D11_SDK_VERSION,
         &device, NULL, &context);
   if (FAILED(hr)) { printf("  device unavailable 0x%08lx\n", (unsigned long)hr); return 0; }
   hr = ID3D11Device_QueryInterface(device, &IID_ID3D11VideoDevice, (void**)&video);
   if (SUCCEEDED(hr))
   {
      count = ID3D11VideoDevice_GetVideoDecoderProfileCount(video);
      for (i = 0; i < count; i++)
      {
         GUID candidate;
         if (SUCCEEDED(ID3D11VideoDevice_GetVideoDecoderProfile(video, i, &candidate))
               && !memcmp(&candidate, &av1_profile0, sizeof(candidate))) profile = 1;
      }
      printf("  decoder_profiles=%u AV1_profile0=%u\n", (unsigned)count, profile);
      if (profile)
      {
         if (probe_format(video, DXGI_FORMAT_NV12)) formats |= 1;
         if (probe_format(video, DXGI_FORMAT_P010)) formats |= 2;
      }
      ID3D11VideoDevice_Release(video);
   }
   ID3D11DeviceContext_Release(context);
   ID3D11Device_Release(device);
   return formats;
}

int main(void)
{
   IDXGIFactory1 *factory = NULL;
   unsigned ordinal, complete = 0;
   HRESULT hr = CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&factory);
   if (FAILED(hr)) return 77;
   for (ordinal = 0; ; ordinal++)
   {
      IDXGIAdapter1 *adapter = NULL;
      hr = IDXGIFactory1_EnumAdapters1(factory, ordinal, &adapter);
      if (hr == DXGI_ERROR_NOT_FOUND) break;
      if (FAILED(hr)) break;
      if (probe_adapter(adapter, ordinal) == 3) complete++;
      IDXGIAdapter1_Release(adapter);
   }
   IDXGIFactory1_Release(factory);
   printf("AV1 NV12/P010 decoder creation: %u hardware adapter(s); pixel validation still required\n", complete);
   return complete ? 0 : 77;
}
#else
int main(void)
{
   puts("D3D11 hardware profile discovery unavailable on this platform");
   return 77;
}
#endif
