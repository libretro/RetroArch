/* XMB's background effects as the menu installs them
 * (menu/drivers/xmb_effects.h), against what each video driver takes
 * from a description:
 *
 * - every effect carries a source for every backend - GLSL for a
 *   legacy and for a core/GLES 3 context, SPIR-V, HLSL SM3 and SM4 -
 *   so none is left out on a driver;
 * - its flags hold together (a ribbon is a mesh, a mesh is not flipped,
 *   one blend at most, a quad blends by its alpha);
 * - each source names the inputs and uniforms the drivers look up and
 *   set for those flags (the ribbon's Ribbon; a quad's time and
 *   OutputSize; the HLSL entry points);
 * - the SPIR-V is SPIR-V, and is written out for spirv-val with
 *   --spirv DIR.
 *
 * Built against GL (EFFECTS_GL: a desktop context, legacy and core
 * profile) or GLES (EFFECTS_GLES: GLES 3, then as a GLES 2 context
 * would take them) on Mesa's EGL device, every GLSL program is made
 * through gfx/drivers_shader/shader_glsl.c's gl_glsl_effect_compile(),
 * as gl2 makes it, its looked-up locations checked, and drawn once into
 * a framebuffer the way gl2 draws it (from a buffer, as a core profile
 * requires; a quad as the full-screen mesh): it must cover all of a
 * quad effect's target, or some of a ribbon's, with no GL error.
 * Skipped without an EGL device.
 *
 * Built for Windows (mingw-w64, run under Wine), every HLSL source is
 * compiled the way D3D9 (main_vertex/main_fragment, SM3), D3D10 and 11
 * (VSMain/PSMain, SM4) and D3D12 (SM5) compile it. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#if defined(_WIN32)
#include <windows.h>
#include "../../../gfx/common/d3dcompiler_common.h"
#include "../../../gfx/drivers/gl_shaders/shaders_common.h"
#include "../../../gfx/drivers/d3d_shaders/bloom_sm4.hlsl.h"
#else
#include "../../../gfx/drivers_shader/shader_glsl.c"
#include <EGL/egl.h>
#endif

/* Every backend's sources, whatever this build draws with */
#ifndef HAVE_GLSL
#define HAVE_GLSL
#endif
#ifndef HAVE_VULKAN
#define HAVE_VULKAN
#endif
#define HAVE_D3D9
#define HAVE_HLSL
#define HAVE_D3D11
#include "../../../menu/drivers/xmb_effects.h"
#include "../../../gfx/common/menu_ribbon.h"

/* The frontend's log, which the sources under test write to */
void RARCH_LOG(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); if (getenv("EFFECTS_LOG")) { printf("      [log] "); vprintf(fmt, ap); } va_end(ap); }
void RARCH_WARN(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); printf("      [warn] "); vprintf(fmt, ap); va_end(ap); }
/* The last error, which a compile failure is told apart by */
static char last_err[1024];
void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(last_err, sizeof(last_err), fmt, ap);
   va_end(ap);
   printf("      [err] %s", last_err);
}

#ifndef _WIN32
/* The rest of the frontend shader_glsl.c refers to */
bool gl2_load_luts(const void *shader, GLuint *textures)
{ (void)shader; (void)textures; return true; }
static bool gles3_context;
bool gl_check_capability(enum gl_capability_enum cap)
{ return cap == GL_CAPS_GLES3_SUPPORTED && gles3_context; }
void input_driver_read_sensor_snapshot(float *g, float *a, float *r)
{ (void)g; (void)a; (void)r; }
void input_driver_set_shader_uses_sensors(bool v) { (void)v; }
const char *path_basename(const char *p) { return p; }
const char *path_get_extension(const char *p) { (void)p; return ""; }
void runloop_msg_queue_push(const char *msg, size_t len, unsigned prio,
      unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{ (void)msg; (void)len; (void)prio; (void)duration; (void)flush;
  (void)title; (void)icon; (void)category; }
float video_driver_get_core_aspect(void) { return 1.0f; }
retro_time_t video_driver_get_frame_time_delta_usec(void) { return 0; }
float video_driver_get_original_fps(void) { return 60.0f; }
unsigned video_driver_get_rotation_snapshot(void) { return 0; }
enum rarch_shader_type video_shader_get_type_from_ext(const char *ext,
      bool *is_preset) { (void)ext; *is_preset = false; return RARCH_SHADER_GLSL; }
bool video_shader_load_preset_into_shader(const char *path,
      struct video_shader *shader) { (void)path; (void)shader; return false; }
bool video_shader_source_read(const char *path, char **buf, int64_t *len)
{ (void)path; (void)buf; (void)len; return false; }
#endif

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

static int has(const char *src, const char *word)
{
   return src && strstr(src, word) != NULL;
}

/* The Vulkan snow sources, as their SPIR-V was compiled from */
static const char vk_snow_frag[] =
#include "../../../gfx/drivers/vulkan_shaders/pipeline_snow.frag.src.inc"
      ;
static const char vk_snow_simple_frag[] =
#include "../../../gfx/drivers/vulkan_shaders/pipeline_snow_simple.frag.src.inc"
      ;

static char *read_text(const char *path)
{
   char *buf = NULL;
   long len;
   FILE *fp  = fopen(path, "rb");
   if (!fp)
      return NULL;
   if (     !fseek(fp, 0, SEEK_END) && (len = ftell(fp)) > 0
         && !fseek(fp, 0, SEEK_SET) && (buf = (char*)malloc(len + 1)))
   {
      buf[fread(buf, 1, len, fp)] = '\0';
   }
   fclose(fp);
   return buf;
}

/* The snow falls through a grid of cells, each lit or not by a hash of
 * its index. Drawn by the GL family, a moment's fall is checked above;
 * every other backend's source is held to the same form here: the hash
 * repeats every 256 cells, and the fall wraps at 12.8 (256 cells) of
 * pos / scale, where the repeat makes the wrap seamless. A fall wrapped
 * anywhere else - the fract(scroll / 0.05) it once had - moves the
 * largest flakes to new places five times a second. */
static void check_snow_source(const char *n, const char *what,
      const char *src)
{
   check(     src && strstr(src, "256.0") && strstr(src, "12.8")
         && !strstr(src, "/ 0.05)"),
         "%s: %s repeats its cells every 256 and wraps its fall there",
         n, what);
}

static void check_snow_sources(void)
{
   char *metal;
   unsigned i;
   static const unsigned snows[2] = { XMB_EFFECT_SNOW_SIMPLE, XMB_EFFECT_SNOW };
   for (i = 0; i < 2; i++)
   {
      const gfx_display_effect_desc_t *d = &xmb_effect_descs[snows[i]];
      const char *n = effect_names[snows[i]];
      check_snow_source(n, "GLSL legacy", d->glsl_fragment[0]);
      check_snow_source(n, "GLSL core", d->glsl_fragment[1]);
      check_snow_source(n, "SM3", d->hlsl_sm3);
      check_snow_source(n, "SM4", d->hlsl_sm4);
   }
   check_snow_source("simple snow", "Vulkan", vk_snow_simple_frag);
   check_snow_source("snow", "Vulkan", vk_snow_frag);
   /* One snow function, both snows */
   if ((metal = read_text("../../../gfx/drivers/metal_shaders/Shaders.metal")))
   {
      const char *ns = strstr(metal, "namespace snow");
      check_snow_source("snow", "Metal", ns);
      free(metal);
   }
   else
      printf("  [skip] Shaders.metal not found from here\n");
}

/* --- What every driver takes from the table ------------------------ */

static void check_table(const char *spirv_dir)
{
   unsigned i, k;
   xmb_effects_describe();
   for (i = 0; i < XMB_EFFECT_COUNT; i++)
   {
      const gfx_display_effect_desc_t *d = &xmb_effect_descs[i];
      const char *n = effect_names[i];
      unsigned f    = d->flags;
      bool mesh     = (f & GFX_EFFECT_MESH) != 0;
      bool ribbon   = (f & GFX_EFFECT_RIBBON) != 0;
      const uint32_t *spv[2];
      size_t spv_size[2];

      printf("%s\n", n);

      /* Flags */
      check(!ribbon || mesh, "%s: a ribbon effect is a mesh", n);
      check(!(mesh && (f & GFX_EFFECT_YFLIP)),
            "%s: a mesh is not y-flipped", n);
      check(!((f & GFX_EFFECT_BLEND_ADD) && (f & GFX_EFFECT_BLEND_SCREEN)),
            "%s: one blend at most", n);
      check(mesh || !(f & (GFX_EFFECT_BLEND_ADD | GFX_EFFECT_BLEND_SCREEN)),
            "%s: a quad blends by its alpha", n);

      /* GLSL, both dialects */
      for (k = 0; k < 2; k++)
      {
         const char *vs = d->glsl_vertex[k];
         const char *fs = d->glsl_fragment[k];
         check(vs && fs, "%s: GLSL %s sources", n, k ? "core" : "legacy");
         check(has(vs, "VertexCoord"), "%s: GLSL %s takes VertexCoord",
               n, k ? "core" : "legacy");
         if (ribbon)
            check(has(vs, "Ribbon"), "%s: GLSL %s reads Ribbon",
                  n, k ? "core" : "legacy");
         if (!mesh)
            check(     (has(vs, "time") || has(fs, "time")
                     || has(vs, "Time") || has(fs, "Time"))
                  && (has(vs, "OutputSize") || has(fs, "OutputSize")),
                  "%s: GLSL %s reads time and OutputSize",
                  n, k ? "core" : "legacy");
      }

      /* SPIR-V */
      spv[0]      = d->spirv_vertex;
      spv[1]      = d->spirv_fragment;
      spv_size[0] = d->spirv_vertex_size;
      spv_size[1] = d->spirv_fragment_size;
      for (k = 0; k < 2; k++)
      {
         check(     spv[k] && spv_size[k] >= 20 && !(spv_size[k] & 3)
               && spv[k][0] == 0x07230203u,
               "%s: SPIR-V %s module", n, k ? "fragment" : "vertex");
         if (spirv_dir && spv[k])
         {
            char path[512];
            FILE *fp;
            snprintf(path, sizeof(path), "%s/%u_%s.spv", spirv_dir, i,
                  k ? "frag" : "vert");
            if ((fp = fopen(path, "wb")))
            {
               fwrite(spv[k], 1, spv_size[k], fp);
               fclose(fp);
            }
         }
      }

      /* HLSL */
      check(     has(d->hlsl_sm3, "main_vertex")
            && has(d->hlsl_sm3, "main_fragment"),
            "%s: SM3 source with main_vertex and main_fragment", n);
      check(has(d->hlsl_sm4, "VSMain") && has(d->hlsl_sm4, "PSMain"),
            "%s: SM4 source with VSMain and PSMain", n);
      if (!mesh)
         check(has(d->hlsl_sm3, "modelViewProj"),
               "%s: SM3 reads modelViewProj", n);
      if (ribbon)
         check(has(d->hlsl_sm3, "Ribbon") && has(d->hlsl_sm4, "Ribbon"),
               "%s: SM3 and SM4 read Ribbon", n);
      if (!mesh)
         check(     has(d->hlsl_sm3, "OutputSize") && has(d->hlsl_sm3, "time")
               && has(d->hlsl_sm4, "OutputSize") && has(d->hlsl_sm4, "time"),
               "%s: SM3 and SM4 read time and OutputSize", n);
   }
}

#if defined(_WIN32)
/* --- D3D: every HLSL source compiles as its drivers compile it ----- */

/* Under Wine the compiler is vkd3d-shader's, which does not yet take
 * all of the HLSL Microsoft's does: SM1 branches and loops, and a few
 * intrinsics. A source it refuses for one of those is not counted
 * against; on Windows every source must compile. */
static bool under_wine;

static bool wine_gap(void)
{
   return under_wine && (strstr(last_err, "not yet implemented")
         || strstr(last_err, "\"atan\" is not defined")
         || strstr(last_err, "\"atan2\" is not defined"));
}

/* 1 compiled, 0 failed, -1 a gap in Wine's compiler; the bytecode in
 * *out when it is wanted */
static int compiles(const char *src, const char *entry, const char *target,
      D3DBlob *out)
{
   D3DBlob blob = NULL;
   bool ok;
   last_err[0] = '\0';
   ok          = d3d_compile(src, strlen(src), NULL, entry, target, &blob);
   if (ok && out)
      *out = blob;
   else if (blob)
      blob->lpVtbl->Release(blob);
   return ok ? 1 : wine_gap() ? -1 : 0;
}

/* --- Linkage: what the vertex stage writes, the pixel stage reads ---
 *
 * From SM4 on, a pixel shader's inputs are matched to the stage before
 * by register, not by name: each must sit in the register the vertex
 * (or geometry) shader writes it to. A pixel shader that leaves out an
 * output ahead of what it reads - the SV_POSITION in front of a
 * TEXCOORD - reads it from the wrong register. The D3D12 runtime then
 * refuses the pipeline; D3D10 and 11 draw it with whatever the driver
 * makes of it. vkd3d matches by name and draws it right, so only this
 * catches it under Wine. */

typedef struct sig_elem
{
   const char *name;
   unsigned index;
   unsigned sv;
   unsigned reg;
} sig_elem_t;

static unsigned rd32(const unsigned char *p)
{
   return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24);
}

/* The elements of the bytecode's input (ISGN) or output (OSGN) chunk;
 * how many, or -1 without one */
static int dxbc_signature(D3DBlob blob, bool output, sig_elem_t *e,
      int max)
{
   const unsigned char *b = (const unsigned char*)
      blob->lpVtbl->GetBufferPointer(blob);
   size_t size            = blob->lpVtbl->GetBufferSize(blob);
   unsigned i, chunks;
   if (size < 32 || memcmp(b, "DXBC", 4))
      return -1;
   chunks = rd32(b + 28);
   for (i = 0; i < chunks && 32 + 4 * i + 4 <= size; i++)
   {
      unsigned off = rd32(b + 32 + 4 * i);
      const unsigned char *c, *d;
      unsigned stride, skip, count, j, len;
      if (off + 8 > size)
         return -1;
      c   = b + off;
      len = rd32(c + 4);
      d   = c + 8;
      if (off + 8 + len > size)
         return -1;
      if (!memcmp(c, output ? "OSGN" : "ISGN", 4))
         stride = 24, skip = 0;
      else if (!memcmp(c, output ? "OSG5" : "ISG5", 4))
         stride = 28, skip = 4;
      else if (!memcmp(c, output ? "OSG1" : "ISG1", 4))
         stride = 32, skip = 4;
      else
         continue;
      count = rd32(d);
      if (8 + count * stride > len)
         return -1;
      for (j = 0; j < count && (int)j < max; j++)
      {
         const unsigned char *el = d + 8 + j * stride + skip;
         unsigned name           = rd32(el);
         if (name >= len)
            return -1;
         e[j].name  = (const char*)d + name;
         e[j].index = rd32(el + 4);
         e[j].sv    = rd32(el + 8);
         e[j].reg   = rd32(el + 16);
      }
      return (int)j;
   }
   return -1;
}

/* Values the rasteriser makes for the pixel stage itself */
static bool sig_rasterised(const sig_elem_t *e)
{
   /* D3D_NAME_PRIMITIVE_ID, IS_FRONT_FACE, SAMPLE_INDEX */
   return e->sv == 7 || e->sv == 9 || e->sv == 10;
}

/* Every pixel input in the register the stage before writes it to */
static void check_linkage(const char *n, const char *what, D3DBlob vs,
      D3DBlob ps)
{
   sig_elem_t out[32], in[32];
   int o = dxbc_signature(vs, true, out, 32);
   int i = dxbc_signature(ps, false, in, 32);
   int a, b;
   if (o < 0 || i < 0)
   {
      check(0, "%s: %s - no signature in the bytecode", n, what);
      return;
   }
   for (a = 0; a < i; a++)
   {
      int written = -1;
      if (sig_rasterised(&in[a]))
         continue;
      for (b = 0; b < o && written < 0; b++)
         if (     !strcasecmp(in[a].name, out[b].name)
               && in[a].index == out[b].index)
            written = (int)out[b].reg;
      check(written == (int)in[a].reg,
            "%s: %s - pixel input %s%u read from register %u, written "
            "to %d", n, what, in[a].name, in[a].index, in[a].reg, written);
   }
}

static void check_pair(const char *n, const char *what, const char *src,
      const char *vs, const char *ps, const char *vs_target,
      const char *ps_target, bool linkage)
{
   D3DBlob vb = NULL, pb = NULL;
   int v = compiles(src, vs, vs_target, linkage ? &vb : NULL);
   int p = compiles(src, ps, ps_target, linkage ? &pb : NULL);
   if (v < 0 || p < 0)
   {
      if (v != 0 && p != 0)
         printf("  [skip] %s: %s - beyond Wine's HLSL compiler\n", n, what);
      else
         check(0, "%s: %s", n, what);
   }
   else
   {
      check(v == 1 && p == 1, "%s: %s", n, what);
      if (vb && pb)
         check_linkage(n, what, vb, pb);
   }
   if (vb)
      vb->lpVtbl->Release(vb);
   if (pb)
      pb->lpVtbl->Release(pb);
}

static void check_hlsl(void)
{
   unsigned i;
   HMODULE ntdll = GetModuleHandleA("ntdll.dll");
   under_wine    = ntdll && GetProcAddress(ntdll, "wine_get_version");
   if (!d3d_compiler_load())
   {
      printf("  [skip] no D3DCompiler DLL\n");
      return;
   }
   for (i = 0; i < XMB_EFFECT_COUNT; i++)
   {
      const gfx_display_effect_desc_t *d = &xmb_effect_descs[i];
      const char *n = effect_names[i];
      check_pair(n, "SM3 compiles for D3D9", d->hlsl_sm3,
            "main_vertex", "main_fragment", "vs_3_0", "ps_3_0", false);
      check_pair(n, "SM4 compiles for D3D10 and 11", d->hlsl_sm4,
            "VSMain", "PSMain", "vs_4_0", "ps_4_0", true);
      check_pair(n, "SM5 compiles for D3D12", d->hlsl_sm4,
            "VSMain", "PSMain", "vs_5_0", "ps_5_0", true);
   }
   /* The ribbon's glow, its passes drawn by D3D10, 11 and 12 */
   {
      size_t vl = strlen(bloom_vs_source), pl = strlen(bloom_ps_source);
      char *bloom = (char*)malloc(vl + pl + 2);
      if (bloom)
      {
         memcpy(bloom, bloom_vs_source, vl);
         bloom[vl] = '\n';
         memcpy(bloom + vl + 1, bloom_ps_source, pl + 1);
         check_pair("ribbon glow", "SM4 compiles for D3D10 and 11", bloom,
               "VSMain", "PSMain", "vs_4_0", "ps_4_0", true);
         check_pair("ribbon glow", "SM5 compiles for D3D12", bloom,
               "VSMain", "PSMain", "vs_5_0", "ps_5_0", true);
         free(bloom);
      }
   }
}

#else
/* --- GL: every program is made and draws as gl2 draws it ----------- */

#define FB_SIZE 128
#define GRID    24

/* The ribbon's surface: a strip over [-1, 1] as XMB's mesh, without
 * its star billboards */
static float grid[(GRID - 1) * GRID * 2 * 2];

static void build_grid(void)
{
   unsigned r, c, i = 0;
   for (r = 0; r < GRID - 1; r++)
      for (c = 0; c < GRID; c++)
      {
         unsigned col  = r % 2 ? GRID - c - 1 : c;
         float x       = (float)col / (GRID - 1) * 2.0f - 1.0f;
         grid[i++]     = x;
         grid[i++]     = (float)r / (GRID - 1) * 2.0f - 1.0f;
         grid[i++]     = x;
         grid[i++]     = (float)(r + 1) / (GRID - 1) * 2.0f - 1.0f;
      }
}

static void draw_and_check(const gl_glsl_effect_t *fx, unsigned idx,
      const gfx_display_effect_desc_t *d, const char *n, const char *ctx)
{
   /* gl2's mvp_no_rot: [0, 1] onto clip space */
   static const float mvp[16] = {
      2, 0, 0, 0,   0, 2, 0, 0,   0, 0, -1, 0,   -1, -1, 0, 1 };
   /* gfx_display_mesh_fullscreen(), which a quad effect is drawn as */
   static const float quad[8]  = { -1, -1, 1, -1, -1, 1, 1, 1 };
   static uint8_t px[FB_SIZE * FB_SIZE * 4];
   static menu_ribbon_t ribbon;
   bool mesh = (d->flags & GFX_EFFECT_MESH) != 0;
   GLuint fbo, rb, vbo;
   unsigned i, lit = 0;
   GLenum err;

   if (!ribbon.random)
      menu_ribbon_update(&ribbon, 7.0, FB_SIZE, FB_SIZE, 1.0f);

   glGenFramebuffers(1, &fbo);
   glGenRenderbuffers(1, &rb);
   glBindRenderbuffer(GL_RENDERBUFFER, rb);
   glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, FB_SIZE, FB_SIZE);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_RENDERBUFFER, rb);
   glViewport(0, 0, FB_SIZE, FB_SIZE);
   glClearColor(0, 0, 0, 0);
   glClear(GL_COLOR_BUFFER_BIT);

   /* As gl2_draw_effect() */
   glUseProgram(fx->prog);
   if (fx->mvp >= 0)
      glUniformMatrix4fv(fx->mvp, 1, GL_FALSE, mvp);
   if (fx->time >= 0)
      glUniform1f(fx->time, 7.0f);
   if (fx->ribbon >= 0 && (d->flags & GFX_EFFECT_RIBBON))
      glUniform4fv(fx->ribbon, MENU_RIBBON_UNIFORMS, &ribbon.data[0][0]);
   if (fx->output_size >= 0 && !mesh)
      glUniform2f(fx->output_size, FB_SIZE, FB_SIZE);
   /* From a buffer, positions alone, as gl2 draws them: a core profile
    * has no client arrays */
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   if (mesh)
      glBufferData(GL_ARRAY_BUFFER, sizeof(grid), grid, GL_STATIC_DRAW);
   else
      glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
   glEnableVertexAttribArray(fx->vertex_coord);
   glVertexAttribPointer(fx->vertex_coord, 2, GL_FLOAT, GL_FALSE, 0,
         (const GLvoid*)0);
   glDrawArrays(GL_TRIANGLE_STRIP, 0,
         mesh ? (GLsizei)(sizeof(grid) / sizeof(float) / 2) : 4);
   glDisableVertexAttribArray(fx->vertex_coord);
   glUseProgram(0);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glDeleteBuffers(1, &vbo);

   glReadPixels(0, 0, FB_SIZE, FB_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, px);
   for (i = 0; i < FB_SIZE * FB_SIZE; i++)
      if (px[i * 4] | px[i * 4 + 1] | px[i * 4 + 2] | px[i * 4 + 3])
         lit++;
   err = glGetError();
   check(err == GL_NO_ERROR, "%s, %s: drawn with no GL error (0x%x)",
         n, ctx, (unsigned)err);
   /* The snowflakes are sparse; the rest of the quads light every pixel
    * they cover, if only in alpha */
   if (mesh || idx == XMB_EFFECT_SNOWFLAKE)
      check(lit > 0, "%s, %s: draws something (%u of %u pixels)",
            n, ctx, lit, FB_SIZE * FB_SIZE);
   else
      check(lit == FB_SIZE * FB_SIZE, "%s, %s: covers the target "
            "(%u of %u pixels)", n, ctx, lit, FB_SIZE * FB_SIZE);

   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glDeleteFramebuffers(1, &fbo);
   glDeleteRenderbuffers(1, &rb);
}

/* A falling effect moves continuously: drawn a moment apart - 0.0002
 * of its time, about a twentieth of a frame at 240 Hz - next to no
 * pixel moves by more than a quarter. Checked across the moments where
 * a fall that wraps could jump: every eighth of a time unit, where the
 * snow's fall once wrapped and its largest flakes snapped to new places
 * five times a second, and at 32 and 64, where it wraps now. The
 * snowflakes spin fast enough to change a few dozen pixels in any
 * moment, and are left out. */
#define MOTION_SIZE 512
#define MOTION_DT   0.0002f
#define MOTION_MAX  8

static void motion_render(const gl_glsl_effect_t *fx, float t,
      uint8_t *alpha)
{
   static uint8_t px[MOTION_SIZE * MOTION_SIZE * 4];
   static const float mvp[16] = {
      2, 0, 0, 0,   0, 2, 0, 0,   0, 0, -1, 0,   -1, -1, 0, 1 };
   unsigned i;
   glClear(GL_COLOR_BUFFER_BIT);
   glUseProgram(fx->prog);
   if (fx->mvp >= 0)
      glUniformMatrix4fv(fx->mvp, 1, GL_FALSE, mvp);
   if (fx->time >= 0)
      glUniform1f(fx->time, t);
   if (fx->output_size >= 0)
      glUniform2f(fx->output_size, MOTION_SIZE, MOTION_SIZE);
   glEnableVertexAttribArray(fx->vertex_coord);
   glVertexAttribPointer(fx->vertex_coord, 2, GL_FLOAT, GL_FALSE, 0,
         (const GLvoid*)0);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisableVertexAttribArray(fx->vertex_coord);
   glUseProgram(0);
   glReadPixels(0, 0, MOTION_SIZE, MOTION_SIZE, GL_RGBA, GL_UNSIGNED_BYTE,
         px);
   for (i = 0; i < MOTION_SIZE * MOTION_SIZE; i++)
      alpha[i] = px[i * 4 + 3];
}

static void check_motion(const gl_glsl_effect_t *fx, const char *n,
      const char *ctx)
{
   static const float quad[8] = { -1, -1, 1, -1, -1, 1, 1, 1 };
   static const float at[] = {
      0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f,
      2.5f, 4.0f, 8.0f, 12.8f, 16.0f, 25.6f, 31.875f, 32.0f,
      38.4f, 48.0f, 51.2f, 63.875f, 64.0f, 64.125f, 96.0f, 128.0f };
   static uint8_t a0[MOTION_SIZE * MOTION_SIZE];
   static uint8_t a1[MOTION_SIZE * MOTION_SIZE];
   GLuint fbo, rb, vbo;
   unsigned k, i, worst = 0;
   float worst_t = 0.0f;
   glGenFramebuffers(1, &fbo);
   glGenRenderbuffers(1, &rb);
   glBindRenderbuffer(GL_RENDERBUFFER, rb);
   glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, MOTION_SIZE, MOTION_SIZE);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_RENDERBUFFER, rb);
   glViewport(0, 0, MOTION_SIZE, MOTION_SIZE);
   glClearColor(0, 0, 0, 0);
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
   for (k = 0; k < sizeof(at) / sizeof(at[0]); k++)
   {
      float t          = at[k] - MOTION_DT * 0.5f;
      unsigned changed = 0;
      motion_render(fx, t, a0);
      motion_render(fx, t + MOTION_DT, a1);
      for (i = 0; i < MOTION_SIZE * MOTION_SIZE; i++)
         if (a0[i] - a1[i] > 64 || a1[i] - a0[i] > 64)
            changed++;
      if (changed > worst)
      {
         worst   = changed;
         worst_t = at[k];
      }
   }
   check(worst <= MOTION_MAX, "%s, %s: moves without jumps (at most %u "
         "pixels change in a moment, %u at %.3f)", n, ctx, MOTION_MAX,
         worst, worst_t);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glDeleteBuffers(1, &vbo);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glDeleteFramebuffers(1, &fbo);
   glDeleteRenderbuffers(1, &rb);
}

static void check_programs(const char *ctx)
{
   unsigned i;
   for (i = 0; i < XMB_EFFECT_COUNT; i++)
   {
      const gfx_display_effect_desc_t *d = &xmb_effect_descs[i];
      const char *n = effect_names[i];
      bool mesh     = (d->flags & GFX_EFFECT_MESH) != 0;
      gl_glsl_effect_t fx;
      bool made     = gl_glsl_effect_compile(&fx, d);

      check(made, "%s, %s: program made", n, ctx);
      if (!made)
         continue;
      check(fx.vertex_coord >= 0, "%s, %s: VertexCoord found", n, ctx);
      if (d->flags & GFX_EFFECT_RIBBON)
         check(fx.ribbon >= 0, "%s, %s: Ribbon found", n, ctx);
      else
         check(fx.time >= 0, "%s, %s: time found", n, ctx);
      if (!mesh)
         check(fx.output_size >= 0, "%s, %s: OutputSize found", n, ctx);
      if (fx.vertex_coord >= 0)
         draw_and_check(&fx, i, d, n, ctx);
      if (fx.vertex_coord >= 0 && !mesh && i != XMB_EFFECT_SNOWFLAKE)
         check_motion(&fx, n, ctx);
      glDeleteProgram(fx.prog);
   }
}

/* A context on Mesa's EGL device: @api EGL_OPENGL_ES_API (version 3) or
 * EGL_OPENGL_API (@core: a 3.3 core profile, else the default) */
static bool make_context(EGLDisplay dpy, EGLenum api, bool core,
      EGLContext *ctx, EGLSurface *surf)
{
   EGLConfig cfg;
   EGLint n;
   EGLint cfg_attr[] = { EGL_RENDERABLE_TYPE,
      api == EGL_OPENGL_ES_API ? 0x40 /* ES3 */ : EGL_OPENGL_BIT,
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE };
   EGLint es_attr[]   = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
   EGLint core_attr[] = { 0x3098 /* EGL_CONTEXT_MAJOR_VERSION */, 3,
      0x30FB /* EGL_CONTEXT_MINOR_VERSION */, 3,
      0x30FD /* EGL_CONTEXT_OPENGL_PROFILE_MASK */, 1 /* core */,
      EGL_NONE };
   EGLint pb_attr[]   = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
   const EGLint *attr = api == EGL_OPENGL_ES_API ? es_attr
      : core ? core_attr : NULL;
   if (     !eglBindAPI(api)
         || !eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || !n
         || (*ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, attr))
            == EGL_NO_CONTEXT)
      return false;
   if (     (*surf = eglCreatePbufferSurface(dpy, cfg, pb_attr))
            == EGL_NO_SURFACE
         || !eglMakeCurrent(dpy, *surf, *surf, *ctx))
   {
      eglDestroyContext(dpy, *ctx);
      return false;
   }
#ifndef EFFECTS_GLES
   rglgen_resolve_symbols((rglgen_proc_address_t)eglGetProcAddress);
#endif
   return true;
}

static void drop_context(EGLDisplay dpy, EGLContext ctx, EGLSurface surf)
{
   eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroySurface(dpy, surf);
   eglDestroyContext(dpy, ctx);
}

/* 0 when drawn, 77 when there is no device to draw on */
static int check_gl(void)
{
   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   EGLContext ctx;
   EGLSurface surf;
   if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, NULL, NULL))
      return 77;
   build_grid();
#ifdef EFFECTS_GLES
   if (!make_context(dpy, EGL_OPENGL_ES_API, false, &ctx, &surf))
   {
      eglTerminate(dpy);
      return 77;
   }
   glsl_major    = 3;
   glsl_minor    = 0;
   gles3_context = true;
   check_programs("GLES 3");
   /* The GLES 2 sources, as a context without GLES 3 takes them */
   gles3_context = false;
   check_programs("GLES 2");
   drop_context(dpy, ctx, surf);
#else
   if (!make_context(dpy, EGL_OPENGL_API, false, &ctx, &surf))
   {
      eglTerminate(dpy);
      return 77;
   }
   glsl_core = false;
   check_programs("GL legacy");
   drop_context(dpy, ctx, surf);
   if (make_context(dpy, EGL_OPENGL_API, true, &ctx, &surf))
   {
      GLuint vao;
      /* A core profile draws nothing without a vertex array */
      glGenVertexArrays(1, &vao);
      glBindVertexArray(vao);
      glsl_core  = true;
      glsl_major = 3;
      glsl_minor = 3;
      check_programs("GL core");
      glBindVertexArray(0);
      glDeleteVertexArrays(1, &vao);
      drop_context(dpy, ctx, surf);
   }
   else
      printf("  [skip] no GL 3.3 core profile\n");
#endif
   eglTerminate(dpy);
   return 0;
}
#endif

int main(int argc, char **argv)
{
   const char *spirv_dir = NULL;
   if (argc == 3 && !strcmp(argv[1], "--spirv"))
      spirv_dir = argv[2];

   check_table(spirv_dir);
   check_snow_sources();
#if defined(_WIN32)
   check_hlsl();
#else
   if (check_gl() == 77)
   {
      printf("  [skip] no EGL device: programs not made\n");
      return fails ? 1 : 77;
   }
#endif
   printf(fails ? "menu_effects_test: %d check(s) failed\n"
                : "menu_effects_test: all checks passed\n", fails);
   return fails ? 1 : 0;
}
