/* gfx/drivers_shader/glsl_rect.h: a GLSL preset's first pass reading its
 * frame where it lies.
 *
 * The rewrite's cases first, on source text alone: what is read in place,
 * and what keeps the copy.
 *
 * Then, where an EGL device with GLES 2 or 3 is found (Mesa's llvmpipe on
 * the CI runners), the oracle: each shader is drawn from a texture
 * holding just the view, as gl2's copy is, and rewritten from the whole
 * frame, software and hardware ordered, for every filter and wrap the
 * rewrite takes; the two images must match byte for byte, but for one
 * step either way under linear filtering, where glsl_rect.h says a read
 * landing on a texel edge may weigh its texels one step apart (llvmpipe
 * on LLVM 21 does for the blur shader's hardware-ordered frame). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/drivers_shader/glsl_rect.h"

#ifdef GLSL_RECT_ORACLE
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif
#endif

static int fails;

#define CHECK(cond, what) do { \
   if (cond) printf("  [ok]   %s\n", what); \
   else { printf("  [FAIL] %s\n", what); fails++; } \
} while (0)

/* The last as RetroArch names a sampler nothing binds, which keeps unit 0 */
static const char *frame_names[] = { "Texture", "rubyTexture", "s_p" };

static char *rewrite(const char *src, enum glsl_rect_mode mode)
{
   return glsl_rect_rewrite(src, frame_names, 3, mode);
}

static bool supported(const char *src)
{
   char *out = rewrite(src, GLSL_RECT_LINEAR_EDGE);
   free(out);
   return out != NULL;
}

/* The converted shaders' prologue, both stages */
#define COMPAT_HEAD \
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
   "#define COMPAT_TEXTURE_LOD textureLod\n" \
   "out vec4 FragColor;\n" \
   "#else\n" \
   "#define COMPAT_VARYING varying\n" \
   "#define FragColor gl_FragColor\n" \
   "#define COMPAT_TEXTURE texture2D\n" \
   "#define COMPAT_TEXTURE_LOD(s, c, l) texture2D(s, c)\n" \
   "#endif\n" \
   "uniform vec2 TextureSize;\n" \
   "COMPAT_VARYING vec2 vTex;\n" \
   COMPAT_FRAME

#define COMPAT_FRAME \
   "uniform sampler2D Texture;\n" \
   "#define Source Texture\n"

/* Five taps a texel and a half apart each way, past the edges */
static const char *shader_blur =
   COMPAT_HEAD
   "void main()\n"
   "{\n"
   "   vec2 d = 1.5 / TextureSize;\n"
   "   vec4 c = COMPAT_TEXTURE(Source, vTex) * 0.2\n"
   "      + COMPAT_TEXTURE(Source, vTex + vec2(d.x, 0.0)) * 0.2\n"
   "      + COMPAT_TEXTURE(Source, vTex - vec2(d.x, 0.0)) * 0.2\n"
   "      + COMPAT_TEXTURE(Source, vTex + vec2(0.0, d.y)) * 0.2\n"
   "      + COMPAT_TEXTURE(Source, vTex - vec2(0.0, d.y)) * 0.2;\n"
   "   FragColor = c;\n"
   "}\n"
   "#endif\n";

/* The frame under two names in one stage, both at unit 0 */
static const char *shader_two =
   COMPAT_HEAD
   "uniform sampler2D s_p;\n"
   "void main()\n"
   "{\n"
   "   vec2 d = 0.75 / TextureSize;\n"
   "   FragColor = COMPAT_TEXTURE(Source, vTex + d) * 0.5\n"
   "      + COMPAT_TEXTURE(s_p, vTex - d) * 0.5;\n"
   "}\n"
   "#endif\n";

/* A function reading the frame ahead of its declaration */
#undef COMPAT_FRAME
#define COMPAT_FRAME ""
static const char *shader_early =
   COMPAT_HEAD
   "vec4 early(sampler2D s, vec2 c)\n"
   "{\n"
   "   return COMPAT_TEXTURE(s, c + 0.75 / TextureSize);\n"
   "}\n"
   "uniform sampler2D Texture;\n"
   "void main()\n"
   "{\n"
   "   FragColor = early(Texture, vTex) * 0.5\n"
   "      + COMPAT_TEXTURE(Texture, vTex) * 0.5;\n"
   "}\n"
   "#endif\n";
#undef COMPAT_FRAME
#define COMPAT_FRAME \
   "uniform sampler2D Texture;\n" \
   "#define Source Texture\n"

/* Reads moved whole texels, as GLSL 1.30 and GLES 3 have them */
static const char *shader_offset =
   COMPAT_HEAD
   "void main()\n"
   "{\n"
   "   FragColor = textureOffset(Source, vTex, ivec2(1, 0)) * 0.25\n"
   "      + textureOffset(Source, vTex, ivec2(-2, 1), 0.0) * 0.25\n"
   "      + textureLodOffset(Source, vTex, 0.0, ivec2(0, -1)) * 0.5;\n"
   "}\n"
   "#endif\n";

#ifdef GLSL_RECT_ORACLE
/* Well past the rectangle on every side */
static const char *shader_wide =
   COMPAT_HEAD
   "void main()\n"
   "{\n"
   "   FragColor = COMPAT_TEXTURE(Source, vTex * 1.8 - 0.4);\n"
   "}\n"
   "#endif\n";
#endif

int test_rewrite(void)
{
   char *out;

   printf("[glsl_rect] rewrite\n");
   out = rewrite(shader_blur, GLSL_RECT_LINEAR_EDGE);
   CHECK(out && !strstr(out, "COMPAT_TEXTURE(Source")
         && strstr(out, "RARCH_frame(vTex + vec2(d.x, 0.0))"),
         "reads through macros naming the read and the frame go in place");
   CHECK(out && strstr(out,
         "uniform sampler2D Texture;\n#ifndef RARCH_FRAME_HELPER"),
         "the helper follows the frame's declaration");
   free(out);
   out = rewrite(shader_two, GLSL_RECT_LINEAR_EDGE);
   CHECK(out && strstr(out, "uniform sampler2D s_p;\n#ifndef RARCH_FRAME_HELPER")
         && strstr(out, "RARCH_frame(vTex - d)"),
         "a second name for the frame in one stage reads through one helper");
   free(out);

   out = rewrite(shader_offset, GLSL_RECT_LINEAR_EDGE);
   CHECK(out && strstr(out, "RARCH_frame_at(vTex, vec2(ivec2(1, 0)))")
         && strstr(out, "RARCH_frame_at(vTex, vec2(ivec2(-2, 1)))")
         && strstr(out, "RARCH_frame_at(vTex, vec2(ivec2(0, -1)))"),
         "reads with an offset go in place, moved in the view's texels");
   free(out);
   out = rewrite(shader_early, GLSL_RECT_LINEAR_EDGE);
   CHECK(out && strstr(out, "\nuniform sampler2D Texture;\n#ifndef RARCH_FRAME_HELPER")
         && strstr(out, "#endif\nvec4 early(")
         && strstr(out, "}\n\nvoid main()"),
         "a read ahead of the frame's declaration takes it and the helper up");
   free(out);
   CHECK(!supported(
         "varying vec2 t;\n"
         "vec4 f(sampler2D s) { return texture2D(s, t); }\n"
         "uniform sampler2D Texture, LUT;\n"
         "void main() { gl_FragColor = f(Texture); }\n"),
         "a declaration of two names is not moved");
   CHECK(supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "#define TEX(x, y) textureOffset(Texture, t, ivec2(x, y))\n"
         "void main() { gl_FragColor = TEX(1, -1); }\n"),
         "an offset read in a function-like macro's body");
   CHECK(!supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "void main() { gl_FragColor = textureOffset(Texture, t); }\n"),
         "an offset read missing its offset keeps the copy");

   CHECK(supported(
         "uniform sampler2D rubyTexture;\n"
         "varying vec2 t;\n"
         "void main() { gl_FragColor = texture2D(rubyTexture, t); }\n"),
         "a prefixed name");
   CHECK(supported(
         "uniform sampler2D Texture;\nuniform sampler2D LUT;\n"
         "varying vec2 t;\n"
         "void main() { gl_FragColor = texture2D(Texture, t)\n"
         "   * texture2D(LUT, t); }\n"),
         "another sampler's reads are left alone");
   CHECK(supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "#define TEX(c) texture2D(Texture, c)\n"
         "void main() { gl_FragColor = TEX(t); }\n"),
         "a read inside a function-like macro's body");
   CHECK(supported(
         "uniform sampler2D Texture; // texelFetch(Texture, x)\n"
         "varying vec2 t; /* textureSize(Texture, 0) */\n"
         "void main() { gl_FragColor = texture2D(Texture, t, 1.0); }\n"),
         "comments, and a bias dropped");

   CHECK(supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "#define Source Texture\n"
         "vec4 f(sampler2D s, vec2 c) { return texture2D(s, c); }\n"
         "vec4 g(vec2 c, sampler2D s) { return f(s, c) * 0.5; }\n"
         "void main() { gl_FragColor = g(t, Source) + f(Texture, t); }\n"),
         "a function every call gives the frame, through another");
   CHECK(supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "#define Source Texture\n"
         "#define GET(c) texture2D(Source, c)\n"
         "vec4 f(sampler2D Source, vec2 c) { return GET(c); }\n"
         "void main() { gl_FragColor = f(Texture, t) + GET(t); }\n"),
         "a define naming a frame parameter that shadows the frame's name");
   CHECK(!supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "#define GET(c) texture2D(tex, c)\n"
         "vec4 f(sampler2D tex, vec2 c) { return GET(c); }\n"
         "void main() { gl_FragColor = f(Texture, t); }\n"),
         "a define reading a frame parameter out of the body keeps the copy");
   CHECK(supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "#define tex2D(c, d) texture2D(c, d)\n"
         "#define texture(c, d) COMPAT_TEXTURE(c, d)\n"
         "#define COMPAT_TEXTURE texture2D\n"
         "#undef PREC_HIGH\n"
         "void main() { gl_FragColor = tex2D(Texture, t) * texture(Texture, t); }\n"),
         "macros forwarding to a read, and an unrelated undef");
   CHECK(supported(
         "#define s_p Texture\nuniform sampler2D s_p;\nvarying vec2 t;\n"
         "void main() { gl_FragColor = texture2D(s_p, t); }\n"),
         "the frame declared through a macro naming it");
   CHECK(!supported(
         "uniform sampler2D Texture;\nuniform sampler2D LUT;\nvarying vec2 t;\n"
         "vec4 f(sampler2D s, vec2 c) { return texture2D(s, c); }\n"
         "void main() { gl_FragColor = f(Texture, t) * f(LUT, t); }\n"),
         "a function given the frame and another sampler keeps the copy");
   CHECK(supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "vec4 f(sampler2D s, vec2 c) { return texture2D(s, c); }\n"
         "vec4 f(sampler2D s, vec3 c) { return texture2D(s, c.xy); }\n"
         "void main() { gl_FragColor = f(Texture, t) + f(Texture, t.xyx); }\n"),
         "an overloaded function every call gives the frame");
   CHECK(!supported(
         "uniform sampler2D Texture;\nuniform sampler2D LUT;\nvarying vec2 t;\n"
         "vec4 f(sampler2D s, vec2 c) { return texture2D(s, c); }\n"
         "vec4 f(sampler2D s, vec3 c) { return texture2D(s, c.xy); }\n"
         "void main() { gl_FragColor = f(Texture, t) * f(LUT, t.xyx); }\n"),
         "an overload given another sampler keeps the copy");
   CHECK(!supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "vec4 f(sampler2D s, vec2 c) { return texture2D(s, c); }\n"
         "vec4 f(float s, vec2 c) { return vec4(s); }\n"
         "void main() { gl_FragColor = f(Texture, t) * f(1.0, t); }\n"),
         "an overload taking no sampler there keeps the copy");
   CHECK(!supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "#define GET(c) texture2D(tex, c)\n"
         "vec4 f(sampler2D s, vec2 c) { return texture2D(s, c); }\n"
         "vec4 f(sampler2D tex, vec3 c) { return GET(c.xy); }\n"
         "void main() { gl_FragColor = f(Texture, t) + f(Texture, t.xyx); }\n"),
         "one overload keeping the copy keeps it for all");
   CHECK(supported(
         "uniform sampler2D Texture;\nuniform sampler2D LUT;\nvarying vec2 t;\n"
         "vec4 g(sampler2D s, vec2 c) { return texture2D(s, c); }\n"
         "vec4 f(sampler2D s, vec2 c) { return g(s, c); }\n"
         "vec4 f(sampler2D s, vec3 c) { return f(s, c.xy); }\n"
         "void main() { gl_FragColor = texture2D(Texture, t) * g(LUT, t); }\n"),
         "functions only calling each other carry nothing");
   CHECK(!supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "void main() { gl_FragColor = texelFetch(Texture, ivec2(t), 0); }\n"),
         "another built-in keeps the copy");
   CHECK(!supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "void main() { gl_FragColor = vec4(textureSize(Texture, 0), 0, 1); }\n"),
         "its size asked of GL keeps the copy");
   CHECK(!supported(
         "uniform sampler2D Texture;\nuniform sampler2D LUT;\nvarying vec2 t;\n"
         "#ifdef A\n#define S Texture\n#else\n#define S LUT\n#endif\n"
         "void main() { gl_FragColor = texture2D(S, t); }\n"),
         "a macro meaning the frame in one branch only keeps the copy");
   CHECK(!supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "void main() { gl_FragColor = texture2D(Texture,\n"
         "   texture2D(Texture, t).xy); }\n"),
         "a read inside a read's coordinate keeps the copy");
   CHECK(!supported(
         "varying vec2 t;\n"
         "vec4 f(sampler2D Texture) { return texture2D(Texture, t); }\n"),
         "a parameter named as the frame keeps the copy");
   CHECK(!supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "#undef Texture\n"
         "void main() { gl_FragColor = texture2D(Texture, t); }\n"),
         "an undef keeps the copy");
   CHECK(!supported(
         "uniform sampler2D Texture;\nvarying vec2 t;\n"
         "void main() { gl_FragColor = texture2DProj(Texture, vec3(t, 1)); }\n"),
         "a projective read keeps the copy");
   CHECK(!supported("void main() { gl_FragColor = vec4(1.0); }\n"),
         "no frame declared: nothing to put the helper after");
   CHECK(!glsl_rect_rewrite(shader_blur, frame_names, 0,
         GLSL_RECT_LINEAR_EDGE), "no frame names");
   return 0;
}

#ifdef GLSL_RECT_ORACLE

#define FRAME_W 40
#define FRAME_H 30
#define TEX_W   64
#define TEX_H   64

static uint8_t frame_px[TEX_H][TEX_W][4];

static GLuint compile_stage(GLenum type, const char *version,
      const char *define, const char *src, bool quiet)
{
   GLint ok;
   const char *parts[3];
   GLuint sh  = glCreateShader(type);
   parts[0]   = version;
   parts[1]   = define;
   parts[2]   = src;
   glShaderSource(sh, 3, parts, NULL);
   glCompileShader(sh);
   glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
   if (!ok)
   {
      char log[2048];
      glGetShaderInfoLog(sh, sizeof(log), NULL, log);
      if (!quiet)
         printf("      [log] %s\n", log);
      glDeleteShader(sh);
      return 0;
   }
   return sh;
}

static GLuint program(const char *version, const char *src)
{
   GLint ok;
   GLuint prog, vs, fs;
   if (!(vs = compile_stage(GL_VERTEX_SHADER, version, "#define VERTEX\n",
               src, false)))
      return 0;
   if (!(fs = compile_stage(GL_FRAGMENT_SHADER, version,
               "#define FRAGMENT\n", src, false)))
   {
      glDeleteShader(vs);
      return 0;
   }
   prog = glCreateProgram();
   glAttachShader(prog, vs);
   glAttachShader(prog, fs);
   glBindAttribLocation(prog, 0, "VertexCoord");
   glBindAttribLocation(prog, 1, "TexCoord");
   glLinkProgram(prog);
   glDeleteShader(vs);
   glDeleteShader(fs);
   glGetProgramiv(prog, GL_LINK_STATUS, &ok);
   if (!ok)
   {
      glDeleteProgram(prog);
      return 0;
   }
   return prog;
}

static GLuint make_texture(unsigned w, unsigned h, const void *px,
      GLenum filter, GLenum wrap)
{
   GLuint tex;
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
         GL_UNSIGNED_BYTE, px);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
   return tex;
}

/* @prog drawn from @tex into a @ow by @oh image read back into @out */
static void draw(GLuint prog, GLuint tex, unsigned in_w, unsigned in_h,
      const float *rect, unsigned ow, unsigned oh, uint8_t *out)
{
   static const GLfloat quad[] = {
      -1, -1, 0, 1,   1, -1, 0, 1,   -1, 1, 0, 1,   1, 1, 0, 1 };
   static const GLfloat coords[] = {
      0, 0, 0, 0,   1, 0, 0, 0,   0, 1, 0, 0,   1, 1, 0, 0 };
   GLuint fbo, target;
   GLint loc;
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
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, tex);
   if ((loc = glGetUniformLocation(prog, "Texture")) >= 0)
      glUniform1i(loc, 0);
   if ((loc = glGetUniformLocation(prog, "TextureSize")) >= 0)
      glUniform2f(loc, (float)in_w, (float)in_h);
   if (rect)
   {
      if ((loc = glGetUniformLocation(prog, GLSL_RECT_NAME_RECT)) >= 0)
         glUniform4fv(loc, 1, rect);
      if ((loc = glGetUniformLocation(prog, GLSL_RECT_NAME_CLAMP)) >= 0)
         glUniform4fv(loc, 1, rect + 4);
      if ((loc = glGetUniformLocation(prog, GLSL_RECT_NAME_TEXELS)) >= 0)
         glUniform4fv(loc, 1, rect + 8);
   }
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, quad);
   glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, coords);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glReadPixels(0, 0, ow, oh, GL_RGBA, GL_UNSIGNED_BYTE, out);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glDeleteFramebuffers(1, &fbo);
   glDeleteTextures(1, &target);
}

/* One shader, mode and view, both orders: false on any difference */
static bool oracle_case(const char *version, const char *src,
      enum glsl_rect_mode mode, unsigned vx, unsigned vy, unsigned vw,
      unsigned vh, bool *compiled)
{
   static uint8_t slot[FRAME_H * FRAME_W * 4];
   static uint8_t img_a[4 * 96 * 96], img_b[4 * 96 * 96];
   static const GLenum wraps[] = { GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
      GL_REPEAT, GL_MIRRORED_REPEAT, GL_CLAMP_TO_BORDER };
   GLenum filter = mode == GLSL_RECT_LINEAR_EDGE ? GL_LINEAR : GL_NEAREST;
   unsigned ow = vw * 2 + 3, oh = vh * 2 + 1, r, flipped;
   char *rw    = rewrite(src, mode);
   GLuint p_copy, p_place;
   bool same   = true;

   *compiled = false;
   if (!rw)
      return false;
   p_copy  = program(version, src);
   p_place = program(version, rw);
   free(rw);
   if (!p_copy || !p_place)
   {
      glDeleteProgram(p_copy);
      glDeleteProgram(p_place);
      return false;
   }
   *compiled = true;

   for (flipped = 0; flipped < 2; flipped++)
   {
      static uint8_t tex_px[TEX_H][TEX_W][4];
      float rect[12];
      GLuint t_slot, t_frame;
      /* The view, its top row first, as gl2's copy leaves it */
      for (r = 0; r < vh; r++)
         memcpy(slot + r * vw * 4, frame_px[vy + r][vx], vw * 4);
      /* The frame from its first row, or, as a hardware frame, its last */
      memcpy(tex_px, frame_px, sizeof(tex_px));
      if (flipped)
         for (r = 0; r < FRAME_H; r++)
            memcpy(tex_px[r], frame_px[FRAME_H - 1 - r], sizeof(tex_px[r]));
      t_slot  = make_texture(vw, vh, slot, filter, wraps[mode]);
      /* The frame's wrap is not what the rewrite reads by */
      t_frame = make_texture(TEX_W, TEX_H, tex_px, filter, GL_REPEAT);
      glsl_rect_values(mode, flipped != 0, vx, vy, vw, vh, FRAME_H,
            TEX_W, TEX_H, rect);
      draw(p_copy, t_slot, vw, vh, NULL, ow, oh, img_a);
      draw(p_place, t_frame, vw, vh, rect, ow, oh, img_b);
      if (memcmp(img_a, img_b, ow * oh * 4))
      {
         unsigned i, n = 0, worst = 0;
         for (i = 0; i < ow * oh * 4; i++)
         {
            unsigned d = img_a[i] > img_b[i] ? img_a[i] - img_b[i]
               : img_b[i] - img_a[i];
            if (d)
               n++;
            if (d > worst)
               worst = d;
         }
         printf("      %u bytes differ, by up to %u (%s)\n", n, worst,
               flipped ? "hardware order" : "software order");
         if (filter != GL_LINEAR || worst > 1)
            same = false;
      }
      glDeleteTextures(1, &t_slot);
      glDeleteTextures(1, &t_frame);
   }
   glDeleteProgram(p_copy);
   glDeleteProgram(p_place);
   return same;
}

static int test_oracle(EGLint client_version, const char *version,
      const char *what)
{
   static const char *mode_names[] = { "linear, clamp to edge",
      "nearest, clamp to edge", "nearest, repeat", "nearest, mirror",
      "nearest, border" };
   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   EGLConfig cfg;
   EGLint n;
   EGLContext ctx;
   EGLint cfg_attr[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE };
   EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
   EGLint pb_attr[]  = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
   EGLSurface surf;
   unsigned m, s, v;
   static const unsigned views[][4] = {
      { 5, 7, 21, 13 },   /* inside, neighbours on every side */
      { 0, 0, 40, 15 },   /* the top screen of two */
      { 0, 15, 40, 15 },  /* the bottom one */
      { 19, 3, 1, 1 } };  /* one texel */
   static const char *const kinds[] = { "blur", "wide", "two-name",
      "offset", "early" };
   const char *shaders[5];
   shaders[0] = shader_blur;
   shaders[1] = shader_wide;
   shaders[2] = shader_two;
   shaders[3] = shader_offset;
   shaders[4] = shader_early;

   if (     dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, NULL, NULL)
         || !eglBindAPI(EGL_OPENGL_ES_API))
   {
      printf("[glsl_rect] oracle (%s): no EGL device, skipped\n", what);
      return 0;
   }
   if (client_version >= 3)
      cfg_attr[1] = 0x40; /* EGL_OPENGL_ES3_BIT */
   ctx_attr[1] = client_version;
   if (     !eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || !n
         || (ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr))
            == EGL_NO_CONTEXT
         || (surf = eglCreatePbufferSurface(dpy, cfg, pb_attr))
            == EGL_NO_SURFACE
         || !eglMakeCurrent(dpy, surf, surf, ctx))
   {
      printf("[glsl_rect] oracle (%s): no context, skipped\n", what);
      return 0;
   }
   printf("[glsl_rect] oracle (%s, %s)\n", what,
         (const char*)glGetString(GL_RENDERER));
   for (m = GLSL_RECT_LINEAR_EDGE; m <= GLSL_RECT_NEAREST_BORDER; m++)
   {
      if (client_version < 3 && m != GLSL_RECT_LINEAR_EDGE
            && m != GLSL_RECT_NEAREST_EDGE && m != GLSL_RECT_NEAREST_REPEAT)
         continue;
      if (m == GLSL_RECT_NEAREST_BORDER
            && !strstr((const char*)glGetString(GL_VERSION), "3.2"))
         continue;
      for (s = 0; s < 5; s++)
      {
         bool all = true, compiled = true;
         char what_case[160];
         /* GLES 2 reads no offsets */
         if (client_version < 3 && shaders[s] == shader_offset)
            continue;
         for (v = 0; v < sizeof(views) / sizeof(views[0]); v++)
         {
            bool c;
            /* Repeat on GLES 2 needs a power of two the view is not */
            if (client_version < 3 && m == GLSL_RECT_NEAREST_REPEAT)
               break;
            if (!oracle_case(version, shaders[s], (enum glsl_rect_mode)m,
                     views[v][0], views[v][1], views[v][2], views[v][3], &c))
               all = false;
            compiled = compiled && c;
         }
         snprintf(what_case, sizeof(what_case),
               "%s shader, %s: in place matches the copy",
               kinds[s], mode_names[m]);
         if (client_version < 3 && m == GLSL_RECT_NEAREST_REPEAT)
            continue;
         CHECK(compiled && all, what_case);
      }
   }
   eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroySurface(dpy, surf);
   eglDestroyContext(dpy, ctx);
   return 0;
}
#endif

int main(void)
{
   test_rewrite();
#ifdef GLSL_RECT_ORACLE
   {
      unsigned x, y, c;
      srand(12345);
      for (y = 0; y < TEX_H; y++)
         for (x = 0; x < TEX_W; x++)
            for (c = 0; c < 4; c++)
               frame_px[y][x][c] = (uint8_t)(rand() >> 7);
   }
   test_oracle(3, "#version 300 es\n", "GLES 3");
   test_oracle(2, "#version 100\n", "GLES 2");
#endif
   if (fails)
   {
      printf("[glsl_rect] %d failure(s)\n", fails);
      return 1;
   }
   printf("[glsl_rect] all passed\n");
   return 0;
}
