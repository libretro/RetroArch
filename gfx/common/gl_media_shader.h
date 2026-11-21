/* Shared GLSL/Cg viewer chain. Programs and parameters are borrowed from
 * the active preset; render targets, feedback and history belong to the viewer.
 * The includer supplies the GL2/GL3 framebuffer entry points. */
#ifndef GL_MEDIA_SHADER_H
#define GL_MEDIA_SHADER_H

/* Preserve signed HDR in the history/feedback targets already required
 * by a preset. This flag creates no additional image or draw. */
#define GL_MEDIA_LINEAR_HDR (1u << 31)

typedef struct gl_media_target
{
   GLuint texture, fbo;
   unsigned dims;
   unsigned flags;
} gl_media_target_t;

typedef struct gl_media_shader
{
   gl_media_target_t pass[GFX_MAX_SHADERS];
   gl_media_target_t history[GFX_MAX_TEXTURES];
   gl_media_target_t feedback;
   uintptr_t history_source;
   unsigned history_copies, history_count, history_latest;
   unsigned history_order[GFX_MAX_TEXTURES];
   unsigned history_refs[GFX_MAX_TEXTURES];
   float history_colour[12];
   struct video_tex_info outputs[GFX_MAX_SHADERS];
   struct video_tex_info previous[GFX_MAX_TEXTURES];
} gl_media_shader_t;

#endif

static void gl_media_target_free(gl_media_target_t *t)
{
   if (t->texture) glDeleteTextures(1, &t->texture);
   if (t->fbo) MEDIA_DELETE_FB(1, &t->fbo);
   memset(t, 0, sizeof(*t));
}

static void gl_media_shader_free(gl_media_shader_t **state)
{
   unsigned i;
   gl_media_shader_t *s = *state;
   if (!s) return;
   for (i = 0; i < GFX_MAX_SHADERS; i++) gl_media_target_free(&s->pass[i]);
   for (i = 0; i < GFX_MAX_TEXTURES; i++) gl_media_target_free(&s->history[i]);
   gl_media_target_free(&s->feedback);
   free(s);
   *state = NULL;
}

static void gl_media_shader_changed(gl_media_shader_t *s, uintptr_t texture)
{
   if (s && s->history_source == texture)
      s->history_copies = 0;
}

static bool gl_media_target_fit(gl_media_target_t *t, unsigned dims,
      unsigned flags)
{
   if (t->dims != dims || t->flags != flags)
   {
      GLenum type = (flags & FBO_SCALE_FLAG_FP_FBO) ? GL_FLOAT : GL_UNSIGNED_BYTE;
      GLint format = GL_RGBA;
      gl_media_target_free(t);
#ifndef HAVE_OPENGLES2
      if (flags & FBO_SCALE_FLAG_FP_FBO) format = GL_RGBA32F;
      else if (flags & GL_MEDIA_LINEAR_HDR)
      {
         format = GL_RGBA16F;
         type = GL_FLOAT;
      }
      else if (flags & FBO_SCALE_FLAG_SRGB_FBO) format = GL_SRGB8_ALPHA8;
#endif
      glGenTextures(1, &t->texture);
      glBindTexture(GL_TEXTURE_2D, t->texture);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexImage2D(GL_TEXTURE_2D, 0, format, VIDEO_SCALE_W(dims),
            VIDEO_SCALE_H(dims), 0, GL_RGBA, type, NULL);
      MEDIA_GEN_FB(1, &t->fbo);
      MEDIA_BIND_FB(t->fbo);
      MEDIA_ATTACH(t->texture);
      if (!MEDIA_FB_COMPLETE())
      {
         gl_media_target_free(t);
         return false;
      }
      glClearColor(0, 0, 0, 0);
      glClear(GL_COLOR_BUFFER_BIT);
      t->dims = dims;
      t->flags = flags;
   }
   return true;
}

static void gl_media_info(struct video_tex_info *info, GLuint texture,
      unsigned dims, bool flipped)
{
   static const float normal[8] = {0, 0, 1, 0, 0, 1, 1, 1};
   static const float reverse[8] = {0, 1, 1, 1, 0, 0, 1, 0};
   info->tex = texture;
   info->input_size[0] = info->tex_size[0] = VIDEO_SCALE_W(dims);
   info->input_size[1] = info->tex_size[1] = VIDEO_SCALE_H(dims);
   memcpy(info->coord, flipped ? reverse : normal, sizeof(normal));
}

static unsigned gl_media_scale(unsigned input, unsigned viewport,
      unsigned original, enum gfx_scale_type type, float scale, unsigned absolute)
{
   unsigned size;
   switch (type)
   {
      case RARCH_SCALE_ABSOLUTE: size = absolute; break;
      case RARCH_SCALE_VIEWPORT: size = (unsigned)(viewport * scale); break;
      case RARCH_SCALE_ORIGINAL: size = (unsigned)(original * scale); break;
      default: size = (unsigned)(input * scale); break;
   }
   return size ? size : 1;
}

static bool gl_media_shader_draw(gl_media_shader_t **state, void *driver,
      const shader_backend_t *shader, void *shader_data, uintptr_t texture,
      unsigned source_dims, unsigned pos, unsigned dims, uint64_t frame,
      const float *colour_rows, bool managed)
{
   static const float vertices[8] = {0, 0, 1, 0, 0, 1, 1, 1};
   static const float white[16] = {1, 1, 1, 1, 1, 1, 1, 1,
                                  1, 1, 1, 1, 1, 1, 1, 1};
   gl_media_shader_t *s = *state;
   struct video_tex_info original, input, feedback;
   video_shader_ctx_params_t params;
   struct video_coords coords;
   math_matrix_4x4 mvp;
   GLint target, viewport[4], old_filter[4], old_texture, max_size;
   GLfloat clear[4];
   GLboolean blend, scissor;
#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
   GLboolean srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
#endif
   unsigned passes, history, feedback_pass = 0, i;
   unsigned hdr_flags = colour_rows ? GL_MEDIA_LINEAR_HDR : 0;
   bool has_feedback, direct = false, result = false;
   if (!shader || !shader_data || !texture
         || !VIDEO_SCALE_W(source_dims) || !VIDEO_SCALE_H(source_dims)
         || !VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return false;
   passes = shader->num_shaders(shader_data);
   history = shader->get_prev_textures(shader_data);
   has_feedback = shader->get_feedback_pass(shader_data, &feedback_pass);
   if (!passes || passes >= GFX_MAX_SHADERS || history >= GFX_MAX_TEXTURES
         || (has_feedback && feedback_pass >= passes))
      return false;
   if (!s)
   {
      s = (gl_media_shader_t*)calloc(1, sizeof(*s));
      if (!s) return false;
      *state = s;
   }
   if (colour_rows && (!shader->set_media_colour
            || !shader->set_media_colour(shader_data, colour_rows))) return false;
   glGetIntegerv(MEDIA_FB_BINDING, &target);
   glGetIntegerv(GL_VIEWPORT, viewport);
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
   glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
   blend = glIsEnabled(GL_BLEND);
   scissor = glIsEnabled(GL_SCISSOR_TEST);
   glDisable(GL_BLEND);
   glDisable(GL_SCISSOR_TEST);
   glActiveTexture(GL_TEXTURE0);
   glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
   glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
   glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &old_filter[0]);
   glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &old_filter[1]);
   glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &old_filter[2]);
   glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &old_filter[3]);
   gl_media_info(&original, (GLuint)texture, source_dims, true);
   input = original;
   memset(&params, 0, sizeof(params));
   memset(&coords, 0, sizeof(coords));
   memset(&feedback, 0, sizeof(feedback));
   coords.vertices = 4;
   coords.vertex = vertices;
   coords.color = white;
   coords.lut_tex_coord = original.coord;
   matrix_4x4_ortho(mvp, 0, 1, 0, 1, -1, 1);
   if (!managed || s->history_source != texture || s->history_count != history)
      s->history_copies = 0;
   if (colour_rows && memcmp(s->history_colour, colour_rows, sizeof(s->history_colour)))
   {
      s->history_copies = 0;
      memcpy(s->history_colour, colour_rows, sizeof(s->history_colour));
   }
   s->history_source = texture;
   if (s->history_count != history)
      for (i = 0; i < history; i++)
      {
         s->history_order[i] = i;
         s->history_refs[i] = 1;
      }
   s->history_count = history;
   for (i = 0; i < history; i++)
   {
      if (s->history[i].dims != source_dims || s->history[i].flags != hdr_flags)
         s->history_copies = 0;
      if (!gl_media_target_fit(&s->history[i], source_dims, hdr_flags)) goto done;
   }
   for (i = 0; i < history; i++)
      gl_media_info(&s->previous[i], s->history[s->history_order[i]].texture,
            source_dims, false);
   for (i = 0; i < passes; i++)
   {
      struct gfx_fbo_scale scale;
      unsigned out_dims = dims;
      memset(&scale, 0, sizeof(scale));
      shader->shader_scale(shader_data, i + 1, &scale);
      if (scale.flags & FBO_SCALE_FLAG_VALID)
         out_dims = VIDEO_SCALE_PACK(
               gl_media_scale((unsigned)input.input_size[0], VIDEO_SCALE_W(dims),
                  VIDEO_SCALE_W(source_dims), scale.type_x, scale.scale_x, scale.abs_x),
               gl_media_scale((unsigned)input.input_size[1], VIDEO_SCALE_H(dims),
                  VIDEO_SCALE_H(source_dims), scale.type_y, scale.scale_y, scale.abs_y));
      else if (i + 1 < passes)
         out_dims = VIDEO_SCALE_PACK((unsigned)input.input_size[0],
               (unsigned)input.input_size[1]);
      if (VIDEO_SCALE_W(out_dims) > (unsigned)max_size ||
            VIDEO_SCALE_H(out_dims) > (unsigned)max_size) goto done;
      if (i + 1 == passes && out_dims == dims
            && !(has_feedback && feedback_pass == i)
            && !(scale.flags & (FBO_SCALE_FLAG_FP_FBO | FBO_SCALE_FLAG_SRGB_FBO)))
      {
         gl_media_target_free(&s->pass[i]);
         s->pass[i].dims = out_dims;
         direct = true;
      }
      else if (!gl_media_target_fit(&s->pass[i], out_dims, scale.flags | hdr_flags)) goto done;
      if (has_feedback && feedback_pass == i)
      {
         if (!gl_media_target_fit(&s->feedback, out_dims, scale.flags | hdr_flags)) goto done;
         gl_media_info(&feedback, s->feedback.texture, out_dims, false);
      }
      gl_media_info(&s->outputs[i], s->pass[i].texture, out_dims, false);
      input = s->outputs[i];
   }
   input = original;
   if (shader->set_media_source) shader->set_media_source(shader_data, source_dims);
   for (i = 0; i < passes; i++)
   {
      unsigned out_dims = s->pass[i].dims;
      bool smooth = true;
      GLenum wrap = GL_CLAMP_TO_EDGE;
      bool viewport_pass = direct && i + 1 == passes;
      MEDIA_BIND_FB(viewport_pass ? (GLuint)target : s->pass[i].fbo);
      glViewport(viewport_pass ? VIDEO_POS_X(pos) : 0,
            viewport_pass ? VIDEO_POS_Y(pos) : 0,
            VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims));
#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
      if ((viewport_pass && srgb) || (s->pass[i].flags & FBO_SCALE_FLAG_SRGB_FBO))
         glEnable(GL_FRAMEBUFFER_SRGB);
      else glDisable(GL_FRAMEBUFFER_SRGB);
#endif
      shader->use(driver, shader_data, i + 1, true);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, input.tex);
      shader->filter_type(shader_data, i + 1, &smooth);
      switch (shader->wrap_type(shader_data, i + 1))
      {
         case RARCH_WRAP_REPEAT: wrap = GL_REPEAT; break;
         case RARCH_WRAP_MIRRORED_REPEAT: wrap = GL_MIRRORED_REPEAT; break;
#ifndef HAVE_OPENGLES
         case RARCH_WRAP_BORDER: wrap = GL_CLAMP_TO_BORDER; break;
#endif
         default: break;
      }
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
      if (shader->mipmap_input(shader_data, i + 1))
      {
         glGenerateMipmap(GL_TEXTURE_2D);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
               smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST);
      }
      params.info = &original;
      params.prev_info = s->previous;
      params.feedback_info = &feedback;
      params.fbo_info = s->outputs;
      params.fbo_info_cnt = i;
      params.vp_dims = dims;
      params.dims = VIDEO_SCALE_PACK((unsigned)input.input_size[0], (unsigned)input.input_size[1]);
      params.tex_dims = params.dims;
      params.out_dims = out_dims;
      params.frame_counter = (unsigned)frame;
      coords.tex_coord = input.coord;
      shader->set_params(&params, shader_data);
      shader->set_coords(shader_data, &coords);
      shader->set_mvp(shader_data, &mvp);
      if (!viewport_pass)
      {
         glClearColor(0, 0, 0, 0);
         glClear(GL_COLOR_BUFFER_BIT);
      }
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      gl_media_info(&s->outputs[i], s->pass[i].texture, out_dims, false);
      input = s->outputs[i];
   }
#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
   if (srgb) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB);
#endif
   MEDIA_BIND_FB((GLuint)target);
   glViewport(VIDEO_POS_X(pos), VIDEO_POS_Y(pos), VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
   shader->use(driver, shader_data, 0, true);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, input.tex);
   coords.tex_coord = input.coord;
   shader->set_coords(shader_data, &coords);
   shader->set_mvp(shader_data, &mvp);
   if (!direct) glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   if (history)
   {
      unsigned slot = s->history_latest;
      s->history_refs[s->history_order[history - 1]]--;
      if (!s->history_copies)
      {
         /* Reuse only an owned snapshot no logical Prev still reads. */
         for (slot = 0; slot < history - 1 && s->history_refs[slot]; slot++) {}
      }
      for (i = history - 1; i; i--)
         s->history_order[i] = s->history_order[i - 1];
      s->history_order[0] = slot;
      s->history_refs[slot]++;
      if (!s->history_copies)
      {
         MEDIA_BIND_FB(s->history[slot].fbo);
         glViewport(0, 0, VIDEO_SCALE_W(source_dims), VIDEO_SCALE_H(source_dims));
         glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
         coords.tex_coord = original.coord;
         shader->set_coords(shader_data, &coords);
         glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
         s->history_latest = slot;
         s->history_copies = 1;
      }
   }
   if (has_feedback)
   {
      gl_media_target_t tmp = s->feedback;
      s->feedback = s->pass[feedback_pass];
      s->pass[feedback_pass] = tmp;
   }
   result = true;
done:
   if (shader->set_media_colour) shader->set_media_colour(shader_data, NULL);
   if (shader->set_media_source) shader->set_media_source(shader_data, 0);
   shader->use(driver, shader_data, VIDEO_SHADER_STOCK_BLEND, true);
   MEDIA_BIND_FB((GLuint)target);
   glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
   glClearColor(clear[0], clear[1], clear[2], clear[3]);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, old_filter[0]);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, old_filter[1]);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, old_filter[2]);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, old_filter[3]);
   glBindTexture(GL_TEXTURE_2D, (GLuint)old_texture);
   if (blend) glEnable(GL_BLEND);
   if (scissor) glEnable(GL_SCISSOR_TEST);
#if defined(GL_FRAMEBUFFER_SRGB) && !defined(HAVE_OPENGLES)
   if (srgb) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB);
#endif
   return result;
}
#undef gl_media_target_free
#undef gl_media_shader_changed
#undef gl_media_shader_free
#undef gl_media_target_fit
#undef gl_media_info
#undef gl_media_scale
#undef gl_media_shader_draw
#undef MEDIA_GEN_FB
#undef MEDIA_DELETE_FB
#undef MEDIA_BIND_FB
#undef MEDIA_FB_BINDING
#undef MEDIA_ATTACH
#undef MEDIA_FB_COMPLETE
