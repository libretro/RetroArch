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
 *  - menu_vertices, font_vertices, menu_frame_vertices, font_queue:
 *    vertices land where the RSX is pointed at, inside their buffer,
 *    and vertices queued for the RSX are not written over by the next
 *    draw, message or frame.
 *  - font_batches: large batches split into draws, independent font
 *    cursors and GPU fences.
 *  - native_menu_frame, menu_dupe_frame: a menu with no new core
 *    frame is presented, over the last core frame if there is one.
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
static unsigned rsx_depth_test_calls;
static u32 rsx_depth_test_enable;

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
static bool fake_defer_backend;
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
void rsxSetWaitForIdle(gcmContextData *context) { }
void rsxSetWriteBackendLabel(gcmContextData *context, u8 index, u32 value)
{ if (!fake_defer_backend) fake_labels[index] = value; }
u32 *gcmGetLabelAddress(const u8 index) { return &fake_labels[index]; }
u32  gcmGetFlipStatus(void) { return 0; }
void gcmResetFlipStatus(void) { }
s32  gcmSetDisplayBuffer(const u8 bufferId, const u32 offset,
      const u32 pitch, const u32 width, const u32 height) { return 0; }
static unsigned rsx_flip_calls;
s32  gcmSetFlip(gcmContextData *context, const u8 bufferId)
{ rsx_flip_calls++; return 0; }
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
#define FAKE_DRAW_OSD UINT32_MAX
#define FAKE_DRAW_WIDGET (UINT32_MAX - 1)
static u32 fake_bound_texture;
static u32 fake_draw_trace[16];
static unsigned fake_draw_count;
static bool fake_trace_draws;

static void fake_record_draw(u32 source)
{
   if (fake_trace_draws && fake_draw_count < ARRAY_SIZE(fake_draw_trace))
      fake_draw_trace[fake_draw_count++] = source;
}
static unsigned rsx_vertex_invalidations;
void rsxInvalidateVertexCache(gcmContextData *context)
{ rsx_vertex_invalidations++; }
void rsxDrawVertexArray(gcmContextData *c, u32 type, u32 start, u32 count)
{ rsx_draw_calls++; fake_record_draw(fake_bound_texture); }
void rsxFlushBuffer(gcmContextData *c) { }
void rsxInvalidateTextureCache(gcmContextData *c, u32 type) { }
void rsxLoadFragmentProgramLocation(gcmContextData *c,
      const rsxFragmentProgram *program, u32 offset, u32 location) { }
void rsxLoadTexture(gcmContextData *c, u8 index, const gcmTexture *t)
{ fake_bound_texture = t->offset; }
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
void rsxSetDepthTestEnable(gcmContextData *c, u32 enable)
{
   rsx_depth_test_calls++;
   rsx_depth_test_enable = enable;
}
void rsxSetDepthWriteEnable(gcmContextData *c, u32 enable) { }
void rsxSetStencilTestEnable(gcmContextData *c, u32 enable) { }
void rsxSetLogicOpEnable(gcmContextData *c, u32 enable) { }
void rsxSetCullFaceEnable(gcmContextData *c, u32 enable) { }
void rsxSetPolygonOffsetFillEnable(gcmContextData *c, u32 enable) { }
void rsxSetShadeModel(gcmContextData *c, u32 mode) { }
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
static bool fake_widgets_visible = true;
static unsigned fake_widget_calls;
bool gfx_widgets_visible(void *data)
{
   video_frame_info_t *info = (video_frame_info_t*)data;
   return info->widgets_active && fake_widgets_visible
      && !info->notifications_hidden && !info->msg_queue_delay;
}
void gfx_widgets_frame(void *data)
{
   fake_widget_calls++;
   if (gfx_widgets_visible(data))
      fake_record_draw(FAKE_DRAW_WIDGET);
}
int32_t sysUtilCheckCallback(void) { return 0; }
uintptr_t video_thread_texture_handle(void *data,
      custom_command_method_t func) { return func(data); }
bool video_coord_array_append(video_coord_array_t *ca,
      const video_coords_t *coords, unsigned count) { return true; }
uint32_t utf8_walk(const char **string) { return *(*string)++; }
void font_driver_render_msg(void *data, const char *msg, size_t msg_len,
      const struct font_params *params, void *font_data)
{ if (msg && msg_len) fake_record_draw(FAKE_DRAW_OSD); }
/* Red of the next message drawn, to tell draws apart */
static float fake_msg_red;
void font_driver_resolve_params(const struct font_params *params,
      font_params_resolved_t *out)
{
   memset(out, 0, sizeof(*out));
   out->color[0] = fake_msg_red;
}
int font_renderer_get_message_width(
      const font_renderer_driver_t *renderer, void *renderer_data,
      const char *msg, size_t msg_len, float scale) { return 0; }

static struct font_atlas *fake_get_atlas(void *data) { return &fake_atlas; }
static void fake_font_free(void *data) { }
static struct font_glyph fake_glyph;
static const struct font_glyph *fake_get_glyph(void *data, uint32_t code)
{ return &fake_glyph; }
static struct font_line_metrics fake_metrics;
static void fake_get_line_metrics(void *data,
      struct font_line_metrics **metrics) { *metrics = &fake_metrics; }
static font_renderer_driver_t fake_font_renderer;

int font_renderer_create_default(const font_renderer_driver_t **drv,
      void **handle, const char *font_path, unsigned font_size,
      enum font_atlas_format fmt)
{
   fake_font_renderer.get_atlas = fake_get_atlas;
   fake_font_renderer.free      = fake_font_free;
   fake_font_renderer.get_glyph = fake_get_glyph;
   fake_font_renderer.get_line_metrics = fake_get_line_metrics;
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

/* The frame as video_driver_frame hands it over: widget visibility
 * answered before the driver draws */
static bool frame_out(rsx_t *rsx, const void *frame, unsigned dims,
      uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *info)
{
   info->widgets_visible = info->widgets_active && gfx_widgets_visible(info);
   return rsx_frame(rsx, frame, dims, frame_count, pitch, msg, info);
}

/* A driver to draw with, its allocations then forgotten so each test
 * counts only its own */
static rsx_t *driver_up(void)
{
   video_info_t video = fake_video_info();
   rsx_t *rsx;
   fake_widgets_visible = true;
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
   frame_out(rsx, frame, VIDEO_SCALE_PACK(16, 8), 1, 16 * 2, NULL,
         &video_info);
   rsx_fail_at     = 0;
   CHECK(rsx_draw_calls == draws,
         "frame_fail: a frame with no texture memory is drawn");

   /* With memory it is */
   draws           = rsx_draw_calls;
   frame_out(rsx, frame, VIDEO_SCALE_PACK(16, 8), 2, 16 * 2, NULL,
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

static void test_native_menu_frame(void)
{
   rsx_t *rsx = driver_up();
   video_frame_info_t info;
   unsigned flips;

   memset(&info, 0, sizeof(info));
   info.menu_st_flags = MENU_ST_FLAG_ALIVE;
   rsx->menu_frame_enable = true;
   flips = rsx_flip_calls;
   CHECK(frame_out(rsx, NULL, 0, 0, 0, NULL, &info),
         "native_menu_frame: frame callback");
   CHECK(rsx_flip_calls == flips + 1,
         "native_menu_frame: menu without a core frame was not presented");
   rsx->menu_frame_enable = false;
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 1, 0, NULL, &info);
   CHECK(rsx_flip_calls == flips + 1,
         "native_menu_frame: disabled menu left on screen");
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 1, 0, NULL, &info);
   CHECK(rsx_flip_calls == flips,
         "native_menu_frame: disabled menu was presented");
   rsx->menu_frame_enable = true;
   info.menu_st_flags = 0;
   frame_out(rsx, NULL, 0, 2, 0, NULL, &info);
   CHECK(rsx_flip_calls == flips,
         "native_menu_frame: inactive menu was presented");
   driver_down(rsx);
   printf("native_menu_frame: ok\n");
}

static void check_vertices(const rsx_vertex_t *vertices, unsigned start,
      const video_coords_t *coords)
{
   unsigned i;
   for (i = 0; i < coords->vertices; i++)
   {
      const rsx_vertex_t *v = &vertices[start + i];
      CHECK(v->x == coords->vertex[2 * i] &&
            v->y == coords->vertex[2 * i + 1] &&
            v->u == coords->tex_coord[2 * i] &&
            v->v == coords->tex_coord[2 * i + 1] &&
            v->r == coords->color[4 * i] &&
            v->g == coords->color[4 * i + 1] &&
            v->b == coords->color[4 * i + 2] &&
            v->a == coords->color[4 * i + 3],
            "vertices: wrong data at vertex %u", start + i);
   }
}

static void test_menu_vertices(void)
{
   static const float vertex[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
   static const float texcoord[] = { 0, 1, 1, 1, 0, 0, 1, 0 };
   static const float color[] = {
      1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1
   };
   video_coords_t coords;
   gfx_display_ctx_draw_t draw;
   rsx_t *rsx = driver_up();
   unsigned i, start, finishes = rsx_finish_calls;
   u32 expected;

   memset(&coords, 0, sizeof(coords));
   memset(&draw, 0, sizeof(draw));
   memset(rsx->texture_vertices, 0,
         sizeof(rsx_vertex_t) * RSX_MAX_TEXTURE_VERTICES);
   coords.vertices = 4;
   coords.vertex = vertex;
   coords.tex_coord = texcoord;
   coords.color = color;
   draw.coords = &coords;
   draw.dims = VIDEO_SCALE_PACK(64, 48);
   for (i = 0; i < RSX_MAX_TEXTURE_VERTICES / 4 + 2; i++)
   {
      start = (i * 4) % RSX_MAX_TEXTURE_VERTICES;
      gfx_display_rsx_draw(&draw, rsx, draw.dims);
      check_vertices(rsx->texture_vertices, start, &coords);
      rsxAddressToOffset(&rsx->texture_vertices[start].x, &expected);
      CHECK(rsx->pos_offset[RSX_SHADER_STOCK_BLEND] == expected,
            "menu_vertices: wrong GPU offset at vertex %u", start);
   }
   CHECK(rsx_finish_calls == finishes, "menu_vertices: waits for the GPU");
   driver_down(rsx);
   printf("menu_vertices: ok\n");
}

static void test_font_vertices(void)
{
   static const float vertex[] = { 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1 };
   static const float color[] = {
      1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1,
      1, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1
   };
   video_coords_t coords;
   rsx_t *rsx = driver_up();
   rsx_font_t *font = (rsx_font_t*)rsx_font_init(rsx, NULL, 12.0f, false);
   unsigned i, start = 0, finishes = rsx_finish_calls;
   u32 expected;

   CHECK(font != NULL, "font_vertices: font initialization");
   if (!font)
   {
      driver_down(rsx);
      return;
   }
   memset(&coords, 0, sizeof(coords));
   memset(font->vertices, 0, sizeof(rsx_vertex_t) * RSX_MAX_FONT_VERTICES);
   coords.vertices = 6;
   coords.vertex = vertex;
   coords.tex_coord = vertex;
   coords.color = color;
   for (i = 0; i < RSX_MAX_FONT_VERTICES / 6 + 2; i++)
   {
      if (start + 6 > RSX_MAX_FONT_VERTICES)
         start = 0;
      rsx_font_draw_vertices(rsx, font, &coords);
      check_vertices(font->vertices, start, &coords);
      rsxAddressToOffset(&font->vertices[start].x, &expected);
      CHECK(font->pos_offset == expected,
            "font_vertices: wrong GPU offset at vertex %u", start);
      start += 6;
   }
   CHECK(rsx_finish_calls == finishes, "font_vertices: waits for the GPU");
   rsx_font_free(font, false);
   driver_down(rsx);
   printf("font_vertices: ok\n");
}

static void test_menu_frame_vertices(void)
{
   static const float vertex[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
   static const float texcoord[] = { 0, 1, 1, 1, 0, 0, 1, 0 };
   float color[16];
   video_coords_t coords;
   gfx_display_ctx_draw_t draw;
   video_frame_info_t info;
   rsx_t *rsx = driver_up();
   rsx_vertex_t *previous = NULL;
   float previous_color = 0;
   unsigned i, j, start, finishes = rsx_finish_calls;

   memset(&coords, 0, sizeof(coords));
   memset(&draw, 0, sizeof(draw));
   memset(&info, 0, sizeof(info));
   info.menu_st_flags = MENU_ST_FLAG_ALIVE;
   rsx->menu_frame_enable = true;
   coords.vertices = 4;
   coords.vertex = vertex;
   coords.tex_coord = texcoord;
   coords.color = color;
   draw.coords = &coords;
   draw.dims = VIDEO_SCALE_PACK(64, 48);
   for (i = 0; i < RSX_MAX_TEXTURE_VERTICES / 4 + 2; i++)
   {
      for (j = 0; j < 16; j++)
         color[j] = (float)i;
      start = (i * 4) % RSX_MAX_TEXTURE_VERTICES;
      gfx_display_rsx_draw(&draw, rsx, draw.dims);
      if (previous)
         CHECK(previous[0].r == previous_color,
               "menu_frame_vertices: queued frame overwritten at frame %u", i);
      check_vertices(rsx->texture_vertices, start, &coords);
      previous = &rsx->texture_vertices[start];
      previous_color = color[0];
      frame_out(rsx, NULL, 0, i, 0, NULL, &info);
   }
   CHECK(rsx_finish_calls == finishes, "menu_frame_vertices: waits for the GPU");
   driver_down(rsx);
   printf("menu_frame_vertices: ok\n");
}

/* Text queued for the RSX is not written over by the next message,
 * raster block or frame */
static void test_font_queue(void)
{
   static float vertex[2 * 6];
   float color[4 * 6];
   video_font_raster_block_t block;
   video_frame_info_t info;
   rsx_t *rsx             = driver_up();
   rsx_font_t *font       = (rsx_font_t*)rsx_font_init(rsx, NULL, 12.0f,
         false);
   rsx_vertex_t *previous = NULL;
   float previous_red     = 0;
   unsigned i, j, start   = 0;
   u32 expected;

   CHECK(font != NULL, "font_queue: font initialization");
   if (!font)
   {
      driver_down(rsx);
      return;
   }
   memset(&block, 0, sizeof(block));
   memset(&info, 0, sizeof(info));
   block.carr.coords.vertex    = vertex;
   block.carr.coords.tex_coord = vertex;
   block.carr.coords.color     = color;
   block.carr.coords.vertices  = 6;
   for (i = 0; i < RSX_MAX_FONT_VERTICES / 6 + 2; i++)
   {
      float red = (float)(i + 1);
      if (start + 6 > RSX_MAX_FONT_VERTICES)
         start = 0;
      if (i & 1)
      {
         fake_msg_red = red;
         rsx_font_render_msg(rsx, font, "a", 1, NULL);
      }
      else
      {
         for (j = 0; j < 6; j++)
         {
            color[4 * j + 0] = red;
            color[4 * j + 1] = 1.0f;
            color[4 * j + 2] = 1.0f;
            color[4 * j + 3] = 1.0f;
         }
         font->block = &block;
         rsx_font_flush_block(VIDEO_SCALE_PACK(64, 48), font);
         font->block = NULL;
      }
      if (previous)
         CHECK(previous->r == previous_red,
               "font_queue: queued text overwritten at draw %u", i);
      CHECK(font->vertices[start].r == red,
            "font_queue: wrong data at vertex %u", start);
      rsxAddressToOffset(&font->vertices[start].x, &expected);
      CHECK(font->pos_offset == expected,
            "font_queue: wrong GPU offset at vertex %u", start);
      previous     = &font->vertices[start];
      previous_red = red;
      start       += 6;
      if (i % 3 == 2)
         frame_out(rsx, NULL, 0, i, 0, NULL, &info);
   }
   fake_msg_red = 0;
   rsx_font_free(font, false);
   driver_down(rsx);
   printf("font_queue: ok\n");
}

/* Ozone batches may exceed a font page: they are split into page-sized
 * draws, nothing is allocated and nothing still on the GPU is reused. */
static void test_font_batches(void)
{
   video_coords_t coords;
   rsx_t *rsx        = driver_up();
   rsx_font_t *font  = (rsx_font_t*)rsx_font_init(rsx, NULL, 12.0f, false);
   rsx_font_t *other = (rsx_font_t*)rsx_font_init(rsx, NULL, 12.0f, false);
   unsigned count    = 9384;
   float *data       = (float*)malloc(count * 8 * sizeof(float));
   float first[RSX_FONT_VERTEX_PAGES];
   unsigned i, draws, allocations, finishes = rsx_finish_calls;
   unsigned first_draw    = rsx_draw_calls;
   unsigned invalidations = rsx_vertex_invalidations;
   CHECK(font && other && data, "font_batches: allocation");
   if (!font || !other || !data)
      exit(1);
   for (i = 0; i < count * 8; i++)
      data[i] = 1.0f;
   memset(&coords, 0, sizeof(coords));
   coords.vertex    = data;
   coords.tex_coord = data + count * 2;
   coords.color     = data + count * 4;
   coords.vertices  = 6;
   rsx_font_draw_vertices(rsx, font, &coords);
   rsx_font_draw_vertices(rsx, other, &coords);
   CHECK(other->vertex_pages[0].used == 6,
         "font_batches: font cursors shared");
   rsx_font_draw_vertices(rsx, font, &coords);
   CHECK(font->vertex_pages[0].used == 12,
         "font_batches: other font moved cursor");

   fake_defer_backend = true;
   coords.vertices    = count;
   allocations        = rsx_alloc_calls;
   draws              = rsx_draw_calls;
   rsx_font_draw_vertices(rsx, font, &coords);
   CHECK(rsx_draw_calls == draws + 2,
         "font_batches: oversized batch not split into page-sized draws");
   CHECK(font->vertex_pages[font->vertex_page].used
         == count - RSX_FONT_VERTEX_CHUNK
         && font->vertices[count - RSX_FONT_VERTEX_CHUNK - 1].a == 1.0f,
         "font_batches: oversized batch truncated");

   /* Fill pages until every one is waiting on the GPU */
   for (i = 0; i < RSX_FONT_VERTEX_PAGES * 2; i++)
   {
      data[count * 4] = (float)(i + 2);
      draws = rsx_draw_calls;
      rsx_font_draw_vertices(rsx, font, &coords);
      if (rsx_draw_calls != draws + 2)
         break;
   }
   CHECK(i < RSX_FONT_VERTEX_PAGES * 2,
         "font_batches: pages never ran out with the GPU stalled");
   for (i = 0; i < RSX_FONT_VERTEX_PAGES; i++)
      first[i] = font->vertex_pages[i].vertices[0].r;
   draws = rsx_draw_calls;
   data[count * 4] = -1.0f;
   rsx_font_draw_vertices(rsx, font, &coords);
   CHECK(rsx_draw_calls == draws,
         "font_batches: reused a buffer still on the GPU");
   for (i = 0; i < RSX_FONT_VERTEX_PAGES; i++)
      CHECK(font->vertex_pages[i].vertices[0].r == first[i],
            "font_batches: pending page %u overwritten", i);
   fake_labels[RSX_FONT_VERTEX_LABEL] = rsx_font_vertex_serial;
   rsx_font_draw_vertices(rsx, font, &coords);
   CHECK(rsx_draw_calls == draws + 2,
         "font_batches: completed pages not reused");
   CHECK(rsx_alloc_calls == allocations,
         "font_batches: allocated while drawing");
   CHECK(rsx_finish_calls == finishes, "font_batches: waits for the GPU");
   CHECK(rsx_vertex_invalidations - invalidations
         == rsx_draw_calls - first_draw,
         "font_batches: vertex cache not invalidated between batches");
   fake_defer_backend = false;
   fake_labels[RSX_FONT_VERTEX_LABEL] = rsx_font_vertex_serial;
   free(data);
   rsx_font_free(other, false);
   rsx_font_free(font, false);
   driver_down(rsx);
   printf("font_batches: ok\n");
}

/* A duped core frame is drawn again behind the menu, as the surface
 * it was drawn to has been cleared */
static void test_menu_dupe_frame(void)
{
   static u8 frame[16 * 8 * 2];
   video_frame_info_t info;
   rsx_t *rsx = driver_up();
   unsigned draws, flips;

   memset(&info, 0, sizeof(info));
   info.menu_st_flags     = MENU_ST_FLAG_ALIVE;
   rsx->menu_frame_enable = true;

   draws = rsx_draw_calls;
   frame_out(rsx, NULL, 0, 0, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws,
         "menu_dupe_frame: drawn with no core frame yet");

   frame_out(rsx, frame, VIDEO_SCALE_PACK(16, 8), 1, 16 * 2, NULL, &info);
   draws = rsx_draw_calls;
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 2, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws + 1,
         "menu_dupe_frame: duped core frame not drawn behind the menu");
   CHECK(rsx_flip_calls == flips + 1,
         "menu_dupe_frame: menu over a duped frame not presented");

   info.menu_st_flags = 0;
   draws = rsx_draw_calls;
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 3, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws + 1 && rsx_flip_calls == flips + 1,
         "menu_dupe_frame: closed menu left on screen over a dupe");
   draws = rsx_draw_calls;
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 4, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws && rsx_flip_calls == flips,
         "menu_dupe_frame: a dupe without the menu was redrawn");

   driver_down(rsx);
   printf("menu_dupe_frame: ok\n");
}

static void test_widget_dupe_frame(void)
{
   static u8 pixels[4 * 4 * 2];
   struct retro_rsx_image image;
   video_frame_info_t info;
   rsx_t *rsx;
   unsigned draws, flips, allocations, finishes;

   memset(&info, 0, sizeof(info));
   fake_hwr.context_type = RETRO_HW_CONTEXT_RSX;
   rsx = driver_up();
   image.offset = 0x10000;
   image.pitch  = 512 * 4;
   image.width  = 512;
   image.height = 240;
   rsx_set_hw_image(rsx, &image);
   frame_out(rsx, RETRO_HW_FRAME_BUFFER_VALID, 0, 0, 0, NULL, &info);

   info.widgets_active = true;
   draws       = rsx_draw_calls;
   flips       = rsx_flip_calls;
   allocations = rsx_alloc_calls;
   finishes    = rsx_finish_calls;
   frame_out(rsx, NULL, 0, 1, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws + 1 && rsx_flip_calls == flips + 1,
         "widget_dupe_frame: hardware image and widgets not presented");
   CHECK(rsx_alloc_calls == allocations && rsx_finish_calls == finishes,
         "widget_dupe_frame: hardware replay allocates or waits");

   info.menu_st_flags     = MENU_ST_FLAG_ALIVE;
   rsx->menu_frame_enable = true;
   draws = rsx_draw_calls;
   frame_out(rsx, NULL, 0, 2, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws + 1,
         "widget_dupe_frame: image drawn twice behind menu and widgets");
   info.menu_st_flags     = 0;
   rsx->menu_frame_enable = false;

   rsx_set_hw_image(rsx, NULL);
   frame_out(rsx, pixels, VIDEO_SCALE_PACK(4, 4), 3, 8, NULL, &info);
   draws = rsx_draw_calls;
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 4, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws + 1 && rsx_flip_calls == flips + 1,
         "widget_dupe_frame: CPU fallback and widgets not presented");

   info.widgets_active = false;
   draws = rsx_draw_calls;
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 5, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws + 1 && rsx_flip_calls == flips + 1,
         "widget_dupe_frame: disabling widgets leaves old pixels");
   draws = rsx_draw_calls;
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 6, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws && rsx_flip_calls == flips,
         "widget_dupe_frame: inactive widgets keep redrawing duplicates");
   driver_down(rsx);

   fake_hwr.context_type = RETRO_HW_CONTEXT_NONE;
   rsx = driver_up();
   info.widgets_active = true;
   frame_out(rsx, pixels, VIDEO_SCALE_PACK(4, 4), 0, 8, NULL, &info);
   draws = rsx_draw_calls;
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 1, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws + 1 && rsx_flip_calls == flips + 1,
         "widget_dupe_frame: software image and widgets not presented");
   driver_down(rsx);
   printf("widget_dupe_frame: ok\n");
}

static void test_widget_visibility(void)
{
   struct retro_rsx_image image;
   video_frame_info_t info;
   rsx_t *rsx;
   unsigned draws, flips, restores, calls;

   memset(&info, 0, sizeof(info));
   fake_hwr.context_type = RETRO_HW_CONTEXT_RSX;
   rsx = driver_up();
   image.offset = 0x10000;
   image.pitch  = 512 * 4;
   image.width  = 512;
   image.height = 240;
   rsx_set_hw_image(rsx, &image);
   frame_out(rsx, RETRO_HW_FRAME_BUFFER_VALID, 0, 0, 0, NULL, &info);

   info.widgets_active  = true;
   fake_widgets_visible = false;
   draws    = rsx_draw_calls;
   flips    = rsx_flip_calls;
   restores = rsx_depth_test_calls;
   calls    = fake_widget_calls;
   frame_out(rsx, NULL, 0, 1, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws && rsx_flip_calls == flips
         && rsx_depth_test_calls == restores,
         "widget_visibility: empty widgets redraw or restore a duplicate");
   CHECK(fake_widget_calls == calls + 1,
         "widget_visibility: invisible widgets skip frame housekeeping");
   frame_out(rsx, NULL, 0, 1, 0, "", &info);
   CHECK(rsx_draw_calls == draws && rsx_flip_calls == flips
         && rsx_depth_test_calls == restores,
         "widget_visibility: empty OSD message redraws an idle duplicate");

   fake_widgets_visible = true;
   frame_out(rsx, NULL, 0, 2, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws + 1 && rsx_flip_calls == flips + 1,
         "widget_visibility: newly visible widgets not presented");
   info.notifications_hidden = true;
   draws = rsx_draw_calls;
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 3, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws + 1 && rsx_flip_calls == flips + 1,
         "widget_visibility: hidden notification leaves old pixels");
   draws = rsx_draw_calls;
   flips = rsx_flip_calls;
   restores = rsx_depth_test_calls;
   frame_out(rsx, NULL, 0, 4, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws && rsx_flip_calls == flips
         && rsx_depth_test_calls == restores,
         "widget_visibility: hidden widgets keep presenting duplicates");
   frame_out(rsx, NULL, 0, 4, 0, "", &info);
   CHECK(rsx_draw_calls == draws && rsx_flip_calls == flips
         && rsx_depth_test_calls == restores,
         "widget_visibility: empty OSD message redraws hidden widgets");

   info.notifications_hidden = false;
   frame_out(rsx, NULL, 0, 5, 0, NULL, &info);
   fake_widgets_visible = false;
   draws = rsx_draw_calls;
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 6, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws + 1 && rsx_flip_calls == flips + 1,
         "widget_visibility: expired notification leaves old pixels");
   draws = rsx_draw_calls;
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 7, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws && rsx_flip_calls == flips,
         "widget_visibility: expired notification needs repeated cleanup");

   fake_widgets_visible = true;
   frame_out(rsx, NULL, 0, 8, 0, NULL, &info);
   fake_widgets_visible = false;
   frame_out(rsx, RETRO_HW_FRAME_BUFFER_VALID, 0, 9, 0, NULL, &info);
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 10, 0, NULL, &info);
   CHECK(rsx_flip_calls == flips,
         "widget_visibility: real frame did not consume pending cleanup");
   driver_down(rsx);
   fake_hwr.context_type = RETRO_HW_CONTEXT_NONE;
   printf("widget_visibility: ok\n");
}

static void test_widget_layer_order(void)
{
   uint32_t pixels[4] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
   struct texture_image overlay;
   struct retro_rsx_image image;
   video_frame_info_t info;
   rsx_t *rsx;
   u32 overlay_offset, core_offset;
   unsigned i, flips;

   memset(&info, 0, sizeof(info));
   memset(&overlay, 0, sizeof(overlay));
   overlay.pixels = pixels;
   overlay.width = overlay.height = 2;
   fake_hwr.context_type = RETRO_HW_CONTEXT_RSX;
   rsx = driver_up();
   image.offset = 0x10000;
   image.pitch  = 512 * 4;
   image.width  = 512;
   image.height = 240;
   rsx_set_hw_image(rsx, &image);
   frame_out(rsx, RETRO_HW_FRAME_BUFFER_VALID, 0, 0, 0, NULL, &info);
   CHECK(rsx_overlay_load(rsx, &overlay, 1), "widget_layer_order: overlay load");
   overlay_offset = rsx->overlay[0].texture.tex.offset;

   for (i = 0; i < 2; i++)
   {
      if (i)
      {
         rsx_set_hw_image(rsx, NULL);
         frame_out(rsx, pixels, VIDEO_SCALE_PACK(2, 2), 1, 4, NULL, &info);
      }
      core_offset = i ? rsx->texture[rsx->tex_index].tex.offset : image.offset;
      rsx->overlay_enable  = true;
      info.widgets_active = true;
      info.statistics_show = true;
      info.stat_text       = "statistics";
      info.stat_text_len   = strlen(info.stat_text);
      fake_trace_draws = true;
      fake_draw_count = 0;
      frame_out(rsx, NULL, 0, 2, 0, NULL, &info);
      CHECK(fake_draw_count == 4 && fake_draw_trace[0] == core_offset
            && fake_draw_trace[1] == FAKE_DRAW_OSD
            && fake_draw_trace[2] == overlay_offset
            && fake_draw_trace[3] == FAKE_DRAW_WIDGET,
            "widget_layer_order: core/statistics/overlay/widgets order (%u)", i);
      fake_trace_draws = false;
      rsx->overlay_enable  = false;
      info.widgets_active = false;
      info.statistics_show = false;
      frame_out(rsx, NULL, 0, 3, 0, NULL, &info);
   }

   for (i = 0; i < 3; i++)
   {
      const char *msg = i == 2 ? "message" : NULL;
      rsx->overlay_enable  = i == 0;
      info.statistics_show = i == 1;
      fake_trace_draws = true;
      fake_draw_count = 0;
      flips = rsx_flip_calls;
      frame_out(rsx, NULL, 0, 4, 0, msg, &info);
      CHECK(fake_draw_count == 2 && fake_draw_trace[0] == core_offset
            && fake_draw_trace[1] == (i == 0 ? overlay_offset : FAKE_DRAW_OSD)
            && rsx_flip_calls == flips + 1,
            "widget_layer_order: standalone frontend layer not composed (%u)", i);
      fake_trace_draws = false;
   }

   rsx_free_overlay(rsx);
   driver_down(rsx);
   fake_hwr.context_type = RETRO_HW_CONTEXT_NONE;
   printf("widget_layer_order: ok\n");
}

/* Any frontend layer that goes away over duped frames is taken off by
 * one more present of the bare core image, then dupes skip again. */
static void test_ui_cleanup(void)
{
   static u8 pixels[4 * 4 * 2];
   video_frame_info_t info;
   rsx_t *rsx;
   unsigned i, draws, flips;

   memset(&info, 0, sizeof(info));
   rsx = driver_up();
   frame_out(rsx, pixels, VIDEO_SCALE_PACK(4, 4), 0, 8, NULL, &info);

   for (i = 0; i < 3; i++)
   {
      const char *msg = i == 0 ? "message" : NULL;
      rsx->overlay_enable  = i == 1;
      info.statistics_show = i == 2;
      frame_out(rsx, NULL, 0, 1, 0, msg, &info);
      rsx->overlay_enable  = false;
      info.statistics_show = false;

      draws = rsx_draw_calls;
      flips = rsx_flip_calls;
      frame_out(rsx, NULL, 0, 2, 0, NULL, &info);
      CHECK(rsx_draw_calls == draws + 1 && rsx_flip_calls == flips + 1,
            "ui_cleanup: removed layer left on screen (%u)", i);
      draws = rsx_draw_calls;
      flips = rsx_flip_calls;
      frame_out(rsx, NULL, 0, 3, 0, "", &info);
      CHECK(rsx_draw_calls == draws && rsx_flip_calls == flips,
            "ui_cleanup: cleanup presented more than once (%u)", i);
   }

   /* A real frame without layers clears the pending cleanup */
   frame_out(rsx, NULL, 0, 4, 0, "message", &info);
   frame_out(rsx, pixels, VIDEO_SCALE_PACK(4, 4), 5, 8, NULL, &info);
   flips = rsx_flip_calls;
   frame_out(rsx, NULL, 0, 6, 0, NULL, &info);
   CHECK(rsx_flip_calls == flips,
         "ui_cleanup: real frame did not consume pending cleanup");

   driver_down(rsx);
   printf("ui_cleanup: ok\n");
}

static void test_hw_interface(void)
{
   const struct retro_hw_render_interface *base = NULL;
   const struct retro_hw_render_interface_rsx *iface;
   struct retro_rsx_image image;
   video_frame_info_t info;
   rsx_t *rsx;
   unsigned draws, allocations, finishes;
   memset(&info, 0, sizeof(info));
   CHECK(!rsx_get_hw_render_interface(NULL, &base),
         "hw_interface: no interface without a driver");
   fake_hwr.context_type = RETRO_HW_CONTEXT_NONE;
   rsx = driver_up();
   CHECK(!rsx_get_hw_render_interface(rsx, &base),
         "hw_interface: software cores keep the software path");
   driver_down(rsx);
   fake_hwr.context_type = RETRO_HW_CONTEXT_RSX;
   rsx = driver_up();
   CHECK(rsx_get_hw_render_interface(rsx, &base),
         "hw_interface: negotiated RSX interface available");
   iface = (const struct retro_hw_render_interface_rsx*)base;
   CHECK(iface->interface_type == RETRO_HW_RENDER_INTERFACE_RSX
         && iface->interface_version == RETRO_HW_RENDER_INTERFACE_RSX_VERSION,
         "hw_interface: type and version advertised");
   CHECK(iface->context == rsx->context && iface->io_address == fake_host_addr,
         "hw_interface: frontend owns context and mapping");
   CHECK(iface->io_size == HOST_SIZE && iface->io_used >= CB_SIZE
         && iface->io_used < iface->io_size,
         "hw_interface: host mapping excludes command buffer");
   rsx->video_info.is_threaded = true;
   CHECK(!rsx_get_hw_render_interface(rsx, &base),
         "hw_interface: threaded access refused");
   image.offset = 0x10000;
   image.pitch = 512 * 4;
   image.width = 480;
   image.height = 272;
   CHECK(!iface->set_image(iface->handle, &image),
         "hw_interface: threaded image submission refused");
   rsx->video_info.is_threaded = false;
   CHECK(iface->set_image(iface->handle, &image),
         "hw_interface: core-owned texture accepted");
   allocations = rsx_alloc_calls;
   finishes = rsx_finish_calls;
   draws = rsx_draw_calls;
   frame_out(rsx, RETRO_HW_FRAME_BUFFER_VALID, VIDEO_SCALE_PACK(480, 272),
         0, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws + 1 && rsx_alloc_calls == allocations,
         "hw_interface: GPU image displayed without allocation or CPU upload");
   CHECK(rsx_finish_calls == finishes,
         "hw_interface: handoff does not wait for the GPU");
   info.menu_st_flags     = MENU_ST_FLAG_ALIVE;
   rsx->menu_frame_enable = true;
   draws = rsx_draw_calls;
   frame_out(rsx, NULL, 0, 1, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws + 1,
         "hw_interface: core image not drawn again behind the menu");
   info.menu_st_flags     = 0;
   rsx->menu_frame_enable = false;
   image.pitch = 1;
   CHECK(!iface->set_image(iface->handle, &image),
         "hw_interface: invalid pitch rejected");
   draws = rsx_draw_calls;
   frame_out(rsx, RETRO_HW_FRAME_BUFFER_VALID, VIDEO_SCALE_PACK(480, 272),
         1, 0, NULL, &info);
   CHECK(rsx_draw_calls == draws, "hw_interface: rejected image not reused");
   image.pitch = 512 * 4;
   image.offset = UINT32_MAX - 128;
   CHECK(!iface->set_image(iface->handle, &image),
         "hw_interface: overflowing local-memory range rejected");
   CHECK(iface->set_image(iface->handle, NULL),
         "hw_interface: cached image released");
   driver_down(rsx);
   fake_hwr.context_type = RETRO_HW_CONTEXT_NONE;
   printf("hw_interface: ok\n");
}

static void test_hw_frame_state(void)
{
   uint16_t pixels[4] = { 0, 0, 0, 0 };
   video_frame_info_t info;
   rsx_t *rsx;
   unsigned calls, draws;
   memset(&info, 0, sizeof(info));
   fake_hwr.context_type = RETRO_HW_CONTEXT_RSX;
   rsx = driver_up();

   rsx_depth_test_enable = GCM_TRUE;
   calls = rsx_depth_test_calls;
   frame_out(rsx, RETRO_HW_FRAME_BUFFER_VALID, 0, 0, 0, NULL, &info);
   CHECK(rsx_depth_test_calls == calls + 1
         && rsx_depth_test_enable == GCM_FALSE,
         "hw_frame_state: hardware frame restores frontend state");

   rsx_depth_test_enable = GCM_TRUE;
   calls = rsx_depth_test_calls;
   draws = rsx_draw_calls;
   frame_out(rsx, pixels, VIDEO_SCALE_PACK(2, 2), 1, 4, NULL, &info);
   CHECK(rsx_depth_test_calls == calls + 1
         && rsx_depth_test_enable == GCM_FALSE
         && rsx_draw_calls == draws + 1,
         "hw_frame_state: software fallback restores frontend state");

   rsx_depth_test_enable = GCM_TRUE;
   calls = rsx_depth_test_calls;
   frame_out(rsx, NULL, 0, 2, 0, NULL, &info);
   CHECK(rsx_depth_test_calls == calls
         && rsx_depth_test_enable == GCM_TRUE,
         "hw_frame_state: duplicate frame does not reset state");

   /* A dupe with the menu on top draws, so it must not inherit the
    * core's state either. */
   info.menu_st_flags     = MENU_ST_FLAG_ALIVE;
   rsx->menu_frame_enable = true;
   rsx_depth_test_enable  = GCM_TRUE;
   calls = rsx_depth_test_calls;
   frame_out(rsx, NULL, 0, 3, 0, NULL, &info);
   CHECK(rsx_depth_test_calls == calls + 1
         && rsx_depth_test_enable == GCM_FALSE,
         "hw_frame_state: duplicate frame under the menu restores frontend state");
   rsx->menu_frame_enable = false;
   info.menu_st_flags     = 0;

   driver_down(rsx);
   fake_hwr.context_type = RETRO_HW_CONTEXT_NONE;
   printf("hw_frame_state: ok\n");
}

/* Retired memory is fenced once at the next frame and freed when the
 * GPU passes it, never waited for. */
static void test_hw_retirement(void)
{
   const struct retro_hw_render_interface *base = NULL;
   const struct retro_hw_render_interface_rsx *iface;
   video_frame_info_t info;
   rsx_t *rsx;
   void *memory;
   unsigned live, finishes;
   memset(&info, 0, sizeof(info));
   fake_hwr.context_type = RETRO_HW_CONTEXT_RSX;
   rsx = driver_up();
   CHECK(rsx_get_hw_render_interface(rsx, &base),
         "hw_retirement: interface available");
   iface = (const struct retro_hw_render_interface_rsx*)base;
   CHECK(iface->retire_memory != NULL,
         "hw_retirement: retirement callback advertised");
   memory   = rsxMemalign(128, 128);
   live     = rsx_live();
   finishes = rsx_finish_calls;
   fake_defer_backend = true;
   CHECK(iface->retire_memory(iface->handle, memory),
         "hw_retirement: allocation ownership transferred");
   frame_out(rsx, NULL, 0, 0, 0, NULL, &info);
   rsx_collect_retired_memory();
   CHECK(rsx_live() == live && rsx_finish_calls == finishes,
         "hw_retirement: pending GPU memory preserved without waiting");
   free(rsx->white_texture);
   free(rsx);
   CHECK(rsx_retired_memory_tail - rsx_retired_memory_head == 1,
         "hw_retirement: survives driver teardown");
   fake_labels[RSX_RETIRED_MEMORY_LABEL] = rsx_retired_memory_serial;
   rsx_collect_retired_memory();
   CHECK(rsx_retired_memory_tail == rsx_retired_memory_head,
         "hw_retirement: completed allocation freed");
   rsx_release_all();
   fake_defer_backend = false;
   fake_hwr.context_type = RETRO_HW_CONTEXT_NONE;
   printf("hw_retirement: ok\n");
}

int main(int argc, char **argv)
{
   static const struct
   {
      const char *name;
      void (*run)(void);
   } tests[] = {
      { "widget_dupe_frame", test_widget_dupe_frame },
      { "widget_visibility", test_widget_visibility },
      { "widget_layer_order", test_widget_layer_order },
      { "ui_cleanup", test_ui_cleanup },
      { "hw_interface", test_hw_interface },
      { "hw_frame_state", test_hw_frame_state },
      { "hw_retirement", test_hw_retirement },
      { "texture_grow",  test_texture_grow  },
      { "texture_fail",  test_texture_fail  },
      { "frame_fail",    test_frame_fail    },
      { "init_sweep",    test_init_sweep    },
      { "font_sweep",    test_font_sweep    },
      { "overlay_sweep", test_overlay_sweep },
      { "menu_vertices", test_menu_vertices },
      { "font_vertices", test_font_vertices },
      { "native_menu_frame", test_native_menu_frame },
      { "menu_frame_vertices", test_menu_frame_vertices },
      { "font_queue", test_font_queue },
      { "font_batches", test_font_batches },
      { "menu_dupe_frame", test_menu_dupe_frame },
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
