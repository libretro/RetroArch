/* gfx/drivers_shader/shader_glsl.c's first pass reading a view where it
 * lies, on an EGL device with GLES 3 (Mesa's llvmpipe on the runners):
 * a preset that may has its passes reading the frame - the first, and
 * those reading the original - rewritten once for the filter and wrap,
 * swapped in for a view and given back; what they draw from the whole
 * frame matches what the passes draw from a copy of the view, byte for
 * byte. A preset reading earlier frames, or the frame in a way the
 * rewrite does not take, keeps its copy. Skipped without a device. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <boolean.h>
#include "../../../gfx/common/hdr_viewer_colour.h"

#include "../../../gfx/drivers_shader/shader_glsl.c"
#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif
#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <EGL/egl.h>
#endif

/* The frontend shader_glsl.c refers to */
void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); printf("      [err] "); vprintf(fmt, ap); va_end(ap); }
bool gl2_load_luts(const void *shader, GLuint *textures)
{ (void)shader; (void)textures; return true; }
bool gl_query_extension(const char *name) { (void)name; return false; }
bool gl_check_capability(enum gl_capability_enum cap)
{
#ifdef HAVE_OPENGLES2
   (void)cap;return false;
#else
   return cap == GL_CAPS_GLES3_SUPPORTED;
#endif
}
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

static int fails;

#define CHECK(cond, what) do { \
   if (cond) printf("  [ok]   %s\n", what); \
   else { printf("  [FAIL] %s\n", what); fails++; } \
} while (0)

/* The rewritten pass against the copy, linearly filtered: equal but for
 * a step either way, which glsl_rect.h allows a read landing on a texel
 * edge (llvmpipe on LLVM 21 takes it for a hardware-ordered frame) */
static bool same_linear(const uint8_t *a, const uint8_t *b, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
      if (a[i] - b[i] > 1 || b[i] - a[i] > 1)
         return false;
   return true;
}

#define SHADER_HEAD \
   "#version 130\n" \
   "#if defined(VERTEX)\n" \
   "#if __VERSION__ >= 130\n" \
   "#define COMPAT_VARYING out\n" \
   "#define COMPAT_ATTRIBUTE in\n" \
   "#else\n" \
   "#define COMPAT_VARYING varying\n" \
   "#define COMPAT_ATTRIBUTE attribute\n" \
   "#endif\n" \
   "COMPAT_ATTRIBUTE vec4 VertexCoord;\n" \
   "COMPAT_ATTRIBUTE vec4 TexCoord;\n" \
   "COMPAT_VARYING vec2 vTex;\n" \
   "void main() { gl_Position = VertexCoord; vTex = TexCoord.xy; }\n" \
   "#elif defined(FRAGMENT)\n" \
   "#ifdef GL_ES\n" \
   "precision mediump float;\n" \
   "#endif\n" \
   "#if __VERSION__ >= 130\n" \
   "#define COMPAT_VARYING in\n" \
   "#define COMPAT_TEXTURE texture\n" \
   "out vec4 FragColor;\n" \
   "#else\n" \
   "#define COMPAT_VARYING varying\n" \
   "#define FragColor gl_FragColor\n" \
   "#define COMPAT_TEXTURE texture2D\n" \
   "#endif\n" \
   "uniform vec2 TextureSize;\n" \
   "uniform sampler2D Texture;\n" \
   "COMPAT_VARYING vec2 vTex;\n"

static const char *shader_blur =
   SHADER_HEAD
   "void main()\n"
   "{\n"
   "   vec2 d = 1.5 / TextureSize;\n"
   "   FragColor = COMPAT_TEXTURE(Texture, vTex) * 0.5\n"
   "      + COMPAT_TEXTURE(Texture, vTex + d) * 0.25\n"
   "      + COMPAT_TEXTURE(Texture, vTex - d) * 0.25;\n"
   "}\n"
   "#endif\n";

static const char *shader_orig =
   SHADER_HEAD
   "uniform sampler2D OrigTexture;\n"
   "void main()\n"
   "{\n"
   "   FragColor = COMPAT_TEXTURE(Texture, vTex)\n"
   "      * COMPAT_TEXTURE(OrigTexture, vTex);\n"
   "}\n"
   "#endif\n";

static const char *shader_prev =
   SHADER_HEAD
   "uniform sampler2D PrevTexture;\n"
   "void main()\n"
   "{\n"
   "   FragColor = COMPAT_TEXTURE(Texture, vTex)\n"
   "      - COMPAT_TEXTURE(PrevTexture, vTex);\n"
   "}\n"
   "#endif\n";

/* The frame under a name nothing binds, left at unit 0 */
static const char *shader_unbound =
   SHADER_HEAD
   "uniform sampler2D decal;\n"
   "void main()\n"
   "{\n"
   "   vec2 d = 0.75 / TextureSize;\n"
   "   FragColor = COMPAT_TEXTURE(decal, vTex + d) * 0.5\n"
   "      + COMPAT_TEXTURE(Texture, vTex - d) * 0.5;\n"
   "}\n"
   "#endif\n";

static const char *shader_fetch =
   SHADER_HEAD
   "void main()\n"
   "{\n"
   "   FragColor = texelFetch(Texture, ivec2(vTex * TextureSize), 0);\n"
   "}\n"
   "#endif\n";

#define FRAME_W 40
#define FRAME_H 30
#define TEX_W   64
#define TEX_H   64

static uint8_t frame_px[TEX_H][TEX_W][4];

/* A preset of @passes passes from @src, each compiled as the backend
 * does */
static glsl_shader_data_t *make_glsl(const char *const *src, unsigned passes)
{
   unsigned p;
   glsl_shader_data_t *glsl = (glsl_shader_data_t*)calloc(1, sizeof(*glsl));
   glsl->shader             = (struct video_shader*)
      calloc(1, sizeof(*glsl->shader));
   glsl->shader->passes     = passes;
   for (p = 1; p <= passes; p++)
   {
      struct shader_program_info info;
      glsl->shader->pass[p - 1].source.string.vertex   = strdup(src[p - 1]);
      glsl->shader->pass[p - 1].source.string.fragment = strdup(src[p - 1]);
      info.vertex   = src[p - 1];
      info.fragment = src[p - 1];
      info.is_file  = false;
      if (gl_glsl_compile_program(glsl, p, &glsl->prg[p], &info))
         gl_glsl_find_uniforms(glsl, p, glsl->prg[p].id, &glsl->uniforms[p]);
   }
   return glsl;
}

static void free_glsl(glsl_shader_data_t *glsl)
{
   unsigned p;
   gl_glsl_in_place_end(glsl);
   for (p = 1; p <= glsl->shader->passes; p++)
   {
      if (glsl->prg[p].id)
         glDeleteProgram(glsl->prg[p].id);
      free(glsl->shader->pass[p - 1].source.string.vertex);
      free(glsl->shader->pass[p - 1].source.string.fragment);
   }
   gl_glsl_in_place_free(glsl);
   gl_glsl_viewer_free(glsl);
#ifdef HAVE_GL_PLANAR
   gl_glsl_planar_free(glsl);
#endif
   for(p=0;p<VIDEO_SHADER_PROGRAM_COUNT;p++)
   {
      if(glsl->vbo[p].vbo_primary)glDeleteBuffers(1,&glsl->vbo[p].vbo_primary);
      if(glsl->vbo[p].vbo_secondary)glDeleteBuffers(1,&glsl->vbo[p].vbo_secondary);
      free(glsl->vbo[p].buffer_primary);free(glsl->vbo[p].buffer_secondary);
   }
   free(glsl->coord_scratch);
   free(glsl->shader);
   free(glsl);
}

static GLuint make_texture(unsigned w, unsigned h, const void *px)
{
   GLuint tex;
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
         GL_UNSIGNED_BYTE, px);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   return tex;
}

/* @prog from @tex, and @orig as the original when not 0, laid out for a
 * @in_w by @in_h input */
static void draw(GLuint prog, GLuint tex, GLuint orig, unsigned in_w,
      unsigned in_h, unsigned ow, unsigned oh, uint8_t *out)
{
   static const GLfloat quad[] = {
      -1, -1, 0, 1,   1, -1, 0, 1,   -1, 1, 0, 1,   1, 1, 0, 1 };
   static const GLfloat coords[] = {
      0, 0, 0, 0,   1, 0, 0, 0,   0, 1, 0, 0,   1, 1, 0, 0 };
   GLuint fbo, target;
   GLint loc, vloc, tloc;
   glGenTextures(1, &target);
   glBindTexture(GL_TEXTURE_2D, target);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ow, oh, 0, GL_RGBA,
         GL_UNSIGNED_BYTE, NULL);
   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, target, 0);
   glViewport(0, 0, ow, oh);
   glUseProgram(prog);
   if (orig)
   {
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, orig);
      if ((loc = glGetUniformLocation(prog, "OrigTexture")) >= 0)
         glUniform1i(loc, 1);
   }
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, tex);
   if ((loc = glGetUniformLocation(prog, "TextureSize")) >= 0)
      glUniform2f(loc, (float)in_w, (float)in_h);
   vloc = glGetAttribLocation(prog, "VertexCoord");
   tloc = glGetAttribLocation(prog, "TexCoord");
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glEnableVertexAttribArray(vloc);
   glEnableVertexAttribArray(tloc);
   glVertexAttribPointer(vloc, 4, GL_FLOAT, GL_FALSE, 0, quad);
   glVertexAttribPointer(tloc, 4, GL_FLOAT, GL_FALSE, 0, coords);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glReadPixels(0, 0, ow, oh, GL_RGBA, GL_UNSIGNED_BYTE, out);
   glDisableVertexAttribArray(vloc);
   glDisableVertexAttribArray(tloc);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glDeleteFramebuffers(1, &fbo);
   glDeleteTextures(1, &target);
}

static void draw_hdr(GLuint prog, GLuint tex, GLuint orig, unsigned in_w,
      unsigned in_h, unsigned ow, unsigned oh, float *out)
{
   static const GLfloat quad[] = {
      -1, -1, 0, 1,   1, -1, 0, 1,   -1, 1, 0, 1,   1, 1, 0, 1 };
   static const GLfloat coords[] = {
      0, 0, 0, 0,   1, 0, 0, 0,   0, 1, 0, 0,   1, 1, 0, 0 };
   GLuint fbo, target;
   GLint loc, vloc, tloc;
   glGenTextures(1, &target);
   glBindTexture(GL_TEXTURE_2D, target);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, ow, oh, 0, GL_RGBA,
         GL_FLOAT, NULL);
   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
         GL_TEXTURE_2D, target, 0);
   glViewport(0, 0, ow, oh);
   glUseProgram(prog);
   if (orig)
   {
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, orig);
      if ((loc = glGetUniformLocation(prog, "OrigTexture")) >= 0)
         glUniform1i(loc, 1);
      if ((loc = glGetUniformLocation(prog, "PrevTexture")) >= 0)
         glUniform1i(loc, 1);
   }
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, tex);
   if ((loc = glGetUniformLocation(prog, "TextureSize")) >= 0)
      glUniform2f(loc, (float)in_w, (float)in_h);
   vloc = glGetAttribLocation(prog, "VertexCoord");
   tloc = glGetAttribLocation(prog, "TexCoord");
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glEnableVertexAttribArray(vloc);
   glEnableVertexAttribArray(tloc);
   glVertexAttribPointer(vloc, 4, GL_FLOAT, GL_FALSE, 0, quad);
   glVertexAttribPointer(tloc, 4, GL_FLOAT, GL_FALSE, 0, coords);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glReadPixels(0, 0, ow, oh, GL_RGBA, GL_FLOAT, out);
   glDisableVertexAttribArray(vloc);
   glDisableVertexAttribArray(tloc);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glDeleteFramebuffers(1, &fbo);
   glDeleteTextures(1, &target);
}

static uint8_t slot[FRAME_H * FRAME_W * 4];
static uint8_t tex_px[TEX_H][TEX_W][4];
static const unsigned vx = 3, vy = 16, vw = 30, vh = 14;

/* The view's copy, and the frame as a hardware frame: its last row first */
static void make_inputs(GLuint *t_slot, GLuint *t_frame)
{
   unsigned r;
   for (r = 0; r < vh; r++)
      memcpy(slot + r * vw * 4, frame_px[vy + r][vx], vw * 4);
   for (r = 0; r < TEX_H; r++)
      memcpy(tex_px[r], frame_px[r < FRAME_H ? FRAME_H - 1 - r : r],
            sizeof(tex_px[r]));
   *t_slot  = make_texture(vw, vh, slot);
   *t_frame = make_texture(TEX_W, TEX_H, tex_px);
}

static void begin(glsl_shader_data_t *glsl)
{
   gl_glsl_in_place_begin(glsl, true, VIDEO_POS_PACK(vx, vy),
         VIDEO_SCALE_PACK(vw, vh), FRAME_H, VIDEO_SCALE_PACK(TEX_W, TEX_H));
}

static void test_blur(void)
{
   static uint8_t img_a[4 * 64 * 64], img_b[4 * 64 * 64];
   const unsigned ow = vw * 2 + 1, oh = vh * 2 + 3;
   glsl_shader_data_t *glsl = make_glsl(&shader_blur, 1);
   GLuint pass_id           = glsl->prg[1].id;
   GLuint t_slot, t_frame;

   printf("[glsl_in_place] one pass\n");
   CHECK(pass_id, "the pass compiles");
   CHECK(gl_glsl_in_place(glsl, GL_LINEAR, GL_CLAMP_TO_EDGE),
         "it can read in place, linear and clamped to edge");
   CHECK(gl_glsl_in_place(glsl, GL_LINEAR, GL_CLAMP_TO_EDGE)
         && glsl->in_place.key == 1 + GLSL_RECT_LINEAR_EDGE
         && glsl->in_place.count == 1,
         "rewritten once for that filter and wrap");
   CHECK(!gl_glsl_in_place(glsl, GL_LINEAR, GL_REPEAT),
         "linear and repeating it can't");
   CHECK(gl_glsl_in_place(glsl, GL_LINEAR, GL_CLAMP_TO_EDGE),
         "back to linear, clamped: made again");

   make_inputs(&t_slot, &t_frame);
   draw(pass_id, t_slot, 0, vw, vh, ow, oh, img_a);
   begin(glsl);
   CHECK(glsl->in_place.swapped && glsl->prg[1].id != pass_id
         && glsl->in_place.pass[0].prg.id == pass_id,
         "a view swaps the rewritten pass in");
   draw(glsl->prg[1].id, t_frame, 0, vw, vh, ow, oh, img_b);
   CHECK(same_linear(img_a, img_b, ow * oh * 4),
         "drawn from the frame as from the copy");
   gl_glsl_in_place_end(glsl);
   CHECK(!glsl->in_place.swapped && glsl->prg[1].id == pass_id,
         "and gives the pass back");
   CHECK(!glsl->current_mat_data_pointer[1],
         "its MVP is set again for the program in place");

   glDeleteTextures(1, &t_slot);
   glDeleteTextures(1, &t_frame);
   free_glsl(glsl);
}

/* Two passes, the second reading the first's output and the original */
static void test_orig(void)
{
   static uint8_t img_a[4 * 64 * 64], img_b[4 * 64 * 64];
   static uint8_t prev_px[FRAME_H * FRAME_W * 4];
   const unsigned ow = vw * 2 + 1, oh = vh * 2 + 3;
   const char *src[2];
   glsl_shader_data_t *glsl;
   GLuint pass2, t_slot, t_frame, t_prev;
   unsigned i;

   printf("[glsl_in_place] a later pass reading the original\n");
   src[0] = shader_blur;
   src[1] = shader_orig;
   glsl   = make_glsl(src, 2);
   pass2  = glsl->prg[2].id;
   CHECK(gl_glsl_in_place(glsl, GL_LINEAR, GL_CLAMP_TO_EDGE)
         && glsl->in_place.count == 2,
         "the first pass and the one reading the original are rewritten");

   for (i = 0; i < sizeof(prev_px); i++)
      prev_px[i] = (uint8_t)(i * 7 + 3);
   make_inputs(&t_slot, &t_frame);
   t_prev = make_texture(vw, vh, prev_px);
   draw(pass2, t_prev, t_slot, vw, vh, ow, oh, img_a);
   begin(glsl);
   CHECK(glsl->prg[2].id != pass2, "a view swaps the second in too");
   draw(glsl->prg[2].id, t_prev, t_frame, vw, vh, ow, oh, img_b);
   CHECK(same_linear(img_a, img_b, ow * oh * 4),
         "its original read from the frame as from the copy");
   gl_glsl_in_place_end(glsl);
   CHECK(glsl->prg[2].id == pass2, "and gives it back");

   glDeleteTextures(1, &t_slot);
   glDeleteTextures(1, &t_frame);
   glDeleteTextures(1, &t_prev);
   free_glsl(glsl);
}

/* A sampler nothing binds reads unit 0: the frame in the first pass, the
 * pass before's output after it; a look-up texture's name is not one */
static void test_unbound(void)
{
   static uint8_t img_a[4 * 64 * 64], img_b[4 * 64 * 64];
   const unsigned ow = vw * 2 + 1, oh = vh * 2 + 3;
   const char *src[2];
   glsl_shader_data_t *glsl;
   GLuint pass1, t_slot, t_frame;

   printf("[glsl_in_place] a sampler nothing binds\n");
   src[0] = shader_unbound;
   src[1] = shader_unbound;
   glsl   = make_glsl(src, 2);
   pass1  = glsl->prg[1].id;
   CHECK(gl_glsl_in_place(glsl, GL_LINEAR, GL_CLAMP_TO_EDGE)
         && glsl->in_place.count == 1 && glsl->in_place.pass[0].idx == 1,
         "the first pass is rewritten, the second keeps its input");

   make_inputs(&t_slot, &t_frame);
   draw(pass1, t_slot, 0, vw, vh, ow, oh, img_a);
   begin(glsl);
   draw(glsl->prg[1].id, t_frame, 0, vw, vh, ow, oh, img_b);
   CHECK(same_linear(img_a, img_b, ow * oh * 4),
         "both names read from the frame as from the copy");
   gl_glsl_in_place_end(glsl);
   glDeleteTextures(1, &t_slot);
   glDeleteTextures(1, &t_frame);
   free_glsl(glsl);

   glsl = make_glsl(src, 1);
   glsl->shader->luts = 1;
   strlcpy(glsl->shader->lut[0].id, "decal", sizeof(glsl->shader->lut[0].id));
   CHECK(gl_glsl_in_place(glsl, GL_LINEAR, GL_CLAMP_TO_EDGE)
         && glsl->in_place.count == 1
         && glGetUniformLocation(glsl->in_place.pass[0].prg.id, "decal") >= 0,
         "as a look-up texture's name it is left alone");
   free_glsl(glsl);
}

static void test_kept(const char *src, const char *what)
{
   glsl_shader_data_t *glsl = make_glsl(&src, 1);
   CHECK(glsl->prg[1].id && !gl_glsl_in_place(glsl, GL_LINEAR,
            GL_CLAMP_TO_EDGE) && !glsl->in_place.ok, what);
   free_glsl(glsl);
}

static void test_viewer_colour(void)
{
   static const float whites[] = {80, 100, 203, 400};
   static const char *src = SHADER_HEAD
      "uniform sampler2D OrigTexture;\n"
      "void main() { FragColor = COMPAT_TEXTURE(Texture,vTex)*0.75"
      "+COMPAT_TEXTURE(OrigTexture,vTex)*0.25; }\n#endif\n";
   float pixels[32], lut[32], out[32], rows[12];
   GLuint texture, lookup, core;
   unsigned g, w, i, c, failures = 0;
   glsl_shader_data_t *glsl = make_glsl(&src, 1);
   core = glsl->prg[1].id;
   glsl->shader->luts = 1;
   strlcpy(glsl->shader->lut[0].id, "OrigTexture", sizeof(glsl->shader->lut[0].id));
   gl_glsl_viewer_prepare(glsl);
   CHECK(glsl->viewer.ok && glsl->viewer.count == 1, "HDR viewer variant compiled during preset loading");
   CHECK(glsl->prg[1].id == core, "preparing the viewer keeps the core program");
   texture = make_texture(8, 1, NULL);
   lookup = make_texture(8, 1, NULL);
   for (i = 0; i < 8; i++)
      for (c = 0; c < 4; c++)
      {
         pixels[i * 4 + c] = c == 3 ? 0.6f : ((float)i - 2.0f) * (float)(c + 1) * 0.8f;
         lut[i * 4 + c] = 0.1f * (float)(c + 1);
      }
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 8, 1, 0, GL_RGBA, GL_FLOAT, pixels);
   glBindTexture(GL_TEXTURE_2D, lookup);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 8, 1, 0, GL_RGBA, GL_FLOAT, lut);
   for (g = 0; g < 4; g++)
      for (w = 0; w < 4; w++)
      {
         hdr_viewer_colour_rows(rows, true, whites[w], g);
         CHECK(gl_glsl_set_media_colour(glsl, rows), "select the precompiled viewer");
         {
            GLint current = 0;
            gl_glsl_use(NULL, glsl, 1, true);
            glGetIntegerv(GL_CURRENT_PROGRAM, &current);
            CHECK((GLuint)current == gl_glsl_program(glsl, 1)->id
                  && (GLuint)current != core && glsl->prg[1].id == core,
                  "shipping use selects the viewer without changing the core table");
         }
         draw_hdr(gl_glsl_program(glsl, 1)->id, texture, lookup, 8, 1, 8, 1, out);
         for (i = 0; i < 8; i++)
            for (c = 0; c < 4; c++)
            {
               float expected = pixels[i * 4 + c];
               if (c != 3)
               {
                  const float *r = rows + c * 4;
                  float x = r[0] * pixels[i * 4] + r[1] * pixels[i * 4 + 1]
                     + r[2] * pixels[i * 4 + 2];
                  expected = (x < 0 ? -1.0f : 1.0f) * powf(fabsf(x), rows[7]);
               }
               expected = expected * 0.75f + lut[i * 4 + c] * 0.25f;
               if (fabsf(out[i * 4 + c] - expected) > 0.006f) failures++;
            }
         gl_glsl_set_media_colour(glsl, NULL);
         CHECK(glsl->prg[1].id == core && !glsl->viewer_active,
               "the core gets its program and uniform metadata back");
      }
   CHECK(!failures, "128 signed HDR GPU vectors preserve alpha and leave LUT samples unchanged");
   glDeleteTextures(1, &texture);
   glDeleteTextures(1, &lookup);
   free_glsl(glsl);
}

static void test_viewer_history(void)
{
   const char *sources[2] = {shader_prev, shader_blur};
   static const float frame[4] = {4.0f, 1.0f, 0.25f, 0.6f};
   static const float history[4] = {1.0f, 0.25f, 0.0625f, 0.3f};
   glsl_shader_data_t *glsl = make_glsl(sources, 2);
   float rows[12], out[4];
   GLuint second = glsl->prg[2].id;
   GLuint texture = make_texture(1, 1, NULL);
   GLuint previous = make_texture(1, 1, NULL);
   unsigned c, failures = 0;
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, frame);
   glBindTexture(GL_TEXTURE_2D, previous);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, history);
   gl_glsl_viewer_prepare(glsl);
   CHECK(glsl->viewer.ok && glsl->viewer.count == 1,
         "history reads are specialized while later processed Source stays untouched");
   hdr_viewer_colour_rows(rows, true, 80, 0);
   CHECK(gl_glsl_set_media_colour(glsl, rows) && gl_glsl_program(glsl, 2)->id == second,
         "later pass keeps its original program");
   draw_hdr(gl_glsl_program(glsl, 1)->id, texture, previous, 1, 1, 1, 1, out);
   for (c = 0; c < 4; c++)
   {
      float expected = c == 3 ? frame[c] - history[c]
         : powf(frame[c], rows[7]) - powf(history[c], rows[7]);
      if (fabsf(out[c] - expected) > 0.006f) failures++;
   }
   CHECK(!failures, "raw HDR history is normalized at its own sample");
   gl_glsl_set_media_colour(glsl, NULL);
   glDeleteTextures(1, &texture);
   glDeleteTextures(1, &previous);
   free_glsl(glsl);
}

#include "planar_gpu_cases.h"
#ifdef _WIN32
static rglgen_func_t resolve_gl(const char *name)
{
   PROC proc=wglGetProcAddress(name);rglgen_func_t result;
   if(!proc || proc==(PROC)1 || proc==(PROC)2 || proc==(PROC)3 || proc==(PROC)-1)
      proc=GetProcAddress(GetModuleHandleA("opengl32.dll"),name);
   memcpy(&result,&proc,sizeof(result));return result;
}
#endif
int main(int argc,char **argv)
{
   bool planar_only=argc==2 && !strcmp(argv[1],"--planar-only");
#ifdef _WIN32
   HWND window;HDC dc;HGLRC ctx;PIXELFORMATDESCRIPTOR descriptor;
   unsigned x,y,c;
   window=CreateWindowExA(0,"STATIC","GLSL planar regression",WS_POPUP,0,0,16,16,NULL,NULL,GetModuleHandle(NULL),NULL);
   if(!window)return 77;
   dc=GetDC(window);memset(&descriptor,0,sizeof(descriptor));
   descriptor.nSize=sizeof(descriptor);descriptor.nVersion=1;
   descriptor.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;descriptor.iPixelType=PFD_TYPE_RGBA;descriptor.cColorBits=32;
   if(!SetPixelFormat(dc,ChoosePixelFormat(dc,&descriptor),&descriptor))return 77;
   ctx=wglCreateContext(dc);if(!ctx || !wglMakeCurrent(dc,ctx))return 77;
   rglgen_resolve_symbols(resolve_gl);
   gl_glsl_set_context_type(false,4,6);
#else
   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   EGLConfig cfg;
   EGLint n;
   EGLContext ctx;
   EGLSurface surf;
#ifdef HAVE_OPENGLES2
   EGLint cfg_attr[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE };
   EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
#else
   EGLint cfg_attr[] = { EGL_RENDERABLE_TYPE, 0x40 /* ES3 */,
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE };
   EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
#endif
   EGLint pb_attr[]  = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
   unsigned x, y, c;

   if (     dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, NULL, NULL)
         || !eglBindAPI(EGL_OPENGL_ES_API)
         || !eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || !n
         || (ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr))
            == EGL_NO_CONTEXT
         || (surf = eglCreatePbufferSurface(dpy, cfg, pb_attr))
            == EGL_NO_SURFACE
         || !eglMakeCurrent(dpy, surf, surf, ctx))
   {
      printf("[glsl_in_place] no GLES 3 device, skipped\n");
      return 0;
   }
#endif
   srand(54321);
   for (y = 0; y < TEX_H; y++)
      for (x = 0; x < TEX_W; x++)
         for (c = 0; c < 4; c++)
            frame_px[y][x][c] = (uint8_t)(rand() >> 7);

   printf("[glsl_in_place] %s\n", (const char*)glGetString(GL_RENDERER));
#ifdef GLSL_PLANAR_ORACLE
   test_planar_dependencies();
   {unsigned layout,linear,passes,lookups,original;for(original=0;original<2;original++)
    for(lookups=0;lookups<=2;lookups+=2)
    for(passes=1;passes<=3;passes++)
    for(layout=0;layout<3;layout++)for(linear=0;linear<2;linear++)
    {
       if(original && passes==1)continue;
       test_planar_fusion(layout,linear!=0,false,passes,lookups,original!=0);
#ifndef HAVE_OPENGLES2
       test_planar_fusion(layout,linear!=0,true,passes,lookups,original!=0);
#endif
    }}
#endif
   if(!planar_only)
   {
   test_viewer_colour();
   test_viewer_history();
   test_blur();
   test_orig();
   test_unbound();
   test_kept(shader_prev, "a preset reading earlier frames keeps its copy");
   test_kept(shader_fetch, "one fetching the frame's texels keeps its copy");
   }

#ifdef _WIN32
   wglMakeCurrent(NULL,NULL);wglDeleteContext(ctx);ReleaseDC(window,dc);DestroyWindow(window);
#else
   eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroySurface(dpy, surf);
   eglDestroyContext(dpy, ctx);
   eglTerminate(dpy);
#endif
   if (fails)
   {
      printf("[glsl_in_place] %d failure(s)\n", fails);
      return 1;
   }
   printf("[glsl_in_place] all passed\n");
   return 0;
}
