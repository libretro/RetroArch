/* The Switch video driver's GPU path, as gfx/drivers/switch_nx_gfx.c
 * ships: the driver is included below against the libnx stand-in
 * headers, and the command stream it submits is run by a model of the
 * FERMI_TWOD_A engine into the window's block-linear buffers, laid out
 * as libnx's own framebuffer lays them out (16Bx2 GOBs, sixteen GOBs a
 * block). The lanes then read the screen back.
 *
 * Lanes: the window's buffers and crop; the core's frame in both pixel
 * formats, copied once and read in place (a lent framebuffer, the
 * threaded wrapper's frame memory) and drawn again by a dupe; the
 * menu's quads - fills, gradients, premultiplied textures modulated
 * by a colour, blending and the scissor; text with its shadow; the
 * overlay; RGUI over the frame; docking; the fences each frame is
 * shown behind; and every word of the stream decoding to a method the
 * engine model knows. On the Switch the 3D engine is modelled too, from
 * deko3d's definitions: its state checked at every draw, the menu
 * shaders' arithmetic run over the strip - turned textures, strips,
 * vertex shading, blending, the scissor, the wait between engines and
 * the frames' descriptor slices - and the macro engine its setup's
 * firmware register writes run through.
 *
 * Built three times: for the Switch (libnx's channel, nvmap and window
 * queue); with __SWITCH2__ for the Switch 2 (GSV memory and VM, the
 * nvhost channel ioctls, KICKOFF_PB and a VI2 layer), the same lanes
 * over the same command stream; and with NXG_S2_3D, the Switch 2's 3D
 * engine - NAK's programs by address, AMPERE_B's method map, where a
 * method only MAXWELL_B has is rejected. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#include <malloc.h>
#define SW_ALIGNED_ALLOC(align, size) _aligned_malloc(size, align)
#define SW_ALIGNED_FREE(p)            _aligned_free(p)
#endif

#include "../../../gfx/drivers/switch_nx_gfx.c"
#if defined(NXG_3D_FX) && defined(__SWITCH2__)
#include "../../../gfx/drivers/deko3d_shaders/menu_effects_sm86.h"
#elif defined(NXG_3D_FX)
#include "../../../gfx/drivers/deko3d_shaders/menu_effects_dksh.h"
#endif

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; \
   printf("[FAIL] " __VA_ARGS__); printf("\n"); } } while (0)

/* --- logging -------------------------------------------------------- */

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }

/* --- memory: the iova is the host address ----------------------------- */

#define MAX_MAPS 256
static struct { void *cpu; u32 size; bool live; } maps[MAX_MAPS];
static unsigned maps_live;

Result nvInitialize(void) { return 0; }
void   nvExit(void) { }
Result nvGpuInit(void) { return 0; }
void   nvGpuExit(void) { }
static nvioctl_gpu_characteristics chars = { 0x20000 };
const nvioctl_gpu_characteristics *nvGpuGetCharacteristics(void) { return &chars; }
Result nvFenceInit(void) { return 0; }
void   nvFenceExit(void) { }
Result nvMapInit(void) { return 0; }
void   nvMapExit(void) { }

Result nvMapCreate(NvMap *m, void *cpu_addr, u32 size, u32 align,
      NvKind kind, bool is_cpu_cacheable)
{
   unsigned i;
   (void)align; (void)kind; (void)is_cpu_cacheable;
   if (((uintptr_t)cpu_addr & 0xfff) || (size & 0xfff) || !size)
      return 1;
   for (i = 1; i < MAX_MAPS; i++)
      if (!maps[i].live)
      {
         maps[i].cpu  = cpu_addr;
         maps[i].size = size;
         maps[i].live = true;
         m->handle    = i;
         m->id        = 1000 + i;
         m->cpu_addr  = cpu_addr;
         m->size      = size;
         maps_live++;
         return 0;
      }
   return 1;
}
void nvMapClose(NvMap *m)
{
   if (m->handle && m->handle < MAX_MAPS && maps[m->handle].live)
   {
      maps[m->handle].live = false;
      maps_live--;
   }
}
u32 nvMapGetHandle(NvMap *m) { return m->handle; }
u32 nvMapGetId(NvMap *m)     { return m->id; }

Result nvAddressSpaceCreate(NvAddressSpace *a, u32 page_size)
{
   a->page_size = page_size;
   a->has_init  = true;
   return 0;
}
void   nvAddressSpaceClose(NvAddressSpace *a) { a->has_init = false; }
Result nvAddressSpaceMap(NvAddressSpace *a, u32 h, bool cached, NvKind kind,
      iova_t *iova)
{
   (void)a; (void)cached; (void)kind;
   if (!h || h >= MAX_MAPS || !maps[h].live)
      return 1;
   *iova = (iova_t)(uintptr_t)maps[h].cpu;
   return 0;
}
Result nvAddressSpaceUnmap(NvAddressSpace *a, iova_t iova)
{
   (void)a; (void)iova;
   return 0;
}

void armDCacheFlush(void *addr, size_t size) { (void)addr; (void)size; }

/* The Switch 2's VM: GPU addresses the driver chose, mapped to CPU
 * memory; on the Switch the iova is the host address */
#define MAX_VMAPS 256
static struct { uint64_t va, size; uint8_t *cpu; bool live; } vmaps[MAX_VMAPS];

static uint8_t *xlat(uint64_t iova)
{
   unsigned i;
   for (i = 0; i < MAX_VMAPS; i++)
      if (vmaps[i].live && iova >= vmaps[i].va
            && iova < vmaps[i].va + vmaps[i].size)
         return vmaps[i].cpu + (iova - vmaps[i].va);
   return (uint8_t*)(uintptr_t)iova;
}

/* --- the 2D engine model --------------------------------------------- */

static uint32_t reg2d[0x800];
static uint32_t syncpt_value;
#ifdef __SWITCH2__
static unsigned host_wfi, host_flush;
#endif
static unsigned bad_words, blits, words_run;
#ifdef NXG_3D
/* The 3D engine model's counts; an engine that drew before the other
 * with no wait for idle between is a hazard */
static unsigned draws3d, wfi3d, hazards, bad3d, tic_slice_bad;
static bool pending3d, pending2d, tic_fresh, tex_fresh;
static int frame_slice = -1;
#endif

/* libnx's 16Bx2 GOB layout, sixteen GOBs a block */
static uint8_t *bl_addr(uint8_t *base, unsigned pitch, unsigned x_bytes,
      unsigned y)
{
   unsigned gob_x = x_bytes >> 6, gob_y = y >> 3;
   unsigned gx    = x_bytes & 63, gy = y & 7;
   unsigned i     = (gy & 1) | (((gx >> 4) & 1) << 1)
                  | (((gy >> 1) & 3) << 2) | (((gx >> 5) & 1) << 4);
   size_t block   = (size_t)(gob_y >> 4) * (pitch >> 6) + gob_x;
   return base + (block * 16 + (gob_y & 15)) * 512 + i * 16 + (gx & 15);
}

static double fx32(unsigned frac_reg)
{
   int64_t v = (int64_t)(((uint64_t)reg2d[frac_reg + 1] << 32)
         | reg2d[frac_reg]);
   return (double)v / 4294967296.0;
}

/* A texel as premultiplied RGBA bytes, R low */
static uint32_t fetch(unsigned x, unsigned y)
{
   uint64_t iova = ((uint64_t)reg2d[0x094] << 32) | reg2d[0x095];
   uint8_t *row  = xlat(iova) + (size_t)y * reg2d[0x091];
   switch (reg2d[0x08C])
   {
      case NXG_FMT_RGBA8:
         return ((uint32_t*)row)[x];
      case NXG_FMT_BGRX8:
      {
         uint32_t p = ((uint32_t*)row)[x];
         return ((p >> 16) & 0xff) | (p & 0xff00) | ((p & 0xff) << 16)
            | 0xff000000u;
      }
      case NXG_FMT_B5G6R5:
      {
         unsigned p = ((uint16_t*)row)[x];
         unsigned r = (p >> 11) & 31, g = (p >> 5) & 63, b = p & 31;
         r = (r << 3) | (r >> 2);
         g = (g << 2) | (g >> 4);
         b = (b << 3) | (b >> 2);
         return r | (g << 8) | (b << 16) | 0xff000000u;
      }
   }
   bad_words++;
   return 0;
}

static uint32_t sample(double u, double v, bool bilinear)
{
   int w = (int)reg2d[0x092], h = (int)reg2d[0x093];
   if (!bilinear)
   {
      int x = (int)floor(u), y = (int)floor(v);
      x = x < 0 ? 0 : x >= w ? w - 1 : x;
      y = y < 0 ? 0 : y >= h ? h - 1 : y;
      return fetch((unsigned)x, (unsigned)y);
   }
   {
      double fu = u - 0.5, fv = v - 0.5;
      int x0 = (int)floor(fu), y0 = (int)floor(fv);
      double ax = fu - x0, ay = fv - y0;
      int x1 = x0 + 1, y1 = y0 + 1;
      uint32_t p00, p10, p01, p11, out = 0;
      unsigned ch;
      x0 = x0 < 0 ? 0 : x0 >= w ? w - 1 : x0;
      x1 = x1 < 0 ? 0 : x1 >= w ? w - 1 : x1;
      y0 = y0 < 0 ? 0 : y0 >= h ? h - 1 : y0;
      y1 = y1 < 0 ? 0 : y1 >= h ? h - 1 : y1;
      p00 = fetch(x0, y0); p10 = fetch(x1, y0);
      p01 = fetch(x0, y1); p11 = fetch(x1, y1);
      for (ch = 0; ch < 32; ch += 8)
      {
         double a = (p00 >> ch & 255) * (1 - ax) + (p10 >> ch & 255) * ax;
         double b = (p01 >> ch & 255) * (1 - ax) + (p11 >> ch & 255) * ax;
         out |= ((uint32_t)(a * (1 - ay) + b * ay + 0.5) & 255) << ch;
      }
      return out;
   }
}

static void run_blit(void)
{
   uint64_t dst_iova = ((uint64_t)reg2d[0x088] << 32) | reg2d[0x089];
   uint8_t *dst      = xlat(dst_iova);
   unsigned dx = reg2d[0x22C], dy = reg2d[0x22D];
   unsigned dw = reg2d[0x22E], dh = reg2d[0x22F];
   double du = fx32(0x230), dv = fx32(0x232);
   double sx = fx32(0x234), sy = fx32(0x236);
   bool bil  = (reg2d[0x223] & NXG_SAMPLE_BILINEAR) != 0;
   unsigned op = reg2d[0x0AB], beta = reg2d[0x0AA];
   unsigned x, y, ch;
   blits++;
#ifdef NXG_3D
   if (pending3d)
      hazards++;
   pending2d = true;
#endif
   if (     reg2d[0x080] != NXG_FMT_RGBA8 || reg2d[0x081] != 0
         || reg2d[0x082] != (NXG_BLOCK_LOG2 << 4) || reg2d[0x08D] != 1
         || !(reg2d[0x223] & NXG_SAMPLE_CORNER)
         || dx + dw > reg2d[0x086] || dy + dh > reg2d[0x087])
   {
      bad_words++;
      return;
   }
   for (y = 0; y < dh; y++)
      for (x = 0; x < dw; x++)
      {
         uint32_t s = sample(sx + du * x, sy + dv * y, bil);
         uint32_t *d = (uint32_t*)bl_addr(dst, NXG_FB_PITCH,
               (dx + x) * 4, dy + y);
         uint32_t o = 0;
         for (ch = 0; ch < 32; ch += 8)
         {
            unsigned sc = (s >> ch) & 255, bc = (beta >> ch) & 255;
            unsigned v  = (sc * bc + 127) / 255;
            if (op == NXG_OP_BLEND_PREMULT)
            {
               unsigned sa = (((s >> 24) & 255) * ((beta >> 24) & 255) + 127) / 255;
               unsigned dc = (*d >> ch) & 255;
               v += (dc * (255 - sa) + 127) / 255;
               if (v > 255)
                  v = 255;
            }
            else if (op != NXG_OP_SRCCOPY_PREMULT)
               bad_words++;
            o |= v << ch;
         }
         *d = o;
      }
}

#ifdef NXG_3D
/* --- the 3D engine model ----------------------------------------------
 *
 * MAXWELL_B as deko3d defines it (engine_3d.def, the TIC and TSC
 * layouts, uam's resource layout), decoded here on its own terms: the
 * state a draw needs is checked word by word, then the strip is run
 * through what the menu shaders compute - gl_Position = mvp * pos from
 * the vertex stage's c[2], the texture by the handle at c[0][0x690] of
 * the fragment stage times the colour - rasterised at pixel centres
 * with a top-left rule, blended, into the block-linear target. */

static uint32_t reg3d[0x1000];
static struct { uint64_t addr; uint32_t size; bool valid; } cbs[5][18];

static float rf(unsigned m)
{
   float f;
   memcpy(&f, &reg3d[m], 4);
   return f;
}

static uint64_t raddr(unsigned m)
{
   return ((uint64_t)reg3d[m] << 32) | reg3d[m + 1];
}

static void bad_state(const char *what)
{
   if (bad3d++ < 8)
      printf("[info] 3D state: %s\n", what);
}

#ifndef __SWITCH2__
static const uint8_t *prog_hdr(const uint8_t *b)
{
   return b + nxg_le32(b + 16);
}
#endif

/* The state every menu draw takes, as the driver must have set it */
static bool check_state(void)
{
   static const uint32_t attribs[3] = {
      0u | (0u << 7) | (0x04u << 21) | (7u << 27),
      0u | (8u << 7) | (0x04u << 21) | (7u << 27),
      0u | (16u << 7) | (0x01u << 21) | (7u << 27)
   };
   unsigned i;
#ifndef __SWITCH2__
   const uint8_t *vb = deko3d_menu_vsh_dksh, *fb = deko3d_menu_fsh_dksh;
   uint64_t region = raddr(0x582);
#endif
   bool ok = true;
#define WANT(c, what) do { if (!(c)) { bad_state(what); ok = false; } } while (0)
   WANT(reg3d[0x204] == 0xD5 && reg3d[0x205] == (NXG_BLOCK_LOG2 << 4)
         && reg3d[0x202] == NXG_FB_W && reg3d[0x203] == NXG_FB_H
         && reg3d[0x206] == 1, "render target");
   WANT(reg3d[0x487] == (1u | (076543210u << 4)), "render target control");
   for (i = 1; i < 8; i++)
      WANT(reg3d[0x204 + i * 0x10] == 0, "another render target");
   WANT(reg3d[0x800] == 0, "VertexA");
   WANT(reg3d[0x810] == 0x11 && reg3d[0x850] == 0x51, "program config");
   for (i = 2; i < 5; i++)
      WANT(reg3d[0x800 + i * 0x10] == i << 4, "a stage between");
#ifdef __SWITCH2__
   /* AMPERE_B: each program by its header's address, NAK's words there */
   WANT(reg3d[0x813] == NXG_S2_MENU_VSH_GPRS
         && reg3d[0x853] == NXG_S2_MENU_FSH_GPRS, "registers");
   WANT(reg3d[0x814] == 0 && reg3d[0x854] == 4, "program bindings");
   WANT(reg3d[0x817] == (sizeof(nxg_s2_menu_vsh) + 255) / 256
         && reg3d[0x857] == (sizeof(nxg_s2_menu_fsh) + 255) / 256,
         "program prefetch");
   WANT(!(raddr(0x815) & 0x7F) && !(raddr(0x855) & 0x7F)
         && !memcmp(xlat(raddr(0x815)), nxg_s2_menu_vsh,
            sizeof(nxg_s2_menu_vsh))
         && !memcmp(xlat(raddr(0x855)), nxg_s2_menu_fsh,
            sizeof(nxg_s2_menu_fsh)), "program code");
   WANT(reg3d[0x0C4] == 0x806, "SPA version");
   for (i = 0x678; i < 0x680; i++)
      WANT(!reg3d[i], "colour compression");
   WANT(!reg3d[0x673], "depth compression");
#else
   WANT(reg3d[0x813] == nxg_le32(prog_hdr(vb) + 8)
         && reg3d[0x853] == nxg_le32(prog_hdr(fb) + 8), "registers");
   /* The code the programs point at is the blobs' */
   WANT(!memcmp(xlat(region + reg3d[0x811] - nxg_le32(prog_hdr(vb) + 4)),
            vb + nxg_le32(vb + 8), nxg_le32(vb + 12))
         && !memcmp(xlat(region + reg3d[0x851]
               - nxg_le32(prog_hdr(fb) + 4)),
            fb + nxg_le32(fb + 8), nxg_le32(fb + 12)), "program code");
   WANT(reg3d[0x0D8] == nxg_le32(prog_hdr(fb) + 44), "fragment D8");
   WANT(reg3d[0x982] == 0, "texture handles not in c[0]");
#endif
   WANT(!cbs[0][1].valid && !cbs[4][1].valid, "compiler constants bound");
   WANT(reg3d[0x0D9] == 0x20 && reg3d[0x1D5] == 1
         && reg3d[0x3D0] == 0, "fragment parameters");
   for (i = 0; i < 3; i++)
      WANT(reg3d[0x458 + i] == attribs[i], "vertex attribute");
   for (i = 3; i < 32; i++)
      WANT(reg3d[0x458 + i] == ((1u << 6) | (0x12u << 21) | (7u << 27)),
            "a fixed attribute");
   WANT(reg3d[0x700] == ((1u << 12) | 32), "vertex array 0");
   for (i = 1; i < 16; i++)
      WANT(!(reg3d[0x700 + i * 4] & (1u << 12)), "another vertex array");
   WANT(reg3d[0x781] == 1 && reg3d[0x782] == 2 && reg3d[0x783] == 6
         && reg3d[0x784] == 1 && reg3d[0x785] == 2 && reg3d[0x786] == 6
         && reg3d[0x4B9] == 1, "premultiplied blend");
   WANT(reg3d[0x680] == 0x1111, "colour writes");
   WANT(reg3d[0x4EB] == 0 && reg3d[0x64B] == 1 && reg3d[0x0DF] == 1
         && reg3d[0x36B] == 0x1B02 && reg3d[0x36C] == 0x1B02, "raster");
   WANT(!reg3d[0x646] && !reg3d[0x54E] && !reg3d[0x4B3] && !reg3d[0x4E0]
         && !reg3d[0x4BB] && !reg3d[0x671] && !reg3d[0x54D], "tests off");
   WANT(cbs[0][0].valid && cbs[4][0].valid && cbs[0][2].valid
         && cbs[0][2].size >= 64, "constant buffers");
   WANT(reg3d[0x380] == 1, "scissor off");
   WANT(rf(0x302) == 0.0f && rf(0x303) == 1.0f, "depth range");
   return ok;
#undef WANT
}

/* A texel of a pitch-linear A8R8G8B8 image at @t, clamped */
static void texel(const uint32_t *t, int x, int y, double *out)
{
   uint64_t a = t[1] | ((uint64_t)(t[2] & 0xFFFF) << 32);
   unsigned pitch = (t[3] & 0xFFFF) << 5;
   int w = (int)(t[4] & 0xFFFF) + 1, h = (int)(t[5] & 0xFFFF) + 1;
   uint32_t p;
   unsigned ch;
   x = x < 0 ? 0 : x >= w ? w - 1 : x;
   y = y < 0 ? 0 : y >= h ? h - 1 : y;
   p = ((const uint32_t*)(xlat(a) + (size_t)y * pitch))[x];
   for (ch = 0; ch < 4; ch++)
      out[ch] = ((p >> (ch * 8)) & 255) / 255.0;
}

static void sample3d(double u, double v, double *out)
{
   uint32_t h = *(uint32_t*)xlat(cbs[4][0].addr + 0x690);
   unsigned ti = h & 0xFFFFF, si = h >> 20;
   const uint32_t *t = (const uint32_t*)xlat(raddr(0x55D) + ti * 32);
   const uint32_t *s = (const uint32_t*)xlat(raddr(0x557) + si * 32);
   int w, hh, x0, y0;
   double fu, fv, ax, ay, a[4], b[4], c[4], d[4];
   unsigned ch;
   if (ti > reg3d[0x55F] || si > reg3d[0x559])
   {
      bad_state("descriptor out of its pool");
      memset(out, 0, 4 * sizeof(double));
      return;
   }
   if (!tic_fresh || !tex_fresh)
      bad_state("descriptors or textures not invalidated this frame");
   if (frame_slice < 0)
      frame_slice = (int)(ti / NXG_3D_TICS);
   else if (frame_slice != (int)(ti / NXG_3D_TICS))
      tic_slice_bad++;
   if (     t[0] != (0x08u | (2u << 7) | (2u << 10) | (2u << 13) | (2u << 16)
               | (2u << 19) | (3u << 22) | (4u << 25) | (5u << 28))
         || ((t[2] >> 21) & 7) != 2 || ((t[4] >> 23) & 15) != 7
         || !(t[5] >> 31) || (t[1] & 31) || (((t[3] & 0xFFFF) << 5) & 31))
      bad_state("texture descriptor");
   if ((s[0] & 0x1FF) != (2u | (2u << 3) | (2u << 6))
         || (s[1] & 3) != 2 || ((s[1] >> 4) & 3) != 2)
      bad_state("sampler descriptor");
   w  = (int)(t[4] & 0xFFFF) + 1;
   hh = (int)(t[5] & 0xFFFF) + 1;
   fu = u * w - 0.5;
   fv = v * hh - 0.5;
   x0 = (int)floor(fu);
   y0 = (int)floor(fv);
   ax = fu - x0;
   ay = fv - y0;
   texel(t, x0, y0, a);
   texel(t, x0 + 1, y0, b);
   texel(t, x0, y0 + 1, c);
   texel(t, x0 + 1, y0 + 1, d);
   for (ch = 0; ch < 4; ch++)
      out[ch] = (a[ch] * (1 - ax) + b[ch] * ax) * (1 - ay)
              + (c[ch] * (1 - ax) + d[ch] * ax) * ay;
}

typedef struct { double x, y, u, v, c[4]; } vtx3d_t;

static void vertex3d(unsigned i, vtx3d_t *o)
{
   const float *m = (const float*)xlat(cbs[0][2].addr);
   uint64_t base  = raddr(0x701);
   double in[3][4];
   double p[4];
   unsigned a, k, r;
   for (a = 0; a < 3; a++)
   {
      uint32_t s = reg3d[0x458 + a];
      unsigned n = ((s >> 21) & 63) == 0x04 ? 2 : 4;
      uint64_t at = base + (uint64_t)i * (reg3d[0x700] & 0xFFF)
                  + ((s >> 7) & 0x3FFF);
      const float *f = (const float*)xlat(at);
#ifdef __SWITCH2__
      if (at + n * 4 > base + raddr(0x180))
#else
      if (at + n * 4 - 1 > raddr(0x7C0))
#endif
         bad_state("vertex beyond its limit");
      for (k = 0; k < 4; k++)
         in[a][k] = k < n ? f[k] : (k == 3 ? 1.0 : 0.0);
   }
   /* mvp * vec4(pos, 0, 1), the matrix column-major */
   for (r = 0; r < 4; r++)
      p[r] = m[r] * in[0][0] + m[4 + r] * in[0][1] + m[12 + r];
   o->x = rf(0x280) * (p[0] / p[3]) + rf(0x283);
   o->y = rf(0x281) * (p[1] / p[3]) + rf(0x284);
   o->u = in[1][0];
   o->v = in[1][1];
   for (k = 0; k < 4; k++)
      o->c[k] = in[2][k];
}

static double edge3d(const vtx3d_t *a, const vtx3d_t *b, double px, double py)
{
   return (b->x - a->x) * (py - a->y) - (b->y - a->y) * (px - a->x);
}

static bool top_left(const vtx3d_t *a, const vtx3d_t *b)
{
   double dx = b->x - a->x, dy = b->y - a->y;
   return dy > 0 || (dy == 0 && dx < 0);
}

static void triangle3d(const vtx3d_t *v0, const vtx3d_t *v1,
      const vtx3d_t *v2)
{
   uint8_t *dst = xlat(raddr(0x200));
   unsigned pitch = SW_ALIGN(reg3d[0x202] * 4, 64);
   double area = edge3d(v0, v1, v2->x, v2->y);
   int x, y, x0, x1, y0, y1;
   int clx0 = (int)(reg3d[0x381] & 0xFFFF), clx1 = (int)(reg3d[0x381] >> 16);
   int cly0 = (int)(reg3d[0x382] & 0xFFFF), cly1 = (int)(reg3d[0x382] >> 16);
   if (area == 0)
      return;
   if (area < 0)
   {
      const vtx3d_t *t = v1;
      v1   = v2;
      v2   = t;
      area = -area;
   }
   if (clx1 > (int)(reg3d[0x3FD] >> 16)) clx1 = (int)(reg3d[0x3FD] >> 16);
   if (cly1 > (int)(reg3d[0x3FE] >> 16)) cly1 = (int)(reg3d[0x3FE] >> 16);
   x0 = (int)floor(MIN(v0->x, MIN(v1->x, v2->x)));
   x1 = (int)ceil(MAX(v0->x, MAX(v1->x, v2->x)));
   y0 = (int)floor(MIN(v0->y, MIN(v1->y, v2->y)));
   y1 = (int)ceil(MAX(v0->y, MAX(v1->y, v2->y)));
   if (x0 < clx0) x0 = clx0;
   if (y0 < cly0) y0 = cly0;
   if (x1 > clx1) x1 = clx1;
   if (y1 > cly1) y1 = cly1;
   for (y = y0; y < y1; y++)
      for (x = x0; x < x1; x++)
      {
         double px = x + 0.5, py = y + 0.5;
         double w0 = edge3d(v1, v2, px, py);
         double w1 = edge3d(v2, v0, px, py);
         double w2 = edge3d(v0, v1, px, py);
         double t[4], s[4];
         uint32_t *d, o = 0;
         unsigned ch;
         if (     (w0 < 0 || (w0 == 0 && !top_left(v1, v2)))
               || (w1 < 0 || (w1 == 0 && !top_left(v2, v0)))
               || (w2 < 0 || (w2 == 0 && !top_left(v0, v1))))
            continue;
         w0 /= area;
         w1 /= area;
         w2 /= area;
         sample3d(w0 * v0->u + w1 * v1->u + w2 * v2->u,
               w0 * v0->v + w1 * v1->v + w2 * v2->v, t);
         for (ch = 0; ch < 4; ch++)
            s[ch] = t[ch] * (w0 * v0->c[ch] + w1 * v1->c[ch]
                  + w2 * v2->c[ch]);
         d = (uint32_t*)bl_addr(dst, pitch, (unsigned)x * 4, (unsigned)y);
         for (ch = 0; ch < 4; ch++)
         {
            double v = s[ch];
            if (reg3d[0x4D8])
               v += ((*d >> (ch * 8)) & 255) / 255.0 * (1.0 - s[3]);
            v = v < 0 ? 0 : v > 1 ? 1 : v;
            o |= (uint32_t)(v * 255.0 + 0.5) << (ch * 8);
         }
         *d = o;
      }
}

#ifdef NXG_3D_FX
#ifdef __SWITCH2__
/* Whether the program in @slot is @words' header and code */
typedef const uint32_t *fxblob_t;
static bool program_is(unsigned slot, const uint32_t *words, size_t size)
{
   return !memcmp(xlat(raddr(0x805 + slot * 0x10)), words, size);
}
#define PROGRAM_IS(slot, a) program_is(slot, a, sizeof(a))
#else
/* Whether the program in @slot is @blob's code */
typedef const uint8_t *fxblob_t;
static bool program_is(unsigned slot, const uint8_t *blob)
{
   uint64_t region = raddr(0x582);
   uint32_t off    = reg3d[0x801 + slot * 0x10];
   uint32_t entry  = nxg_le32(prog_hdr(blob) + 4);
   return off >= entry && !memcmp(xlat(region + off - entry),
         blob + nxg_le32(blob + 8), nxg_le32(blob + 12))
      && reg3d[0x803 + slot * 0x10] == nxg_le32(prog_hdr(blob) + 8);
}
#define PROGRAM_IS(slot, a) program_is(slot, a)
#endif

/* An effect's draw as the engine saw it, for the lanes to read */
typedef struct
{
   fxblob_t vs, fs;
   uint64_t va, limit, ubo_v, ubo_f;
   uint32_t blend[6], blend_en, va_cfg, attr[2], count, sc[2];
   uint32_t ubo_size_v, ubo_size_f, d8;
   uint32_t gprs[2], prefetch[2], binding[2];
   float    xf[6];
   float    ubo[204];
   unsigned shader_invalidates;
   bool     ok;
} fxrec_t;
static fxrec_t fxrec[16];
static unsigned fxrecs, shader_invalidates;

static void effect3d(void)
{
   fxrec_t *r = &fxrec[fxrecs++ & 15];
   unsigned i;
   memset(r, 0, sizeof(*r));
#ifdef __SWITCH2__
#define FX_VS(a) if (PROGRAM_IS(1, a)) r->vs = a
#define FX_FS(a) if (PROGRAM_IS(5, a)) r->fs = a
   FX_VS(nxg_s2_fx_quad_vert);
   FX_VS(nxg_s2_fx_ribbon_vert);
   FX_VS(nxg_s2_fx_ribbon_simple_vert);
   FX_FS(nxg_s2_fx_ribbon_frag);
   FX_FS(nxg_s2_fx_ribbon_simple_frag);
   FX_FS(nxg_s2_fx_snow_simple_frag);
   FX_FS(nxg_s2_fx_snow_frag);
   FX_FS(nxg_s2_fx_bokeh_frag);
   FX_FS(nxg_s2_fx_snowflake_frag);
#else
#define FX_VS(a) if (PROGRAM_IS(1, a)) r->vs = a
#define FX_FS(a) if (PROGRAM_IS(5, a)) r->fs = a
   FX_VS(deko3d_fx_quad_vert_dksh);
   FX_VS(deko3d_fx_ribbon_vert_dksh);
   FX_VS(deko3d_fx_ribbon_simple_vert_dksh);
   FX_FS(deko3d_fx_ribbon_frag_dksh);
   FX_FS(deko3d_fx_ribbon_simple_frag_dksh);
   FX_FS(deko3d_fx_snow_simple_frag_dksh);
   FX_FS(deko3d_fx_snow_frag_dksh);
   FX_FS(deko3d_fx_bokeh_frag_dksh);
   FX_FS(deko3d_fx_snowflake_frag_dksh);
#endif
#undef FX_VS
#undef FX_FS
   for (i = 0; i < 6; i++)
      r->blend[i] = reg3d[0x781 + i];
   for (i = 0; i < 6; i++)
      r->xf[i] = rf(0x280 + i);
   r->blend_en   = reg3d[0x4D8];
   r->va_cfg     = reg3d[0x700];
   r->attr[0]    = reg3d[0x459];
   r->attr[1]    = reg3d[0x45A];
   r->va         = raddr(0x701);
#ifdef __SWITCH2__
   r->limit      = r->va + raddr(0x180) - 1;
#else
   r->limit      = raddr(0x7C0);
#endif
   r->count      = reg3d[0x35E];
   r->sc[0]      = reg3d[0x381];
   r->sc[1]      = reg3d[0x382];
   r->d8         = reg3d[0x0D8];
   r->gprs[0]    = reg3d[0x813];
   r->gprs[1]    = reg3d[0x853];
   r->prefetch[0] = reg3d[0x817];
   r->prefetch[1] = reg3d[0x857];
   r->binding[0] = reg3d[0x814];
   r->binding[1] = reg3d[0x854];
   r->ubo_v      = cbs[0][2].valid ? cbs[0][2].addr : 0;
   r->ubo_f      = cbs[4][2].valid ? cbs[4][2].addr : 0;
   r->ubo_size_v = cbs[0][2].size;
   r->ubo_size_f = cbs[4][2].size;
   r->shader_invalidates = shader_invalidates;
   if (r->ubo_v)
      memcpy(r->ubo, xlat(r->ubo_v), sizeof(r->ubo));
   r->ok = r->vs && r->fs && !cbs[0][1].valid && !cbs[4][1].valid
        && reg3d[0x586] == NXG_3D_PRIM_STRIP && reg3d[0x35D] == 0
        && reg3d[0x3D0] == 0 && reg3d[0x380] == 1;
}

#endif

static void draw3d(void)
{
   unsigned n = reg3d[0x35E], i;
   vtx3d_t a, b, c;
   draws3d++;
   if (pending2d)
      hazards++;
   pending3d = true;
#ifdef NXG_3D_FX
   /* An effect's programs: its state recorded, not shaded */
#ifdef __SWITCH2__
   if (     !PROGRAM_IS(1, nxg_s2_menu_vsh)
         || !PROGRAM_IS(5, nxg_s2_menu_fsh))
#else
   if (     !program_is(1, deko3d_menu_vsh_dksh)
         || !program_is(5, deko3d_menu_fsh_dksh))
#endif
   {
      effect3d();
      return;
   }
#endif
   if (reg3d[0x586] != NXG_3D_PRIM_STRIP || reg3d[0x35D] != 0 || n < 3
         || !check_state())
   {
      bad_words++;
      return;
   }
   vertex3d(0, &a);
   vertex3d(1, &b);
   for (i = 2; i < n; i++)
   {
      vertex3d(i, &c);
      triangle3d(&a, &b, &c);
      a = b;
      b = c;
   }
}

#ifndef __SWITCH2__
/* --- the MME, and the firmware behind it ------------------------------
 *
 * The Fermi macro engine as envytools and mesa's mme_fermi describe it,
 * the ops the driver's macro may use: add immediate, read shadow state,
 * branch. Sends go to the 3D engine's methods; a write to
 * FirmwareCall[4] is a firmware register write, recorded with the
 * arguments then in MmeFirmwareArgs[1..2], acknowledged by setting
 * MmeFirmwareArgs[0] to 1 two reads later. */

static uint32_t mme_iram[0x400], mme_start[0x80];
static unsigned mme_iram_pos, mme_start_pos, mme_bad, fw_acks_late;
static struct { uint32_t reg, v, mask; } fw_calls[16];
static unsigned fw_count, mme_reads;
static int fw_ack_in = -1;
static bool fw_mute;

static void method3d(unsigned m, uint32_t v);

static void mme_send(uint32_t *maddr, uint32_t v)
{
   unsigned m = *maddr & 0xFFF;
   if (m == 0x8C4)
   {
      fw_calls[fw_count & 15].reg  = v;
      fw_calls[fw_count & 15].v    = reg3d[0xD01];
      fw_calls[fw_count & 15].mask = reg3d[0xD02];
      fw_count++;
      fw_ack_in = fw_mute ? -1 : 2;
   }
   else
      method3d(m, v);
   *maddr = (*maddr & ~0xFFFu) | ((m + ((*maddr >> 12) & 0x3F)) & 0xFFF);
}

static void mme_run(unsigned id, const uint32_t *par, unsigned np)
{
   uint32_t r[8], maddr = 0;
   unsigned pc = mme_start[id & 0x7F], pi = 1, steps = 0;
   bool exiting = false;
   memset(r, 0, sizeof(r));
   r[1] = np ? par[0] : 0;
   for (;;)
   {
      uint32_t o = mme_iram[pc & 0x3FF], v = 0;
      unsigned op = o & 7, res = (o >> 4) & 7, d = (o >> 8) & 7;
      unsigned a = (o >> 11) & 7;
      int imm = (int)(o >> 14);
      if (imm & 0x20000)
         imm -= 0x40000;
      if (++steps > 100000)
      {
         mme_bad++;
         return;
      }
      if (op == 7)
      {
         bool t = ((o >> 4) & 1) ? r[a] != 0 : r[a] == 0;
         if ((o & 0x80) || !((o >> 5) & 1))
         {
            mme_bad++; /* the macro's branches carry no delay slot */
            return;
         }
         pc = t ? pc + imm : pc + 1;
         continue;
      }
      if (op == 1)
         v = r[a] + (uint32_t)imm;
      else if (op == 5)
      {
         v = reg3d[(r[a] + (uint32_t)imm) & 0xFFF];
         mme_reads++;
         if (fw_ack_in > 0 && --fw_ack_in == 0)
            reg3d[0xD00] = 1;
      }
      else
      {
         mme_bad++;
         return;
      }
      switch (res)
      {
         case 0: /* the next parameter */
            if (d)
               r[d] = pi < np ? par[pi++] : 0;
            break;
         case 1:
            if (d) r[d] = v;
            break;
         case 2:
            if (d) r[d] = v;
            maddr = v;
            break;
         case 4:
            if (d) r[d] = v;
            mme_send(&maddr, v);
            break;
         default:
            mme_bad++;
            return;
      }
      if (exiting)
         break;
      if (o & 0x80)
         exiting = true;
      pc++;
   }
   /* The next call may not overwrite arguments not yet acknowledged */
   if (reg3d[0xD00] != 1)
      fw_acks_late++;
   if (pi < np)
      mme_bad++;
}
#endif

static void method3d(unsigned m, uint32_t v)
{
   if (m >= 0x1000)
   {
      bad_words++;
      return;
   }
#ifdef __SWITCH2__
   /* AMPERE_B has none of these: the program region and offsets, the
    * vertex limits, the bindless slot, the Fermi macro engine and the
    * firmware calls */
   if (     m == 0x582 || m == 0x583 || (m >= 0x7C0 && m < 0x7E0)
         || m == 0x982 || (m >= 0x800 && m < 0x860 && (m & 0xF) == 1)
         || (m >= 0x045 && m <= 0x048) || (m >= 0x8C0 && m < 0x8E0))
   {
      bad_state("a method AMPERE_B does not have");
      bad_words++;
      return;
   }
#endif
   if (m == NXG_3D_CB_DATA)
   {
      /* Into the selected buffer, the offset stepping */
      uint64_t a  = raddr(0x8E1);
      uint32_t of = reg3d[0x8E3];
      if ((a & 0xFF) || of + 4 > reg3d[0x8E0])
         bad_words++;
      else
         memcpy(xlat(a + of), &v, 4);
      reg3d[0x8E3] = of + 4;
      return;
   }
#ifndef __SWITCH2__
   if (m == NXG_3D_MME_IRAM + 1)
   {
      mme_iram[mme_iram_pos++ & 0x3FF] = v;
      return;
   }
   if (m == NXG_3D_MME_START + 1)
   {
      mme_start[mme_start_pos++ & 0x7F] = v;
      return;
   }
   reg3d[m] = v;
   if (m == NXG_3D_MME_IRAM)
      mme_iram_pos = v;
   else if (m == NXG_3D_MME_START)
      mme_start_pos = v;
#else
   reg3d[m] = v;
   if (0)
      ;
#endif
   else if (m >= 0x904 && m < 0x904 + 5 * 8 && !((m - 0x904) & 7))
   {
      unsigned st = (m - 0x904) / 8, idx = (v >> 4) & 31;
      if (idx >= 18)
         bad_words++;
      else
      {
         cbs[st][idx].valid = v & 1;
         cbs[st][idx].addr  = raddr(0x8E1);
         cbs[st][idx].size  = reg3d[0x8E0];
      }
   }
   else if (m == NXG_3D_WAIT_FOR_IDLE)
   {
      wfi3d++;
      pending2d = pending3d = false;
   }
   else if (m == NXG_3D_INVALIDATE_TIC)
      tic_fresh = true;
#ifdef NXG_3D_FX
   else if (m == NXG_3D_INVALIDATE_SHADER)
      shader_invalidates++;
#endif
   else if (m == NXG_3D_INVALIDATE_TEX)
      tex_fresh = true;
   else if (m == NXG_3D_VERTEX_END)
      draw3d();
}
#endif

static void method(unsigned sub, unsigned m, uint32_t v)
{
   if (m == 0)
   {
      if (     !(sub == NXG_SUB_3D && v == NXG_CLASS_3D)
            && !(sub == NXG_SUB_2D && v == NXG_CLASS_2D))
         bad_words++;
      return;
   }
   if (m < 0x40)
   {
#ifdef __SWITCH2__
      /* host: the wait for idle and the L2 flush before the fence */
      if (m == NXG_HOST_WFI)
         host_wfi++;
      else if (m == NXG_HOST_MEMOP_A + 3 && v == NXG_MEMOP_L2_FLUSH_DIRTY)
         host_flush++;
      else if (m < NXG_HOST_MEMOP_A || m > NXG_HOST_MEMOP_A + 3)
         bad_words++;
#else
      /* host: the syncpoint wait */
      if (m != NXG_HOST_SYNCPT_PAYLOAD && m != NXG_HOST_SYNCPT_PAYLOAD + 1)
         bad_words++;
#endif
      return;
   }
   if (sub == NXG_SUB_3D)
   {
      if (m == NXG_3D_SYNCPT_ACTION)
      {
         if ((v & NXG_SYNCPT_INCR) && (v & 0xfff) == 9)
            syncpt_value++;
         else
            bad_words++;
      }
      else if (m != NXG_3D_FLUSH)
#ifdef NXG_3D
         method3d(m, v);
#else
         bad_words++;
#endif
      return;
   }
   if (sub != NXG_SUB_2D || m >= 0x800)
   {
      bad_words++;
      return;
   }
   reg2d[m] = v;
   if (m == 0x237)
      run_blit();
}

static void run_words(const uint32_t *w, unsigned n)
{
   unsigned i = 0;
   while (i < n)
   {
      uint32_t h    = w[i++];
      unsigned m    = h & 0x1fff, sub = (h >> 13) & 7;
      unsigned cnt  = (h >> 16) & 0x1fff, mode = h >> 29, k;
      words_run++;
      if (mode == NXG_INLINE)
         method(sub, m, cnt);
#ifndef __SWITCH2__
      else if (mode == NXG_ONCE && sub == NXG_SUB_3D && m >= 0xE00
            && !(m & 1) && cnt && i + cnt <= n)
      {
         /* A macro call: its parameters, then the macro */
         mme_run((m - 0xE00) / 2, &w[i], cnt);
         i += cnt;
      }
      else if (mode == NXG_ONCE && sub == NXG_SUB_3D && m < 0xE00
            && cnt && i + cnt <= n)
      {
         for (k = 0; k < cnt; k++)
            method(sub, k ? m + 1 : m, w[i++]);
      }
#endif
      else if (mode == NXG_INCR || mode == NXG_NONINCR)
      {
         if (i + cnt > n)
         {
            bad_words++;
            return;
         }
         for (k = 0; k < cnt; k++)
            method(sub, mode == NXG_INCR ? m + k : m, w[i++]);
      }
      else
      {
         bad_words++;
         return;
      }
   }
}

/* --- the channel ------------------------------------------------------ */

static struct { iova_t at; unsigned n; } entries[64];
static unsigned num_entries;

Result nvGpuChannelCreate(NvGpuChannel *c, NvAddressSpace *as,
      NvChannelPriority prio)
{
   (void)as; (void)prio;
   memset(c, 0, sizeof(*c));
   c->fence.id = 9;
   return 0;
}
void   nvGpuChannelClose(NvGpuChannel *c) { (void)c; }
Result nvGpuChannelAppendEntry(NvGpuChannel *c, iova_t start, size_t n,
      u32 flags, u32 thr)
{
   (void)c; (void)thr;
   if (flags != GPFIFO_ENTRY_NOT_MAIN || num_entries >= 64)
      bad_words++;
   else
   {
      entries[num_entries].at = start;
      entries[num_entries].n  = (unsigned)n;
      num_entries++;
   }
   return 0;
}
Result nvGpuChannelKickoff(NvGpuChannel *c)
{
   unsigned i;
#ifdef NXG_3D
   /* A frame: its descriptors and textures fresh from its own
    * invalidates, its pool slice one; its own buffer, so no engine
    * waits on the last frame's */
   tic_fresh   = tex_fresh = false;
   pending2d   = pending3d = false;
   frame_slice = -1;
#endif
   for (i = 0; i < num_entries; i++)
      run_words((const uint32_t*)xlat(entries[i].at), entries[i].n);
   num_entries     = 0;
   c->fence.value += c->fence_incr;
   c->fence_incr   = 0;
   return 0;
}
u32  nvGpuChannelGetSyncpointId(NvGpuChannel *c) { return c->fence.id; }
void nvGpuChannelGetFence(NvGpuChannel *c, NvFence *f)
{
   f->id    = c->fence.id;
   f->value = c->fence.value + c->fence_incr;
}
void nvGpuChannelIncrFence(NvGpuChannel *c) { c->fence_incr++; }
void nvMultiFenceCreate(NvMultiFence *mf, const NvFence *f)
{
   mf->num_fences = 1;
   mf->fences[0]  = *f;
}
Result nvFenceWait(NvFence *f, s32 timeout)
{
   (void)timeout;
   return syncpt_value >= f->value ? 0 : 1;
}

/* --- the window -------------------------------------------------------- */

static unsigned win_w, win_h, crop_w, crop_h, configured, swap_interval;
static int next_slot, queued_slot = -1;
static NvMultiFence queued_fence;
static NvGraphicBuffer buf_cfg;
static unsigned queued_count;
static AppletOperationMode op_mode = AppletOperationMode_Handheld;

NWindow *nwindowGetDefault(void) { return (NWindow*)&win_w; }
Result nwindowSetDimensions(NWindow *nw, u32 w, u32 h)
{
   (void)nw;
   win_w = w;
   win_h = h;
   return 0;
}
Result nwindowConfigureBuffer(NWindow *nw, s32 slot, NvGraphicBuffer *b)
{
   (void)nw; (void)slot;
   buf_cfg = *b;
   configured++;
   return 0;
}
Result nwindowDequeueBuffer(NWindow *nw, s32 *slot, NvMultiFence *mf)
{
   (void)nw;
   *slot          = next_slot;
   next_slot      = (next_slot + 1) % NXG_BUFFERS;
   mf->num_fences = 1;
   mf->fences[0].id    = 3;
   mf->fences[0].value = 0;
   return 0;
}
Result nwindowQueueBuffer(NWindow *nw, s32 slot, const NvMultiFence *mf)
{
   (void)nw;
   queued_slot  = slot;
   queued_fence = *mf;
   queued_count++;
   return 0;
}
Result nwindowReleaseBuffers(NWindow *nw) { (void)nw; configured = 0; return 0; }
Result nwindowSetCrop(NWindow *nw, s32 l, s32 t, s32 r, s32 b)
{
   (void)nw;
   crop_w = (unsigned)(r - l);
   crop_h = (unsigned)(b - t);
   return 0;
}
Result nwindowSetSwapInterval(NWindow *nw, u32 v) { (void)nw; swap_interval = v; return 0; }
AppletOperationMode appletGetOperationMode(void) { return op_mode; }

#ifdef __SWITCH2__
/* --- the Switch 2: GSV, nvhost and VI2 ----------------------------------- */
static struct { uint8_t *cpu; uint64_t size; bool live; } ranges[MAX_VMAPS];
static uint32_t producer_counter, kickoffs;
static unsigned images_live, image_mem_live, layer_held, regs_bad;
static Vi2PresentOptions last_opts;
static Vi2Dependency last_producer;
static struct Vi2Layer { int dummy; } the_layer;

Result svcUnknown80(void *a, u64 s, u32 m, u32 v) { (void)a; (void)s; (void)m; (void)v; return 0; }
void   svcSleepThread(s64 n) { (void)n; }
u64    armGetSystemTick(void) { return 1; }
Result eventWait(Event *t, u64 to) { (void)t; (void)to; return 0; }
Result eventClear(Event *t) { (void)t; return 0; }
void   eventClose(Event *t) { (void)t; }

Result nvOpen(u32 *fd, const char *path)
{
   if (!strcmp(path, "/dev/nvhost-ctrl-gpu")) *fd = 1;
   else if (!strcmp(path, "/dev/nvhost-as-gpu")) *fd = 2;
   else if (!strcmp(path, "/dev/nvhost-tsg-gpu")) *fd = 3;
   else if (!strcmp(path, "/dev/nvhost-gpu")) *fd = 4;
   else return 1;
   return 0;
}
Result nvClose(u32 fd) { (void)fd; return 0; }
Result nvIoctl(u32 fd, u32 req, void *argp)
{
   if (req == NXG_AS_GET_VA_REGIONS)
      ((u32*)argp)[2] = 24; /* one region */
   else if (req == NXG_CH_ALLOC_GPFIFO_EX2)
      ((u32*)argp)[3] = 5;  /* producer id */
   else if (req == NXG_CH_SETUP_USERD)
      ((u64*)argp)[0] = 0x1000;
   else if (req == NXG_CH_ALLOC_OBJ_CTX && ((u32*)argp)[0] != NXG_CLASS_3D)
      regs_bad++;
   else if (req == NXG_AS_INITIALIZE_EX && ((u32*)argp)[0] != 5)
      regs_bad++;
   (void)fd;
   return 0;
}
Result nvIoctl3(u32 fd, u32 req, void *argp, void *out, size_t n)
{
   (void)fd; (void)n;
   if (req == NXG_GP_GET_ENGINE_LIST)
   {
      u32 *e = (u32*)out;
      ((u32*)argp)[0] = 32; /* two records: a copy engine, then graphics */
      e[0] = 1; e[1] = 0; e[2] = 3; e[3] = 0;
      e[4] = 0; e[5] = 0; e[6] = 7; e[7] = 0;
   }
   else if (req == NXG_AS_GET_VA_REGIONS)
   {
      struct { u64 offset; u32 page_size; u32 r; u64 pages; } *rg = out;
      rg->offset    = 0x100000000ULL;
      rg->page_size = 0x10000;
      rg->pages     = 0x10000;
   }
   return 0;
}
Result nvIoctl2(u32 fd, u32 req, void *argp, const void *in, size_t n)
{
   struct { u64 reserved; u32 num_entries; u32 flags; u64 fence; } *d = argp;
   const u32 *e = (const u32*)in;
   u32 i;
#ifdef NXG_3D
   tic_fresh   = tex_fresh = false;
   pending2d   = pending3d = false;
   frame_slice = -1;
#endif
   if (fd != 4 || req != NXG_CH_KICKOFF_PB || d->flags != NXG_KICKOFF_FLAGS
         || n != d->num_entries * 8)
   {
      bad_words++;
      return 1;
   }
   for (i = 0; i < d->num_entries; i++)
   {
      uint64_t iova = e[2 * i] | ((uint64_t)(e[2 * i + 1] & 0xff) << 32);
      if (!(e[2 * i + 1] & (1u << 9)))
         bad_words++;
      run_words((const uint32_t*)xlat(iova), (e[2 * i + 1] >> 10) & 0x1fffff);
   }
   kickoffs++;
   producer_counter++;
   syncpt_value = producer_counter;
   d->fence     = 5u | ((u64)producer_counter << 32);
   return 0;
}

Result gsvRegisterFrame(u64 m, u64 *o) { (void)m; (void)o; return 0; }
Result gsvRegisterVm(u32 v) { (void)v; return 0; }
void   gsvMakeImageLayout(GsvImageLayout *o, u32 w, u32 h, u32 bh, u32 pg,
      u32 fmt, u8 bf, u16 l)
{
   memset(o, 0, sizeof(*o));
   if (w != NXG_FB_W || h != NXG_FB_H || bh != 7 || pg != NXG_FB_PITCH / 64
         || fmt != 1 || bf != 1 || l)
      regs_bad++;
}
Result gsvGetImageSize(const GsvImageLayout *l, u64 *s) { (void)l; *s = NXG_FB_SIZE; return 0; }
Result gsvReserveImageMemory(u8 t, u64 s, u32 *id)
{
   static u32 next = 50;
   if (t != 3 || s != NXG_FB_SIZE)
      regs_bad++;
   *id = next++;
   image_mem_live++;
   return 0;
}
Result gsvRegisterImageMemory(u32 id, u64 cpu, u64 size, u32 range, u64 off)
{
   (void)id; (void)size;
   if (!ranges[range].live || (uint8_t*)(uintptr_t)cpu != ranges[range].cpu + off)
      regs_bad++;
   return 0;
}
static u32 next_image = 100;
Result gsvCreateImage(const GsvImageLayout *l, u32 m, u32 *id) { (void)l; (void)m; *id = next_image++; images_live++; return 0; }
Result gsvCreateDisplayAlias(u32 i, u32 c) { (void)i; (void)c; return 0; }
Result gsvDestroyDisplayAlias(u32 i) { (void)i; return 0; }
Result gsvDestroyImage(u32 i) { (void)i; images_live--; return 0; }
Result gsvReleaseImageMemory(u32 m) { (void)m; image_mem_live--; return 0; }
Result gsvOpenSync(GsvSync *o) { memset(o, 0, sizeof(*o)); return 0; }
void   gsvSyncClose(GsvSync *s) { (void)s; }
Result gsvSyncAllocateSlot(GsvSync *s, u32 *o) { (void)s; *o = 2; return 0; }
void   gsvSyncFreeSlot(GsvSync *s, u32 slot) { (void)s; (void)slot; }
Result gsvSyncReadCounter(GsvSync *s, u32 id, u64 *v, u8 *mode)
{
   (void)s;
   if (id != 5)
      regs_bad++;
   *v = producer_counter;
   if (mode)
      *mode = 0;
   return 0;
}
Result gsvSyncCreateEvent(GsvSync *s, u32 sl, Event *e, bool a) { (void)s; (void)sl; (void)e; (void)a; return 0; }
Result gsvSyncArmEvent(GsvSync *s, u32 sl, u32 id, u64 t, u8 m) { (void)s; (void)sl; (void)id; (void)t; (void)m; return 0; }
Result gsvSyncDisarmEvent(GsvSync *s, u32 sl) { (void)s; (void)sl; return 0; }
Result gsvSyncWaitNative(GsvSync *s, const GsvNativeSync *n, u64 t) { (void)s; (void)n; (void)t; return 0; }
Result gsvOpenGpu(GsvGpu *o) { memset(o, 0, sizeof(*o)); return 0; }
void   gsvGpuClose(GsvGpu *g) { (void)g; }
Result gsvGpuGetSlotRange(GsvGpu *g, u32 *f, u32 *l) { (void)g; *f = 0; *l = 1; return 0; }
Result gsvGpuCreateSlotEvent(GsvGpu *g, u32 s, Event *e, bool a) { (void)g; (void)s; (void)e; (void)a; return 0; }
Result gsvGpuOpenWorker(GsvGpu *g, u32 s, Event *e) { (void)g; (void)s; (void)e; return 0; }
Result gsvGpuRegisterClient(GsvGpu *g, u64 n, u8 *c) { (void)g; (void)n; *c = 3; return 0; }
Result gsvGpuReleaseClient(GsvGpu *g, u8 c) { (void)g; (void)c; return 0; }
Result gsvGpuCreateVm(GsvGpu *g, u32 *vm) { (void)g; *vm = 11; return 0; }
Result gsvGpuGetVmUserRanges(GsvGpu *g, u32 vm, GsvGpuVaRange r[4], u32 *n)
{
   (void)g; (void)vm;
   r[0].first = 0x100000000ULL;
   r[0].last  = 0x1ffffffffULL;
   *n         = 1;
   return 0;
}
Result gsvGpuRegisterMemory(GsvGpu *g, u64 addr, u64 size, u8 c, u32 *id)
{
   unsigned i;
   (void)g;
   if (c != 3 || (addr & 0xfff) || (size & 0xfff))
      regs_bad++;
   for (i = 1; i < MAX_VMAPS; i++)
      if (!ranges[i].live)
      {
         ranges[i].cpu  = (uint8_t*)(uintptr_t)addr;
         ranges[i].size = size;
         ranges[i].live = true;
         maps_live++;
         *id = i;
         return 0;
      }
   return 1;
}
Result gsvGpuUnregisterMemory(GsvGpu *g, u32 id, u32 slot)
{
   (void)g; (void)slot;
   if (id < MAX_VMAPS && ranges[id].live)
   {
      ranges[id].live = false;
      maps_live--;
   }
   return 0;
}
Result gsvGpuMap(GsvGpu *g, const GsvGpuMapping *m, u32 slot, u8 *kind)
{
   unsigned i;
   (void)g; (void)slot;
   if (     m->vm_id != 11 || !ranges[m->range_id].live || (m->va & 0xffff)
         || m->kind != NXG_KIND_GENERIC || m->va < 0x100000000ULL
         || m->va + m->size > 0x200000000ULL)
      regs_bad++;
   for (i = 0; i < MAX_VMAPS; i++)
      if (vmaps[i].live && m->va < vmaps[i].va + vmaps[i].size
            && vmaps[i].va < m->va + m->size)
         regs_bad++; /* overlapping a live mapping */
   for (i = 0; i < MAX_VMAPS; i++)
      if (!vmaps[i].live)
      {
         vmaps[i].va   = m->va;
         vmaps[i].size = m->size;
         vmaps[i].cpu  = ranges[m->range_id].cpu + m->range_offset;
         vmaps[i].live = true;
         break;
      }
   *kind = m->kind;
   return 0;
}
Result gsvGpuUnmap(GsvGpu *g, u32 vm, u64 va, u64 size, u32 slot)
{
   unsigned i;
   (void)g; (void)vm; (void)size; (void)slot;
   for (i = 0; i < MAX_VMAPS; i++)
      if (vmaps[i].live && vmaps[i].va == va)
         vmaps[i].live = false;
   return 0;
}
void vi2MakeBasicPresentOptions(Vi2PresentOptions *o, u32 w, u32 h, u64 t)
{
   memset(o, 0, sizeof(*o));
   o->crop_right_inclusive  = (u16)(w - 1);
   o->crop_bottom_inclusive = (u16)(h - 1);
   o->flags                 = 3;
   o->system_tick           = t;
}
Result vi2GetLayerAvailability(Vi2Layer *l, u64 *s) { (void)l; *s = 0xf; return 0; }
Result vi2PresentLayer(Vi2Layer *l, u8 slot, u32 image, const Vi2PresentOptions *o,
      const Vi2Dependency *producer, u64 *slots, Vi2Dependency *release)
{
   (void)l; (void)slot;
   last_opts     = *o;
   last_producer = *producer;
   queued_slot   = (int)((image - 100) % NXG_BUFFERS);
   queued_count++;
   *slots = 0xf;
   memset(release, 0, sizeof(*release));
   return 0;
}
Result vi2UnregisterLayerImage(Vi2Layer *l, u8 s, const Vi2PresentOptions *o,
      u64 *slots, Vi2Dependency *r) { (void)l; (void)s; (void)o; *slots = 0xf; memset(r, 0, sizeof(*r)); return 0; }
Result nwindowGetVi2Layer(NWindow *nw, Vi2Layer **out) { (void)nw; *out = &the_layer; layer_held++; return 0; }
Result nwindowReleaseVi2Layer(NWindow *nw) { (void)nw; layer_held--; return 0; }
Result nwindowGetPresentInterval(NWindow *nw, u16 *n, u16 *d) { (void)nw; *n = 1; *d = 60; return 0; }
#endif

/* The CPU path's, never reached here */
Result framebufferCreate(Framebuffer *fb, NWindow *win, u32 w, u32 h,
      u32 format, u32 n) { (void)fb; (void)win; (void)w; (void)h; (void)format; (void)n; return 1; }
Result framebufferMakeLinear(Framebuffer *fb) { (void)fb; return 1; }
static uint32_t no_fb[1280 * 720];
void  *framebufferBegin(Framebuffer *fb, u32 *s) { (void)fb; *s = 1280 * 4; bad_words++; return no_fb; }
void   framebufferEnd(Framebuffer *fb) { (void)fb; }
void   framebufferClose(Framebuffer *fb) { (void)fb; }

/* --- the frontend ------------------------------------------------------ */

uint64_t cpu_features_get(void) { return 0; }
bool video_context_driver_set(const gfx_ctx_driver_t *d) { (void)d; return true; }
static unsigned retired_frames;
void video_driver_cached_frame_retire(void) { retired_frames++; }
void video_driver_update_viewport(struct video_viewport *vp,
      bool force_full, bool keep_aspect, bool y_down)
{
   (void)force_full; (void)keep_aspect; (void)y_down;
   vp->pos  = VIDEO_POS_PACK(0, 0);
   vp->dims = vp->full_dims;
}
void video_driver_set_viewport_core(void) { }
void input_driver_video_window(enum input_window_kind window, void *d)
{
   (void)window; (void)d;
}

/* The menu: one frame's draws, set by the lane */
static void (*menu_draws)(switch_video_t *sw);
static switch_video_t *cur_sw;
void menu_driver_frame(bool alive, video_frame_info_t *vi)
{
   (void)alive; (void)vi;
   if (menu_draws)
      menu_draws(cur_sw);
}

/* A font of one 4x4 glyph, coverage 255 on its left half, 128 on its
 * right, for every codepoint */
static uint8_t atlas_px[16 * 8];
static struct font_atlas atlas;
static struct font_glyph glyph;
static struct font_line_metrics metrics = { 8.0f, 6.0f, 2.0f };
static void *fake_init(uint8_t *d, size_t l, unsigned f, float s,
      enum font_atlas_format fmt) { (void)d; (void)l; (void)f; (void)s; (void)fmt; return &atlas; }
static struct font_atlas *fake_atlas(void *d) { (void)d; return &atlas; }
static const struct font_glyph *fake_glyph(void *d, uint32_t c) { (void)d; (void)c; return &glyph; }
static void fake_free(void *d) { (void)d; }
static void fake_metrics(void *d, struct font_line_metrics **m) { (void)d; *m = &metrics; }
static const font_renderer_driver_t fake_font = {
   fake_init, fake_atlas, fake_glyph, fake_free, NULL, "fake",
   fake_metrics, true
};
int font_renderer_create_default(const font_renderer_driver_t **drv,
      void **handle, const char *path, unsigned size,
      enum font_atlas_format fmt)
{
   unsigned x, y;
   (void)path; (void)size; (void)fmt;
   memset(atlas_px, 0, sizeof(atlas_px));
   for (y = 0; y < 4; y++)
      for (x = 0; x < 4; x++)
         atlas_px[y * 16 + 8 + x] = x < 2 ? 255 : 128;
   atlas.buffer    = atlas_px;
   atlas.width     = 16;
   atlas.height    = 8;
   atlas.dirty     = true;
   atlas.dirty_xy0 = VIDEO_SCALE_PACK(0, 0);
   atlas.dirty_xy1 = VIDEO_SCALE_PACK(16, 8);
   glyph.dims          = VIDEO_SCALE_PACK(4, 4);
   glyph.atlas_pos     = VIDEO_SCALE_PACK(8, 0);
   glyph.draw_offset_x = 0;
   glyph.draw_offset_y = -4;
   glyph.advance_x     = 5;
   *drv    = &fake_font;
   *handle = &atlas;
   return 1;
}
int font_renderer_get_message_width(const font_renderer_driver_t *r,
      void *d, const char *msg, size_t len, float scale)
{
   (void)r; (void)d; (void)msg;
   return (int)(len * 5 * scale);
}
uint32_t utf8_walk(const char **s) { return (uint32_t)(unsigned char)*(*s)++; }

static void *the_font;
static font_params_resolved_t next_rp;
void font_driver_resolve_params(const struct font_params *p,
      font_params_resolved_t *out)
{
   (void)p;
   *out = next_rp;
}
void font_driver_render_msg(void *data, const char *msg, size_t len,
      const struct font_params *params, void *font_data)
{
   (void)font_data;
   switch_font_render_msg(data, the_font, msg, len, params);
}

/* --- helpers ------------------------------------------------------------- */

static video_frame_info_t vfi;
static const video_poke_interface_t *poke;
static uint8_t core_frame[320 * 240 * 4];

static switch_video_t *driver_new(unsigned w, unsigned h, bool rgb32)
{
   video_info_t info;
   switch_video_t *sw;
   memset(&info, 0, sizeof(info));
   info.dims  = VIDEO_SCALE_PACK(w, h);
   info.rgb32 = rgb32;
   info.vsync = true;
   sw         = (switch_video_t*)video_switch.init(&info);
   video_switch.poke_interface(sw, &poke);
   cur_sw     = sw;
   the_font   = switch_font_init(sw, NULL, 16.0f, false);
   return sw;
}

static void driver_free(switch_video_t *sw)
{
   switch_font_free(the_font, false);
   the_font = NULL;
   video_switch.free(sw);
}

/* The pixel at (x, y) of the buffer last shown, R low */
static uint32_t at(switch_video_t *sw, unsigned x, unsigned y)
{
   nxg_t *g = (nxg_t*)sw->gpu;
   return *(uint32_t*)bl_addr(g->fb.cpu + (size_t)queued_slot * NXG_FB_SIZE,
         NXG_FB_PITCH, x * 4, y) & 0x00ffffffu;
}

static bool near(uint32_t a, uint32_t b, unsigned tol)
{
   unsigned ch;
   for (ch = 0; ch < 24; ch += 8)
   {
      int d = (int)((a >> ch) & 255) - (int)((b >> ch) & 255);
      if (d < -(int)tol || d > (int)tol)
         return false;
   }
   return true;
}

static void frame(switch_video_t *sw, const void *data, unsigned w,
      unsigned h, unsigned pitch)
{
   video_switch.frame(sw, data, data ? VIDEO_SCALE_PACK(w, h) : 0, 1,
         pitch, NULL, &vfi);
}

/* --- lanes ----------------------------------------------------------- */

static void lane_window(void)
{
   switch_video_t *sw = driver_new(256, 224, true);
   CHECK(sw && sw->gpu, "no GPU path");
#ifdef __SWITCH2__
   CHECK(images_live == NXG_BUFFERS && image_mem_live == NXG_BUFFERS
         && layer_held == 1, "%u images, %u image memories, layer %u",
         images_live, image_mem_live, layer_held);
   frame(sw, core_frame, 256, 224, 1024);
   CHECK(last_opts.crop_right_inclusive == 1279
         && last_opts.crop_bottom_inclusive == 719
         && last_opts.interval_numerator == 1, "handheld present crop %ux%u",
         last_opts.crop_right_inclusive + 1, last_opts.crop_bottom_inclusive + 1);
   driver_free(sw);
   CHECK(!images_live && !image_mem_live && !layer_held,
         "display images or layer left at free");
   CHECK(!regs_bad, "%u GSV or nvhost calls with bad arguments", regs_bad);
#else
   CHECK(configured == NXG_BUFFERS, "%u buffers configured", configured);
   CHECK(buf_cfg.planes[0].layout == NvLayout_BlockLinear
         && buf_cfg.planes[0].kind == NvKind_Generic_16BX2
         && buf_cfg.planes[0].block_height_log2 == NXG_BLOCK_LOG2
         && buf_cfg.planes[0].pitch == NXG_FB_PITCH
         && buf_cfg.stride == NXG_FB_PITCH / 4
         && buf_cfg.planes[0].width == NXG_FB_W
         && win_w == NXG_FB_W && win_h == NXG_FB_H,
         "the window's buffers are not libnx's layout");
   CHECK(crop_w == 1280 && crop_h == 720, "handheld crop %ux%u", crop_w, crop_h);
   driver_free(sw);
   CHECK(!configured, "buffers not released");
#endif
   CHECK(maps_live == 0, "%u maps left at free", maps_live);
   printf("[pass] window buffers, crop and teardown\n");
}

static void lane_frame(bool rgb32)
{
   unsigned x, y, bad = 0, bad_border = 0;
   switch_video_t *sw = driver_new(256, 224, rgb32);
   unsigned x0 = (1280 - 256) / 2, y0 = (720 - 224) / 2;
   for (y = 0; y < 224; y++)
      for (x = 0; x < 256; x++)
      {
         unsigned r = x & 0xf8, g = y & 0xfc, b = (x + y) & 0xf8;
         if (rgb32)
            ((uint32_t*)core_frame)[y * 256 + x] = (r << 16) | (g << 8) | b;
         else
            ((uint16_t*)core_frame)[y * 256 + x] = (uint16_t)
               (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
      }
   frame(sw, core_frame, 256, 224, 256 * (rgb32 ? 4 : 2));
   for (y = 0; y < 224; y++)
      for (x = 0; x < 256; x++)
      {
         unsigned r = x & 0xf8, g = y & 0xfc, b = (x + y) & 0xf8;
         uint32_t want = r | (g << 8) | (b << 16);
         if (!near(at(sw, x0 + x, y0 + y), want, rgb32 ? 0 : 7))
            bad++;
      }
   for (y = 0; y < 720; y += 7)
      for (x = 0; x < 1280; x += 5)
         if ((x < x0 || x >= x0 + 256 || y < y0 || y >= y0 + 224)
               && at(sw, x, y) != 0)
            bad_border++;
   CHECK(!bad, "%s: %u frame pixels wrong", rgb32 ? "XRGB8888" : "RGB565", bad);
   CHECK(!bad_border, "%s: %u border pixels not black",
         rgb32 ? "XRGB8888" : "RGB565", bad_border);
   /* A dupe draws it again, from the slot it was copied into */
   memset(core_frame, 0, sizeof(core_frame));
   frame(sw, NULL, 0, 0, 0);
   CHECK(near(at(sw, x0 + 100, y0 + 50), (100 & 0xf8) | ((50 & 0xfc) << 8)
            | (((150) & 0xf8) << 16), rgb32 ? 0 : 7),
         "a dupe did not draw the last frame");
   driver_free(sw);
   printf("[pass] the core's frame, copied once (%s) and duped\n",
         rgb32 ? "XRGB8888" : "RGB565");
}

static void lane_in_place(void)
{
   struct retro_framebuffer fb;
   switch_video_t *sw = driver_new(320, 240, true);
   nxg_t *g           = (nxg_t*)sw->gpu;
   uintptr_t id;
   bool cached;
   unsigned align, x, y;
   uint8_t *mem;

   /* A lent framebuffer: drawn from where the core wrote it */
   memset(&fb, 0, sizeof(fb));
   fb.width  = 320;
   fb.height = 240;
   CHECK(poke->get_current_software_framebuffer(sw, &fb), "no framebuffer lent");
   CHECK(fb.pitch % SW_PITCH_ALIGN == 0
         && fb.memory_flags == RETRO_MEMORY_TYPE_CACHED, "lent pitch %u", (unsigned)fb.pitch);
   for (y = 0; y < 240; y++)
      for (x = 0; x < 320; x++)
         ((uint32_t*)((uint8_t*)fb.data + y * fb.pitch))[x] = 0x00102030u;
   frame(sw, fb.data, 320, 240, fb.pitch);
   CHECK(g->last_owner >= g->sw && g->last_owner < g->sw + NXG_SW_SLOTS
         && g->last_owner->mem.cpu == (uint8_t*)fb.data
         && g->last_iova == g->last_owner->mem.iova,
         "the lent framebuffer was copied");
   CHECK(at(sw, 640, 360) == 0x302010u, "lent frame drawn as %06x",
         at(sw, 640, 360));

   /* The wrapper's frame memory: the same */
   mem = (uint8_t*)poke->frame_mem_alloc(sw, 320 * 4 * 240, &id, &cached, &align);
   CHECK(mem && cached && align == SW_PITCH_ALIGN, "no frame memory");
   if (mem)
   {
      for (y = 0; y < 240; y++)
         for (x = 0; x < 320; x++)
            ((uint32_t*)mem)[y * 320 + x] = 0x00405060u;
      frame(sw, mem, 320, 240, 320 * 4);
      CHECK(g->last_owner == &g->frame_mem[id - 1], "frame memory was copied");
      CHECK(at(sw, 640, 360) == 0x605040u, "frame memory drawn as %06x",
            at(sw, 640, 360));
      frame(sw, NULL, 0, 0, 0);
      frame(sw, NULL, 0, 0, 0);
      frame(sw, NULL, 0, 0, 0);
      CHECK(poke->frame_mem_ready(sw, id), "frame memory never ready again");
      poke->frame_mem_free(sw, id);
      CHECK(!g->last_owner, "a dupe would read freed frame memory");
   }
   driver_free(sw);
   CHECK(maps_live == 0, "%u maps left", maps_live);
   printf("[pass] read in place: lent framebuffer, threaded frame memory\n");
}

/* --- the menu's quads ----------------------------------------------- */

static uintptr_t tex;
static void draws_quads(switch_video_t *sw)
{
   gfx_display_ctx_draw_t d;
   struct video_coords c;
   float red[16], grad[16], half[16];
   unsigned i;
   for (i = 0; i < 4; i++)
   {
      red[4 * i] = 1; red[4 * i + 1] = 0; red[4 * i + 2] = 0; red[4 * i + 3] = 1;
      half[4 * i] = 1; half[4 * i + 1] = 1; half[4 * i + 2] = 1; half[4 * i + 3] = 0.5f;
   }
   /* the strip's vertices are BL BR TL TR: a gradient black at the
    * bottom, white at the top */
   for (i = 0; i < 16; i++)
      grad[i] = 1.0f;
   for (i = 0; i < 2; i++)
      grad[4 * i] = grad[4 * i + 1] = grad[4 * i + 2] = 0.0f;
   memset(&c, 0, sizeof(c));
   memset(&d, 0, sizeof(d));
   c.vertices     = 4;
   d.coords       = &c;
   d.scale_factor = 1.0f;

   /* a red fill at (100, 100) 50x40, top-down: bottom-up y = 720-140 */
   c.color = red;
   d.pos   = VIDEO_POS_PACK(100, 720 - 140);
   d.dims  = VIDEO_SCALE_PACK(50, 40);
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));

   /* a gradient at (300, 100) 64x64 */
   c.color = grad;
   d.pos   = VIDEO_POS_PACK(300, 720 - 164);
   d.dims  = VIDEO_SCALE_PACK(64, 64);
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));

   /* the texture at (500, 100), 2x, at half alpha, blended */
   gfx_display_ctx_switch.blend_begin(sw);
   c.color   = half;
   d.texture = tex;
   d.pos     = VIDEO_POS_PACK(500, 720 - 108);
   d.dims    = VIDEO_SCALE_PACK(8, 8);
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));

   /* the scissor cuts a fill to its right half */
   gfx_display_ctx_switch.scissor_begin(sw, VIDEO_SCALE_PACK(1280, 720),
         720, 100, VIDEO_SCALE_PACK(20, 40));
   c.color   = red;
   d.texture = 0;
   d.pos     = VIDEO_POS_PACK(700, 720 - 140);
   d.dims    = VIDEO_SCALE_PACK(40, 40);
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));
   gfx_display_ctx_switch.scissor_end(sw, VIDEO_SCALE_PACK(1280, 720));
   gfx_display_ctx_switch.blend_end(sw);
}

static void lane_menu(void)
{
   struct texture_image img;
   uint32_t px[4 * 4];
   unsigned i;
   uint32_t top, bot, mid, t, want;
   switch_video_t *sw = driver_new(1280, 720, true);
   memset(core_frame, 0, sizeof(core_frame));
   /* a 4x4 texture, opaque green in ARGB */
   for (i = 0; i < 16; i++)
      px[i] = 0xff00ff00u;
   memset(&img, 0, sizeof(img));
   img.pixels = px;
   img.width  = 4;
   img.height = 4;
   tex        = poke->load_texture(sw, &img, false, TEXTURE_FILTER_LINEAR);
   CHECK(tex, "texture not loaded");

   vfi.menu_st_flags = MENU_ST_FLAG_ALIVE;
   menu_draws        = draws_quads;
   frame(sw, NULL, 0, 0, 0);
   menu_draws        = NULL;
   vfi.menu_st_flags = 0;

   CHECK(at(sw, 100, 100) == 0x0000ff && at(sw, 149, 139) == 0x0000ff,
         "fill not drawn");
   CHECK(at(sw, 99, 100) == 0 && at(sw, 150, 100) == 0 && at(sw, 100, 140) == 0,
         "fill outside its rectangle");
   top = at(sw, 330, 100);
   bot = at(sw, 330, 163);
   mid = at(sw, 330, 132);
   CHECK(near(top, 0xffffff, 6) && near(bot, 0x000000, 6)
         && near(mid, 0x808080, 6),
         "gradient %06x / %06x / %06x", top, mid, bot);
   /* green at half alpha over black */
   want = SW_MUL255(255u, 128u, t) << 8;
   CHECK(near(at(sw, 500, 100), want, 1) && near(at(sw, 507, 107), want, 1),
         "texture drawn as %06x", at(sw, 500, 100));
   CHECK(at(sw, 508, 100) == 0, "texture beyond its quad");
   CHECK(at(sw, 719, 120) == 0 && at(sw, 720, 120) == 0x0000ff
         && at(sw, 739, 120) == 0x0000ff, "scissor not applied");
   poke->unload_texture(sw, false, tex);
   driver_free(sw);
   CHECK(maps_live == 0, "%u maps left", maps_live);
   printf("[pass] the menu: fills, gradients, textures, blending, scissor\n");
}

static void lane_text(void)
{
   switch_video_t *sw = driver_new(1280, 720, true);
   uint32_t t, half;
   memset(&next_rp, 0, sizeof(next_rp));
   next_rp.x          = 100.0f / 1280.0f;
   next_rp.y          = 1.0f - 200.0f / 720.0f;
   next_rp.scale      = 1.0f;
   next_rp.color[0]   = 1.0f;
   next_rp.color[1]   = 1.0f;
   next_rp.color[2]   = 0.0f;
   next_rp.color[3]   = 1.0f;
   next_rp.drop_x     = 2;
   next_rp.drop_y     = -2;
   next_rp.drop_mod   = 0.0f;
   next_rp.drop_alpha = 1.0f;
   next_rp.text_align = TEXT_ALIGN_LEFT;
   video_switch.frame(sw, NULL, 0, 1, 0, "AB", &vfi);
   /* The glyph's 4x4 box sits on the baseline at y 200: rows 196..199.
    * The shadow 2 px right and down: black, then the text over it */
   half = SW_MUL255(255u, 128u, t);
   CHECK(at(sw, 100, 196) == 0x00ffff, "full coverage: %06x", at(sw, 100, 196));
   CHECK(near(at(sw, 103, 196), half | (half << 8), 1),
         "half coverage: %06x", at(sw, 103, 196));
   CHECK(at(sw, 105, 199) == 0x00ffff, "second glyph: %06x", at(sw, 105, 199));
   CHECK(at(sw, 101, 201) == 0 && at(sw, 100, 195) == 0,
         "text outside its box");
   driver_free(sw);
   printf("[pass] text: coverage, colour, shadow, advance\n");
}

static void lane_overlay(void)
{
   const video_overlay_interface_t *ov = NULL;
   struct texture_image img;
   uint32_t px[16];
   unsigned i;
   uint32_t t;
   switch_video_t *sw = driver_new(1280, 720, true);
   video_switch.overlay_interface(sw, &ov);
   for (i = 0; i < 16; i++)
      px[i] = 0xffff0000u; /* opaque red, ARGB */
   memset(&img, 0, sizeof(img));
   img.pixels = px;
   img.width  = 4;
   img.height = 4;
   CHECK(ov->load(sw, &img, 1), "overlay not loaded");
   ov->vertex_geom(sw, 0, 0.25f, 0.25f, 0.5f, 0.5f);
   ov->set_alpha(sw, 0, 0.5f);
   ov->full_screen(sw, true);
   ov->enable(sw, true);
   frame(sw, NULL, 0, 0, 0);
   CHECK(near(at(sw, 640, 360), SW_MUL255(255u, 128u, t), 1),
         "overlay drawn as %06x", at(sw, 640, 360));
   CHECK(at(sw, 319, 360) == 0 && at(sw, 960, 360) == 0,
         "overlay outside its rectangle");
   ov->load(sw, NULL, 0);
   driver_free(sw);
   CHECK(maps_live == 0, "%u maps left", maps_live);
   printf("[pass] overlay through the 2D engine\n");
}

static void lane_rgui(void)
{
   uint16_t menu[320 * 240];
   unsigned i, x0, y0;
   switch_video_t *sw = driver_new(320, 240, true);
   for (i = 0; i < 320 * 240; i++)
      ((uint32_t*)core_frame)[i] = 0x000000ffu; /* blue frame */
   /* RGBA4444, red in the high nibble: opaque white on the left half,
    * transparent on the right */
   for (i = 0; i < 320 * 240; i++)
      menu[i] = (i % 320) < 160 ? 0xffffu : 0x0000u;
   poke->set_texture_frame(sw, menu, false, VIDEO_SCALE_PACK(320, 240), 1.0f);
   poke->set_texture_enable(sw, true, false);
   frame(sw, core_frame, 320, 240, 320 * 4);
   /* 3x: 960x720 centred */
   x0 = (1280 - 960) / 2;
   y0 = 0;
   CHECK(at(sw, x0 + 10, y0 + 10) == 0xffffff, "RGUI's opaque half: %06x",
         at(sw, x0 + 10, y0 + 10));
   CHECK(at(sw, 640 + 100, 360) == 0xff0000, "the frame behind RGUI: %06x",
         at(sw, 640 + 100, 360));
   poke->set_texture_enable(sw, false, false);
   driver_free(sw);
   printf("[pass] RGUI over the frame, integer-scaled, its alpha kept\n");
}

static void lane_dock_and_fences(void)
{
   switch_video_t *sw = driver_new(256, 224, true);
   uint32_t v0;
#ifdef __SWITCH2__
   u32 id;
   uint64_t target;
   frame(sw, core_frame, 256, 224, 1024);
   memcpy(&id, &last_producer.data[0], 4);
   memcpy(&target, &last_producer.data[8], 8);
   CHECK(~id == 5u && last_producer.data[4] == 0,
         "shown behind producer %u mode %u", ~id, last_producer.data[4]);
   v0 = (uint32_t)target;
   CHECK(producer_counter >= v0, "the frame's fence never signalled");
   CHECK(host_wfi && host_flush, "no wait for idle or L2 flush before the fence");
   op_mode = AppletOperationMode_Console;
   frame(sw, core_frame, 256, 224, 1024);
   CHECK(last_opts.crop_right_inclusive == 1919
         && last_opts.crop_bottom_inclusive == 1079, "docked present crop");
   memcpy(&target, &last_producer.data[8], 8);
   CHECK((uint32_t)target == v0 + 1, "fence went %u -> %u", v0, (uint32_t)target);
#else
   frame(sw, core_frame, 256, 224, 1024);
   CHECK(queued_fence.num_fences == 1 && queued_fence.fences[0].id == 9,
         "shown behind fence %u", queued_fence.fences[0].id);
   v0 = queued_fence.fences[0].value;
   CHECK(syncpt_value >= v0, "the frame's fence never signalled");
   op_mode = AppletOperationMode_Console;
   frame(sw, core_frame, 256, 224, 1024);
   CHECK(crop_w == 1920 && crop_h == 1080, "docked crop %ux%u", crop_w, crop_h);
   CHECK(queued_fence.fences[0].value == v0 + 2, "fence went %u -> %u",
         v0, queued_fence.fences[0].value);
#endif
   /* The blue frame lane_rgui left, centred at 1080p */
   CHECK(at(sw, (1920 - 256) / 2, (1080 - 224) / 2) == 0xff0000
         && at(sw, (1920 - 256) / 2 - 1, (1080 - 224) / 2) == 0
         && at(sw, (1920 + 256) / 2 - 1, (1080 + 224) / 2 - 1) == 0xff0000
         && at(sw, (1920 + 256) / 2, (1080 + 224) / 2 - 1) == 0,
         "docked frame not centred at 1080p");
   op_mode = AppletOperationMode_Handheld;
   CHECK(swap_interval == 1, "swap interval %u", swap_interval);
#ifdef __SWITCH2__
   CHECK(!regs_bad, "%u GSV or nvhost calls with bad arguments", regs_bad);
#endif
   driver_free(sw);
   printf("[pass] docking, and frames shown behind their fences\n");
}

/* Textures share mapped blocks: loading and freeing many maps once */
static void lane_heap(void)
{
   struct texture_image img;
   uint32_t px[16 * 16];
   uintptr_t t[64];
   unsigned i, before;
   switch_video_t *sw = driver_new(256, 224, true);
   memset(px, 0x80, sizeof(px));
   memset(&img, 0, sizeof(img));
   img.pixels = px;
   img.width  = 16;
   img.height = 16;
   CHECK(atlas.inflight == NXG_FRAMES,
         "the atlas keeps cells for %u frames, not %u",
         atlas.inflight, NXG_FRAMES);
   before = maps_live;
   for (i = 0; i < 64; i++)
      t[i] = poke->load_texture(sw, &img, false, TEXTURE_FILTER_LINEAR);
   CHECK(maps_live <= before + 1, "64 textures took %u mappings",
         maps_live - before);
   for (i = 0; i < 64; i++)
      poke->unload_texture(sw, false, t[i]);
   for (i = 0; i < NXG_FRAMES + 1; i++)
      frame(sw, NULL, 0, 0, 0);
   t[0] = poke->load_texture(sw, &img, false, TEXTURE_FILTER_LINEAR);
   CHECK(t[0] && ((sw_surf_t*)t[0])->heap
         && ((nxg_t*)sw->gpu)->heap[0].free
         && ((nxg_t*)sw->gpu)->heap[0].free->next == NULL,
         "freed ranges not joined back");
   poke->unload_texture(sw, false, t[0]);
   driver_free(sw);
   CHECK(maps_live == 0, "%u maps left", maps_live);
   printf("[pass] surfaces share mapped blocks; the atlas keeps in-flight cells\n");
}

#ifdef NXG_3D
/* --- the 3D engine's draws ------------------------------------------- */

/* Box space turned a quarter left about its centre, then the default
 * projection: (x, y) -> (1 - y, x) */
static const float quarter[16] = {
   0.0f, 2.0f, 0.0f, 0.0f,
  -2.0f, 0.0f, 0.0f, 0.0f,
   0.0f, 0.0f, -1.0f, 0.0f,
   1.0f, -1.0f, 0.0f, 1.0f
};
static uintptr_t tex4, white_tex;

static void draws_3d(switch_video_t *sw)
{
   gfx_display_ctx_draw_t d;
   struct video_coords c;
   math_matrix_4x4 m;
   float half_white[16], red[16], blue[16], shade[16];
   /* two quads joined: BL BR TL TR, the join, BL BR TL TR */
   static const float sv[20] = {
      0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 1.0f, 0.25f, 1.0f,
      0.25f, 1.0f, 0.5f, 0.0f,
      0.5f, 0.0f, 0.75f, 0.0f, 0.5f, 1.0f, 0.75f, 1.0f
   };
   static const float st[20] = { 0 };
   float sc[40];
   unsigned i;
   memcpy(m.data, quarter, sizeof(quarter));
   for (i = 0; i < 4; i++)
   {
      half_white[4 * i] = 1; half_white[4 * i + 1] = 1;
      half_white[4 * i + 2] = 1; half_white[4 * i + 3] = 0.5f;
      blue[4 * i] = 0; blue[4 * i + 1] = 0; blue[4 * i + 2] = 1; blue[4 * i + 3] = 1;
      red[4 * i] = 1; red[4 * i + 1] = 0; red[4 * i + 2] = 0; red[4 * i + 3] = 1;
      /* bottom blue, top red: BL BR TL TR */
      shade[4 * i]     = i >= 2 ? 1.0f : 0.0f;
      shade[4 * i + 1] = 0.0f;
      shade[4 * i + 2] = i >= 2 ? 0.0f : 1.0f;
      shade[4 * i + 3] = 1.0f;
   }
   for (i = 0; i < 10; i++)
   {
      sc[4 * i] = i < 5 ? 1.0f : 0.0f; sc[4 * i + 1] = i < 5 ? 0.0f : 1.0f;
      sc[4 * i + 2] = 0.0f; sc[4 * i + 3] = 1.0f;
   }
   memset(&c, 0, sizeof(c));
   memset(&d, 0, sizeof(d));
   c.vertices     = 4;
   d.coords       = &c;
   d.scale_factor = 1.0f;

   /* the 2x2 texture turned a quarter, at (200, 200) 64x64 */
   d.texture   = tex4;
   d.matrix_data = &m;
   d.pos       = VIDEO_POS_PACK(200, 720 - 264);
   d.dims      = VIDEO_SCALE_PACK(64, 64);
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));

   /* a strip of two quads, at (400, 200) 100x50: red, then green */
   d.texture     = 0;
   d.matrix_data = NULL;
   c.vertices    = 10;
   c.vertex      = sv;
   c.tex_coord   = st;
   c.color       = sc;
   d.pos         = VIDEO_POS_PACK(400, 720 - 250);
   d.dims        = VIDEO_SCALE_PACK(100, 50);
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));

   /* a white texture shaded red at the top, blue at the bottom */
   c.vertices  = 4;
   c.vertex    = NULL;
   c.tex_coord = NULL;
   c.color     = shade;
   d.texture   = white_tex;
   d.pos       = VIDEO_POS_PACK(600, 720 - 164);
   d.dims      = VIDEO_SCALE_PACK(64, 64);
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));

   /* blue by the 2D engine, half white turned over it by the 3D engine,
    * then red by the 2D engine over its right half */
   d.texture = 0;
   c.color   = blue;
   d.pos     = VIDEO_POS_PACK(800, 720 - 264);
   d.dims    = VIDEO_SCALE_PACK(64, 64);
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));
   gfx_display_ctx_switch.blend_begin(sw);
   c.color       = half_white;
   d.matrix_data = &m;
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));
   gfx_display_ctx_switch.blend_end(sw);
   c.color       = red;
   d.matrix_data = NULL;
   d.pos         = VIDEO_POS_PACK(832, 720 - 264);
   d.dims        = VIDEO_SCALE_PACK(32, 64);
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));

   /* the scissor cuts a turned fill to its left half */
   gfx_display_ctx_switch.scissor_begin(sw, VIDEO_SCALE_PACK(1280, 720),
         1000, 200, VIDEO_SCALE_PACK(32, 64));
   d.matrix_data = &m;
   d.pos         = VIDEO_POS_PACK(1000, 720 - 264);
   d.dims        = VIDEO_SCALE_PACK(64, 64);
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));
   gfx_display_ctx_switch.scissor_end(sw, VIDEO_SCALE_PACK(1280, 720));
}

static uintptr_t load_px(switch_video_t *sw, const uint32_t *px,
      unsigned w, unsigned h)
{
   struct texture_image img;
   memset(&img, 0, sizeof(img));
   img.pixels = (uint32_t*)px;
   img.width  = w;
   img.height = h;
   return poke->load_texture(sw, &img, false, TEXTURE_FILTER_LINEAR);
}

static void lane_3d(void)
{
   /* ARGB: red, green / blue, white */
   static const uint32_t px4[4] = {
      0xffff0000u, 0xff00ff00u, 0xff0000ffu, 0xffffffffu
   };
   uint32_t white[16];
   unsigned d0, b0, top, bot, mid, t;
#ifndef __SWITCH2__
   static const uint32_t fw_want[6][3] = {
      { 0x00418800, 0x00000001, 0x00000001 },
      { 0x00419A08, 0x00000000, 0x00000010 },
      { 0x00419F78, 0x00000000, 0x00000008 },
      { 0x00404468, 0x07FFFFFF, 0x3FFFFFFF },
      { 0x00419A04, 0x00000001, 0x00000001 },
      { 0x00419A04, 0x00000002, 0x00000002 }
   };
   unsigned fw0 = fw_count, i;
#endif
   switch_video_t *sw = driver_new(1280, 720, true);
   nxg_t *g           = (nxg_t*)sw->gpu;
   CHECK(g && g->has3d, "no 3D engine");
   CHECK(!gfx_display_ctx_switch.handles_transform
         && gfx_display_ctx_switch.handles_vertex_strip,
         "the menus still hand over values and quads");
   memset(white, 0xff, sizeof(white));
   tex4      = load_px(sw, px4, 2, 2);
   white_tex = load_px(sw, white, 4, 4);
   d0 = draws3d;
   b0 = blits;
   vfi.menu_st_flags = MENU_ST_FLAG_ALIVE;
   menu_draws        = draws_3d;
   frame(sw, NULL, 0, 0, 0);
   menu_draws        = NULL;
   vfi.menu_st_flags = 0;
   CHECK(draws3d - d0 == 5, "%u draws on the 3D engine, not 5",
         draws3d - d0);
#ifndef __SWITCH2__
   /* deko3d's register writes, through the firmware, in its order */
   CHECK(fw_count - fw0 == 6, "%u firmware register writes, not 6",
         fw_count - fw0);
   for (i = 0; i < 6; i++)
      CHECK(fw_calls[(fw0 + i) & 15].reg == fw_want[i][0]
            && fw_calls[(fw0 + i) & 15].v == fw_want[i][1]
            && fw_calls[(fw0 + i) & 15].mask == fw_want[i][2],
            "firmware write %u: %06x = %08x / %08x", i,
            fw_calls[(fw0 + i) & 15].reg, fw_calls[(fw0 + i) & 15].v,
            fw_calls[(fw0 + i) & 15].mask);
   CHECK(!mme_bad && !fw_acks_late,
         "macro: %u faults, %u calls past an unacknowledged one",
         mme_bad, fw_acks_late);
   CHECK(reg3d[0x564] == 0x3F, "zcull region %x", reg3d[0x564]);
#endif
   CHECK(blits - b0 >= 2, "the default quads left the 2D engine");
   CHECK(!bad3d, "%u draws with state the model rejects", bad3d);
   CHECK(!hazards, "%u draws with no wait for the other engine", hazards);

   /* turned a quarter left: red bottom left, green top left, white top
    * right, blue bottom right */
   CHECK(at(sw, 202, 202) == 0x00ff00 && at(sw, 261, 202) == 0xffffff
         && at(sw, 202, 261) == 0x0000ff && at(sw, 261, 261) == 0xff0000,
         "turned texture %06x %06x / %06x %06x", at(sw, 202, 202),
         at(sw, 261, 202), at(sw, 202, 261), at(sw, 261, 261));
   CHECK(at(sw, 199, 230) == 0 && at(sw, 264, 230) == 0
         && at(sw, 230, 199) == 0 && at(sw, 230, 264) == 0,
         "turned texture outside its box");

   /* the strip: [400, 425) red, [425, 450) not drawn, [450, 475) green */
   CHECK(at(sw, 400, 210) == 0x0000ff && at(sw, 424, 249) == 0x0000ff
         && at(sw, 425, 210) == 0 && at(sw, 449, 230) == 0
         && at(sw, 450, 210) == 0x00ff00 && at(sw, 474, 249) == 0x00ff00
         && at(sw, 475, 210) == 0,
         "strip %06x %06x %06x", at(sw, 410, 210), at(sw, 437, 210),
         at(sw, 460, 210));

   /* shaded vertex by vertex */
   top = at(sw, 630, 100);
   bot = at(sw, 630, 163);
   mid = at(sw, 630, 132);
   CHECK(near(top, 0x0000ff, 6) && near(bot, 0xff0000, 6)
         && near(mid, 0x7f007f, 6),
         "shading %06x / %06x / %06x", top, mid, bot);

   /* half white over blue, premultiplied; then red on the right */
   t = SW_MUL255(255u, 128u, t);
   CHECK(near(at(sw, 810, 230), t | (t << 8) | (0xffu << 16), 2),
         "blended %06x", at(sw, 810, 230));
   CHECK(at(sw, 840, 230) == 0x0000ff, "the 2D engine's draw after: %06x",
         at(sw, 840, 230));

   /* the scissor */
   CHECK(near(at(sw, 1031, 230), 0x0000ff, 0) && at(sw, 1032, 230) == 0,
         "scissor on the 3D engine: %06x | %06x", at(sw, 1031, 230),
         at(sw, 1032, 230));

   poke->unload_texture(sw, false, tex4);
   poke->unload_texture(sw, false, white_tex);
   driver_free(sw);
   CHECK(gfx_display_ctx_switch.handles_transform
         && !gfx_display_ctx_switch.handles_vertex_strip,
         "the menus' flags not restored at free");
   CHECK(maps_live == 0, "%u maps left", maps_live);
   printf("[pass] the 3D engine: turned textures, strips, shading, blending, scissor\n");
}

/* A frame with more textures than its descriptor slice: the rest fall
 * back to the 2D engine; frames take turns at the slices */
static uintptr_t many[NXG_3D_TICS + 8];
static void draws_many(switch_video_t *sw)
{
   gfx_display_ctx_draw_t d;
   struct video_coords c;
   math_matrix_4x4 m;
   unsigned i;
   memcpy(m.data, quarter, sizeof(quarter));
   memset(&c, 0, sizeof(c));
   memset(&d, 0, sizeof(d));
   c.vertices     = 4;
   d.coords       = &c;
   d.scale_factor = 1.0f;
   d.matrix_data  = &m;
   d.dims         = VIDEO_SCALE_PACK(4, 4);
   for (i = 0; i < NXG_3D_TICS + 8; i++)
   {
      d.texture = many[i];
      d.pos     = VIDEO_POS_PACK((i % 256) * 5, 720 - 8 - (i / 256) * 5);
      gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));
   }
   /* the same texture again: its descriptor again */
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));
}

static void draws_one(switch_video_t *sw)
{
   gfx_display_ctx_draw_t d;
   struct video_coords c;
   math_matrix_4x4 m;
   memcpy(m.data, quarter, sizeof(quarter));
   memset(&c, 0, sizeof(c));
   memset(&d, 0, sizeof(d));
   c.vertices     = 4;
   d.coords       = &c;
   d.scale_factor = 1.0f;
   d.matrix_data  = &m;
   d.texture      = many[0];
   d.pos          = VIDEO_POS_PACK(10, 10);
   d.dims         = VIDEO_SCALE_PACK(4, 4);
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));
}

static void lane_3d_pool(void)
{
   uint32_t px = 0xff808080u;
   unsigned i, d0, b0;
   int slices[NXG_FRAMES + 1];
   switch_video_t *sw = driver_new(1280, 720, true);
   nxg_t *g           = (nxg_t*)sw->gpu;
   for (i = 0; i < NXG_3D_TICS + 8; i++)
      many[i] = load_px(sw, &px, 1, 1);
   d0 = draws3d;
   b0 = blits;
   vfi.menu_st_flags = MENU_ST_FLAG_ALIVE;
   menu_draws        = draws_many;
   frame(sw, NULL, 0, 0, 0);
   CHECK(draws3d - d0 == NXG_3D_TICS, "%u draws on the 3D engine, not %u",
         draws3d - d0, NXG_3D_TICS);
   CHECK(blits - b0 >= 9, "%u textures past the slice not drawn by the 2D engine",
         blits - b0);
   CHECK(g->tic_used == NXG_3D_TICS, "%u descriptors written", g->tic_used);
   menu_draws = draws_one;
   for (i = 0; i < NXG_FRAMES + 1; i++)
   {
      frame(sw, NULL, 0, 0, 0);
      slices[i] = frame_slice;
   }
   menu_draws        = NULL;
   vfi.menu_st_flags = 0;
   for (i = 1; i < NXG_FRAMES + 1; i++)
      CHECK(slices[i] == (slices[i - 1] + 1) % NXG_FRAMES,
            "frame %u drew from slice %d after %d", i, slices[i],
            slices[i - 1]);
   CHECK(!tic_slice_bad, "%u draws outside their frame's slice",
         tic_slice_bad);
   CHECK(!bad3d && !hazards, "state %u, hazards %u", bad3d, hazards);
   for (i = 0; i < NXG_3D_TICS + 8; i++)
      poke->unload_texture(sw, false, many[i]);
   driver_free(sw);
   CHECK(maps_live == 0, "%u maps left", maps_live);
   printf("[pass] descriptors: a slice a frame, the rest of a full one on the 2D engine\n");
}
#ifndef __SWITCH2__
/* A firmware that never answers: each call gives up after its 4096
 * reads, and the frames still go out */
static void lane_3d_firmware_mute(void)
{
   unsigned r0, f0;
   switch_video_t *sw;
   fw_mute = true;
   r0      = mme_reads;
   f0      = fw_count;
   sw      = driver_new(256, 224, true);
   frame(sw, NULL, 0, 0, 0);
   frame(sw, NULL, 0, 0, 0);
   fw_mute = false;
   CHECK(fw_count - f0 == 6 && mme_reads - r0 == 6 * 4096,
         "unanswered firmware: %u calls, %u reads", fw_count - f0,
         mme_reads - r0);
   CHECK(!mme_bad, "%u macro faults", mme_bad);
   CHECK(syncpt_value >= ((nxg_t*)sw->gpu)->frames[0].fence.value,
         "frames stopped behind the firmware");
   driver_free(sw);
   printf("[pass] firmware writes: deko3d's six through the macro, bounded with no answer\n");
}
#endif

#ifdef NXG_3D_FX
/* --- the menu's effects ------------------------------------------------ */

/* The ribbon's uniforms, a pattern; the y flip the driver asks for */
static float ribbon_pat[204];
static float ribbon_yflip_asked;
const float *gfx_display_ribbon_ubo(gfx_display_t *p_disp, float yflip)
{
   unsigned i;
   (void)p_disp;
   for (i = 0; i < 204; i++)
      ribbon_pat[i] = (float)i * 0.5f;
   ribbon_pat[1]      = yflip;
   ribbon_yflip_asked = yflip;
   return ribbon_pat;
}

static gfx_display_effect_desc_t fxd[6];
static void *fxh[6];
static gfx_display_t fx_disp;
static gfx_display_mesh_t fx_ribbon, fx_quad;
static float fx_ribbon_pos[24];
static float fx_quad_pos[8] = { -1, -1, 1, -1, -1, 1, 1, 1 };
static float fx_color[16];

#ifdef __SWITCH2__
#define FX_V(e) fxd[e].sm86_vertex
#define FX_F(e) fxd[e].sm86_fragment
static void fx_describe(void)
{
#define FXD(e, f, v, vg, fr, fg) \
   fxd[e].flags = f; \
   fxd[e].sm86_vertex = v; fxd[e].sm86_vertex_size = sizeof(v); \
   fxd[e].sm86_vertex_gprs = vg; \
   fxd[e].sm86_fragment = fr; fxd[e].sm86_fragment_size = sizeof(fr); \
   fxd[e].sm86_fragment_gprs = fg
   FXD(0, GFX_EFFECT_MESH | GFX_EFFECT_BLEND_ADD | GFX_EFFECT_RIBBON,
         nxg_s2_fx_ribbon_vert, NXG_S2_FX_RIBBON_VERT_GPRS,
         nxg_s2_fx_ribbon_frag, NXG_S2_FX_RIBBON_FRAG_GPRS);
   FXD(1, GFX_EFFECT_MESH | GFX_EFFECT_BLEND_SCREEN,
         nxg_s2_fx_ribbon_simple_vert, NXG_S2_FX_RIBBON_SIMPLE_VERT_GPRS,
         nxg_s2_fx_ribbon_simple_frag, NXG_S2_FX_RIBBON_SIMPLE_FRAG_GPRS);
   FXD(2, 0, nxg_s2_fx_quad_vert, NXG_S2_FX_QUAD_VERT_GPRS,
         nxg_s2_fx_snow_simple_frag, NXG_S2_FX_SNOW_SIMPLE_FRAG_GPRS);
   FXD(3, 0, nxg_s2_fx_quad_vert, NXG_S2_FX_QUAD_VERT_GPRS,
         nxg_s2_fx_snow_frag, NXG_S2_FX_SNOW_FRAG_GPRS);
   FXD(4, GFX_EFFECT_YFLIP, nxg_s2_fx_quad_vert, NXG_S2_FX_QUAD_VERT_GPRS,
         nxg_s2_fx_bokeh_frag, NXG_S2_FX_BOKEH_FRAG_GPRS);
   FXD(5, GFX_EFFECT_YFLIP, nxg_s2_fx_quad_vert, NXG_S2_FX_QUAD_VERT_GPRS,
         nxg_s2_fx_snowflake_frag, NXG_S2_FX_SNOWFLAKE_FRAG_GPRS);
#undef FXD
}
#else
#define FX_V(e) fxd[e].dksh_vertex
#define FX_F(e) fxd[e].dksh_fragment
static void fx_describe(void)
{
#define FXD(e, f, v, fr) \
   fxd[e].flags = f; \
   fxd[e].dksh_vertex = v; fxd[e].dksh_vertex_size = sizeof(v); \
   fxd[e].dksh_fragment = fr; fxd[e].dksh_fragment_size = sizeof(fr)
   FXD(0, GFX_EFFECT_MESH | GFX_EFFECT_BLEND_ADD | GFX_EFFECT_RIBBON,
         deko3d_fx_ribbon_vert_dksh, deko3d_fx_ribbon_frag_dksh);
   FXD(1, GFX_EFFECT_MESH | GFX_EFFECT_BLEND_SCREEN,
         deko3d_fx_ribbon_simple_vert_dksh, deko3d_fx_ribbon_simple_frag_dksh);
   FXD(2, 0, deko3d_fx_quad_vert_dksh, deko3d_fx_snow_simple_frag_dksh);
   FXD(3, 0, deko3d_fx_quad_vert_dksh, deko3d_fx_snow_frag_dksh);
   FXD(4, GFX_EFFECT_YFLIP, deko3d_fx_quad_vert_dksh, deko3d_fx_bokeh_frag_dksh);
   FXD(5, GFX_EFFECT_YFLIP, deko3d_fx_quad_vert_dksh,
         deko3d_fx_snowflake_frag_dksh);
#undef FXD
}
#endif

static void draws_fx(switch_video_t *sw)
{
   gfx_display_ctx_draw_t d;
   struct video_coords c;
   math_matrix_4x4 m;
   unsigned e;
   memset(&c, 0, sizeof(c));
   memset(&d, 0, sizeof(d));
   c.vertices     = 4;
   c.color        = fx_color;
   d.coords       = &c;
   d.scale_factor = 1.0f;
   d.color        = fx_color;
   /* the menu's matrix sent before the effects, which write over it */
   memcpy(m.data, quarter, sizeof(quarter));
   d.texture      = tex4;
   d.matrix_data  = &m;
   d.pos          = VIDEO_POS_PACK(300, 720 - 364);
   d.dims         = VIDEO_SCALE_PACK(64, 64);
   c.color        = NULL;
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));
   c.color        = fx_color;
   d.texture      = 0;
   d.matrix_data  = NULL;
   d.pos          = VIDEO_POS_PACK(0, 0);
   d.dims         = VIDEO_SCALE_PACK(1280, 720);
   for (e = 0; e < 6; e++)
   {
      fx_disp.effect_mesh     = e < 2 ? &fx_ribbon : &fx_quad;
      fx_disp.effect_vertices = e < 2 ? 10 : 4;
      d.effect                = fxh[e];
      gfx_display_ctx_switch.draw_pipeline(&d, &fx_disp, sw,
            VIDEO_SCALE_PACK(1280, 720));
      /* and the draw after it, which an effect leaves to the pipeline */
      gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));
   }
   /* a menu draw after the effects: its own state again */
   c.color       = NULL;
   d.effect      = NULL;
   d.texture     = tex4;
   d.matrix_data = &m;
   d.pos         = VIDEO_POS_PACK(200, 720 - 264);
   d.dims        = VIDEO_SCALE_PACK(64, 64);
   gfx_display_ctx_switch.draw(&d, sw, VIDEO_SCALE_PACK(1280, 720));
}

static void lane_3d_effects(void)
{
   static const uint32_t px4[4] = {
      0xffff0000u, 0xff00ff00u, 0xff0000ffu, 0xffffffffu
   };
   /* blend factors by effect: ribbon adds, ribbon simple screens, the
    * rest by their alpha */
   static const uint32_t blend[6][6] = {
      { 1, 2, 2, 1, 2, 2 }, { 1, 9, 2, 1, 1, 2 }, { 1, 5, 6, 1, 2, 6 },
      { 1, 5, 6, 1, 2, 6 }, { 1, 5, 6, 1, 2, 6 }, { 1, 5, 6, 1, 2, 6 }
   };
   unsigned e, i, r0, d0, inv0;
   uint64_t ribbon_va;
   uint32_t used;
   switch_video_t *sw = driver_new(1280, 720, true);
   nxg_t *g           = (nxg_t*)sw->gpu;

   fx_describe();
   for (i = 0; i < 12; i++)
   {
      fx_ribbon_pos[2 * i]     = (float)i * 0.1f;
      fx_ribbon_pos[2 * i + 1] = 1.0f - (float)i * 0.05f;
   }
   fx_ribbon.positions    = fx_ribbon_pos;
   fx_ribbon.vertex_count = 12;
   fx_ribbon.id           = 77;
   fx_quad.positions      = fx_quad_pos;
   fx_quad.vertex_count   = 4;
   fx_quad.id             = 1;
   fx_disp.effect_time    = 1.5f;
   for (i = 0; i < 16; i++)
      fx_color[i] = (i & 3) == 3 ? 0.75f : 1.0f;
   tex4 = load_px(sw, px4, 2, 2);
   /* the setup's own invalidate out of the way */
   frame(sw, NULL, 0, 0, 0);
   inv0 = shader_invalidates;
   for (e = 0; e < 6; e++)
   {
      fxh[e] = gfx_display_ctx_switch.effect_install(sw, &fxd[e]);
      CHECK(fxh[e], "effect %u not installed", e);
   }
   r0 = fxrecs;
   d0 = draws3d;
   vfi.menu_st_flags = MENU_ST_FLAG_ALIVE;
   menu_draws        = draws_fx;
   frame(sw, NULL, 0, 0, 0);
   CHECK(fxrecs - r0 == 6 && draws3d - d0 == 8,
         "%u effect draws and %u draws, not 6 and 8", fxrecs - r0,
         draws3d - d0);
   /* the draw after each effect left to the pipeline: nothing else */
   CHECK(at(sw, 100, 100) == 0 && at(sw, 900, 600) == 0,
         "an effect drawn as a quad too: %06x", at(sw, 100, 100));
   for (e = 0; e < 6 && fxrecs - r0 == 6; e++)
   {
      const fxrec_t *r = &fxrec[(r0 + e) & 15];
      const gfx_display_mesh_t *mesh = e < 2 ? &fx_ribbon : &fx_quad;
      unsigned n = e < 2 ? 10 : 4;
#ifdef __SWITCH2__
      CHECK(r->ok && r->vs == FX_V(e) && r->fs == FX_F(e)
            && r->d8 == 0x087F6080, "effect %u: programs or state", e);
      CHECK(r->gprs[0] == fxd[e].sm86_vertex_gprs
            && r->gprs[1] == fxd[e].sm86_fragment_gprs
            && r->prefetch[0] == (fxd[e].sm86_vertex_size + 255) / 256
            && r->prefetch[1] == (fxd[e].sm86_fragment_size + 255) / 256
            && r->binding[0] == 0 && r->binding[1] == 4,
            "effect %u: registers, prefetch or binding", e);
#else
      CHECK(r->ok && r->vs == FX_V(e) && r->fs == FX_F(e)
            && r->d8 == nxg_le32(prog_hdr(FX_F(e)) + 44),
            "effect %u: programs or state", e);
#endif
      CHECK(!memcmp(r->blend, blend[e], sizeof(blend[e])) && r->blend_en,
            "effect %u: blend %u %u %u / %u %u %u", e, r->blend[0],
            r->blend[1], r->blend[2], r->blend[3], r->blend[4], r->blend[5]);
      CHECK(r->va_cfg == ((1u << 12) | 8)
            && r->attr[0] == ((1u << 6) | (0x12u << 21) | (7u << 27))
            && r->attr[1] == r->attr[0], "effect %u: vertex layout", e);
      CHECK(r->count == n && r->limit == r->va + n * 8 - 1
            && !memcmp(xlat(r->va), mesh->positions, n * 8),
            "effect %u: %u vertices", e, r->count);
      CHECK(r->ubo_v && r->ubo_v == r->ubo_f && r->ubo_size_v >= 0x400
            && r->ubo_size_f >= 0x400, "effect %u: uniforms not bound", e);
      CHECK(r->xf[0] == 640.0f && r->xf[1] == -360.0f && r->xf[3] == 640.0f
            && r->xf[4] == 360.0f, "effect %u: viewport", e);
      CHECK(r->shader_invalidates > inv0, "effect %u: code not invalidated",
            e);
      if (e == 0)
         CHECK(!memcmp(r->ubo, ribbon_pat, sizeof(ribbon_pat))
               && ribbon_yflip_asked == 1.0f, "ribbon uniforms");
      else if (e == 1)
         CHECK(r->ubo[0] == 1.5f && r->ubo[1] == 1.0f && r->ubo[2] == 0.75f,
               "ribbon simple uniforms %f %f %f", r->ubo[0], r->ubo[1],
               r->ubo[2]);
      else
         CHECK(r->ubo[16] == 1280.0f && r->ubo[17] == 720.0f
               && r->ubo[18] == 1.5f
               && r->ubo[19] == (e >= 4 ? -1.0f : 1.0f),
               "effect %u uniforms %f %f %f %f", e, r->ubo[16], r->ubo[17],
               r->ubo[18], r->ubo[19]);
   }
   /* the menu's draw after them: its state and its pixels */
   CHECK(!bad3d && !hazards, "state %u, hazards %u", bad3d, hazards);
   CHECK(at(sw, 202, 202) == 0x00ff00 && at(sw, 261, 261) == 0xff0000
         && at(sw, 302, 302) == 0x00ff00,
         "the menu after the effects: %06x %06x", at(sw, 202, 202),
         at(sw, 261, 261));
   /* the next frame: the mesh where it was, not made again */
   ribbon_va = fxrec[r0 & 15].va;
   r0        = fxrecs;
   frame(sw, NULL, 0, 0, 0);
   CHECK(fxrecs - r0 == 6 && fxrec[r0 & 15].va == ribbon_va,
         "the ribbon's positions made again");
   menu_draws        = NULL;
   vfi.menu_st_flags = 0;
   /* installs stop where the code memory or the table ends */
   used = g->code3d_used;
   for (i = 0; i < 32; i++)
      if (!gfx_display_ctx_switch.effect_install(sw, &fxd[5]))
         break;
   CHECK(i < 32 && g->fx_count <= NXG_3D_EFFECTS
         && g->code3d_used + NXG_3D_CODE_TAIL <= g->code3d.size
         && g->code3d_used >= used,
         "installs past the code memory: %u, %u of %u bytes", i,
         g->code3d_used, g->code3d.size);
   poke->unload_texture(sw, false, tex4);
   driver_free(sw);
   CHECK(maps_live == 0, "%u maps left", maps_live);
   printf("[pass] effects: programs, blends, uniforms and the meshes, the menu after them\n");
}
#endif
#else
/* The Switch 2 keeps the 2D engine's terms */
static void lane_3d(void)
{
   switch_video_t *sw = driver_new(1280, 720, true);
   CHECK(gfx_display_ctx_switch.handles_transform
         && !gfx_display_ctx_switch.handles_vertex_strip,
         "the menus hand over matrices with no 3D engine");
   CHECK(!gfx_display_ctx_switch.effect_install,
         "effects offered with no 3D engine");
   driver_free(sw);
   printf("[pass] no 3D engine: the menus hand over values and quads\n");
}
#endif

int main(void)
{
#if defined(__SWITCH2__) && defined(NXG_3D)
   /* What AMPERE_B's golden context may hold: the setup must write it */
   unsigned r;
   reg3d[0x0C4] = reg3d[0x673] = 0xDEADBEEFu;
   for (r = 0x678; r < 0x680; r++)
      reg3d[r] = 0xDEADBEEFu;
#endif
   lane_window();
   lane_frame(true);
   lane_frame(false);
   lane_in_place();
   lane_menu();
   lane_text();
   lane_overlay();
   lane_rgui();
   lane_dock_and_fences();
   lane_heap();
   lane_3d();
#ifdef NXG_3D
   lane_3d_pool();
#endif
#ifndef __SWITCH2__
   lane_3d_firmware_mute();
#endif
#ifdef NXG_3D_FX
   lane_3d_effects();
#endif
   CHECK(!bad_words, "%u words or blits the engine model rejected", bad_words);
#if defined(__SWITCH2__) && defined(NXG_3D)
#define HARNESS "switch_nx_2d_test (Switch 2, 3D)"
#elif defined(__SWITCH2__)
#define HARNESS "switch_nx_2d_test (Switch 2)"
#else
#define HARNESS "switch_nx_2d_test"
#endif
#ifdef NXG_3D
   printf("[info] %u words run, %u blits, %u 3D draws\n", words_run, blits,
         draws3d);
#else
   printf("[info] %u words run, %u blits\n", words_run, blits);
#endif
   if (failures)
   {
      printf("FAIL " HARNESS " (%d)\n", failures);
      return 1;
   }
   printf("PASS " HARNESS "\n");
   return 0;
}
