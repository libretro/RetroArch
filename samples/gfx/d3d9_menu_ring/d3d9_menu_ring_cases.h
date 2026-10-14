/* The menu's vertex ring, as a GPU a few frames behind sees it: each
 * D3D9 driver's display draw is run against a stand-in device whose
 * vertex buffer remembers which vertices a draw still reads. A lock
 * that starts the ring over has to be DISCARD, which hands out fresh
 * memory; one that appends is NOOVERWRITE, and must not change a
 * vertex a draw since the last DISCARD reads - the GPU would draw
 * with the new contents. NOOVERWRITE is a hint only a dynamic buffer
 * takes, so the ring has to be made dynamic too. Included by each test
 * after the driver source it includes. */

#ifndef D3D9_MENU_RING_CASES_H
#define D3D9_MENU_RING_CASES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool video_shader_load_preset_into_shader(const char *path,
      struct video_shader *shader)
{
   (void)path;
   (void)shader;
   return false;
}

#define RING_MAX 16384

struct fake_vb
{
   IDirect3DVertexBuffer9 iface;
   Vertex  mem[RING_MAX];
   Vertex  snap[RING_MAX];
   uint8_t in_use[RING_MAX];
   unsigned verts;
   DWORD    usage;
   DWORD    flags;
};

struct fake_dev
{
   IDirect3DDevice9 iface;
};

static struct fake_vb  vb;
static struct fake_dev dev;
static IDirect3DVertexBuffer9Vtbl vb_vtbl;
static IDirect3DDevice9Vtbl       dev_vtbl;

static unsigned draws, appends, restarts, overwrites, static_hints;

static void trap(void)
{
   printf("  [FAIL] a device or buffer method the stand-in lacks\n");
   exit(1);
}

static void mark(UINT start, UINT n)
{
   UINT i;
   for (i = start; i < start + n && i < vb.verts; i++)
      vb.in_use[i] = 1;
}

static HRESULT STDMETHODCALLTYPE vb_lock(IDirect3DVertexBuffer9 *p,
      UINT offset, UINT size, void **data, DWORD flags)
{
   (void)p;
   (void)size;
   vb.flags = flags;
   if (flags & D3DLOCK_NOOVERWRITE)
   {
      appends++;
      if (!(vb.usage & D3DUSAGE_DYNAMIC))
         static_hints++;
   }
   else
   {
      /* DISCARD: fresh memory; no hint: the lock waits for the GPU */
      memset(vb.in_use, 0, sizeof(vb.in_use));
      if (flags & D3DLOCK_DISCARD)
      {
         restarts++;
         memset(vb.mem, 0xCD, sizeof(vb.mem));
      }
   }
   memcpy(vb.snap, vb.mem, sizeof(vb.mem));
   *data = (uint8_t*)vb.mem + offset;
   return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE vb_unlock(IDirect3DVertexBuffer9 *p)
{
   unsigned i;
   (void)p;
   if (!(vb.flags & D3DLOCK_NOOVERWRITE))
      return D3D_OK;
   for (i = 0; i < vb.verts; i++)
      if (vb.in_use[i] && memcmp(&vb.mem[i], &vb.snap[i], sizeof(Vertex)))
         overwrites++;
   return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE dev_create_vb(IDirect3DDevice9 *p,
      UINT length, DWORD usage, DWORD fvf, D3DPOOL pool,
      IDirect3DVertexBuffer9 **out, HANDLE *shared)
{
   (void)p; (void)fvf; (void)pool; (void)shared;
   if (length > sizeof(vb.mem))
      return E_OUTOFMEMORY;
   vb.verts = length / sizeof(Vertex);
   vb.usage = usage;
   *out     = &vb.iface;
   return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE dev_draw(IDirect3DDevice9 *p,
      D3DPRIMITIVETYPE type, UINT start, UINT count)
{
   (void)p;
   draws++;
   mark(start, type == D3DPT_TRIANGLELIST ? count * 3 : count + 2);
   return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE dev_draw_up(IDirect3DDevice9 *p,
      D3DPRIMITIVETYPE type, UINT count, const void *data, UINT stride)
{
   (void)p; (void)type; (void)count; (void)data; (void)stride;
   draws++;
   return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE dev_set_texture(IDirect3DDevice9 *p,
      DWORD stage, IDirect3DBaseTexture9 *t)
{ (void)p; (void)stage; (void)t; return D3D_OK; }
static HRESULT STDMETHODCALLTYPE dev_set_sampler(IDirect3DDevice9 *p,
      DWORD s, D3DSAMPLERSTATETYPE t, DWORD v)
{ (void)p; (void)s; (void)t; (void)v; return D3D_OK; }
static HRESULT STDMETHODCALLTYPE dev_set_render_state(IDirect3DDevice9 *p,
      D3DRENDERSTATETYPE s, DWORD v)
{ (void)p; (void)s; (void)v; return D3D_OK; }
static HRESULT STDMETHODCALLTYPE dev_set_vs_const(IDirect3DDevice9 *p,
      UINT r, const float *d, UINT n)
{ (void)p; (void)r; (void)d; (void)n; return D3D_OK; }
static HRESULT STDMETHODCALLTYPE dev_set_stream(IDirect3DDevice9 *p,
      UINT s, IDirect3DVertexBuffer9 *b, UINT o, UINT st)
{ (void)p; (void)s; (void)b; (void)o; (void)st; return D3D_OK; }
static HRESULT STDMETHODCALLTYPE dev_set_decl(IDirect3DDevice9 *p,
      IDirect3DVertexDeclaration9 *d)
{ (void)p; (void)d; return D3D_OK; }
static HRESULT STDMETHODCALLTYPE dev_set_vs(IDirect3DDevice9 *p,
      IDirect3DVertexShader9 *s)
{ (void)p; (void)s; return D3D_OK; }
static HRESULT STDMETHODCALLTYPE dev_set_ps(IDirect3DDevice9 *p,
      IDirect3DPixelShader9 *s)
{ (void)p; (void)s; return D3D_OK; }

static void fakes_init(void)
{
   size_t i;
   void **slot = (void**)&dev_vtbl;
   for (i = 0; i < sizeof(dev_vtbl) / sizeof(void*); i++)
      slot[i] = (void*)trap;
   slot = (void**)&vb_vtbl;
   for (i = 0; i < sizeof(vb_vtbl) / sizeof(void*); i++)
      slot[i] = (void*)trap;

   vb_vtbl.Lock                     = vb_lock;
   vb_vtbl.Unlock                   = vb_unlock;
   dev_vtbl.CreateVertexBuffer      = dev_create_vb;
   dev_vtbl.DrawPrimitive           = dev_draw;
   dev_vtbl.DrawPrimitiveUP         = dev_draw_up;
   dev_vtbl.SetTexture              = dev_set_texture;
   dev_vtbl.SetSamplerState         = dev_set_sampler;
   dev_vtbl.SetRenderState          = dev_set_render_state;
   dev_vtbl.SetVertexShaderConstantF= dev_set_vs_const;
   dev_vtbl.SetStreamSource         = dev_set_stream;
   dev_vtbl.SetVertexDeclaration    = dev_set_decl;
   dev_vtbl.SetVertexShader         = dev_set_vs;
   dev_vtbl.SetPixelShader          = dev_set_ps;
   vb.iface.lpVtbl                  = &vb_vtbl;
   dev.iface.lpVtbl                 = &dev_vtbl;
}

#define MAX_STRIP 190

static int run_cases(void (*draw_fn)(gfx_display_ctx_draw_t*, void*,
         unsigned), size_t chain_size)
{
   /* Strips as gfx_display hands them over - a lone quad, a nine-slice,
    * a full batch - and quads built from pos and dims */
   static const unsigned sizes[] = { 4, 22, MAX_STRIP, 4, 64, 0, 10, 0 };
   static float xy[MAX_STRIP * 2], uv[MAX_STRIP * 2], rgba[MAX_STRIP * 4];
   d3d9_video_t d3d;
   unsigned frame, n = 0, fails = 0;

   fakes_init();
   memset(&d3d, 0, sizeof(d3d));
   d3d.dev              = (LPDIRECT3DDEVICE9)&dev.iface;
   d3d.renderchain_data = calloc(1, chain_size);

   if (!d3d9_menu_vbuf_create(&d3d) || !d3d.menu_display.buffer)
   {
      printf("  [FAIL] the menu's vertex ring is made\n");
      return 1;
   }
   if (vb.usage & D3DUSAGE_DYNAMIC)
      printf("  [ok]   the ring is a dynamic buffer\n");
   else
   {
      printf("  [FAIL] the ring is a dynamic buffer\n");
      fails++;
   }

   for (frame = 0; frame < 4; frame++)
   {
      unsigned total = 0;
      /* Where each frame's menu draw starts the ring over */
      d3d.menu_display.offset = 0;
      while (total < (unsigned)d3d.menu_display.size * 5 / 2)
      {
         gfx_display_ctx_draw_t draw;
         struct video_coords coords;
         unsigned i, v = sizes[n % (sizeof(sizes) / sizeof(sizes[0]))];

         memset(&draw, 0, sizeof(draw));
         memset(&coords, 0, sizeof(coords));
         for (i = 0; i < MAX_STRIP * 4; i++)
            rgba[i] = (float)((n + i) % 7) / 7.0f;
         for (i = 0; i < MAX_STRIP * 2; i++)
         {
            xy[i] = (float)((n * 31 + i) % 997) / 997.0f;
            uv[i] = (float)((n * 17 + i) % 991) / 991.0f;
         }
         coords.vertices   = v ? v : 4;
         coords.vertex     = v ? xy : NULL;
         coords.tex_coord  = v ? uv : NULL;
         coords.color      = rgba;
         draw.coords       = &coords;
         draw.pos          = VIDEO_POS_PACK(n % 300, n % 200);
         draw.dims         = VIDEO_SCALE_PACK(16 + n % 64, 16 + n % 48);
         draw.scale_factor = 1.0f;
         draw_fn(&draw, &d3d, VIDEO_SCALE_PACK(640, 480));
         total += coords.vertices;
         n++;
      }
   }

   printf("  [%s]   %u draws, %u appends, %u restarts\n",
         (draws && appends && restarts > 4) ? "ok" : "FAIL",
         draws, appends, restarts);
   if (!(draws && appends && restarts > 4))
      fails++;
   if (overwrites)
   {
      printf("  [FAIL] %u vertices a pending draw reads were rewritten\n",
            overwrites);
      fails++;
   }
   else
      printf("  [ok]   no vertex a pending draw reads was rewritten\n");
   if (static_hints)
   {
      printf("  [FAIL] %u NOOVERWRITE locks of a static buffer\n",
            static_hints);
      fails++;
   }
   free(d3d.renderchain_data);
   return fails ? 1 : 0;
}

#endif
