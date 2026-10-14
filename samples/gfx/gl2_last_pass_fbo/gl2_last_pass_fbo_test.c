/* The gl2 driver's render chain against a scripted GL and shader
 * backend: the last pass of a preset gets an FBO of its own, copied to
 * the backbuffer by the stock pass after it, only when drawing it
 * straight there would give a different image - a scale other than the
 * viewport at 1.0, an FBO format, or the feedback the preset asks of
 * it.  A one-pass preset that needs no FBO sets up no chain at all. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/drivers/gl2.c"

/* The scripted shader backend. */
static struct gfx_fbo_scale scales[GFX_MAX_SHADERS];
static unsigned             num_passes;
static int                  feedback_pass; /* < 0: none */

static unsigned fake_num_shaders(void *data)
{
   (void)data;
   return num_passes;
}

static void fake_shader_scale(void *data, unsigned idx,
      struct gfx_fbo_scale *scale)
{
   (void)data;
   if (idx && idx <= num_passes)
      *scale = scales[idx - 1];
   else
      scale->flags &= ~FBO_SCALE_FLAG_VALID;
}

static bool fake_get_feedback_pass(void *data, unsigned *pass)
{
   (void)data;
   if (feedback_pass < 0)
      return false;
   *pass = (unsigned)feedback_pass;
   return true;
}

static bool fake_mipmap_input(void *data, unsigned idx)
{
   (void)data; (void)idx;
   return false;
}

static bool fake_filter_type(void *data, unsigned idx, bool *smooth)
{
   (void)data; (void)idx; (void)smooth;
   return false;
}

static enum gfx_wrap_type fake_wrap_type(void *data, unsigned idx)
{
   (void)data; (void)idx;
   return RARCH_WRAP_EDGE;
}

/* The scripted GL. */
static GLuint next_name = 1;

static void fake_gen_framebuffers(GLsizei n, GLuint *ids)
{
   GLsizei i;
   for (i = 0; i < n; i++)
      ids[i] = next_name++;
}

static void fake_delete_framebuffers(GLsizei n, const GLuint *ids)
{
   (void)n; (void)ids;
}

static void fake_bind_framebuffer(GLenum target, GLuint id)
{
   (void)target; (void)id;
}

static void fake_framebuffer_texture_2d(GLenum target, GLenum attachment,
      GLenum textarget, GLuint texture, GLint level)
{
   (void)target; (void)attachment; (void)textarget; (void)texture;
   (void)level;
}

static GLenum fake_check_framebuffer_status(GLenum target)
{
   (void)target;
   return GL_FRAMEBUFFER_COMPLETE;
}

/* GL 1.1 entry points the driver links directly.  Windows links them
 * from opengl32 through import pointers, so there the test supplies the
 * pointers, aimed at its own functions. */
#ifdef _WIN32
#define GL_FAKE(ret, name, params) \
   static ret APIENTRY fake_##name params; \
   ret (APIENTRY *__imp_##name) params = fake_##name; \
   static ret APIENTRY fake_##name params
#else
#define GL_FAKE(ret, name, params) ret name params
#endif

GL_FAKE(void, glPixelStorei, (GLenum pname, GLint param)) { (void)pname; (void)param; }
GL_FAKE(void, glReadBuffer, (GLenum mode)) { (void)mode; }
GL_FAKE(void, glReadPixels, (GLint x, GLint y, GLsizei width, GLsizei height,
      GLenum format, GLenum type, GLvoid *data))
{ (void)x; (void)y; (void)width; (void)height; (void)format; (void)type; (void)data; }
GL_FAKE(void, glBindTexture, (GLenum t, GLuint i)) { (void)t; (void)i; }
GL_FAKE(void, glBlendFunc, (GLenum a, GLenum b)) { (void)a; (void)b; }
GL_FAKE(void, glClear, (GLbitfield m)) { (void)m; }
GL_FAKE(void, glClearColor, (GLclampf r, GLclampf g, GLclampf b, GLclampf a))
{ (void)r; (void)g; (void)b; (void)a; }
GL_FAKE(void, glCopyTexSubImage2D, (GLenum a, GLint b, GLint c, GLint d,
      GLint e, GLint f, GLsizei g, GLsizei h))
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; }
GL_FAKE(void, glDeleteTextures, (GLsizei n, const GLuint *t)) { (void)n; (void)t; }
GL_FAKE(void, glDisable, (GLenum c)) { (void)c; }
GL_FAKE(void, glDrawArrays, (GLenum m, GLint f, GLsizei c)) { (void)m; (void)f; (void)c; }
GL_FAKE(void, glEnable, (GLenum c)) { (void)c; }
GL_FAKE(void, glFinish, (void)) { }
GL_FAKE(void, glFlush, (void)) { }
GL_FAKE(void, glGenTextures, (GLsizei n, GLuint *t))
{
   GLsizei i;
   for (i = 0; i < n; i++)
      t[i] = next_name++;
}
GL_FAKE(GLenum, glGetError, (void)) { return GL_NO_ERROR; }
GL_FAKE(void, glGetIntegerv, (GLenum p, GLint *v))
{ *v = (p == GL_MAX_TEXTURE_SIZE) ? 4096 : 0; }
GL_FAKE(const GLubyte *, glGetString, (GLenum n)) { (void)n; return (const GLubyte*)""; }
GL_FAKE(GLboolean, glIsEnabled, (GLenum c)) { (void)c; return GL_FALSE; }
GL_FAKE(void, glScissor, (GLint a, GLint b, GLsizei c, GLsizei d))
{ (void)a; (void)b; (void)c; (void)d; }
GL_FAKE(void, glTexImage2D, (GLenum a, GLint b, GLint c, GLsizei d, GLsizei e,
      GLint f, GLenum g, GLenum h, const GLvoid *i))
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; }
GL_FAKE(void, glTexParameteri, (GLenum a, GLenum b, GLint c)) { (void)a; (void)b; (void)c; }
GL_FAKE(void, glTexSubImage2D, (GLenum a, GLint b, GLint c, GLint d, GLsizei e,
      GLsizei f, GLenum g, GLenum h, const GLvoid *i))
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; }
GL_FAKE(void, glViewport, (GLint a, GLint b, GLsizei c, GLsizei d))
{ (void)a; (void)b; (void)c; (void)d; }
GL_FAKE(void, glGetTexLevelParameteriv, (GLenum a, GLint b, GLenum c, GLint *d))
{ (void)a; (void)b; (void)c; *d = 0; }

#ifdef _WIN32
/* The Windows build's context fallback, not reached by the chain. */
void video_driver_force_fallback(const char *driver) { (void)driver; }
#endif

static int fails;

#define CHECK(cond, what) do { \
   if (cond) printf("  [ok]   %s\n", what); \
   else { printf("  [FAIL] %s\n", what); fails++; } \
} while (0)

static void scale_source(struct gfx_fbo_scale *s, float f)
{
   memset(s, 0, sizeof(*s));
   s->flags   = FBO_SCALE_FLAG_VALID;
   s->type_x  = RARCH_SCALE_INPUT;
   s->type_y  = RARCH_SCALE_INPUT;
   s->scale_x = f;
   s->scale_y = f;
}

static void scale_viewport(struct gfx_fbo_scale *s, float fx, float fy)
{
   memset(s, 0, sizeof(*s));
   s->flags   = FBO_SCALE_FLAG_VALID;
   s->type_x  = RARCH_SCALE_VIEWPORT;
   s->type_y  = RARCH_SCALE_VIEWPORT;
   s->scale_x = fx;
   s->scale_y = fy;
}

/* Builds the chain for the scripted preset; the FBO count, or -1 for
 * no chain. */
static int run_chain(void)
{
   static shader_backend_t       backend;
   static gl2_renderchain_data_t chain;
   static gl2_t                  gl;

   memset(&backend, 0, sizeof(backend));
   backend.num_shaders       = fake_num_shaders;
   backend.shader_scale      = fake_shader_scale;
   backend.get_feedback_pass = fake_get_feedback_pass;
   backend.mipmap_input      = fake_mipmap_input;
   backend.filter_type       = fake_filter_type;
   backend.wrap_type         = fake_wrap_type;

   memset(&chain, 0, sizeof(chain));
   memset(&gl, 0, sizeof(gl));
   gl.shader           = &backend;
   gl.renderchain_data = &chain;
   gl.flags            = GL2_FLAG_HAVE_FBO;
   gl.tex_dims         = VIDEO_SCALE_PACK(256, 256);
   gl.video_dims       = VIDEO_SCALE_PACK(320, 240);
   gl.vp.dims          = VIDEO_SCALE_PACK(1280, 960);

   gl2_renderchain_init(&gl, &chain);

   if (!(gl.flags & GL2_FLAG_FBO_INITED))
      return -1;
   return chain.fbo_pass;
}

int main(void)
{
   __rglgen_glGenFramebuffers        = fake_gen_framebuffers;
   __rglgen_glDeleteFramebuffers     = fake_delete_framebuffers;
   __rglgen_glBindFramebuffer        = fake_bind_framebuffer;
   __rglgen_glFramebufferTexture2D   = fake_framebuffer_texture_2d;
   __rglgen_glCheckFramebufferStatus = fake_check_framebuffer_status;

   printf("two passes:\n");
   num_passes    = 2;
   feedback_pass = -1;
   scale_source(&scales[0], 2.0f);

   memset(&scales[1], 0, sizeof(scales[1]));
   CHECK(run_chain() == 1, "no scale on the last pass: drawn straight");

   scale_viewport(&scales[1], 1.0f, 1.0f);
   CHECK(run_chain() == 1, "viewport at 1.0: drawn straight");

   scale_viewport(&scales[1], 2.0f, 1.0f);
   CHECK(run_chain() == 2, "viewport at 2.0 across: an FBO");

   scale_viewport(&scales[1], 1.0f, 0.5f);
   CHECK(run_chain() == 2, "viewport at 0.5 down: an FBO");

   scale_viewport(&scales[1], 1.0f, 1.0f);
   scales[1].type_y = RARCH_SCALE_ABSOLUTE;
   scales[1].abs_y  = 240;
   CHECK(run_chain() == 2, "absolute down: an FBO");

   scale_source(&scales[1], 1.0f);
   CHECK(run_chain() == 2, "source at 1.0: an FBO");

   scale_viewport(&scales[1], 1.0f, 1.0f);
   scales[1].flags |= FBO_SCALE_FLAG_FP_FBO;
   CHECK(run_chain() == 2, "float_framebuffer: an FBO");

   scale_viewport(&scales[1], 1.0f, 1.0f);
   scales[1].flags |= FBO_SCALE_FLAG_SRGB_FBO;
   CHECK(run_chain() == 2, "srgb_framebuffer: an FBO");

   scale_viewport(&scales[1], 1.0f, 1.0f);
   feedback_pass = 1;
   CHECK(run_chain() == 2, "feedback of the last pass: an FBO");

   feedback_pass = 0;
   CHECK(run_chain() == 1, "feedback of an earlier pass: drawn straight");
   feedback_pass = -1;

   printf("one pass:\n");
   num_passes = 1;

   memset(&scales[0], 0, sizeof(scales[0]));
   CHECK(run_chain() == -1, "no scale: no chain");

   scale_viewport(&scales[0], 1.0f, 1.0f);
   CHECK(run_chain() == -1, "viewport at 1.0: no chain");

   scale_source(&scales[0], 2.0f);
   CHECK(run_chain() == 1, "source at 2.0: an FBO");

   scale_viewport(&scales[0], 1.0f, 1.0f);
   feedback_pass = 0;
   CHECK(run_chain() == 1, "feedback of the pass: an FBO");

   if (fails)
   {
      printf("%d check(s) failed\n", fails);
      return 1;
   }
   printf("all checks passed\n");
   return 0;
}
