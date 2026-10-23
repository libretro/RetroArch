/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_android_capture_draw.h"
static unsigned shaders, programs, arrays, draws, source_calls;
static unsigned fail_shader, fail_compile, fail_uniform, uniform_calls;
static bool fail_program, fail_link, fail_array;
static GLenum next_error;
static GLint selected_mode, selected_flip;
static GLuint GL_APIENTRY create_shader(GLenum type)
{
   source_calls++;
   assert(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);
   if (source_calls == fail_shader)
      return 0;
   shaders++;
   return source_calls;
}
static void GL_APIENTRY source(
      GLuint s, GLsizei count, const GLchar *const *text, const GLint *lengths)
{
   GLsizei i;
   assert(s && !lengths && (count == 1 || count == 4));
   assert(strstr(text[0], "#version 300 es"));
   for (i = 0; i < count; i++)
      assert(text[i] && strlen(text[i]) <= 509);
}
static void GL_APIENTRY compile(GLuint s)
{
   assert(s);
}
static void GL_APIENTRY shader_status(GLuint s, GLenum type, GLint *v)
{
   assert(s && type == GL_COMPILE_STATUS);
   *v = s != fail_compile;
}
static void GL_APIENTRY delete_shader(GLuint s)
{
   assert(s && shaders);
   shaders--;
}
static GLuint GL_APIENTRY program(void)
{
   if (fail_program)
      return 0;
   programs++;
   return 10;
}
static void GL_APIENTRY attach(GLuint p, GLuint s)
{
   assert(p == 10 && s);
}
static void GL_APIENTRY link(GLuint p)
{
   assert(p == 10);
}
static void GL_APIENTRY program_status(GLuint p, GLenum type, GLint *v)
{
   assert(p == 10 && type == GL_LINK_STATUS);
   *v = !fail_link;
}
static void GL_APIENTRY delete_program(GLuint p)
{
   assert(p == 10 && programs);
   programs--;
}
static GLint GL_APIENTRY location(GLuint p, const GLchar *name)
{
   assert(p == 10);
   uniform_calls++;
   if (uniform_calls == fail_uniform)
      return -1;
   if (!strcmp(name, "image"))
      return 0;
   if (!strcmp(name, "mode"))
      return 1;
   assert(!strcmp(name, "flip_y"));
   return 2;
}
static void GL_APIENTRY gen(GLsizei n, GLuint *a)
{
   assert(n == 1);
   *a = fail_array ? 0 : 20;
   arrays += !fail_array;
}
static void GL_APIENTRY del(GLsizei n, const GLuint *a)
{
   assert(n == 1 && *a == 20 && arrays);
   arrays--;
}
static void GL_APIENTRY bind(GLuint a)
{
   assert(a == 20);
}
static void GL_APIENTRY use(GLuint p)
{
   assert(p == 10);
}
static void GL_APIENTRY uniform(GLint at, GLint v)
{
   if (at == 0)
      assert(v == 0);
   else if (at == 1)
   {
      assert(v == selected_mode);
   }
   else
   {
      assert(at == 2 && v == selected_flip);
   }
}
static void GL_APIENTRY active(GLenum t)
{
   assert(t == GL_TEXTURE0);
}
static void GL_APIENTRY texture(GLenum t, GLuint id)
{
   assert(t == GL_TEXTURE_2D && id == 30);
}
static void GL_APIENTRY framebuffer(GLenum t, GLuint id)
{
   assert(t == GL_FRAMEBUFFER && id == 0);
}
static void GL_APIENTRY viewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
   assert(!x && !y && w == 320 && h == 240);
}
static void GL_APIENTRY disable(GLenum s)
{
   assert(s == GL_BLEND || s == GL_DEPTH_TEST || s == GL_STENCIL_TEST
         || s == GL_SCISSOR_TEST || s == GL_CULL_FACE || s == GL_DITHER
         || s == GL_RASTERIZER_DISCARD || s == GL_SAMPLE_ALPHA_TO_COVERAGE
         || s == GL_SAMPLE_COVERAGE);
}
static void GL_APIENTRY mask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
   assert(r && g && b && a);
}
static void GL_APIENTRY draw(GLenum t, GLint first, GLsizei n)
{
   assert(t == GL_TRIANGLES && !first && n == 3);
   draws++;
}
static GLenum GL_APIENTRY error(void)
{
   GLenum result = next_error;
   next_error = GL_NO_ERROR;
   return result;
}
int main(void)
{
   record_android_capture_draw_ops_t ops;
   record_android_capture_draw_t *d;
   unsigned mode, flip, i;
   memset(&ops, 0, sizeof(ops));
   ops.create_shader = create_shader;
   ops.shader_source = source;
   ops.compile_shader = compile;
   ops.shader_status = shader_status;
   ops.delete_shader = delete_shader;
   ops.create_program = program;
   ops.attach_shader = attach;
   ops.link_program = link;
   ops.program_status = program_status;
   ops.delete_program = delete_program;
   ops.uniform_location = location;
   ops.gen_arrays = gen;
   ops.delete_arrays = del;
   ops.bind_array = bind;
   ops.use_program = use;
   ops.uniform = uniform;
   ops.active_texture = active;
   ops.bind_texture = texture;
   ops.bind_framebuffer = framebuffer;
   ops.viewport = viewport;
   ops.disable = disable;
   ops.color_mask = mask;
   ops.draw = draw;
   ops.error = error;
   for (mode = 0; mode < 3; mode++)
      for (flip = 0; flip < 2; flip++)
      {
         selected_mode = (GLint)mode;
         selected_flip = (GLint)flip;
         d = record_android_capture_draw_new(&ops, 320, 240, mode, flip != 0);
         assert(d && !shaders && programs == 1 && arrays == 1);
         assert(!record_android_capture_draw_frame(d, 0));
         for (i = 0; i < 1000; i++)
            assert(record_android_capture_draw_frame(d, 30));
         next_error = GL_INVALID_OPERATION;
         assert(!record_android_capture_draw_frame(d, 30));
         i = draws;
         assert(!record_android_capture_draw_frame(d, 30) && draws == i);
         record_android_capture_draw_free(d, false);
         assert(!programs && !arrays);
      }
   for (i = 0; i < 10; i++)
   {
      source_calls = uniform_calls = 0;
      fail_shader = i < 2 ? i + 1 : 0;
      fail_compile = i >= 2 && i < 4 ? i - 1 : 0;
      fail_program = i == 4;
      fail_link = i == 5;
      fail_uniform = i >= 6 && i < 9 ? i - 5 : 0;
      fail_array = i == 9;
      assert(!record_android_capture_draw_new(&ops, 320, 240, 0, false));
      assert(!shaders && !programs && !arrays);
   }
   fail_shader = fail_compile = fail_uniform = 0;
   fail_program = fail_link = fail_array = false;
   assert(!record_android_capture_draw_new(&ops, 320, 240, 3, false));
   d = record_android_capture_draw_new(&ops, 320, 240, 0, false);
   assert(d);
   programs = arrays = 0;
   record_android_capture_draw_free(d, true);
   puts("PASS: capture draw state, cold GPU resource faults and sticky draw "
        "failure");
   return 0;
}
