/* gfx/drivers/rsx_gfx.c's RSX allocations, on the host.
 *
 * The driver is compiled in whole (its functions are static) against
 * the tree's PSL1GHT stand-ins in tools/platform_stubs/ps3, and the
 * RSX calls it makes are defined here: rsxMemalign() hands out host
 * memory and can be made to fail on its Nth call, so every allocation
 * the driver makes is failed in turn.
 *
 *  - texture_grow: a texture's buffer is kept from load to load; a
 *    frame bigger than it (a wider pitch, 16 to 32 bits, a taller
 *    frame) gets a new one, the RSX waited for first, and a smaller
 *    one reuses it.
 *  - texture_fail: rsx_load_texture_internal() with no RSX memory
 *    gives no texture.
 *  - frame_fail: a core frame whose texture gets no RSX memory is
 *    not drawn.
 *  - init_sweep: rsx_init() with each of its allocations failed in
 *    turn gives a driver with everything it draws with, or none -
 *    and then leaves only the display and depth buffers it gave the
 *    RSX.
 *  - font_sweep: rsx_font_init() likewise gives a font with its
 *    vertices and texture, or none.
 *  - overlay_sweep: rsx_overlay_load() likewise loads whole images
 *    or nothing.
 *
 * Plain C and the C library only, so it builds and runs the same with
 * mingw on Windows. */

#include <stdlib.h>
#include <malloc.h>

/* The driver's I/O buffer: alignment means nothing to the host, and
 * not every C library has memalign() */
#define memalign(alignment, size) malloc(size)

#include "../../../gfx/drivers/rsx_gfx.c"

/* ---- RSX memory --------------------------------------------------- */

#define FAKE_RSX_MAX_ALLOCS 256

static struct
{
   void *ptr;
   u32   size;
} rsx_allocs[FAKE_RSX_MAX_ALLOCS];
static unsigned rsx_alloc_calls;   /* rsxMemalign calls so far */
static unsigned rsx_fail_at;       /* the call to fail, 1-based; 0 none */
static unsigned rsx_finish_calls;

static unsigned rsx_live(void)
{
   unsigned i, n = 0;
   for (i = 0; i < FAKE_RSX_MAX_ALLOCS; i++)
      if (rsx_allocs[i].ptr)
         n++;
   return n;
}

static void rsx_release_all(void)
{
   unsigned i;
   for (i = 0; i < FAKE_RSX_MAX_ALLOCS; i++)
   {
      free(rsx_allocs[i].ptr);
      rsx_allocs[i].ptr = NULL;
   }
}

void *rsxMemalign(u32 alignment, u32 size)
{
   unsigned i;
   void *p = NULL;
   if (++rsx_alloc_calls == rsx_fail_at)
      return NULL;
   if (!(p = malloc(size ? size : 1)))
      return NULL;
   for (i = 0; i < FAKE_RSX_MAX_ALLOCS; i++)
      if (!rsx_allocs[i].ptr)
      {
         rsx_allocs[i].ptr  = p;
         rsx_allocs[i].size = size;
         return p;
      }
   fprintf(stderr, "FAIL: more than %d RSX allocations live\n",
         FAKE_RSX_MAX_ALLOCS);
   exit(1);
}

void rsxFree(void *ptr)
{
   unsigned i;
   for (i = 0; i < FAKE_RSX_MAX_ALLOCS; i++)
      if (rsx_allocs[i].ptr && rsx_allocs[i].ptr == ptr)
      {
         free(ptr);
         rsx_allocs[i].ptr = NULL;
         return;
      }
   fprintf(stderr, "FAIL: rsxFree of %p, not RSX memory\n", ptr);
   exit(1);
}

s32 rsxAddressToOffset(const void *ptr, u32 *offset)
{
   unsigned i;
   for (i = 0; i < FAKE_RSX_MAX_ALLOCS; i++)
   {
      const u8 *base = (const u8*)rsx_allocs[i].ptr;
      if (     base
            && (const u8*)ptr >= base
            && (const u8*)ptr <  base + rsx_allocs[i].size)
      {
         *offset = ((i + 1) << 20) + (u32)((const u8*)ptr - base);
         return 0;
      }
   }
   /* PSL1GHT reports nothing either: it hands back a bogus offset */
   fprintf(stderr, "FAIL: rsxAddressToOffset of %p, not RSX memory\n", ptr);
   exit(1);
}

/* ---- the rest of PSL1GHT ------------------------------------------ */

static gcmContextData fake_context;
static void *fake_host_addr;
static u32 fake_labels[256];
static u8  fake_ucode[64];
static rsxProgramConst  fake_const;
static rsxProgramAttrib fake_attrib;

const u8 modern_opaque_vpo[16], modern_opaque_fpo[16];
const u8 modern_alpha_blend_vpo[16], modern_alpha_blend_fpo[16];

s32 rsxInit(gcmContextData **context, u32 cmdSize, u32 ioSize,
      const void *ioAddress)
{
   fake_host_addr = (void*)ioAddress;
   *context       = &fake_context;
   return 0;
}

void rsxFinish(gcmContextData *context, u32 ref_value) { rsx_finish_calls++; }
void rsxSetWriteBackendLabel(gcmContextData *context, u8 index, u32 value)
{ fake_labels[index] = value; }
u32 *gcmGetLabelAddress(const u8 index) { return &fake_labels[index]; }
u32  gcmGetFlipStatus(void) { return 0; }
void gcmResetFlipStatus(void) { }
s32  gcmSetDisplayBuffer(const u8 bufferId, const u32 offset,
      const u32 pitch, const u32 width, const u32 height) { return 0; }
s32  gcmSetFlip(gcmContextData *context, const u8 bufferId) { return 0; }
void gcmSetFlipMode(const u32 mode) { }
void gcmSetWaitFlip(gcmContextData *context) { }

void rsxVertexProgramGetUCode(const rsxVertexProgram *vp, void **ucode,
      u32 *size) { *ucode = fake_ucode; *size = sizeof(fake_ucode); }
void rsxFragmentProgramGetUCode(const rsxFragmentProgram *fp,
      void **ucode, u32 *size) { *ucode = fake_ucode; *size = sizeof(fake_ucode); }
rsxProgramConst *rsxVertexProgramGetConst(const rsxVertexProgram *vp,
      const char *name) { return &fake_const; }
rsxProgramAttrib *rsxVertexProgramGetAttrib(const rsxVertexProgram *vp,
      const char *name) { return &fake_attrib; }
rsxProgramConst *rsxFragmentProgramGetConst(const rsxFragmentProgram *fp,
      const char *name) { return &fake_const; }
rsxProgramAttrib *rsxFragmentProgramGetAttrib(const rsxFragmentProgram *fp,
      const char *name) { return &fake_attrib; }

void rsxBindVertexArrayAttrib(gcmContextData *c, u8 attr, u16 frequency,
      u32 offset, u8 stride, u8 elems, u8 dtype, u8 location) { }
void rsxClearSurface(gcmContextData *c, u32 clear_mask) { }
static unsigned rsx_draw_calls;
void rsxDrawVertexArray(gcmContextData *c, u32 type, u32 start, u32 count)
{ rsx_draw_calls++; }
void rsxFlushBuffer(gcmContextData *c) { }
void rsxInvalidateTextureCache(gcmContextData *c, u32 type) { }
void rsxLoadFragmentProgramLocation(gcmContextData *c,
      const rsxFragmentProgram *program, u32 offset, u32 location) { }
void rsxLoadTexture(gcmContextData *c, u8 index, const gcmTexture *t) { }
void rsxLoadVertexProgram(gcmContextData *c,
      const rsxVertexProgram *program, const void *ucode) { }
void rsxSetAlphaFunc(gcmContextData *c, u32 alphaFunc, u32 ref) { }
void rsxSetAlphaTestEnable(gcmContextData *c, u32 enable) { }
void rsxSetBlendEnable(gcmContextData *c, u32 enable) { }
void rsxSetBlendEnableMrt(gcmContextData *c, u32 mrt1, u32 mrt2,
      u32 mrt3) { }
void rsxSetBlendEquation(gcmContextData *c, u16 color, u16 alpha) { }
void rsxSetBlendFunc(gcmContextData *c, u16 sfcolor, u16 dfcolor,
      u16 sfalpha, u16 dfalpha) { }
void rsxSetClearColor(gcmContextData *c, u32 color) { }
void rsxSetClearDepthStencil(gcmContextData *c, u32 value) { }
void rsxSetColorMask(gcmContextData *c, u32 mask) { }
void rsxSetColorMaskMrt(gcmContextData *c, u32 mask) { }
void rsxSetDepthFunc(gcmContextData *c, u32 func) { }
void rsxSetDepthTestEnable(gcmContextData *c, u32 enable) { }
void rsxSetScissor(gcmContextData *c, u16 x, u16 y, u16 w, u16 h) { }
void rsxSetSurface(gcmContextData *c, const gcmSurface *surface) { }
void rsxSetUserClipPlaneControl(gcmContextData *c, u32 plane0,
      u32 plane1, u32 plane2, u32 plane3, u32 plane4, u32 plane5) { }
void rsxSetVertexProgramParameter(gcmContextData *c,
      const rsxVertexProgram *program, const rsxProgramConst *param,
      const f32 *value) { }
void rsxSetViewport(gcmContextData *c, u16 x, u16 y, u16 width,
      u16 height, f32 min, f32 max, const f32 scale[4],
      const f32 offset[4]) { }
void rsxSetViewportClip(gcmContextData *c, u8 sel, u16 width,
      u16 height) { }
void rsxSetWaitLabel(gcmContextData *c, u8 index, u32 value) { }
void rsxSetZMinMaxControl(gcmContextData *c, u8 cullNearFar,
      u8 zClampEnable, u8 cullIgnoreW) { }
void rsxTextureControl(gcmContextData *c, u8 index, u32 enable,
      u16 minlod, u16 maxlod, u8 maxaniso) { }
void rsxTextureFilter(gcmContextData *c, u8 index, u16 bias, u8 min,
      u8 mag, u8 conv) { }
void rsxTextureWrapMode(gcmContextData *c, u8 index, u8 wraps,
      u8 wrapt, u8 wrapr, u8 unsignedRemap, u8 zfunc, u8 gamma) { }

int32_t videoGetState(int32_t videoOut, int32_t deviceIndex,
      videoState *state)
{
   memset(state, 0, sizeof(*state));
   state->displayMode.resolution = VIDEO_RESOLUTION_480;
   return 0;
}
int32_t videoGetResolution(int32_t resolutionId, videoResolution *res)
{
   res->width  = 64;
   res->height = 48;
   return 0;
}
int32_t videoConfigure(int32_t videoOut, videoConfiguration *config,
      void *option, int32_t blocking) { return 0; }
unsigned ps3_display_server_resolution(unsigned system_id)
{ return system_id; }

/* ---- the rest of RetroArch ---------------------------------------- */

static settings_t fake_settings;
static struct retro_hw_render_callback fake_hwr;
static gfx_ctx_driver_t fake_ctx_driver;
static struct font_atlas fake_atlas;
static uint8_t fake_atlas_buffer[32 * 16];

void RARCH_ERR(const char *fmt, ...) { }
settings_t *config_get_ptr(void) { return &fake_settings; }
uint32_t runloop_get_flags(void) { return 0; }
struct retro_hw_render_callback *video_driver_get_hw_context(void)
{ return &fake_hwr; }
const gfx_ctx_driver_t *video_context_driver_init_first(
      void *data, const char *ident,
      enum gfx_ctx_api api, unsigned major, unsigned minor,
      bool hw_render_ctx, void **ctx_data) { return &fake_ctx_driver; }
bool video_context_driver_set(const gfx_ctx_driver_t *data) { return true; }
void video_driver_set_output_dims(unsigned dims) { }
void video_driver_update_viewport(struct video_viewport* vp,
      bool force_full, bool keep_aspect, bool y_down) { }
void input_driver_video_window(enum input_window_kind window,
      void *window_data) { }
void menu_driver_frame(bool menu_is_alive, video_frame_info_t *video_info) { }
void gfx_widgets_frame(void *data) { }
int32_t sysUtilCheckCallback(void) { return 0; }
uintptr_t video_thread_texture_handle(void *data,
      custom_command_method_t func) { return func(data); }
bool video_coord_array_append(video_coord_array_t *ca,
      const video_coords_t *coords, unsigned count) { return true; }
uint32_t utf8_walk(const char **string) { return *(*string)++; }
void font_driver_render_msg(void *data, const char *msg, size_t msg_len,
      const struct font_params *params, void *font_data) { }
void font_driver_resolve_params(const struct font_params *params,
      font_params_resolved_t *out) { memset(out, 0, sizeof(*out)); }
int font_renderer_get_message_width(
      const font_renderer_driver_t *renderer, void *renderer_data,
      const char *msg, size_t msg_len, float scale) { return 0; }

static struct font_atlas *fake_get_atlas(void *data) { return &fake_atlas; }
static void fake_font_free(void *data) { }
static font_renderer_driver_t fake_font_renderer;

int font_renderer_create_default(const font_renderer_driver_t **drv,
      void **handle, const char *font_path, unsigned font_size,
      enum font_atlas_format fmt)
{
   fake_font_renderer.get_atlas = fake_get_atlas;
   fake_font_renderer.free      = fake_font_free;
   fake_atlas.buffer            = fake_atlas_buffer;
   fake_atlas.width             = 32;
   fake_atlas.height            = 16;
   *drv                         = &fake_font_renderer;
   *handle                      = &fake_atlas;
   return 1;
}

/* ---- the tests ---------------------------------------------------- */

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { \
   fprintf(stderr, "FAIL: " __VA_ARGS__); \
   fprintf(stderr, "\n"); failures++; } } while (0)

static video_info_t fake_video_info(void)
{
   video_info_t video;
   memset(&video, 0, sizeof(video));
   return video;
}

/* A driver to draw with, its allocations then forgotten so each test
 * counts only its own */
static rsx_t *driver_up(void)
{
   video_info_t video = fake_video_info();
   rsx_t *rsx;
   rsx_fail_at        = 0;
   rsx                = (rsx_t*)rsx_init(&video);
   if (!rsx)
   {
      fprintf(stderr, "FAIL: rsx_init with all memory available\n");
      exit(1);
   }
   return rsx;
}

static void driver_down(rsx_t *rsx)
{
   free(rsx->white_texture);
   free(rsx);
   rsx_release_all();
}

static void test_texture_grow(void)
{
   static u8 frame16[16 * 4 * 2], frame32[16 * 4 * 4], tall[16 * 8 * 4];
   rsx_t *rsx = driver_up();
   rsx_texture_t tex;
   unsigned i, finishes;

   for (i = 0; i < sizeof(frame16); i++) frame16[i] = (u8)i;
   for (i = 0; i < sizeof(frame32); i++) frame32[i] = (u8)(i * 3);
   for (i = 0; i < sizeof(tall);    i++) tall[i]    = (u8)(i * 7);

   /* Sized, as the driver sizes its frame textures, for a screen
    * height: 4 rows here */
   memset(&tex, 0, sizeof(tex));
   tex.width  = 16;
   tex.height = 4;

   rsx_load_texture_data(rsx, &tex, frame16, 16, 4, 16 * 2,
         false, false, TEXTURE_FILTER_NEAREST);
   CHECK(tex.data && !memcmp(tex.data, frame16, sizeof(frame16)),
         "texture_grow: a 16-bit frame loads");

   /* 16 to 32 bits: twice the pitch */
   finishes = rsx_finish_calls;
   rsx_load_texture_data(rsx, &tex, frame32, 16, 4, 16 * 4,
         true, false, TEXTURE_FILTER_NEAREST);
   CHECK(tex.data && !memcmp(tex.data, frame32, sizeof(frame32)),
         "texture_grow: a 32-bit frame loads into the same texture");
   CHECK(rsx_finish_calls == finishes + 1,
         "texture_grow: the RSX is waited for before its buffer goes");

   /* Taller than the screen */
   rsx_load_texture_data(rsx, &tex, tall, 16, 8, 16 * 4,
         true, false, TEXTURE_FILTER_NEAREST);
   CHECK(tex.data && !memcmp(tex.data, tall, sizeof(tall)),
         "texture_grow: a frame taller than the screen loads");

   /* Back down: the buffer is kept */
   finishes = rsx_finish_calls;
   rsx_load_texture_data(rsx, &tex, frame16, 16, 4, 16 * 2,
         false, false, TEXTURE_FILTER_NEAREST);
   CHECK(tex.data && !memcmp(tex.data, frame16, sizeof(frame16)),
         "texture_grow: a smaller frame loads");
   CHECK(rsx_finish_calls == finishes,
         "texture_grow: a smaller frame keeps the buffer");

   driver_down(rsx);
   printf("texture_grow: ok\n");
}

static void test_texture_fail(void)
{
   static const uint32_t pixels[4 * 4] = { 0 };
   struct texture_image image;
   rsx_t *rsx = driver_up();
   unsigned live;
   uintptr_t id;

   memset(&image, 0, sizeof(image));
   image.pixels = (uint32_t*)pixels;
   image.width  = 4;
   image.height = 4;

   /* Every allocation from here on fails */
   live            = rsx_live();
   rsx_alloc_calls = 0;
   rsx_fail_at     = 1;
   id              = rsx_load_texture_internal(rsx, &image,
         TEXTURE_FILTER_NEAREST);
   rsx_fail_at     = 2;
   if (!id)
      id           = rsx_load_texture_internal(rsx, &image,
         TEXTURE_FILTER_NEAREST);
   rsx_fail_at     = 0;
   CHECK(!id || ((rsx_texture_t*)id)->data,
         "texture_fail: a texture with no RSX memory has no data");
   if (!id)
      CHECK(rsx_live() == live, "texture_fail: nothing is left allocated");
   else
      free((void*)id);

   driver_down(rsx);
   printf("texture_fail: ok\n");
}

static void test_frame_fail(void)
{
   static u8 frame[16 * 8 * 2];
   video_frame_info_t video_info;
   rsx_t *rsx = driver_up();
   unsigned draws;

   memset(&video_info, 0, sizeof(video_info));

   /* The frame texture's allocation fails */
   draws           = rsx_draw_calls;
   rsx_alloc_calls = 0;
   rsx_fail_at     = 1;
   rsx_frame(rsx, frame, VIDEO_SCALE_PACK(16, 8), 1, 16 * 2, NULL,
         &video_info);
   rsx_fail_at     = 0;
   CHECK(rsx_draw_calls == draws,
         "frame_fail: a frame with no texture memory is drawn");

   /* With memory it is */
   draws           = rsx_draw_calls;
   rsx_frame(rsx, frame, VIDEO_SCALE_PACK(16, 8), 2, 16 * 2, NULL,
         &video_info);
   CHECK(rsx_draw_calls > draws, "frame_fail: a frame is drawn");

   driver_down(rsx);
   printf("frame_fail: ok\n");
}

static bool driver_whole(const rsx_t *rsx)
{
   unsigned i;
   for (i = 0; i < RSX_MAX_SHADERS; i++)
      if (!rsx->fp_buffer[i])
         return false;
   for (i = 0; i < RSX_MAX_BUFFERS; i++)
      if (!rsx->buffers[i].ptr)
         return false;
   return rsx->context && rsx->depth_buffer && rsx->vertices
#if RSX_MAX_TEXTURE_VERTICES > 0
      && rsx->texture_vertices
#endif
      && rsx->white_texture && rsx->white_texture->data;
}

static void test_init_sweep(void)
{
   video_info_t video = fake_video_info();
   unsigned allocs, k;
   rsx_t *rsx;

   /* How many allocations rsx_init makes */
   rsx_alloc_calls = 0;
   rsx_fail_at     = 0;
   rsx             = (rsx_t*)rsx_init(&video);
   allocs          = rsx_alloc_calls;
   CHECK(rsx && driver_whole(rsx), "init_sweep: rsx_init with memory");
   if (rsx)
      driver_down(rsx);

   for (k = 1; k <= allocs; k++)
   {
      unsigned i;
      rsx_alloc_calls = 0;
      rsx_fail_at     = k;
      rsx             = (rsx_t*)rsx_init(&video);
      rsx_fail_at     = 0;
      if (rsx)
      {
         CHECK(driver_whole(rsx),
               "init_sweep: allocation %u of %u failed, and rsx_init "
               "gave a driver missing what it draws with", k, allocs);
         driver_down(rsx);
         continue;
      }
      /* What is left is the display and depth buffers, each a
       * screen of 32-bit pixels */
      for (i = 0; i < FAKE_RSX_MAX_ALLOCS; i++)
         CHECK(!rsx_allocs[i].ptr || rsx_allocs[i].size == 64 * 48 * 4,
               "init_sweep: allocation %u of %u failed, and rsx_init "
               "left a %u-byte one behind", k, allocs,
               (unsigned)rsx_allocs[i].size);
      rsx_release_all();
   }
   printf("init_sweep: ok (%u allocations)\n", allocs);
}

static void test_font_sweep(void)
{
   rsx_t *rsx = driver_up();
   unsigned allocs, k, live = rsx_live();
   rsx_font_t *font;

   rsx_alloc_calls = 0;
   font            = (rsx_font_t*)rsx_font_init(rsx, NULL, 12.0f, false);
   allocs          = rsx_alloc_calls;
   CHECK(font && font->vertices && font->texture.data,
         "font_sweep: rsx_font_init with memory");
   if (font)
      rsx_font_free(font, false);

   for (k = 1; k <= allocs; k++)
   {
      rsx_alloc_calls = 0;
      rsx_fail_at     = k;
      font            = (rsx_font_t*)rsx_font_init(rsx, NULL, 12.0f, false);
      rsx_fail_at     = 0;
      if (font)
      {
         CHECK(font->vertices && font->texture.data,
               "font_sweep: allocation %u of %u failed, and "
               "rsx_font_init gave a font missing its vertices or "
               "texture", k, allocs);
         rsx_font_free(font, false);
      }
      CHECK(rsx_live() == live,
            "font_sweep: allocation %u of %u failed, and RSX memory "
            "was left behind", k, allocs);
   }

   driver_down(rsx);
   printf("font_sweep: ok (%u allocations)\n", allocs);
}

static void test_overlay_sweep(void)
{
   static const uint32_t pixels[4 * 4] = { 0 };
   struct texture_image images[2];
   rsx_t *rsx = driver_up();
   unsigned allocs, k, i, live = rsx_live();

   memset(images, 0, sizeof(images));
   for (i = 0; i < 2; i++)
   {
      images[i].pixels = (uint32_t*)pixels;
      images[i].width  = 4;
      images[i].height = 4;
   }

   rsx_alloc_calls = 0;
   CHECK(rsx_overlay_load(rsx, images, 2), "overlay_sweep: with memory");
   allocs          = rsx_alloc_calls;
   rsx_free_overlay(rsx);

   for (k = 1; k <= allocs; k++)
   {
      bool ok;
      rsx_alloc_calls = 0;
      rsx_fail_at     = k;
      ok              = rsx_overlay_load(rsx, images, 2);
      rsx_fail_at     = 0;
      if (ok)
      {
         for (i = 0; i < rsx->overlays; i++)
            CHECK(rsx->overlay[i].vertices && rsx->overlay[i].texture.data,
                  "overlay_sweep: allocation %u of %u failed, and "
                  "rsx_overlay_load kept an image missing its vertices "
                  "or texture", k, allocs);
         rsx_free_overlay(rsx);
      }
      else
         CHECK(!rsx->overlay && rsx_live() == live,
               "overlay_sweep: allocation %u of %u failed, and "
               "rsx_overlay_load left images behind", k, allocs);
   }

   driver_down(rsx);
   printf("overlay_sweep: ok (%u allocations)\n", allocs);
}

int main(int argc, char **argv)
{
   static const struct
   {
      const char *name;
      void (*run)(void);
   } tests[] = {
      { "texture_grow",  test_texture_grow  },
      { "texture_fail",  test_texture_fail  },
      { "frame_fail",    test_frame_fail    },
      { "init_sweep",    test_init_sweep    },
      { "font_sweep",    test_font_sweep    },
      { "overlay_sweep", test_overlay_sweep },
   };
   unsigned i;

   /* One test by name, or all of them */
   for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
      if (argc < 2 || !strcmp(argv[1], tests[i].name))
         tests[i].run();
   free(fake_host_addr);
   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("rsx_alloc: all passed\n");
   return 0;
}
