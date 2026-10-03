/* Recording stand-in for the PSP hardware gfx/drivers/psp1_gfx.c draws
 * with, and the handful of frontend entry points it calls.
 *
 * The driver is compiled into fake_psp.c as it ships, so the contracts
 * the tests hold are the shipping file's. What this layer models is the
 * part of the machine the driver's correctness depends on and a desktop
 * cannot otherwise show:
 *
 *   - Main RAM and VRAM are each mapped twice, at the cached address and
 *     at the same address with bit 0x40000000 set, so TO_UNCACHED_PTR()
 *     and TO_CACHED_PTR() reach one another's writes exactly as they do
 *     on an Allegrex.
 *   - A page with no access follows VRAM, so a blit that runs off the
 *     end of it is a fault rather than a silent overwrite of whatever
 *     the next allocation happens to be.
 *   - A display list the GE has been handed is busy until it is waited
 *     on, and refilling a busy list is recorded.
 *   - The vblank sub-interrupt can be fired on demand, including after
 *     the driver has been freed, which is where its handler has to have
 *     stopped running.
 */

#ifndef PSP1_FAKE_PSP_H
#define PSP1_FAKE_PSP_H

#include <stddef.h>
#include <pspdisplay.h>

/* Mirrors of the driver's own geometry, so a test can address the same
 * windows it does. */
#define PSP1_FAKE_SCR_WIDTH   480
#define PSP1_FAKE_SCR_HEIGHT  272
#define PSP1_FAKE_VRAM_WIDTH  512
#define PSP1_FAKE_VRAM_TOP    0x44000000UL

#define PSP1_FAKE_RAM_BASE   0x08800000UL
#define PSP1_FAKE_RAM_SIZE   0x01000000UL
#define PSP1_FAKE_VRAM_BASE  0x04000000UL
#define PSP1_FAKE_VRAM_SIZE  0x00200000UL
#define PSP1_FAKE_UNCACHED   0x40000000UL

#define PSP1_FAKE_MAX_EVENTS 512

enum psp1_fake_event
{
   PSP1_EV_NONE = 0,
   PSP1_EV_ALLOC,
   PSP1_EV_FREE,
   PSP1_EV_INTR_REGISTER,
   PSP1_EV_INTR_ENABLE,
   PSP1_EV_INTR_DISABLE,
   PSP1_EV_INTR_RELEASE,
   PSP1_EV_VBLANK_FIRED,
   PSP1_EV_GU_START,
   PSP1_EV_GU_FINISH,
   PSP1_EV_GU_SYNC,
   PSP1_EV_GU_SENDLIST,
   PSP1_EV_GU_COPYIMAGE,
   PSP1_EV_DEBUG_PUTS
};

typedef struct
{
   enum psp1_fake_event  kind;
   void                 *ptr;
} psp1_fake_ev_t;

typedef struct
{
   /* event tape, in order */
   psp1_fake_ev_t  ev[PSP1_FAKE_MAX_EVENTS];
   int             ev_count;

   /* display lists */
   void           *filling;      /* list sceGuStart() is writing, else NULL */
   void           *busy;         /* list the GE holds, else NULL            */
   int             refilled_busy_list;
   int             wrote_busy_framebuffer;

   /* vblank sub-interrupt */
   void          (*vblank_handler)(int, void*);
   void           *vblank_arg;
   int             vblank_registered;
   int             vblank_enabled;
   int             vblank_after_release;

   /* allocator */
   int             live_allocs;
   int             total_allocs;
   int             fail_alloc_after;   /* -1: never fail                    */
   void           *last_alloc;
   size_t          last_alloc_size;

   /* what the GE was told to write */
   void           *copy_dest;
   size_t          copy_dest_bytes;
   int             copy_past_vram;
   int             copy_past_dest;
   int             bad_blit_geometry;
   void           *ge_context;
   void           *draw_vertices;      /* last sceGuDrawArray() array       */
   void           *init_vertices;      /* first one, i.e. psp->frame_coords */
   void           *menu_vertices;      /* the menu list's array             */
   void           *tex_image;
   int             tex_image_w;
   int             tex_image_h;
   int             tex_image_bw;

   /* framebuffer */
   void           *debug_base;
   int             debug_puts;
   int             swap_count;
   int             sync_count;
   int             vblank_waits;
} psp1_fake_t;

extern psp1_fake_t psp1_fake;

/* Maps the two aliased windows and the guard page. Once per process. */
int  psp1_fake_mem_init(void);

/* Clears the tape and every counter, drops the allocator's bump pointer
 * back to the start, and forgets any registered handler. */
void psp1_fake_reset(void);

/* Runs the registered vblank handler, as the kernel would. Safe to call
 * when nothing is registered; it records that it had nothing to run. */
void psp1_fake_fire_vblank(void);

/* Fire a vblank from inside every free(), which is the window a
 * teardown leaves open for the handler. */
void psp1_fake_fire_vblank_on_free(int on);

/* The driver's vertex grid, so a test can read the arrays the GE is
 * pointed at. Both TUs take the same -D, so a build comparing another
 * split checks that split. */
#ifndef PSP_FRAME_ROWS_COUNT
#define PSP_FRAME_ROWS_COUNT     4
#endif
#ifndef PSP_FRAME_COLUMNS_COUNT
#define PSP_FRAME_COLUMNS_COUNT  16
#endif
#define PSP1_FAKE_ROWS      PSP_FRAME_ROWS_COUNT
#define PSP1_FAKE_COLUMNS   PSP_FRAME_COLUMNS_COUNT
#define PSP1_FAKE_SLICES    (PSP1_FAKE_ROWS * PSP1_FAKE_COLUMNS)

typedef struct { float u, v, x, y, z; } psp1_fake_vertex_t;
typedef struct { psp1_fake_vertex_t v0, v1; } psp1_fake_sprite_t;

/* Viewport the stand-in video_driver_update_viewport() hands back. */
void psp1_fake_set_viewport(int x, int y, int width, int height);

/* Pixel format sceDisplayGetFrameBuf() reports, and the buffer it points
 * at; the buffer lives in the VRAM window. */
void psp1_fake_set_display_format(int fmt);
void *psp1_fake_display_buffer(void);

/* Scratch inside the main-RAM window, for the buffers a core or the
 * menu would hand the driver. Freed by psp1_fake_reset(). */
void *psp1_fake_ram_alloc(size_t size);

/* Core pixel format video_state_get_ptr() reports. */
void psp1_fake_set_core_pixel_format(int rgb565);

/* Index of the first event of a kind at or after @from, or -1. */
int  psp1_fake_find_event(enum psp1_fake_event kind, int from);

/* The vtable under test. */
#include "../../../gfx/video_driver.h"
extern video_driver_t video_psp1;

#endif
