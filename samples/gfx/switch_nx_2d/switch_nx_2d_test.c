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
 * engine model knows.
 *
 * Built twice: for the Switch (libnx's channel, nvmap and window queue)
 * and with __SWITCH2__ for the Switch 2 (GSV memory and VM, the nvhost
 * channel ioctls, KICKOFF_PB and a VI2 layer), the same lanes over the
 * same command stream. */

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
         bad_words++;
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

int main(void)
{
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
   CHECK(!bad_words, "%u words or blits the engine model rejected", bad_words);
#ifdef __SWITCH2__
#define HARNESS "switch_nx_2d_test (Switch 2)"
#else
#define HARNESS "switch_nx_2d_test"
#endif
   printf("[info] %u words run, %u blits\n", words_run, blits);
   if (failures)
   {
      printf("FAIL " HARNESS " (%d)\n", failures);
      return 1;
   }
   printf("PASS " HARNESS "\n");
   return 0;
}
