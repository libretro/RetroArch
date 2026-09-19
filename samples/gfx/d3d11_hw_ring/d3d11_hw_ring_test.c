/* The threaded wrapper's D3D11 hardware ring, as a hardware core sees it.
 *
 * Under threaded video a D3D11 core is not given the immediate context:
 * it records into a deferred context behind gfx/common/
 * d3d11_deferred_proxy.c, the ring closes that recording into a command
 * list at every video_refresh, and the video thread replays the list on
 * the immediate context ahead of the frame that shows the core's
 * texture (d3d11_hw_ring_capture / d3d11_hw_ring_present_slot).
 *
 * This runs that sequence with the real proxy and the driver's own
 * FinishCommandList argument, against a core that behaves the way the
 * PS2 core's GSDevice11 does: it binds through a cache, rebinds a fixed
 * set of state after every present, and has one binding it makes once
 * at device creation and never again. It draws a different colour each
 * frame over a black clear, and the frontend's copy must hold that
 * colour.
 *
 *   d3d11_hw_ring_test            the ring as the driver runs it
 *   d3d11_hw_ring_test immediate  no ring: the core on the immediate context
 *   d3d11_hw_ring_test reset      the ring resetting the context each frame,
 *                                 which is what it did: right for one frame,
 *                                 black for every frame after
 *
 * run.sh requires the first two to pass and the third to fail.
 * Windows only; CI builds it with mingw-w64 and runs it under Wine,
 * where wined3d implements deferred contexts over OpenGL.
 */
#ifndef CINTERFACE
#define CINTERFACE
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "../../../gfx/common/d3d11_deferred_proxy.h"

static const char hlsl[] =
 "cbuffer C : register(b0) { float4 col; };"
 "cbuffer V : register(b1) { float4 va; float4 vb; float4 vc; };"
 "float4 vs(uint id : SV_VertexID) : SV_Position { if (id == 0) return va; if (id == 1) return vb; return vc; }"
 "float4 ps() : SV_Target { return col; }";

int main(int argc, char **argv)
{
   int use_proxy         = !(argc > 1 && !strcmp(argv[1], "immediate"));
   BOOL restore_deferred = (argc > 1 && !strcmp(argv[1], "reset"))
      ? FALSE : D3D11_HW_RING_KEEP_CONTEXT_STATE;
   int bad = 0;
   ID3D11Device *dev = NULL; ID3D11DeviceContext *imm = NULL, *def = NULL, *ctx;
   ID3D11Texture2D *tex, *staging; ID3D11RenderTargetView *rtv; ID3D11ShaderResourceView *srv;
   ID3D11VertexShader *vs; ID3D11PixelShader *ps; ID3D11Buffer *cb; ID3DBlob *vb = NULL, *pb = NULL, *err = NULL;
   D3D11_TEXTURE2D_DESC td; D3D11_BUFFER_DESC bd; D3D11_VIEWPORT vp = {0,0,64,64,0,1};
   D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0; int f; HRESULT hr;

   hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &fl, 1, D3D11_SDK_VERSION, &dev, NULL, &imm);
   if (FAILED(hr)) { printf("no device %08lx\n", hr); return 2; }
   if (use_proxy) {
      ID3D11Device_CreateDeferredContext(dev, 0, &def);
      ctx = d3d11_deferred_proxy_new(def);
   } else ctx = imm;

   memset(&td, 0, sizeof(td)); td.Width = td.Height = 64; td.MipLevels = td.ArraySize = 1;
   td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
   td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
   ID3D11Device_CreateTexture2D(dev, &td, NULL, &tex);
   td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
   ID3D11Device_CreateTexture2D(dev, &td, NULL, &staging);
   ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource*)tex, NULL, &rtv);
   ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource*)tex, NULL, &srv);
   if (FAILED(D3DCompile(hlsl, sizeof(hlsl)-1, NULL, NULL, NULL, "vs", "vs_4_0", 0, 0, &vb, &err))
    || FAILED(D3DCompile(hlsl, sizeof(hlsl)-1, NULL, NULL, NULL, "ps", "ps_4_0", 0, 0, &pb, &err)))
   { printf("compile failed: %s\n", err ? (char*)ID3D10Blob_GetBufferPointer(err) : "?"); return 2; }
   ID3D11Device_CreateVertexShader(dev, ID3D10Blob_GetBufferPointer(vb), ID3D10Blob_GetBufferSize(vb), NULL, &vs);
   ID3D11Device_CreatePixelShader(dev, ID3D10Blob_GetBufferPointer(pb), ID3D10Blob_GetBufferSize(pb), NULL, &ps);
   memset(&bd, 0, sizeof(bd)); bd.ByteWidth = 16; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
   ID3D11Device_CreateBuffer(dev, &bd, NULL, &cb);

   {
      /* GSDevice11::Create binds m_expand_vb_srv to VS slot 0 exactly once,
       * and RestoreAPIState never binds it again. Same here. */
      static const float pos[3][4] = { {-1,-1,0,1}, {-1,3,0,1}, {3,-1,0,1} };
      D3D11_BUFFER_DESC vbd; D3D11_SUBRESOURCE_DATA init; D3D11_SHADER_RESOURCE_VIEW_DESC sd;
      ID3D11Buffer *vbuf; ID3D11ShaderResourceView *vsrv;
      memset(&vbd, 0, sizeof(vbd)); vbd.ByteWidth = sizeof(pos); vbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      init.pSysMem = pos; init.SysMemPitch = 0; init.SysMemSlicePitch = 0;
      ID3D11Device_CreateBuffer(dev, &vbd, &init, &vbuf);
      memset(&sd, 0, sizeof(sd)); sd.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER; sd.Buffer.NumElements = 3;
      vsrv = NULL;
      (void)vsrv; ID3D11DeviceContext_VSSetConstantBuffers(ctx, 1, 1, &vbuf);
   }
   for (f = 0; f < 4; f++)
   {
      float col[4] = { (f & 1) ? 1.0f : 0.0f, (f & 2) ? 1.0f : 0.0f, 1.0f, 1.0f };
      ID3D11ShaderResourceView *got = NULL; ID3D11Texture2D *gtex = NULL;
      ID3D11RenderTargetView *nullrtv = NULL; ID3D11CommandList *list = NULL;
      D3D11_MAPPED_SUBRESOURCE m; unsigned char *p;

      /* --- the core, drawing a frame. Like GSDevice11 it binds through a
       * cache: what it believes is bound is not bound again. Frame 0 binds
       * everything; RestoreAPIState (below) is what rebinds after that. */
      if (f == 0) {
         ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
         ID3D11DeviceContext_VSSetShader(ctx, vs, NULL, 0);
         ID3D11DeviceContext_PSSetShader(ctx, ps, NULL, 0);
         ID3D11DeviceContext_PSSetConstantBuffers(ctx, 0, 1, &cb);
         ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp);
      }
      ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv, NULL);
      { float black[4] = {0,0,0,1}; ID3D11DeviceContext_ClearRenderTargetView(ctx, rtv, black); }
      ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource*)cb, 0, NULL, col, 0, 0);
      ID3D11DeviceContext_Draw(ctx, 3, 0);

      /* --- GSDevice11::PresentRect */
      ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &nullrtv, NULL);
      ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &srv);

      /* --- video_cb: RetroArch's d3d11_hw_ring_capture, main thread */
      if (use_proxy) {
         ID3D11DeviceContext_PSGetShaderResources(ctx, 0, 1, &got);
         if (got) { ID3D11ShaderResourceView_GetResource(got, (ID3D11Resource**)&gtex); ID3D11ShaderResourceView_Release(got); }
         hr = ID3D11DeviceContext_FinishCommandList(ctx, restore_deferred, &list);
         if (FAILED(hr)) { printf("FinishCommandList %08lx\n", hr); return 2; }
      } else gtex = tex;

      /* --- GSDevice11::EndPresent -> RestoreAPIState, next list */
      ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ID3D11DeviceContext_VSSetShader(ctx, vs, NULL, 0);
      ID3D11DeviceContext_PSSetShader(ctx, ps, NULL, 0);
      ID3D11DeviceContext_PSSetConstantBuffers(ctx, 0, 1, &cb);
      ID3D11DeviceContext_RSSetViewports(ctx, 1, &vp);

      /* --- video thread: d3d11_hw_ring_present_slot, then the frame's copy */
      if (list) { ID3D11DeviceContext_ExecuteCommandList(imm, list, TRUE); ID3D11CommandList_Release(list); }
      ID3D11DeviceContext_CopyResource(imm, (ID3D11Resource*)staging, (ID3D11Resource*)gtex);
      if (FAILED(ID3D11DeviceContext_Map(imm, (ID3D11Resource*)staging, 0, D3D11_MAP_READ, 0, &m))) { puts("map failed"); return 2; }
      p = (unsigned char*)m.pData + 32 * m.RowPitch + 32 * 4;
      printf("frame %d: wanted %3d %3d %3d, the frontend's copy has %3d %3d %3d  %s\n", f,
            (int)(col[0]*255), (int)(col[1]*255), (int)(col[2]*255), p[0], p[1], p[2],
            (p[0]==(int)(col[0]*255) && p[1]==(int)(col[1]*255) && p[2]==255) ? "ok" : "WRONG");
      if (!(p[0]==(int)(col[0]*255) && p[1]==(int)(col[1]*255) && p[2]==255))
         bad = 1;
      ID3D11DeviceContext_Unmap(imm, (ID3D11Resource*)staging, 0);
   }
   printf(bad ? "d3d11_hw_ring: FAILED\n" : "d3d11_hw_ring: ok\n");
   return bad;
}
