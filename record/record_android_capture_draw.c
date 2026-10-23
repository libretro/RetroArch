/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_android_capture_draw.h"
#include "shaders/capture_gl.h"
#include <stdlib.h>

struct record_android_capture_draw
{
   record_android_capture_draw_ops_t ops;
   GLuint program, array;
   GLint image, mode_location, flip_location;
   unsigned width, height, mode;
   bool flip, failed;
};
void record_android_capture_draw_free(
      record_android_capture_draw_t *d, bool abandon)
{
   if (!d)
      return;
   if (!abandon)
   {
      if (d->array)
         d->ops.delete_arrays(1, &d->array);
      if (d->program)
         d->ops.delete_program(d->program);
   }
   free(d);
}
static GLuint record_capture_draw_shader(
      const record_android_capture_draw_ops_t *ops, GLenum type,
      const char *const *text, GLsizei count)
{
   GLint status = 0;
   GLuint shader = ops->create_shader(type);
   if (!shader)
      return 0;
   ops->shader_source(shader, count, text, NULL);
   ops->compile_shader(shader);
   ops->shader_status(shader, GL_COMPILE_STATUS, &status);
   if (status)
      return shader;
   ops->delete_shader(shader);
   return 0;
}
record_android_capture_draw_t *record_android_capture_draw_new(
      const record_android_capture_draw_ops_t *ops, unsigned width,
      unsigned height, unsigned mode, bool flip_y)
{
   record_android_capture_draw_t *d;
   GLuint vertex, fragment;
   GLint status = 0;
   if (!ops || !width || !height || width > 16384 || height > 16384 || mode > 2
         || !ops->create_shader || !ops->shader_source || !ops->compile_shader
         || !ops->shader_status || !ops->delete_shader || !ops->create_program
         || !ops->attach_shader || !ops->link_program || !ops->program_status
         || !ops->delete_program || !ops->uniform_location || !ops->gen_arrays
         || !ops->delete_arrays || !ops->bind_array || !ops->use_program
         || !ops->uniform || !ops->active_texture || !ops->bind_texture
         || !ops->bind_framebuffer || !ops->viewport || !ops->disable
         || !ops->color_mask || !ops->draw || !ops->error
         || ops->error() != GL_NO_ERROR)
      return NULL;
   vertex = record_capture_draw_shader(
         ops, GL_VERTEX_SHADER, record_capture_gl_vertex, 1);
   if (!vertex)
      return NULL;
   fragment = record_capture_draw_shader(
         ops, GL_FRAGMENT_SHADER, record_capture_gl_fragment, 4);
   if (!fragment)
   {
      ops->delete_shader(vertex);
      return NULL;
   }
   d = (record_android_capture_draw_t *)calloc(1, sizeof(*d));
   if (!d)
   {
      ops->delete_shader(vertex);
      ops->delete_shader(fragment);
      return NULL;
   }
   d->ops = *ops;
   d->width = width;
   d->height = height;
   d->mode = mode;
   d->flip = flip_y;
   d->program = ops->create_program();
   if (d->program)
   {
      ops->attach_shader(d->program, vertex);
      ops->attach_shader(d->program, fragment);
      ops->link_program(d->program);
      ops->program_status(d->program, GL_LINK_STATUS, &status);
   }
   ops->delete_shader(vertex);
   ops->delete_shader(fragment);
   if (status)
   {
      d->image = ops->uniform_location(d->program, "image");
      d->mode_location = ops->uniform_location(d->program, "mode");
      d->flip_location = ops->uniform_location(d->program, "flip_y");
      ops->gen_arrays(1, &d->array);
      if (d->image >= 0 && d->mode_location >= 0 && d->flip_location >= 0
            && d->array && ops->error() == GL_NO_ERROR)
         return d;
   }
   record_android_capture_draw_free(d, false);
   return NULL;
}
bool record_android_capture_draw_frame(
      record_android_capture_draw_t *d, GLuint texture)
{
   static const GLenum disabled[] = {GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST,
         GL_SCISSOR_TEST, GL_CULL_FACE, GL_DITHER, GL_RASTERIZER_DISCARD,
         GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_COVERAGE};
   unsigned i;
   if (!d || d->failed || !texture)
      return false;
   d->ops.bind_framebuffer(GL_FRAMEBUFFER, 0);
   d->ops.viewport(0, 0, (GLsizei)d->width, (GLsizei)d->height);
   for (i = 0; i < sizeof(disabled) / sizeof(disabled[0]); i++)
      d->ops.disable(disabled[i]);
   d->ops.color_mask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
   d->ops.use_program(d->program);
   d->ops.uniform(d->image, 0);
   d->ops.uniform(d->mode_location, (GLint)d->mode);
   d->ops.uniform(d->flip_location, d->flip ? 1 : 0);
   d->ops.active_texture(GL_TEXTURE0);
   d->ops.bind_texture(GL_TEXTURE_2D, texture);
   d->ops.bind_array(d->array);
   d->ops.draw(GL_TRIANGLES, 0, 3);
   d->failed = d->ops.error() != GL_NO_ERROR;
   return !d->failed;
}
