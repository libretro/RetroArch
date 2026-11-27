/* The link test core's two-part picture in OpenGL: cleared rectangles
 * through the entry points the frontend gives, so no GL header or
 * library */

#include <string.h>

#include <libretro.h>

#include "link_test_render.h"

#ifdef _WIN32
#define LT_GL_API __stdcall
#else
#define LT_GL_API
#endif

#define LT_GL_COLOR_BUFFER_BIT     0x4000
#define LT_GL_SCISSOR_TEST         0x0C11
#define LT_GL_PACK_ROW_LENGTH      0x0D02
#define LT_GL_PACK_SKIP_ROWS       0x0D03
#define LT_GL_PACK_SKIP_PIXELS     0x0D04
#define LT_GL_PACK_ALIGNMENT       0x0D05
#define LT_GL_FRAMEBUFFER          0x8D40
#define LT_GL_FRAMEBUFFER_COMPLETE 0x8CD5

enum
{
   LT_GL_BIND_FRAMEBUFFER = 0,
   LT_GL_CHECK_FRAMEBUFFER_STATUS,
   LT_GL_ENABLE,
   LT_GL_DISABLE,
   LT_GL_SCISSOR,
   LT_GL_CLEAR_COLOR,
   LT_GL_CLEAR,
   LT_GL_PIXEL_STOREI,
   LT_GL_GET_INTEGERV,
   LT_GL_ENTRIES
};

static const char *const lt_gl_names[LT_GL_ENTRIES] = {
   "glBindFramebuffer", "glCheckFramebufferStatus", "glEnable", "glDisable",
   "glScissor", "glClearColor", "glClear", "glPixelStorei", "glGetIntegerv"
};

typedef void (LT_GL_API *lt_gl_bind_framebuffer_t)(unsigned target, unsigned framebuffer);
typedef unsigned (LT_GL_API *lt_gl_check_framebuffer_status_t)(unsigned target);
typedef void (LT_GL_API *lt_gl_enable_t)(unsigned cap); /* and glDisable */
typedef void (LT_GL_API *lt_gl_scissor_t)(int x, int y, int width, int height);
typedef void (LT_GL_API *lt_gl_clear_color_t)(float r, float g, float b, float a);
typedef void (LT_GL_API *lt_gl_clear_t)(unsigned mask);
typedef void (LT_GL_API *lt_gl_pixel_storei_t)(unsigned name, int value);
typedef void (LT_GL_API *lt_gl_get_integerv_t)(unsigned name, int *value);

/* The pixel-pack state of a dirty context: rows longer than any picture
 * and a start that is not its first pixel */
static const struct
{
   unsigned name;
   int value;
} lt_gl_dirty[] = {
   { LT_GL_PACK_ALIGNMENT,   8   },
   { LT_GL_PACK_ROW_LENGTH,  200 },
   { LT_GL_PACK_SKIP_ROWS,   1   },
   { LT_GL_PACK_SKIP_PIXELS, 1   }
};

static struct
{
   const struct lt_render_host *host;
   struct retro_hw_render_callback hw;
   retro_proc_address_t entry[LT_GL_ENTRIES]; /* all set, or the first NULL */
   unsigned width, height;
   bool dirty;                                /* link_test_gl_state */
   bool late;                                 /* link_test_gl_ask run, not yet asked */
   bool refused;                              /* asked late and refused: dupes, no error */
   bool drawing;                              /* this frame's picture was begun */
   bool said_no_context;                      /* logged; context_reset clears it */
} lt_gl;

/* host->fail with a line in two pieces, cut to fit */
static void lt_gl_fail(const char *what, const char *more)
{
   char line[64];
   line[0] = '\0';
   strncat(line, what, sizeof(line) - 1);
   strncat(line, more, sizeof(line) - 1 - strlen(line));
   lt_gl.host->fail(line, 0, 0);
}

/* A dirty context still has the core's pixel-pack state: the frontend
 * that read a picture by its own put the core's back, and the context
 * current is the core's. Every context has the alignment, so its answer
 * is not taken on trust; one that refused another name leaves that value
 * as it is. */
static void lt_gl_check_dirty(void)
{
   lt_gl_get_integerv_t get = (lt_gl_get_integerv_t)lt_gl.entry[LT_GL_GET_INTEGERV];
   unsigned i;
   for (i = 0; i < sizeof(lt_gl_dirty) / sizeof(*lt_gl_dirty); i++)
   {
      int value = lt_gl_dirty[i].name == LT_GL_PACK_ALIGNMENT ? 0 : lt_gl_dirty[i].value;
      get(lt_gl_dirty[i].name, &value);
      if (value != lt_gl_dirty[i].value)
         lt_gl.host->fail("pixel-pack state that is not the core's",
               lt_gl_dirty[i].name, (uint64_t)value);
   }
}

static void RETRO_CALLCONV lt_gl_context_reset(void)
{
   unsigned i;
   lt_gl.said_no_context = false;
   for (i = 0; i < LT_GL_ENTRIES; i++)
   {
      if (!(lt_gl.entry[i] = lt_gl.hw.get_proc_address(lt_gl_names[i])))
      {
         lt_gl.entry[0] = NULL;
         lt_gl_fail("no entry point ", lt_gl_names[i]);
         return;
      }
   }
   if (lt_gl.dirty)
   {
      /* What a frontend must not read the picture by: that pixel-pack
       * state (an OpenGL ES 2 context may refuse all of it but the
       * alignment), and an error, of a name glPixelStorei does not know,
       * that nothing reads */
      lt_gl_pixel_storei_t store = (lt_gl_pixel_storei_t)lt_gl.entry[LT_GL_PIXEL_STOREI];
      for (i = 0; i < sizeof(lt_gl_dirty) / sizeof(*lt_gl_dirty); i++)
         store(lt_gl_dirty[i].name, lt_gl_dirty[i].value);
      store(0, 0);
   }
}

static void RETRO_CALLCONV lt_gl_context_destroy(void)
{
   if (lt_gl.dirty && lt_gl.entry[0])
      lt_gl_check_dirty();
   lt_gl.entry[0] = NULL;
}

static bool lt_gl_load(const struct lt_render_host *host)
{
   static const char *const contexts[] = { LT_GL_CONTEXT_DEFAULT, "compat", "gles2", "core99" };
   static const char *const origins[]  = { LT_GL_ORIGIN_DEFAULT, "bottom" };
   static const char *const states[]   = { LT_GL_STATE_DEFAULT, "dirty" };
   static const char *const asks[]     = { LT_GL_ASK_DEFAULT, "run" };
   static const struct
   {
      enum retro_hw_context_type type;
      unsigned major, minor;
      bool depth, stencil; /* in the frontend's framebuffer, which must be complete */
   } asked[] = {
      { RETRO_HW_CONTEXT_OPENGL_CORE, 3, 3, true,  true  },
      { RETRO_HW_CONTEXT_OPENGL,      0, 0, true,  false },
      { RETRO_HW_CONTEXT_OPENGLES2,   0, 0, false, false },
      { RETRO_HW_CONTEXT_OPENGL_CORE, 9, 9, true,  true  } /* a version no driver has */
   };
   unsigned context;
   memset(&lt_gl, 0, sizeof(lt_gl));
   lt_gl.host = host;
   context    = lt_render_option("link_test_gl_context", contexts,
         sizeof(contexts) / sizeof(*contexts));
   lt_gl.hw.context_type       = asked[context].type;
   lt_gl.hw.version_major      = asked[context].major;
   lt_gl.hw.version_minor      = asked[context].minor;
   lt_gl.hw.depth              = asked[context].depth;
   lt_gl.hw.stencil            = asked[context].stencil;
   lt_gl.hw.context_reset      = lt_gl_context_reset;
   lt_gl.hw.context_destroy    = lt_gl_context_destroy;
   lt_gl.hw.bottom_left_origin = lt_render_option("link_test_gl_origin", origins,
         sizeof(origins) / sizeof(*origins)) == 1;
   lt_gl.dirty                 = lt_render_option("link_test_gl_state", states,
         sizeof(states) / sizeof(*states)) == 1;
   lt_gl.late                  = lt_render_option("link_test_gl_ask", asks,
         sizeof(asks) / sizeof(*asks)) == 1;
   if (lt_gl.late || host->environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &lt_gl.hw))
      return true;
   host->fail("SET_HW_RENDER refused", lt_gl.hw.context_type, lt_gl.hw.version_major);
   return false;
}

/* Rows count from the framebuffer's first */
static void lt_gl_fill(unsigned x, unsigned y, unsigned width, unsigned height, uint32_t rgb)
{
   ((lt_gl_scissor_t)lt_gl.entry[LT_GL_SCISSOR])((int)x, (int)y, (int)width, (int)height);
   ((lt_gl_clear_color_t)lt_gl.entry[LT_GL_CLEAR_COLOR])(
         (float)((rgb >> 16) & 0xFF) / 255.0f, (float)((rgb >> 8) & 0xFF) / 255.0f,
         (float)(rgb & 0xFF) / 255.0f, 1.0f);
   ((lt_gl_clear_t)lt_gl.entry[LT_GL_CLEAR])(LT_GL_COLOR_BUFFER_BIT);
}

/* The scissor test is on only while the core draws: the frontend's
 * context has it off */
static void lt_gl_scissor(bool on)
{
   ((lt_gl_enable_t)lt_gl.entry[on ? LT_GL_ENABLE : LT_GL_DISABLE])(LT_GL_SCISSOR_TEST);
}

static void lt_gl_begin(unsigned width, unsigned height)
{
   unsigned status;
   lt_gl.width   = width;
   lt_gl.height  = height;
   lt_gl.drawing = false;
   if (lt_gl.late)
   {
      /* Refused, the core says nothing: the frontend must fail the run */
      lt_gl.late    = false;
      lt_gl.refused = !lt_gl.host->environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &lt_gl.hw);
   }
   if (lt_gl.refused)
      return;
   if (!lt_render_tell(width, height))
      lt_gl.host->fail("SET_SYSTEM_AV_INFO refused", width, height);
   if (!lt_gl.entry[0])
   {
      if (!lt_gl.said_no_context)
         lt_gl.host->fail("a frame with no OpenGL context", 0, 0);
      lt_gl.said_no_context = true;
      return;
   }
   if (lt_gl.dirty)
      lt_gl_check_dirty();
   ((lt_gl_bind_framebuffer_t)lt_gl.entry[LT_GL_BIND_FRAMEBUFFER])(LT_GL_FRAMEBUFFER,
         (unsigned)lt_gl.hw.get_current_framebuffer());
   status = ((lt_gl_check_framebuffer_status_t)lt_gl.entry[LT_GL_CHECK_FRAMEBUFFER_STATUS])(
         LT_GL_FRAMEBUFFER);
   if (status != LT_GL_FRAMEBUFFER_COMPLETE)
   {
      lt_gl.host->fail("the framebuffer is not complete", status, 0);
      return;
   }
   lt_gl_scissor(true);
   lt_gl_fill(0, 0, width, height, 0);
   lt_gl_scissor(false);
   lt_gl.drawing = true;
}

static void lt_gl_part(unsigned part, uint64_t value)
{
   unsigned i;
   if (!lt_gl.drawing)
      return;
   lt_gl_scissor(true);
   for (i = 0; i < LT_RENDER_CELLS; i++)
   {
      struct lt_render_cell c;
      lt_render_cell(lt_gl.width, lt_gl.height, part, value, i, &c);
      /* With a top origin the framebuffer's first row is the picture's top */
      lt_gl_fill(c.x, lt_gl.hw.bottom_left_origin ? lt_gl.height - c.y - c.height : c.y,
            c.width, c.height, c.rgb);
   }
   lt_gl_scissor(false);
}

static void lt_gl_present(retro_video_refresh_t video, bool dupe)
{
   video(dupe || !lt_gl.drawing ? NULL : RETRO_HW_FRAME_BUFFER_VALID,
         lt_gl.width, lt_gl.height, 0);
}

/* The core makes no object in the context, so it has none to delete */
static void lt_gl_unload(void)
{
}

const struct lt_render lt_render_gl = {
   lt_gl_load, lt_gl_begin, lt_gl_part, lt_gl_present, lt_gl_unload
};
