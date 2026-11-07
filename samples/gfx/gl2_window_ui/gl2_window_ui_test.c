/* The gl2 driver's UI layers in another window, against a scripted GL
 * and window context. Laid out at the window's size, the menu is drawn
 * on that window's back buffer, with no offscreen image made and the
 * main window's viewport left as it was. Laid out at another size, or
 * with the window not drawn after all, the image carries it, and the
 * menu's frame still runs once. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/drivers/gl2.c"

extern unsigned stub_menu_frames;
extern unsigned stub_menu_bound;
extern unsigned stub_bound_window;

/* The scripted GL: framebuffers counted, the rest accepted */
static unsigned fbos_made;

static void fake_gen_framebuffers(GLsizei n, GLuint *ids)
{
   GLsizei i;
   for (i = 0; i < n; i++)
      ids[i] = 100 + ++fbos_made;
}
static void fake_delete_framebuffers(GLsizei n, const GLuint *ids)
{ (void)n; (void)ids; }
static void fake_bind_framebuffer(GLenum t, GLuint id) { (void)t; (void)id; }
static void fake_framebuffer_texture_2d(GLenum a, GLenum b, GLenum c,
      GLuint d, GLint e) { (void)a; (void)b; (void)c; (void)d; (void)e; }
static GLenum fake_check_framebuffer_status(GLenum t)
{ (void)t; return GL_FRAMEBUFFER_COMPLETE; }
static void fake_use_program(GLuint p) { (void)p; }
static void fake_uniform_matrix_4fv(GLint l, GLsizei c, GLboolean t,
      const GLfloat *v) { (void)l; (void)c; (void)t; (void)v; }
static void fake_uniform_1i(GLint l, GLint v) { (void)l; (void)v; }
static void fake_bind_buffer(GLenum t, GLuint id) { (void)t; (void)id; }
static void fake_buffer_data(GLenum t, GLsizeiptr s, const void *d,
      GLenum u) { (void)t; (void)s; (void)d; (void)u; }
static void fake_vertex_attrib_array(GLuint i) { (void)i; }
static void fake_vertex_attrib_pointer(GLuint i, GLint s, GLenum t,
      GLboolean n, GLsizei st, const void *p)
{ (void)i; (void)s; (void)t; (void)n; (void)st; (void)p; }
static void fake_blend_equation(GLenum m) { (void)m; }
static void fake_blend_func_separate(GLenum a, GLenum b, GLenum c,
      GLenum d) { (void)a; (void)b; (void)c; (void)d; }

#include "../gl2_pbo_readback/gl11_fakes.h"

/* The scripted window context: what is bound, and what was presented */
static unsigned presents;

static bool fake_bind_gl(void *owner, video_window_native_t *window)
{
   (void)owner;
   stub_bound_window = window ? (unsigned)(uintptr_t)window->state : 0;
   return true;
}
static bool fake_present_gl(void *owner, video_window_native_t *window)
{
   (void)owner; (void)window;
   presents++;
   return true;
}
static void fake_destroy(void *owner, video_window_native_t *window)
{ (void)owner; (void)window; }

static gfx_ctx_window_driver_t test_windows;
static gfx_ctx_driver_t        test_ctx;

static int fails;

#define CHECK(cond, what) do { \
   if (cond) printf("  [ok]   %s\n", what); \
   else { printf("  [FAIL] %s\n", what); fails++; } \
} while (0)

#define WIN_DIMS   VIDEO_SCALE_PACK(320, 240)
#define OTHER_DIMS VIDEO_SCALE_PACK(400, 300)
#define MAIN_DIMS  VIDEO_SCALE_PACK(640, 480)

static gl2_t                 gl;
static video_frame_info_t    vi;
static video_windows_frame_t frame;

/* Window 1 up at WIN_DIMS, the menu open and laid out for it at @dims */
static void setup(unsigned dims, bool wait)
{
   struct gl2_window *win;
   gl2_vv_target_drop(&gl, &gl.windows.ui_target, &gl.windows.ui);
   memset(&gl, 0, sizeof(gl));
   memset(&vi, 0, sizeof(vi));
   memset(&frame, 0, sizeof(frame));
   gl.ctx_driver               = &test_ctx;
   gl.views.tried              = true;
   gl.flags                   |= GL2_FLAG_MENU_TEXTURE_ENABLE
      | GL2_FLAG_HAVE_FBO;
   gl.vp.pos                   = VIDEO_POS_PACK(7, 9);
   gl.vp.dims                  = VIDEO_SCALE_PACK(500, 400);
   gl.windows.held.live        = 1u << 1;
   win                         = &gl.windows.win[1];
   win->native.state           = (void*)(uintptr_t)1;
   win->native.configured      = true;
   win->native.presentable     = true;
   win->native.dims            = WIN_DIMS;
   frame.ui[VIDEO_WINDOW_UI_MENU].window = 1;
   frame.ui[VIDEO_WINDOW_UI_MENU].dims   = dims;
   frame.ui[VIDEO_WINDOW_UI_MENU].wait   = wait;
   frame.window_ui[1]          = VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU);
   vi.windows                  = &frame;
   vi.dims                     = MAIN_DIMS;
   vi.menu_st_flags            = MENU_ST_FLAG_ALIVE;
   fbos_made                   = 0;
   presents                    = 0;
   stub_menu_frames            = 0;
   stub_menu_bound             = 0;
   stub_bound_window           = 0;
}

int main(void)
{
   unsigned away, draw;
   bool in_main;
   struct video_viewport vp;

   __rglgen_glGenFramebuffers         = fake_gen_framebuffers;
   __rglgen_glDeleteFramebuffers      = fake_delete_framebuffers;
   __rglgen_glBindFramebuffer         = fake_bind_framebuffer;
   __rglgen_glFramebufferTexture2D    = fake_framebuffer_texture_2d;
   __rglgen_glCheckFramebufferStatus  = fake_check_framebuffer_status;
   __rglgen_glUseProgram              = fake_use_program;
   __rglgen_glUniformMatrix4fv        = fake_uniform_matrix_4fv;
   __rglgen_glUniform1i               = fake_uniform_1i;
   __rglgen_glBindBuffer              = fake_bind_buffer;
   __rglgen_glBufferData              = fake_buffer_data;
   __rglgen_glEnableVertexAttribArray = fake_vertex_attrib_array;
   __rglgen_glDisableVertexAttribArray = fake_vertex_attrib_array;
   __rglgen_glVertexAttribPointer     = fake_vertex_attrib_pointer;
   __rglgen_glBlendEquation           = fake_blend_equation;
   __rglgen_glBlendFuncSeparate       = fake_blend_func_separate;
   test_windows.bind_gl               = fake_bind_gl;
   test_windows.present_gl            = fake_present_gl;
   test_windows.destroy               = fake_destroy;
   test_ctx.windows                   = &test_windows;

   printf("[gl2_window_ui] menu laid out at its window's size\n");
   setup(WIN_DIMS, false);
   away = gl2_window_ui_prepare(&gl, &vi, NULL, &draw, &in_main);
   CHECK(away == VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU),
         "the menu leaves the main window");
   CHECK(!draw && !in_main, "no image is drawn for it");
   CHECK(gl.windows.ui_direct == VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU),
         "it goes straight to the window");
   CHECK(!fbos_made && !gl.windows.ui.fbo, "no offscreen image is made");
   vp = gl.vp;
   CHECK(gl2_window_draw(&gl, &vi, 1, true), "the window is drawn");
   CHECK(stub_menu_frames == 1, "the menu's frame ran once");
   CHECK(stub_menu_bound == 1, "with the window bound");
   CHECK(presents == 1 && !stub_bound_window,
         "the window presented, the main window bound again");
   CHECK(!memcmp(&vp, &gl.vp, sizeof(vp)),
         "the main window's viewport is as it was");
   CHECK(!gl.windows.ui_window && !gl.windows.ui_direct,
         "nothing is left for a fallback");
   CHECK(!fbos_made, "still no offscreen image");

   printf("[gl2_window_ui] menu laid out at another size\n");
   setup(OTHER_DIMS, false);
   away = gl2_window_ui_prepare(&gl, &vi, NULL, &draw, &in_main);
   CHECK(draw == VIDEO_WINDOW_UI_BIT(VIDEO_WINDOW_UI_MENU) && !in_main,
         "the image carries it");
   CHECK(!gl.windows.ui_direct, "not drawn straight to the window");
   CHECK(fbos_made == 1, "the image is made");

   printf("[gl2_window_ui] window not drawn after all\n");
   setup(WIN_DIMS, false);
   gl2_window_ui_prepare(&gl, &vi, NULL, &draw, &in_main);
   CHECK(gl2_window_ui_fallback(&gl, &vi),
         "the main window shows the menu instead");
   CHECK(stub_menu_frames == 1 && !stub_menu_bound,
         "its frame ran once, on the main window's image");
   CHECK(fbos_made == 1, "the image is made only now");
   CHECK(!gl.windows.ui_direct, "the direct layers are spent");

   printf("[gl2_window_ui] window not drawn, its layer waiting\n");
   setup(WIN_DIMS, true);
   gl2_window_ui_prepare(&gl, &vi, NULL, &draw, &in_main);
   CHECK(!gl2_window_ui_fallback(&gl, &vi), "nothing goes to the main window");
   CHECK(!stub_menu_frames && !fbos_made, "nor is anything drawn");

   gl2_vv_target_drop(&gl, &gl.windows.ui_target, &gl.windows.ui);
   if (fails)
   {
      printf("[gl2_window_ui] %d failure(s)\n", fails);
      return 1;
   }
   printf("[gl2_window_ui] all passed\n");
   return 0;
}
