/* A scripted GL and shader backend for the GL drivers' GLSL/Cg render
 * chains, and the cases each chain must decide alike: the last pass of a
 * preset gets an FBO of its own, copied to the backbuffer by the stock
 * pass after it, only when drawing it straight there would give a
 * different image - a scale other than the viewport at 1.0, an FBO
 * format, or the feedback the preset asks of it.
 *
 * Included by each test after the driver source it includes. */

#ifndef GL_LAST_PASS_FAKES_H
#define GL_LAST_PASS_FAKES_H

#include "../gl2_pbo_readback/gl11_fakes.h"

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

static unsigned fake_get_prev_textures(void *data)
{
   (void)data;
   return 0;
}

static enum gfx_wrap_type fake_wrap_type(void *data, unsigned idx)
{
   (void)data; (void)idx;
   return RARCH_WRAP_EDGE;
}

/* The scripted GL. */
static void fake_gen_framebuffers(GLsizei n, GLuint *ids)
{
   GLsizei i;
   for (i = 0; i < n; i++)
      ids[i] = gl11_next_name++;
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

/* The scripted FBO calls, made through the GL loader's pointers. */
static void gl_fakes_install(void)
{
   __rglgen_glGenFramebuffers        = fake_gen_framebuffers;
   __rglgen_glDeleteFramebuffers     = fake_delete_framebuffers;
   __rglgen_glBindFramebuffer        = fake_bind_framebuffer;
   __rglgen_glFramebufferTexture2D   = fake_framebuffer_texture_2d;
   __rglgen_glCheckFramebufferStatus = fake_check_framebuffer_status;
}

static void backend_init(shader_backend_t *backend)
{
   memset(backend, 0, sizeof(*backend));
   backend->num_shaders       = fake_num_shaders;
   backend->shader_scale      = fake_shader_scale;
   backend->get_feedback_pass = fake_get_feedback_pass;
   backend->mipmap_input      = fake_mipmap_input;
   backend->filter_type       = fake_filter_type;
   backend->wrap_type         = fake_wrap_type;
   backend->get_prev_textures = fake_get_prev_textures;
}

/* Each case through @run_chain, which builds the chain for the scripted
 * preset and gives its FBO count; @no_chain is what it gives for a
 * one-pass preset drawn straight to the backbuffer. */
static int run_cases(int (*run_chain)(void), int no_chain)
{
   gl_fakes_install();

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
   CHECK(run_chain() == no_chain, "no scale: no chain");

   scale_viewport(&scales[0], 1.0f, 1.0f);
   CHECK(run_chain() == no_chain, "viewport at 1.0: no chain");

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

#endif
