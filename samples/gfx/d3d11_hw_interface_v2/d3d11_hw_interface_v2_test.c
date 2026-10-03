/* libretro_d3d11.h version 2, run: a core and a frontend that presents
 * from another thread, sharing a device's one immediate context by
 * taking turns, with the handoff the header describes.
 *
 *   core      each frame: takes the lock; if told the frontend has had
 *             the context, binds everything again; draws frame n (a
 *             colour that encodes n) with the state it believes is
 *             bound; READS ITS OWN RENDER TARGET BACK and checks it -
 *             the thing a core cannot do at all on the deferred context
 *             version 1 leaves a threaded frontend to hand out; names
 *             the texture, releases the lock, calls video_refresh
 *   frontend  on its own thread: takes the lock, binds its own render
 *             target, shaders and viewport and draws with them - which
 *             is what a real frontend's frame does to the context - then
 *             reads the frame it was handed and checks it holds frame n
 *
 * Three things must hold: every frame reaches the frontend intact, every
 * readback the core makes is right, and nothing deadlocks.
 *
 * "norebind" is the control: a core that ignores lock_context's return
 * value. Its next draw goes through whatever the frontend left bound, and
 * its readback catches it. run.sh requires that to fail.
 *
 * "v2" runs version 3 of the interface: the frontend copies nothing at
 * video_refresh, reads the core's own texture when its thread gets to
 * it, and the core keeps a texture per sync index and waits before it
 * draws into one again. The frontend is slowed down so the core laps it.
 * "v2-nowait" is its control: a core that skips wait_sync_index draws
 * over a texture the frontend has yet to read. That must fail too.
 *
 * The frontend below is the smallest thing that honours the contract; it
 * is not RetroArch's driver. What this proves is that the contract in the
 * header is enough, on a real D3D11 device.
 *
 * Windows only. run.sh builds it with mingw-w64 and runs it under Wine. */
#ifndef CINTERFACE
#define CINTERFACE
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <boolean.h>
#include <libretro.h>
#include <libretro_d3d11.h>

#define FRAMES 200
#define W 64
#define H 64
#define SLOTS 3

static const char hlsl[] =
 "cbuffer C : register(b0) { float4 col; };"
 "float4 vs(uint id : SV_VertexID) : SV_Position {"
 "  if (id == 0) return float4(-1,-1,0,1); if (id == 1) return float4(-1,3,0,1); return float4(3,-1,0,1); }"
 "float4 ps() : SV_Target { return col; }"
 "float4 ps_grey() : SV_Target { return float4(0.5, 0.5, 0.5, 1); }";

typedef struct
{
   ID3D11Device *dev; ID3D11DeviceContext *ctx;
   CRITICAL_SECTION lock; DWORD core_thread; bool disturbed;
   ID3D11Texture2D *handed;                 /* set_texture */
   ID3D11Texture2D *slot[SLOTS]; unsigned slot_frame[SLOTS]; unsigned next_slot;
   CRITICAL_SECTION qlock; HANDLE have_work; unsigned q[FRAMES + 8]; unsigned head, tail;
   /* the frontend's own drawing, which is what disturbs the context */
   ID3D11Texture2D *fe_rt_tex, *staging; ID3D11RenderTargetView *fe_rtv;
   ID3D11VertexShader *vs; ID3D11PixelShader *ps_grey;
   volatile LONG bad, shown, quit; unsigned frame_no;
   /* version 3: the core's own textures, read directly */
   int v2; unsigned index; ID3D11Texture2D *direct[SLOTS];
   HANDLE slot_done[SLOTS]; bool slot_out[SLOTS];
   struct retro_hw_render_interface_d3d11 iface;
} frontend_t;

/* lock_context: true if the frontend has used the context since the core
 * last released it. */
static bool fe_lock_context(void *h)
{
   frontend_t *fe = (frontend_t*)h; bool d;
   EnterCriticalSection(&fe->lock);
   d = fe->disturbed; fe->disturbed = false;
   return d;
}
static void fe_unlock_context(void *h) { LeaveCriticalSection(&((frontend_t*)h)->lock); }
static void fe_set_texture(void *h, ID3D11Texture2D *t) { ((frontend_t*)h)->handed = t; }

/* video_refresh(RETRO_HW_FRAME_BUFFER_VALID), on the core's thread, which
 * does not hold the lock: take it, take the frame on the context - ahead
 * of whatever the core does next, since the core is in here - and give
 * it back. One call, no submission. */
static unsigned fe_get_sync_index(void *h)      { return ((frontend_t*)h)->index; }
static unsigned fe_get_sync_index_mask(void *h) { (void)h; return (1u << SLOTS) - 1; }
static void fe_wait_sync_index(void *h)
{
   frontend_t *fe = (frontend_t*)h; unsigned i = fe->index;
   if (fe->slot_out[i])
   {
      WaitForSingleObject(fe->slot_done[i], INFINITE);
      fe->slot_out[i] = false;
   }
}

static void fe_video_refresh(frontend_t *fe)
{
   unsigned k = fe->next_slot;
   if (fe->v2)
   {
      /* nothing is copied: remember whose texture this index is */
      k                 = fe->index;
      fe->direct[k]     = fe->handed;
      fe->slot_frame[k] = fe->frame_no++;
      fe->slot_out[k]   = true;
      fe->index         = (k + 1) % SLOTS;
      EnterCriticalSection(&fe->qlock);
      fe->q[fe->tail++] = k | (fe->slot_frame[k] << 8);
      LeaveCriticalSection(&fe->qlock);
      SetEvent(fe->have_work);
      return;
   }
   EnterCriticalSection(&fe->lock);
   ID3D11DeviceContext_CopyResource(fe->ctx, (ID3D11Resource*)fe->slot[k], (ID3D11Resource*)fe->handed);
   LeaveCriticalSection(&fe->lock);
   fe->slot_frame[k] = fe->frame_no++;
   fe->next_slot     = (k + 1) % SLOTS;
   EnterCriticalSection(&fe->qlock);
   fe->q[fe->tail++] = k | (fe->slot_frame[k] << 8);
   LeaveCriticalSection(&fe->qlock);
   SetEvent(fe->have_work);
}

static unsigned read_frame_no(frontend_t *fe, ID3D11Texture2D *tex)
{
   D3D11_MAPPED_SUBRESOURCE m; unsigned char *p; unsigned n = 0xffffffffu;
   ID3D11DeviceContext_CopyResource(fe->ctx, (ID3D11Resource*)fe->staging, (ID3D11Resource*)tex);
   if (SUCCEEDED(ID3D11DeviceContext_Map(fe->ctx, (ID3D11Resource*)fe->staging, 0, D3D11_MAP_READ, 0, &m)))
   {
      p = (unsigned char*)m.pData + (H / 2) * m.RowPitch + (W / 2) * 4;
      n = p[0] | ((unsigned)p[1] << 8);
      ID3D11DeviceContext_Unmap(fe->ctx, (ID3D11Resource*)fe->staging, 0);
   }
   return n;
}

static DWORD WINAPI frontend_thread(void *p)
{
   frontend_t *fe = (frontend_t*)p; D3D11_VIEWPORT vp = {0, 0, 8, 8, 0, 1};
   for (;;)
   {
      unsigned item, k, want, got;
      EnterCriticalSection(&fe->qlock);
      if (fe->head == fe->tail)
      {
         LeaveCriticalSection(&fe->qlock);
         if (fe->quit)
            return 0;
         WaitForSingleObject(fe->have_work, 20);
         continue;
      }
      item = fe->q[fe->head++];
      LeaveCriticalSection(&fe->qlock);
      k = item & 0xff; want = item >> 8;

      if (fe->v2)
         Sleep((fe->shown % 5) == 0 ? 12 : 4);   /* slower than the core */
      EnterCriticalSection(&fe->lock);
      fe->disturbed = true;
      /* a frontend's frame: its own target, shaders and viewport */
      ID3D11DeviceContext_OMSetRenderTargets(fe->ctx, 1, &fe->fe_rtv, NULL);
      ID3D11DeviceContext_RSSetViewports(fe->ctx, 1, &vp);
      ID3D11DeviceContext_VSSetShader(fe->ctx, fe->vs, NULL, 0);
      ID3D11DeviceContext_PSSetShader(fe->ctx, fe->ps_grey, NULL, 0);
      ID3D11DeviceContext_IASetPrimitiveTopology(fe->ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ID3D11DeviceContext_Draw(fe->ctx, 3, 0);
      got = read_frame_no(fe, fe->v2 ? fe->direct[k] : fe->slot[k]);
      LeaveCriticalSection(&fe->lock);
      if (fe->v2)
      {
         /* the read is issued: the core may have the texture back */
         if (got != want)
         {
            if (!fe->bad)
               printf("  FAIL: index %u should hold frame %u, holds frame %u\n", k, want, got);
            InterlockedExchange(&fe->bad, 1);
         }
         InterlockedIncrement(&fe->shown);
         SetEvent(fe->slot_done[k]);
         continue;
      }

      /* a slot can be taken again before it is shown if the core laps us;
       * what matters is that a slot never holds a torn or foreign frame */
      if (got != want && got != fe->slot_frame[k])
      {
         if (!fe->bad)
            printf("  FAIL: the frontend read frame %u where %u was published\n", got, want);
         InterlockedExchange(&fe->bad, 1);
      }
      InterlockedIncrement(&fe->shown);
   }
}

int main(int argc, char **argv)
{
   int norebind = argc > 1 && !strcmp(argv[1], "norebind");
   int v2       = 1; /* version 2 is the sync-index path; there is no other */
   int nowait   = argc > 1 && !strcmp(argv[1], "v2-nowait");
   ID3D11Texture2D *rt_v2[SLOTS]; ID3D11RenderTargetView *rtv_v2[SLOTS];
   frontend_t fe; const struct retro_hw_render_interface_d3d11 *d3d11;
   ID3D11Texture2D *rt_tex; ID3D11RenderTargetView *rtv; ID3D11PixelShader *ps; ID3D11Buffer *cb;
   ID3DBlob *b_vs = NULL, *b_ps = NULL, *b_grey = NULL, *err = NULL;
   D3D11_TEXTURE2D_DESC td; D3D11_BUFFER_DESC bd; D3D11_VIEWPORT vp = {0, 0, W, H, 0, 1};
   D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0; HANDLE thread; unsigned n, i; int core_bad = 0;

   setvbuf(stdout, NULL, _IONBF, 0);
   memset(&fe, 0, sizeof(fe));
   if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &fl, 1, D3D11_SDK_VERSION, &fe.dev, NULL, &fe.ctx)))
   { puts("no D3D11 device"); return 2; }
   InitializeCriticalSection(&fe.lock); InitializeCriticalSection(&fe.qlock);
   fe.have_work = CreateEvent(NULL, FALSE, FALSE, NULL);

   if (   FAILED(D3DCompile(hlsl, sizeof(hlsl) - 1, NULL, NULL, NULL, "vs", "vs_4_0", 0, 0, &b_vs, &err))
       || FAILED(D3DCompile(hlsl, sizeof(hlsl) - 1, NULL, NULL, NULL, "ps", "ps_4_0", 0, 0, &b_ps, &err))
       || FAILED(D3DCompile(hlsl, sizeof(hlsl) - 1, NULL, NULL, NULL, "ps_grey", "ps_4_0", 0, 0, &b_grey, &err)))
   { printf("shader compile failed: %s\n", err ? (char*)ID3D10Blob_GetBufferPointer(err) : "?"); return 2; }
   ID3D11Device_CreateVertexShader(fe.dev, ID3D10Blob_GetBufferPointer(b_vs), ID3D10Blob_GetBufferSize(b_vs), NULL, &fe.vs);
   ID3D11Device_CreatePixelShader(fe.dev, ID3D10Blob_GetBufferPointer(b_ps), ID3D10Blob_GetBufferSize(b_ps), NULL, &ps);
   ID3D11Device_CreatePixelShader(fe.dev, ID3D10Blob_GetBufferPointer(b_grey), ID3D10Blob_GetBufferSize(b_grey), NULL, &fe.ps_grey);

   memset(&td, 0, sizeof(td)); td.Width = W; td.Height = H; td.MipLevels = td.ArraySize = 1;
   td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
   td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
   ID3D11Device_CreateTexture2D(fe.dev, &td, NULL, &rt_tex);
   for (i = 0; i < SLOTS; i++)
      ID3D11Device_CreateTexture2D(fe.dev, &td, NULL, &rt_v2[i]);
   ID3D11Device_CreateTexture2D(fe.dev, &td, NULL, &fe.fe_rt_tex);
   for (i = 0; i < SLOTS; i++)
      ID3D11Device_CreateTexture2D(fe.dev, &td, NULL, &fe.slot[i]);
   td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
   ID3D11Device_CreateTexture2D(fe.dev, &td, NULL, &fe.staging);
   ID3D11Device_CreateRenderTargetView(fe.dev, (ID3D11Resource*)rt_tex, NULL, &rtv);
   for (i = 0; i < SLOTS; i++)
   {
      ID3D11Device_CreateRenderTargetView(fe.dev, (ID3D11Resource*)rt_v2[i], NULL, &rtv_v2[i]);
      fe.slot_done[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
   }
   ID3D11Device_CreateRenderTargetView(fe.dev, (ID3D11Resource*)fe.fe_rt_tex, NULL, &fe.fe_rtv);
   memset(&bd, 0, sizeof(bd)); bd.ByteWidth = 16; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
   ID3D11Device_CreateBuffer(fe.dev, &bd, NULL, &cb);

   fe.iface.interface_type    = RETRO_HW_RENDER_INTERFACE_D3D11;
   fe.iface.interface_version = RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2;
   fe.iface.handle            = &fe;
   fe.iface.device            = fe.dev;
   fe.iface.context           = fe.ctx;
   fe.iface.lock_context      = fe_lock_context;
   fe.iface.unlock_context    = fe_unlock_context;
   fe.iface.set_texture       = fe_set_texture;
   if (v2)
   {
      fe.v2                        = 1;
      fe.iface.interface_version   = RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2;
      fe.iface.get_sync_index      = fe_get_sync_index;
      fe.iface.get_sync_index_mask = fe_get_sync_index_mask;
      fe.iface.wait_sync_index     = fe_wait_sync_index;
   }
   d3d11 = &fe.iface;
   fe.disturbed = true;          /* a fresh context is nobody's */
   thread = CreateThread(NULL, 0, frontend_thread, &fe, 0, NULL);

   for (n = 0; n < FRAMES; n++)
   {
      float col[4]; unsigned got; ID3D11Texture2D *target = rt_tex; bool rebind;
      if (v2)
      {
         /* not with the lock held */
         unsigned idx = d3d11->get_sync_index(d3d11->handle);
         if (!nowait)
            d3d11->wait_sync_index(d3d11->handle);
         target = rt_v2[idx];
         rtv    = rtv_v2[idx];
      }
      col[0] = (float)(n & 0xff) / 255.0f; col[1] = (float)((n >> 8) & 0xff) / 255.0f; col[2] = 0.0f; col[3] = 1.0f;

      rebind = d3d11->lock_context(d3d11->handle) && (!norebind || n == 0);
      if (rebind)
      {
         /* the frontend has had the context: everything, again */
         ID3D11DeviceContext_IASetPrimitiveTopology(d3d11->context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
         ID3D11DeviceContext_VSSetShader(d3d11->context, fe.vs, NULL, 0);
         ID3D11DeviceContext_PSSetShader(d3d11->context, ps, NULL, 0);
         ID3D11DeviceContext_PSSetConstantBuffers(d3d11->context, 0, 1, &cb);
         ID3D11DeviceContext_RSSetViewports(d3d11->context, 1, &vp);
      }
      /* The target belongs to this frame's sync index, so it is bound
       * every frame whatever the context has been doing. Binding it is
       * not what lock_context is about, and doing it here is what keeps
       * the norebind run below a real control: a core that ignores
       * lock_context still draws into the right texture, and still
       * draws it wrong. */
      ID3D11DeviceContext_OMSetRenderTargets(d3d11->context, 1, &rtv, NULL);
      ID3D11DeviceContext_UpdateSubresource(d3d11->context, (ID3D11Resource*)cb, 0, NULL, col, 0, 0);
      ID3D11DeviceContext_Draw(d3d11->context, 3, 0);

      /* a readback, mid-frame, on the context the core was given */
      got = read_frame_no(&fe, target);
      if (got != n)
      {
         if (!core_bad)
            printf("  FAIL: the core drew frame %u and read back %u\n", n, got);
         core_bad = 1;
      }

      d3d11->set_texture(d3d11->handle, target);
      d3d11->unlock_context(d3d11->handle);
      fe_video_refresh(&fe);      /* not with the lock held: see the header */
      if ((n % 3) == 0)
         Sleep(1);               /* let the other thread in */
   }

   fe.quit = 1; SetEvent(fe.have_work);
   WaitForSingleObject(thread, 60000);
   printf("  %u frames drawn and read back by the core, %ld seen by the frontend\n", FRAMES, (long)fe.shown);
   if (core_bad || fe.bad || fe.shown != FRAMES) { puts("d3d11_hw_interface_v2: FAILED"); return 1; }
   puts("d3d11_hw_interface_v2: ok");
   return 0;
}
