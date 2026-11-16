/* XMB's background effects as gl2 draws them in a build with Cg and no
 * GLSL (menu/drivers/xmb_effects.h's cg_gl sources), on Mesa's EGL
 * device in a legacy GL context, through the Cg backend itself
 * (gfx/drivers_shader/shader_gl_cg.c):
 *
 * - every effect's Cg compiles in the backend's profiles;
 * - it draws the same whether or not the backend's own program drew
 *   just before. That program leaves its vertex arrays on, and one of
 *   them is the attribute an effect's position aliases: left on, it
 *   won, and the ribbons were drawn from the wrong vertices - the
 *   simple ribbon a solid blot, layer on layer;
 * - the ribbon's glow (gfx/common/gl_common.c) leaves no GLSL program
 *   bound: one would win over the Cg backend's ARB programs, and
 *   everything after was drawn through it.
 *
 * Skipped without an X display (run under xvfb-run), or without ARB
 * vertex and fragment program profiles. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../../../gfx/drivers_shader/shader_gl_cg.c"
#include "../../../gfx/common/gl_common.c"
#include <GL/glx.h>

#include "../../../menu/drivers/xmb_effects.h"
#include "../../../gfx/common/menu_ribbon.h"

/* The frontend's log and the rest it refers to */
void RARCH_LOG(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); if (getenv("EFFECTS_LOG")) { printf("      [log] "); vprintf(fmt, ap); } va_end(ap); }
void RARCH_WARN(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); printf("      [warn] "); vprintf(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); printf("      [err] "); vprintf(fmt, ap); va_end(ap); }
bool gl2_load_luts(const void *shader, GLuint *textures)
{ (void)shader; (void)textures; return true; }
const char *path_get_extension(const char *p) { (void)p; return ""; }
float video_driver_get_core_aspect(void) { return 1.0f; }
unsigned video_driver_get_rotation_snapshot(void) { return 0; }
enum rarch_shader_type video_shader_get_type_from_ext(const char *ext,
      bool *is_preset)
{ (void)ext; *is_preset = false; return RARCH_SHADER_CG; }
bool video_shader_load_preset_into_shader(const char *path,
      struct video_shader *shader) { (void)path; (void)shader; return false; }
bool video_shader_load_current_parameter_values(config_file_t *conf,
      struct video_shader *shader) { (void)conf; (void)shader; return true; }
void video_shader_resolve_parameters(struct video_shader *shader)
{ (void)shader; }
bool gl_check_capability(enum gl_capability_enum cap)
{ (void)cap; return false; }
bool gl_query_extension(const char *ext) { (void)ext; return false; }
void image_yuv_coefficients(unsigned flags, float c[6])
{ unsigned i; (void)flags; for (i = 0; i < 6; i++) c[i] = 0.0f; }

static const char *const effect_names[XMB_EFFECT_COUNT] = {
   "ribbon", "simple ribbon", "simple snow", "snow", "bokeh", "snowflake"
};

static int fails;

static void check(int cond, const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   printf(cond ? "  [ok]   " : "  [FAIL] ");
   vprintf(fmt, ap);
   printf("\n");
   va_end(ap);
   if (!cond)
      fails++;
}

#define FB_SIZE 128
#define GRID    24

/* The ribbon's surface, as gfx_display keeps a mesh: a strip over
 * [-1, 1], and the full-screen quad a quad effect is drawn as */
static gfx_display_mesh_vertex_t grid[(GRID - 1) * GRID * 2];
static gfx_display_mesh_vertex_t quad[4];

static void build_meshes(void)
{
   static const float q[8] = { -1, -1, 1, -1, -1, 1, 1, 1 };
   unsigned r, c, i = 0;
   memset(grid, 0, sizeof(grid));
   memset(quad, 0, sizeof(quad));
   for (r = 0; r < GRID - 1; r++)
      for (c = 0; c < GRID; c++)
      {
         unsigned col  = r % 2 ? GRID - c - 1 : c;
         float x       = (float)col / (GRID - 1) * 2.0f - 1.0f;
         grid[i].x   = x;
         grid[i++].y = (float)r / (GRID - 1) * 2.0f - 1.0f;
         grid[i].x   = x;
         grid[i++].y = (float)(r + 1) / (GRID - 1) * 2.0f - 1.0f;
      }
   for (i = 0; i < 4; i++)
   {
      quad[i].x = q[i * 2];
      quad[i].y = q[i * 2 + 1];
   }
}

/* What gl2's draws through the backend's program do before an effect:
 * its program, client arrays for every stream it reads, one draw */
static void draw_stock(cg_shader_data_t *cg, GLuint white)
{
   static const float vertex[8]    = { 0, 0, 1, 0, 0, 1, 1, 1 };
   static const float tex_coord[8] = { 0, 1, 1, 1, 0, 0, 1, 0 };
   static const float color[16]    = { 1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1 };
   static const float mvp[16]      = {
      2, 0, 0, 0,   0, 2, 0, 0,   0, 0, -1, 0,   -1, -1, 0, 1 };
   struct video_coords coords;
   math_matrix_4x4 m;
   memset(&coords, 0, sizeof(coords));
   coords.vertex        = vertex;
   coords.tex_coord     = tex_coord;
   coords.lut_tex_coord = tex_coord;
   coords.color         = color;
   coords.vertices      = 4;
   memcpy(m.data, mvp, sizeof(mvp));
   glBindTexture(GL_TEXTURE_2D, white);
   gl_cg_backend.use(NULL, cg, VIDEO_SHADER_STOCK_BLEND, true);
   gl_cg_backend.set_coords(cg, &coords);
   gl_cg_backend.set_mvp(cg, &m);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/* @fx drawn as gl2_draw_cg_effect() draws it, over a grey target, and
 * read back */
static void draw_effect(cg_shader_data_t *cg, const gl_cg_effect_t *fx,
      const gfx_display_effect_desc_t *d, const menu_ribbon_t *ribbon,
      uint8_t *px)
{
   static const float mvp[16] = {
      2, 0, 0, 0,   0, 2, 0, 0,   0, 0, -1, 0,   -1, -1, 0, 1 };
   bool mesh = (d->flags & GFX_EFFECT_MESH) != 0;
   GLuint vbo;
   glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT);
   glEnable(GL_BLEND);
   if (d->flags & GFX_EFFECT_BLEND_ADD)
      glBlendFunc(GL_ONE, GL_ONE);
   else if (d->flags & GFX_EFFECT_BLEND_SCREEN)
      glBlendFunc(GL_DST_COLOR, GL_ONE);
   else
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   if (mesh)
      glBufferData(GL_ARRAY_BUFFER, sizeof(grid), grid, GL_STATIC_DRAW);
   else
      glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
   gl_cg_effect_begin(cg, fx, mvp, 7.0f,
         (d->flags & GFX_EFFECT_RIBBON) ? &ribbon->data[0][0] : NULL,
         FB_SIZE, FB_SIZE, sizeof(gfx_display_mesh_vertex_t));
   glDrawArrays(GL_TRIANGLE_STRIP, 0, mesh
         ? (GLsizei)(sizeof(grid) / sizeof(grid[0])) : 4);
   gl_cg_effect_end(fx);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glDeleteBuffers(1, &vbo);
   glDisable(GL_BLEND);
   glReadPixels(0, 0, FB_SIZE, FB_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, px);
}

static void check_effects(cg_shader_data_t *cg, GLuint white)
{
   static uint8_t clean[FB_SIZE * FB_SIZE * 4];
   static uint8_t after[FB_SIZE * FB_SIZE * 4];
   static menu_ribbon_t ribbon;
   unsigned i;
   menu_ribbon_update(&ribbon, 7.0, FB_SIZE, FB_SIZE, 1.0f);
   for (i = 0; i < XMB_EFFECT_COUNT; i++)
   {
      const gfx_display_effect_desc_t *d = &xmb_effect_descs[i];
      const char *n = effect_names[i];
      gl_cg_effect_t fx;
      unsigned p, changed = 0, differ = 0;
      if (!gl_cg_effect_compile(cg, &fx, d->cg_gl))
      {
         check(0, "%s: its Cg compiles in %s/%s", n,
               cgGetProfileString(cg->cgVProf),
               cgGetProfileString(cg->cgFProf));
         continue;
      }
      check(1, "%s: its Cg compiles in %s/%s", n,
            cgGetProfileString(cg->cgVProf),
            cgGetProfileString(cg->cgFProf));
      /* First with nothing drawn before it, then after the backend's
       * own program has drawn */
      gl_cg_backend.use(NULL, cg, VIDEO_SHADER_STOCK_BLEND, true);
      draw_effect(cg, &fx, d, &ribbon, clean);
      draw_stock(cg, white);
      draw_effect(cg, &fx, d, &ribbon, after);
      for (p = 0; p < FB_SIZE * FB_SIZE * 4; p += 4)
      {
         if (     clean[p] != 128 || clean[p + 1] != 128
               || clean[p + 2] != 128)
            changed++;
         if (memcmp(clean + p, after + p, 3))
            differ++;
      }
      /* The snowflakes are sparse and may miss so small a target */
      if (i != XMB_EFFECT_SNOWFLAKE)
         check(changed > 0, "%s: draws something (%u of %u pixels)",
               n, changed, FB_SIZE * FB_SIZE);
      check(differ == 0, "%s: drawn the same after the backend's own "
            "program (%u pixels differ)", n, differ);
      check(glGetError() == GL_NO_ERROR, "%s: no GL error", n);
      gl_cg_effect_free(&fx);
   }
}

/* The glow's passes leave no GLSL program bound */
static void check_glow(void)
{
   gl_menu_bloom_t *bloom = NULL;
   GLint prog = -1;
   unsigned dims = VIDEO_SCALE_PACK(FB_SIZE, FB_SIZE);
   if (!gl_menu_bloom_begin(&bloom, dims, false, false, false))
   {
      printf("  [skip] the glow does not run here\n");
      gl_menu_bloom_free(&bloom);
      return;
   }
   gl_menu_bloom_end(bloom, dims, false);
   glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
   check(prog == 0, "glow: no GLSL program left bound after its passes "
         "(%d)", (int)prog);
   gl_menu_bloom_free(&bloom);
}

int main(void)
{
   static int attr[] = { GLX_RGBA, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
      GLX_BLUE_SIZE, 8, None };
   static const uint8_t white_px[4] = { 255, 255, 255, 255 };
   Display *dpy = XOpenDisplay(NULL);
   XVisualInfo *vi;
   XSetWindowAttributes swa;
   Window win;
   GLXContext ctx;
   GLuint fbo, rb, white;
   cg_shader_data_t *cg;

   xmb_effects_describe();
   build_meshes();
   if (     !dpy
         || !(vi = glXChooseVisual(dpy, DefaultScreen(dpy), attr)))
   {
      printf("  [skip] no X display with a GLX visual\n");
      return 77;
   }
   swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
         vi->visual, AllocNone);
   swa.border_pixel = 0;
   win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, 16, 16, 0,
         vi->depth, InputOutput, vi->visual, CWColormap | CWBorderPixel,
         &swa);
   if (     !(ctx = glXCreateContext(dpy, vi, NULL, True))
         || !glXMakeCurrent(dpy, win, ctx))
   {
      printf("  [skip] no legacy GLX context\n");
      return 77;
   }
   rglgen_resolve_symbols((rglgen_proc_address_t)glXGetProcAddress);
   if (!(cg = (cg_shader_data_t*)gl_cg_backend.init(NULL, NULL)))
   {
      printf("  [skip] no Cg profiles for this context\n");
      return 77;
   }

   glGenFramebuffers(1, &fbo);
   glGenRenderbuffers(1, &rb);
   glBindRenderbuffer(GL_RENDERBUFFER, rb);
   glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, FB_SIZE, FB_SIZE);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_RENDERBUFFER, rb);
   glViewport(0, 0, FB_SIZE, FB_SIZE);
   glGenTextures(1, &white);
   glBindTexture(GL_TEXTURE_2D, white);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
         GL_UNSIGNED_BYTE, white_px);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

   check_effects(cg, white);
   check_glow();

   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glDeleteFramebuffers(1, &fbo);
   glDeleteRenderbuffers(1, &rb);
   glDeleteTextures(1, &white);
   gl_cg_backend.deinit(cg);
   glXMakeCurrent(dpy, None, NULL);
   glXDestroyContext(dpy, ctx);
   XDestroyWindow(dpy, win);
   XFree(vi);
   XCloseDisplay(dpy);
   printf(fails ? "menu_effects_cg_test: %d check(s) failed\n"
                : "menu_effects_cg_test: all checks passed\n", fails);
   return fails ? 1 : 0;
}
