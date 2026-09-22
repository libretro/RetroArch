/* See fake_psp.h. The driver is included below as it ships, after the
 * headers it needs have been pulled in under their real names, so only
 * the driver's own allocator calls are redirected into the aliased
 * window. */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

/* Exactly what gfx/drivers/psp1_gfx.c includes, so that its own copies
 * of these are no-ops and nothing below gets renamed with it. */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <malloc.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psprtc.h>
#include <retro_inline.h>
#include <retro_math.h>
#include "../../../menu/menu_driver.h"
#include <defines/psp_defines.h>

#include "fake_psp.h"

psp1_fake_t psp1_fake;

#define VRAM_LO  ((uintptr_t)PSP1_FAKE_VRAM_BASE)
#define VRAM_HI  ((uintptr_t)(PSP1_FAKE_VRAM_BASE + PSP1_FAKE_VRAM_SIZE))

static int      mem_ready;
static uintptr_t bump;
static int      display_format = PSP_DISPLAY_PIXEL_FORMAT_565;
static int      core_is_565    = 1;
static int      vp_x, vp_y, vp_w = 480, vp_h = 272;
static int      fire_on_free;
static int      filling_cid;
static uintptr_t scratch;

#define MAX_BLOCKS 64
static struct { uintptr_t addr; size_t size; int live; } blocks[MAX_BLOCKS];
static int block_count;

static void record(enum psp1_fake_event kind, void *ptr)
{
   if (psp1_fake.ev_count >= PSP1_FAKE_MAX_EVENTS)
      return;
   psp1_fake.ev[psp1_fake.ev_count].kind = kind;
   psp1_fake.ev[psp1_fake.ev_count].ptr  = ptr;
   psp1_fake.ev_count++;
}

int psp1_fake_find_event(enum psp1_fake_event kind, int from)
{
   int i;
   for (i = (from < 0) ? 0 : from; i < psp1_fake.ev_count; i++)
      if (psp1_fake.ev[i].kind == kind)
         return i;
   return -1;
}

/* ---------------------------------------------------------------- memory */

static int map_aliased(unsigned long base, unsigned long size)
{
   int   fd;
   void *lo, *hi;

   if ((fd = memfd_create("psp1_fake", 0)) < 0)
      return -1;
   if (ftruncate(fd, (off_t)size) != 0)
      return -1;

   lo = mmap((void*)base, size, PROT_READ | PROT_WRITE,
         MAP_SHARED | MAP_FIXED_NOREPLACE, fd, 0);
   if (lo == MAP_FAILED)
      return -1;
   hi = mmap((void*)(base | PSP1_FAKE_UNCACHED), size,
         PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED_NOREPLACE, fd, 0);
   if (hi == MAP_FAILED)
      return -1;

   close(fd);
   return 0;
}

int psp1_fake_mem_init(void)
{
   if (mem_ready)
      return 0;

   if (map_aliased(PSP1_FAKE_RAM_BASE,  PSP1_FAKE_RAM_SIZE)  != 0)
      return -1;
   if (map_aliased(PSP1_FAKE_VRAM_BASE, PSP1_FAKE_VRAM_SIZE) != 0)
      return -1;

   /* No access straight after VRAM, in both aliases: a blit that runs
    * off the end faults here instead of landing on live data. */
   if (mmap((void*)VRAM_HI, 0x1000, PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0)
         == MAP_FAILED)
      return -1;
   if (mmap((void*)(VRAM_HI | PSP1_FAKE_UNCACHED), 0x1000, PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0)
         == MAP_FAILED)
      return -1;

   mem_ready = 1;
   return 0;
}

void psp1_fake_reset(void)
{
   memset(&psp1_fake, 0, sizeof(psp1_fake));
   psp1_fake.fail_alloc_after = -1;
   memset(blocks, 0, sizeof(blocks));
   block_count    = 0;
   bump           = PSP1_FAKE_RAM_BASE;
   scratch        = PSP1_FAKE_RAM_BASE + PSP1_FAKE_RAM_SIZE;
   fire_on_free   = 0;
   vp_x = vp_y    = 0;
   vp_w           = 480;
   vp_h           = 272;
   display_format = PSP_DISPLAY_PIXEL_FORMAT_565;
   core_is_565    = 1;
   memset((void*)PSP1_FAKE_VRAM_BASE, 0, PSP1_FAKE_VRAM_SIZE);
}

void psp1_fake_fire_vblank_on_free(int on) { fire_on_free = on; }

/* Carved down from the top of the window, so it never meets the
 * driver's own allocations coming up from the bottom. */
void *psp1_fake_ram_alloc(size_t size)
{
   scratch -= (size + 63) & ~(uintptr_t)63;
   if (scratch < bump)
      return NULL;
   return (void*)scratch;
}

/* --------------------------------------------------------------- allocator */

static void *psp1_memalign(size_t align, size_t size);
static void *psp1_calloc(size_t n, size_t size);
static void  psp1_free(void *ptr);

static void *psp1_memalign(size_t align, size_t size)
{
   uintptr_t addr;

   if (     psp1_fake.fail_alloc_after >= 0
         && psp1_fake.total_allocs >= psp1_fake.fail_alloc_after)
      return NULL;
   if (align < 4)
      align = 4;

   addr = (bump + (align - 1)) & ~(uintptr_t)(align - 1);
   if (addr + size > PSP1_FAKE_RAM_BASE + PSP1_FAKE_RAM_SIZE)
      return NULL;
   bump = addr + size;

   if (block_count < MAX_BLOCKS)
   {
      blocks[block_count].addr = addr;
      blocks[block_count].size = size;
      blocks[block_count].live = 1;
      block_count++;
   }
   psp1_fake.live_allocs++;
   psp1_fake.total_allocs++;
   psp1_fake.last_alloc      = (void*)addr;
   psp1_fake.last_alloc_size = size;
   record(PSP1_EV_ALLOC, (void*)addr);
   return (void*)addr;
}

static void *psp1_calloc(size_t n, size_t size)
{
   void *p = psp1_memalign(16, n * size);
   if (p)
      memset(p, 0, n * size);
   return p;
}

static void psp1_free(void *ptr)
{
   int i;

   if (!ptr)
      return;
   record(PSP1_EV_FREE, ptr);

   for (i = 0; i < block_count; i++)
   {
      if (blocks[i].addr != (uintptr_t)ptr)
         continue;
      if (!blocks[i].live)
      {
         fprintf(stderr, "fake_psp: double free of %p\n", ptr);
         abort();
      }
      blocks[i].live = 0;
      psp1_fake.live_allocs--;

      /* Poison, then let a vblank land in the window a teardown leaves
       * open. A handler still registered writes through its copy of
       * this pointer, and the poison shows it. */
      memset(ptr, 0xDD, blocks[i].size);
      if (fire_on_free && psp1_fake.vblank_handler && psp1_fake.vblank_enabled)
      {
         unsigned char *p = (unsigned char*)ptr;
         size_t         n = blocks[i].size;
         size_t         k;

         psp1_fake_fire_vblank();
         for (k = 0; k < n; k++)
         {
            if (p[k] != 0xDD)
            {
               psp1_fake.vblank_after_release = 1;
               break;
            }
         }
      }
      return;
   }

   fprintf(stderr, "fake_psp: free of unknown pointer %p\n", ptr);
   abort();
}

/* The driver's own allocator calls, and only those. */
#define memalign psp1_memalign
#define calloc   psp1_calloc
#define free     psp1_free

#include "../../../gfx/drivers/psp1_gfx.c"

#undef memalign
#undef calloc
#undef free

/* ------------------------------------------------------------ sce kernel */

void sceKernelDcacheWritebackInvalidateAll(void) { }
void sceKernelDcacheWritebackRange(const void *addr, unsigned int size)
{
   (void)addr; (void)size;
}

int sceKernelRegisterSubIntrHandler(int intno, int no, void *handler,
      void *arg)
{
   (void)intno; (void)no;
   psp1_fake.vblank_handler    = (void (*)(int, void*))handler;
   psp1_fake.vblank_arg        = arg;
   psp1_fake.vblank_registered = 1;
   record(PSP1_EV_INTR_REGISTER, arg);
   return 0;
}

int sceKernelReleaseSubIntrHandler(int intno, int no)
{
   (void)intno; (void)no;
   psp1_fake.vblank_handler    = NULL;
   psp1_fake.vblank_arg        = NULL;
   psp1_fake.vblank_registered = 0;
   record(PSP1_EV_INTR_RELEASE, NULL);
   return 0;
}

int sceKernelEnableSubIntr(int intno, int no)
{
   (void)intno; (void)no;
   psp1_fake.vblank_enabled = 1;
   record(PSP1_EV_INTR_ENABLE, NULL);
   return 0;
}

int sceKernelDisableSubIntr(int intno, int no)
{
   (void)intno; (void)no;
   psp1_fake.vblank_enabled = 0;
   record(PSP1_EV_INTR_DISABLE, NULL);
   return 0;
}

void psp1_fake_fire_vblank(void)
{
   record(PSP1_EV_VBLANK_FIRED, psp1_fake.vblank_arg);
   if (psp1_fake.vblank_handler && psp1_fake.vblank_enabled)
      psp1_fake.vblank_handler(0, psp1_fake.vblank_arg);
}

long long sceKernelGetSystemTimeWide(void) { return 0; }

/* ----------------------------------------------------------- debug screen */

void pspDebugScreenSetColorMode(int mode) { (void)mode; }
void pspDebugScreenSetXY(int x, int y)    { (void)x; (void)y; }

void pspDebugScreenSetBase(void *base)
{
   psp1_fake.debug_base = base;
}

void pspDebugScreenPuts(const char *str)
{
   (void)str;
   psp1_fake.debug_puts++;
   record(PSP1_EV_DEBUG_PUTS, psp1_fake.debug_base);
   /* The CPU is writing the framebuffer here; if the GE still holds a
    * list it is rendering into that same buffer. */
   if (psp1_fake.busy)
      psp1_fake.wrote_busy_framebuffer++;
}

/* --------------------------------------------------------------- display */

int sceDisplayWaitVblankStart(void)
{
   psp1_fake.vblank_waits++;
   psp1_fake_fire_vblank();
   return 0;
}

int sceDisplaySetFrameBuf(void *topaddr, int bufferwidth, int pixelformat,
      int sync)
{
   (void)topaddr; (void)bufferwidth; (void)pixelformat; (void)sync;
   return 0;
}

void *psp1_fake_display_buffer(void)
{
   return (void*)(uintptr_t)SCEGU_VRAM_TOP;
}

int sceDisplayGetFrameBuf(void **topaddr, int *bufferwidth,
      int *pixelformat, int sync)
{
   (void)sync;
   *topaddr     = psp1_fake_display_buffer();
   *bufferwidth = SCEGU_VRAM_WIDTH;
   *pixelformat = display_format;
   return 0;
}

void psp1_fake_set_display_format(int fmt) { display_format = fmt; }
void psp1_fake_set_core_pixel_format(int rgb565) { core_is_565 = rgb565; }

/* -------------------------------------------------------------------- GU */

void sceGuInit(void) { }
void sceGuTerm(void) { }
void sceGuDisplay(int state) { (void)state; }
void sceGuCallMode(int mode)  { (void)mode; }

void sceGuStart(int cid, void *list)
{
   record(PSP1_EV_GU_START, list);
   if (psp1_fake.busy == list)
      psp1_fake.refilled_busy_list++;
   psp1_fake.filling = list;
   filling_cid       = cid;
}

int sceGuFinish(void)
{
   record(PSP1_EV_GU_FINISH, psp1_fake.filling);
   /* A finished GU_DIRECT list is handed to the GE and stays busy until
    * it is waited on; a GU_CALL list is only closed. */
   if (filling_cid == GU_DIRECT)
      psp1_fake.busy = psp1_fake.filling;
   psp1_fake.filling = NULL;
   return 0;
}

int sceGuSync(int mode, int what)
{
   (void)mode; (void)what;
   record(PSP1_EV_GU_SYNC, psp1_fake.busy);
   psp1_fake.busy = NULL;
   psp1_fake.sync_count++;
   return 0;
}

void sceGuCallList(const void *list) { (void)list; }

void sceGuSendList(int mode, const void *list, PspGeContext *context)
{
   (void)mode;
   record(PSP1_EV_GU_SENDLIST, (void*)context);
   psp1_fake.ge_context = (void*)context;
   /* The GE writes its state into the caller's context storage. */
   if (context)
      memset(context, 0xA5, sizeof(*context));
   psp1_fake.busy = (void*)list;
}

void *sceGuSwapBuffers(void)
{
   psp1_fake.swap_count++;
   /* GU-relative offsets of the two buffers, alternating. */
   if (psp1_fake.swap_count & 1)
      return (void*)(uintptr_t)(SCEGU_VRAM_WIDTH * SCEGU_SCR_HEIGHT * 2);
   return (void*)(uintptr_t)0;
}

void sceGuDrawBuffer(int psm, void *fbp, int fbw)
{
   (void)psm; (void)fbp; (void)fbw;
}

void sceGuDispBuffer(int width, int height, void *dispbp, int dispbw)
{
   (void)width; (void)height; (void)dispbp; (void)dispbw;
}

void sceGuClear(int flags)          { (void)flags; }
void sceGuClearColor(unsigned int c){ (void)c; }
void sceGuScissor(int x, int y, int w, int h)
{
   (void)x; (void)y; (void)w; (void)h;
}
void sceGuEnable(int state)  { (void)state; }
void sceGuDisable(int state) { (void)state; }

void sceGuTexMode(int tpsm, int maxmips, int a2, int swizzle)
{
   (void)tpsm; (void)maxmips; (void)a2; (void)swizzle;
}
void sceGuTexFunc(int tfx, int tcc)   { (void)tfx; (void)tcc; }
void sceGuTexFilter(int min, int mag) { (void)min; (void)mag; }
void sceGuTexWrap(int u, int v)       { (void)u; (void)v; }

void sceGuTexImage(int mipmap, int width, int height, int tbw,
      const void *tbp)
{
   (void)mipmap;
   psp1_fake.tex_image    = (void*)tbp;
   psp1_fake.tex_image_w  = width;
   psp1_fake.tex_image_h  = height;
   psp1_fake.tex_image_bw = tbw;
}

void sceGuClutMode(int cpsm, unsigned int shift, unsigned int mask,
      unsigned int a3)
{
   (void)cpsm; (void)shift; (void)mask; (void)a3;
}
void sceGuClutLoad(int num_blocks, const void *cbp)
{
   (void)num_blocks; (void)cbp;
}
void sceGuBlendFunc(int op, int src, int dest, unsigned int sf,
      unsigned int df)
{
   (void)op; (void)src; (void)dest; (void)sf; (void)df;
}

void sceGuDrawArray(int prim, int vtype, int count, const void *indices,
      const void *vertices)
{
   (void)prim; (void)vtype; (void)count; (void)indices;
   psp1_fake.draw_vertices = (void*)vertices;
   if (!psp1_fake.init_vertices)
      psp1_fake.init_vertices = (void*)vertices;
   else if (vertices != psp1_fake.init_vertices)
      psp1_fake.menu_vertices = (void*)vertices;
}

void sceGuCopyImage(int psm, int sx, int sy, int width, int height,
      int srcw, void *src, int dx, int dy, int destw, void *dest)
{
   int    bpp   = (psm == GU_PSM_8888) ? 4 : 2;
   size_t bytes = (size_t)destw * (size_t)(dy + height) * (size_t)bpp;
   uintptr_t lo = (uintptr_t)dest & ~PSP1_FAKE_UNCACHED;
   int    row;

   /* pspsdk asserts these; the GE transfer is malformed outside them. */
   if (     srcw  <= 8 || srcw  > 1024 || (srcw  & 0x7)
         || destw <= 8 || destw > 1024 || (destw & 0x7)
         || width  <= 0 || width  > 1023
         || height <= 0 || height > 1023
         || dx < 0 || dx >= 1023 || dy < 0 || dy >= 1023)
   {
      psp1_fake.bad_blit_geometry++;
      fprintf(stderr,
            "  [ge] malformed transfer: %dx%d srcw=%d destw=%d at %d,%d\n",
            width, height, srcw, destw, dx, dy);
   }

   record(PSP1_EV_GU_COPYIMAGE, dest);
   psp1_fake.copy_dest       = dest;
   psp1_fake.copy_dest_bytes = bytes;

   /* Running off the end of VRAM is the fault the guard page catches;
    * record it and clamp so the tests can report it instead. */
   if (lo >= VRAM_LO && lo < VRAM_HI && lo + bytes > VRAM_HI)
   {
      psp1_fake.copy_past_vram++;
      return;
   }
   /* Same question for a destination in main RAM: does the blit fit the
    * block that was allocated for it? */
   if (lo >= PSP1_FAKE_RAM_BASE)
   {
      int i;
      for (i = 0; i < block_count; i++)
      {
         if (blocks[i].addr != lo)
            continue;
         if (bytes > blocks[i].size)
         {
            psp1_fake.copy_past_dest++;
            return;
         }
         break;
      }
   }

   for (row = 0; row < height; row++)
      memcpy((unsigned char*)dest + ((size_t)(dy + row) * destw + dx) * bpp,
             (unsigned char*)src  + ((size_t)(sy + row) * srcw  + sx) * bpp,
             (size_t)width * bpp);
}

/* ------------------------------------------------------- frontend stand-ins */

void psp1_fake_set_viewport(int x, int y, int width, int height)
{
   vp_x = x; vp_y = y; vp_w = width; vp_h = height;
}

void video_driver_update_viewport(struct video_viewport *vp,
      bool force_full, bool keep_aspect, bool y_down)
{
   (void)force_full; (void)keep_aspect; (void)y_down;
   vp->pos    = VIDEO_POS_PACK(vp_x, vp_y);
   vp->dims   = VIDEO_SCALE_PACK((unsigned)vp_w, (unsigned)vp_h);
}

video_driver_state_t *video_state_get_ptr(void)
{
   static video_driver_state_t st;
   st.pix_fmt = core_is_565
      ? RETRO_PIXEL_FORMAT_RGB565 : RETRO_PIXEL_FORMAT_0RGB1555;
   return &st;
}

settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

input_driver_t input_psp;

void *input_driver_init_wrap(input_driver_t *input, const char *name)
{
   (void)input; (void)name;
   return NULL;
}

void menu_driver_frame(bool menu_is_alive, video_frame_info_t *video_info)
{
   (void)menu_is_alive; (void)video_info;
}
