/* OpenVG's on-screen text, drawn by the shipping gfx/drivers/vg.c
 * through the real glyph cache, onto a software OpenVG.
 *
 * vg.c is included here so its text path - vg_render_msg() and the
 * atlas and child-image handling under it - can be driven directly.
 * The OpenVG it calls is below: A8 images with child views that share
 * their parent's pixels, the image-user-to-surface matrix, and
 * VG_DRAW_IMAGE_MULTIPLY drawing into a float RGBA surface whose y
 * axis points up, as OpenVG's does.  Each check maps every inked texel
 * of a glyph in the atlas to the surface pixel it has to land on, so
 * position, orientation, decoding, uploads and regrowth are all held
 * to the cache's own bitmaps. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../../gfx/drivers/vg.c"

int read_should_fail = 0;
extern int read_real_files;

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); fails++; } } while (0)

/* ---- the frontend the rest of vg.c calls -------------------------- */

/* Never reached: only the text path runs. They stand in for the
 * frontend so the driver links whole. */
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void video_driver_set_output_dims(unsigned dims) { (void)dims; }
unsigned video_driver_get_output_dims(void) { return 0; }
bool video_driver_test_all_flags(enum display_flags f) { (void)f; return false; }
bool video_context_driver_set(const gfx_ctx_driver_t *d) { (void)d; return false; }
void video_context_driver_free(void) { }
void video_driver_update_viewport(struct video_viewport *vp,
      bool force_full, bool keep_aspect, bool y_down)
{ (void)vp; (void)force_full; (void)keep_aspect; (void)y_down; }
const gfx_ctx_driver_t *video_context_driver_init_first(void *data,
      const char *ident, enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx, void **ctx_data)
{
   (void)data; (void)ident; (void)api; (void)major; (void)minor;
   (void)hw_render_ctx; (void)ctx_data;
   return NULL;
}

/* ---- a software OpenVG ------------------------------------------- */

#define SURF_W 480
#define SURF_H 200
#define MAX_IMAGES 4096

typedef struct
{
   int      used;
   int      parent;   /* 0 for a root image */
   int      x, y, w, h;
   uint8_t *a8;       /* root images only */
   int      stride;
} mock_image_t;

static mock_image_t images[MAX_IMAGES];
static float  surface[SURF_H][SURF_W][4];
static float  matrix[9]   = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
static VGint  image_mode  = VG_DRAW_IMAGE_NORMAL;
static VGint  scissoring  = VG_TRUE;
static VGint  matrix_mode = VG_MATRIX_IMAGE_USER_TO_SURFACE;
static float  paint_color[4];
static int    live_images = 0;

VGErrorCode vgGetError(void) { return VG_NO_ERROR; }
void vgSeti(VGParamType type, VGint value)
{
   if (type == VG_IMAGE_MODE)
      image_mode = value;
   else if (type == VG_SCISSORING)
      scissoring = value;
   else if (type == VG_MATRIX_MODE)
      matrix_mode = value;
}
void vgSetfv(VGParamType t, VGint c, const VGfloat *v) { (void)t; (void)c; (void)v; }
void vgSetiv(VGParamType t, VGint c, const VGint *v) { (void)t; (void)c; (void)v; }
VGint vgGeti(VGParamType type)
{
   return (type == VG_MAX_IMAGE_WIDTH || type == VG_MAX_IMAGE_HEIGHT) ? 2048 : 0;
}
void vgSetParameteri(VGHandle o, VGint t, VGint v) { (void)o; (void)t; (void)v; }
void vgSetParameterfv(VGHandle o, VGint t, VGint c, const VGfloat *v)
{
   (void)o;
   if (t == VG_PAINT_COLOR && c == 4)
      memcpy(paint_color, v, sizeof(paint_color));
}
void vgLoadIdentity(void)
{
   static const float id[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
   memcpy(matrix, id, sizeof(matrix));
}
/* OpenVG matrices are column-major: { sx, shy, w0, shx, sy, w1, tx, ty, w2 } */
void vgLoadMatrix(const VGfloat *m) { memcpy(matrix, m, sizeof(matrix)); }
void vgTranslate(VGfloat tx, VGfloat ty)
{
   matrix[6] += matrix[0] * tx + matrix[3] * ty;
   matrix[7] += matrix[1] * tx + matrix[4] * ty;
}
void vgScale(VGfloat sx, VGfloat sy)
{
   matrix[0] *= sx; matrix[1] *= sx;
   matrix[3] *= sy; matrix[4] *= sy;
}
VGPaint vgCreatePaint(void) { return 1; }
void vgDestroyPaint(VGPaint p) { (void)p; }
void vgSetPaint(VGPaint p, VGbitfield m) { (void)p; (void)m; }

static int image_new(void)
{
   int i;
   for (i = 1; i < MAX_IMAGES; i++)
      if (!images[i].used)
      {
         memset(&images[i], 0, sizeof(images[i]));
         images[i].used = 1;
         live_images++;
         return i;
      }
   return 0;
}

VGImage vgCreateImage(VGImageFormat f, VGint w, VGint h, VGbitfield q)
{
   int i;
   (void)q;
   if (f != VG_A_8 || !(i = image_new()))
      return VG_INVALID_HANDLE;
   images[i].w = w; images[i].h = h; images[i].stride = w;
   images[i].a8 = (uint8_t*)calloc((size_t)w * h, 1);
   return (VGImage)i;
}

VGImage vgChildImage(VGImage parent, VGint x, VGint y, VGint w, VGint h)
{
   int i;
   if (!parent || !images[parent].used || !(i = image_new()))
      return VG_INVALID_HANDLE;
   images[i].parent = (int)parent;
   images[i].x = x; images[i].y = y; images[i].w = w; images[i].h = h;
   return (VGImage)i;
}

void vgDestroyImage(VGImage img)
{
   if (!img || !images[img].used)
      return;
   free(images[img].a8);
   images[img].a8   = NULL;
   images[img].used = 0;
   live_images--;
}

void vgImageSubData(VGImage img, const void *data, VGint stride,
      VGImageFormat f, VGint x, VGint y, VGint w, VGint h)
{
   int r;
   const uint8_t *src = (const uint8_t*)data;
   if (f != VG_A_8 || !img || !images[img].a8)
      return;
   for (r = 0; r < h; r++)
      memcpy(images[img].a8 + (size_t)(y + r) * images[img].stride + x,
            src + (size_t)r * stride, (size_t)w);
}

/* Image texel (u, v) of a child reads its root at (x + u, y + v) */
static uint8_t texel(int img, int u, int v)
{
   int root = images[img].parent ? images[img].parent : img;
   int x    = images[img].parent ? images[img].x : 0;
   int y    = images[img].parent ? images[img].y : 0;
   if (!images[root].used || !images[root].a8)
      return 0;
   return images[root].a8[(size_t)(y + v) * images[root].stride + x + u];
}

void vgDrawImage(VGImage img)
{
   int u, v;
   if (!img || !images[img].used || image_mode != VG_DRAW_IMAGE_MULTIPLY)
      return;
   for (v = 0; v < images[img].h; v++)
      for (u = 0; u < images[img].w; u++)
      {
         float a  = texel((int)img, u, v) / 255.0f * paint_color[3];
         float fx = matrix[0] * (u + 0.5f) + matrix[3] * (v + 0.5f) + matrix[6];
         float fy = matrix[1] * (u + 0.5f) + matrix[4] * (v + 0.5f) + matrix[7];
         int   sx = (int)floorf(fx), sy = (int)floorf(fy), c;
         if (a <= 0.0f || sx < 0 || sy < 0 || sx >= SURF_W || sy >= SURF_H)
            continue;
         for (c = 0; c < 3; c++)
            surface[sy][sx][c] = paint_color[c] * a + surface[sy][sx][c] * (1 - a);
         surface[sy][sx][3] = a + surface[sy][sx][3] * (1 - a);
      }
}

VGFont vgCreateFont(VGint h) { (void)h; return 0; }
void vgDestroyFont(VGFont f) { (void)f; }
void vgClear(VGint x, VGint y, VGint w, VGint h) { (void)x; (void)y; (void)w; (void)h; }
const VGubyte *vgGetString(VGStringID n) { (void)n; return (const VGubyte*)""; }

/* ---- the checks -------------------------------------------------- */

static vg_t vg_st;

static void clear_surface(void) { memset(surface, 0, sizeof(surface)); }

static int surface_ink(void)
{
   int x, y, n = 0;
   for (y = 0; y < SURF_H; y++)
      for (x = 0; x < SURF_W; x++)
         if (surface[y][x][3] > 0.0f)
            n++;
   return n;
}

/* Every inked texel of @code's glyph is at the surface pixel it maps
 * to with the pen at (@pen_x, @base_y): rows top first, the top row
 * highest on a y-up surface. Returns the inked texels found there. */
static int glyph_lands(uint32_t code, float pen_x, float base_y,
      int *missing)
{
   const struct font_glyph *g = vg_st.font_driver->get_glyph(vg_st.font_data, code);
   struct font_atlas *atlas   = vg_st.font_driver->get_atlas(vg_st.font_data);
   unsigned u, v;
   int found = 0;
   *missing  = 0;
   if (!g || !atlas)
   {
      (*missing)++;
      return 0;
   }
   for (v = 0; v < g->height; v++)
      for (u = 0; u < g->width; u++)
      {
         uint8_t a = atlas->buffer[(size_t)(g->atlas_offset_y + v) * atlas->width
               + g->atlas_offset_x + u];
         int sx    = (int)floorf(pen_x + g->draw_offset_x + u + 0.5f);
         int sy    = (int)floorf(base_y - g->draw_offset_y - v - 0.5f);
         if (!a)
            continue;
         if (     sx >= 0 && sy >= 0 && sx < SURF_W && sy < SURF_H
               && surface[sy][sx][3] > 0.0f)
            found++;
         else
            (*missing)++;
      }
   return found;
}

static struct font_params plain(float x, float y, enum text_alignment align)
{
   struct font_params p;
   memset(&p, 0, sizeof(p));
   p.x          = x;
   p.y          = y;
   p.scale      = 1.0f;
   p.color      = 0xFFFFFFFF;
   p.text_align = align;
   return p;
}

int main(void)
{
   static const char *dejavu =
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
   static const float transform[9] = { 2, 0, 0, 0, 3, 0, 5, 7, 1 };
   struct font_params p;
   struct font_line_metrics *lm = NULL;
   int found, missing, i;
   unsigned first_w = 0, first_h = 0;
   FILE *f;

   if (!(f = fopen(dejavu, "rb")))
   {
      printf("  skip: no %s on this runner\n", dejavu);
      return 0;
   }
   fclose(f);
   read_real_files = 1;
   if (!vg_font_init(&vg_st, dejavu, 16))
      return 1;
   memcpy(vg_st.mTransformMatrix.data, transform, sizeof(transform));
   vg_st.font_driver->get_line_metrics(vg_st.font_data, &lm);
   {
      struct font_atlas *a0 = vg_st.font_driver->get_atlas(vg_st.font_data);
      first_w = a0->width;
      first_h = a0->height;
   }

   /* One glyph, where the message position puts it, upright */
   font_driver_frame_begin();
   clear_surface();
   p = plain(0.1f, 0.5f, TEXT_ALIGN_LEFT);
   vg_render_msg(&vg_st, "T", 1, &p, SURF_W, SURF_H);
   found = glyph_lands('T', 0.1f * SURF_W, 0.5f * SURF_H, &missing);
   CHECK(found > 0 && missing == 0, "'T' lands texel for texel, upright");
   CHECK(surface_ink() == found, "nothing drawn but 'T'");

   /* State the frame's own image draw relies on is as it was */
   CHECK(image_mode == VG_DRAW_IMAGE_NORMAL, "image mode restored");
   CHECK(scissoring == VG_TRUE, "scissoring restored");
   CHECK(!memcmp(matrix, transform, sizeof(transform)), "frame matrix restored");

   /* UTF-8: two bytes, one glyph */
   font_driver_frame_begin();
   clear_surface();
   vg_render_msg(&vg_st, "\xC3\xA9", 2, &p, SURF_W, SURF_H);
   found = glyph_lands(0xE9, 0.1f * SURF_W, 0.5f * SURF_H, &missing);
   CHECK(found > 0 && missing == 0, "U+00E9 drawn as one glyph");
   CHECK(surface_ink() == found, "and nothing else");

   /* A glyph new to the cache reaches the atlas image before it is
    * drawn: the dirty region is uploaded */
   font_driver_frame_begin();
   clear_surface();
   vg_render_msg(&vg_st, "W", 1, &p, SURF_W, SURF_H);
   found = glyph_lands('W', 0.1f * SURF_W, 0.5f * SURF_H, &missing);
   CHECK(found > 0 && missing == 0, "a newly cached glyph is uploaded and drawn");

   /* Second line one line height lower */
   font_driver_frame_begin();
   clear_surface();
   vg_render_msg(&vg_st, "T\nW", 3, &p, SURF_W, SURF_H);
   found = glyph_lands('W', 0.1f * SURF_W, 0.5f * SURF_H - lm->height, &missing);
   CHECK(found > 0 && missing == 0, "the second line is a line height down");

   /* Right alignment ends the line at the position */
   font_driver_frame_begin();
   clear_surface();
   p = plain(0.9f, 0.5f, TEXT_ALIGN_RIGHT);
   vg_render_msg(&vg_st, "T", 1, &p, SURF_W, SURF_H);
   found = glyph_lands('T', 0.9f * SURF_W
         - vg_st.font_driver->get_glyph(vg_st.font_data, 'T')->advance_x,
         0.5f * SURF_H, &missing);
   CHECK(found > 0 && missing == 0, "right-aligned text ends at the position");

   /* The default parameters draw the drop shadow, two pixels down and
    * left, darker */
   font_driver_frame_begin();
   clear_surface();
   config_get_ptr()->floats.video_msg_pos_x   = 0.1f;
   config_get_ptr()->floats.video_msg_pos_y   = 0.5f;
   config_get_ptr()->floats.video_msg_color_r = 1.0f;
   config_get_ptr()->floats.video_msg_color_g = 1.0f;
   config_get_ptr()->floats.video_msg_color_b = 1.0f;
   vg_render_msg(&vg_st, "T", 1, NULL, SURF_W, SURF_H);
   found = glyph_lands('T', 0.1f * SURF_W, 0.5f * SURF_H, &missing);
   CHECK(found > 0 && missing == 0, "the text is drawn over its shadow");
   CHECK(surface_ink() > found, "the drop shadow adds ink beside the text");
   found = glyph_lands('T', 0.1f * SURF_W - 2, 0.5f * SURF_H - 2, &missing);
   CHECK(found > 0 && missing == 0, "the shadow is the glyph, two pixels down and left");

   /* Enough glyphs in one frame to grow the atlas: the image is made
    * again at the new size and its child images with it */
   for (i = 0; i < 3; i++)
   {
      char     text[4096];
      size_t   n = 0;
      uint32_t c;
      font_driver_frame_begin();
      /* Two-byte UTF-8, U+0100 to U+04FF: more glyphs than the first
       * atlas has cells for, so a later frame's lookup grows it */
      for (c = 0x0100; c < 0x0500; c++)
      {
         text[n++] = (char)(0xC0 | (c >> 6));
         text[n++] = (char)(0x80 | (c & 0x3F));
      }
      clear_surface();
      p = plain(0.0f, 0.5f, TEXT_ALIGN_LEFT);
      vg_render_msg(&vg_st, text, n, &p, SURF_W, SURF_H);
   }
   font_driver_frame_begin();
   clear_surface();
   p = plain(0.1f, 0.5f, TEXT_ALIGN_LEFT);
   vg_render_msg(&vg_st, "T", 1, &p, SURF_W, SURF_H);
   found = glyph_lands('T', 0.1f * SURF_W, 0.5f * SURF_H, &missing);
   CHECK(     vg_st.font_atlas_w > first_w
         || vg_st.font_atlas_h > first_h, "the atlas grew and its image with it");
   CHECK(found > 0 && missing == 0, "after the atlas grew, glyphs still land");
   printf("  atlas image %ux%u (first %ux%u)\n", vg_st.font_atlas_w,
         vg_st.font_atlas_h, first_w, first_h);

   vg_font_release_images(&vg_st);
   CHECK(live_images == 0, "every image released");
   vg_st.font_driver->free(vg_st.font_data);

   if (fails)
   {
      printf("font_vg_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("font_vg_test: all checks passed\n");
   return 0;
}
