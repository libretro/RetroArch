/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pyrowave_source.h"
#if defined(HAVE_OPENGL_CORE) && !defined(HAVE_OPENGLES)
#include "record_pyrowave_gl.h"
#include "record_pyrowave_player_gl.h"
#include <glsym/glsym.h>
#include <stdlib.h>
#include <string.h>

typedef struct record_pyrowave_source_gl
{
   record_pyrowave_gl_t *codec;
   GLuint programs[3], texture;
   GLenum read_buffer;
   unsigned width, height;
   bool lost;
} record_pyrowave_source_gl_t;

typedef struct record_pyrowave_source_gl_state
{
   GLint program, active, texture, sampler, read_fbo, read_buffer;
   GLint generic[4], buffers[6], image[6];
   GLint64 starts[6], sizes[6];
} record_pyrowave_source_gl_state_t;

static void record_pyrowave_source_gl_state(record_pyrowave_source_gl_state_t *s, bool restore)
{
   static const GLenum targets[4] = { 0x90d2, 0x8a11, 0x8f36, 0x8f37 };
   static const GLenum bindings[4] = { 0x90d3, 0x8a28, 0x8f36, 0x8f37 };
   static const GLenum image_queries[6] = { 0x8f3a, 0x8f3b, 0x8f3c, 0x8f3d, 0x8f3e, 0x906e };
   unsigned i;
   if (!restore)
   {
      glGetIntegerv(GL_CURRENT_PROGRAM, &s->program);
      glGetIntegerv(GL_ACTIVE_TEXTURE, &s->active);
      glActiveTexture(GL_TEXTURE0);
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &s->texture);
      glGetIntegeri_v(0x8919, 0, &s->sampler);
      glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &s->read_fbo);
      glGetIntegerv(GL_READ_BUFFER, &s->read_buffer);
      for (i = 0; i < 4; i++) glGetIntegerv(bindings[i], &s->generic[i]);
      for (i = 0; i < 6; i++)
      {
         unsigned index = i < 4 ? i : (i == 4 ? 2 : 4);
         GLenum binding = i < 4 ? 0x90d3 : 0x8a28;
         glGetIntegeri_v(binding, index, &s->buffers[i]);
         glGetInteger64i_v(i < 4 ? 0x90d4 : 0x8a29, index, &s->starts[i]);
         glGetInteger64i_v(i < 4 ? 0x90d5 : 0x8a2a, index, &s->sizes[i]);
         glGetIntegeri_v(image_queries[i], 3, &s->image[i]);
      }
   }
   else
   {
      for (i = 0; i < 6; i++)
      {
         GLenum target = i < 4 ? 0x90d2 : 0x8a11;
         unsigned index = i < 4 ? i : (i == 4 ? 2 : 4);
         if (s->buffers[i] && s->sizes[i])
            glBindBufferRange(target, index, s->buffers[i],
                  (GLintptr)s->starts[i], (GLsizeiptr)s->sizes[i]);
         else glBindBufferBase(target, index, s->buffers[i]);
      }
      glBindImageTexture(3, s->image[0], s->image[1], (GLboolean)s->image[2],
            s->image[3], s->image[4], s->image[5]);
      for (i = 0; i < 4; i++) glBindBuffer(targets[i], s->generic[i]);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, s->read_fbo);
      glReadBuffer(s->read_buffer);
      glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, s->texture);
      glBindSampler(0, s->sampler);
      glActiveTexture(s->active); glUseProgram(s->program);
   }
}

static bool record_pyrowave_source_gl_poll(void *user)
{
   record_pyrowave_source_gl_t *s = (record_pyrowave_source_gl_t*)user;
   return record_pyrowave_gl_poll(s->codec);
}
static bool record_pyrowave_source_gl_idle(void *user)
{
   return record_pyrowave_gl_idle(((record_pyrowave_source_gl_t*)user)->codec);
}
static uint64_t record_pyrowave_source_gl_retired(void *user)
{
   return record_pyrowave_gl_source_retired(((record_pyrowave_source_gl_t*)user)->codec);
}
static bool record_pyrowave_source_gl_lost(record_pyrowave_source_gl_t *s)
{
   if (!s->lost)
      s->lost = glGetError() == 0x0507; /* GL_CONTEXT_LOST */
   return s->lost;
}

static bool record_pyrowave_source_gl_destroy(void *user, bool recover)
{
   record_pyrowave_source_gl_t *s = (record_pyrowave_source_gl_t*)user;
   GLint previous;
   unsigned i;
   bool valid;
   if (!s) return true;
   if (recover && record_pyrowave_source_gl_lost(s))
   {
      if (!record_pyrowave_gl_abandon(s->codec)) return false;
      free(s);
      return true;
   }
   glGetIntegerv(0x8f36, &previous);
   valid = recover ? record_pyrowave_gl_retire(s->codec) : record_pyrowave_gl_free(s->codec);
   if (!valid) return false;
   for (i = 0; i < 3; i++) if (s->programs[i]) glDeleteProgram(s->programs[i]);
   if (s->texture) glDeleteTextures(1, &s->texture);
   glBindBuffer(0x8f36, previous);
   free(s);
   return true;
}

static bool record_pyrowave_source_gl_free(void *user)
{ return record_pyrowave_source_gl_destroy(user, false); }

bool record_pyrowave_player_gl_retire(record_pyrowave_player_gl_t *p)
{ return record_pyrowave_source_gl_destroy(p, true); }

static bool record_pyrowave_source_gl_quiesce(void *user, void *renderer)
{
   record_pyrowave_source_gl_t *s = (record_pyrowave_source_gl_t*)user;
   (void)renderer;
   return record_pyrowave_source_gl_lost(s) || record_pyrowave_gl_quiesce(s->codec);
}
static bool record_pyrowave_source_gl_abandon(void *user)
{ return record_pyrowave_source_gl_destroy(user, true); }

static bool record_pyrowave_source_gl_create(record_pyrowave_source_t *source,
      const record_pyrowave_source_config_t *config, bool decode)
{
   static const char *const names[3] = { "color.comp", "dwt.comp", "packet.comp" };
   record_pyrowave_source_gl_t *s;
   record_pyrowave_gl_config_t cfg;
   record_pyrowave_source_gl_state_t state;
   GLint major = 0, minor = 0;
   GLboolean double_buffered;
   unsigned i;
   bool valid = false;
   if (!source || !config || !config->gl_proc || !glGetInteger64i_v || !glBindImageTexture) return false;
   glGetIntegerv(GL_MAJOR_VERSION, &major); glGetIntegerv(GL_MINOR_VERSION, &minor);
   if (major < 4 || (major == 4 && minor < 3)) return false;
   s = (record_pyrowave_source_gl_t*)calloc(1, sizeof(*s));
   if (!s) return false;
   memset(&cfg, 0, sizeof(cfg));
   record_pyrowave_source_gl_state(&state, false);
   glGetBooleanv(GL_DOUBLEBUFFER, &double_buffered);
   s->read_buffer = double_buffered ? GL_BACK : GL_FRONT;
   for (i = 0; i < 3; i++)
   {
      record_pyrowave_shader_t text;
      const GLchar *strings;
      GLint status = 0;
      GLuint shader;
      if (!record_pyrowave_source_shader(config->directory, names[i], &text)) goto done;
      shader = glCreateShader(0x91b9);
      if (!shader) { record_pyrowave_source_shader_release(&text); goto done; }
      strings = (const GLchar*)text.data;
      glShaderSource(shader, 1, &strings, NULL); glCompileShader(shader);
      record_pyrowave_source_shader_release(&text);
      glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
      if (!status) { glDeleteShader(shader); goto done; }
      s->programs[i] = glCreateProgram();
      if (s->programs[i])
      {
         glAttachShader(s->programs[i], shader); glLinkProgram(s->programs[i]);
         glGetProgramiv(s->programs[i], GL_LINK_STATUS, &status);
      }
      else status = 0;
      glDeleteShader(shader);
      if (!status) goto done;
      cfg.programs[i] = s->programs[i];
   }
   s->width = decode ? config->stream.sequence.width : config->full_width;
   s->height = decode ? config->stream.sequence.height : config->full_height;
   if (!decode)
   {
      glGenTextures(1, &s->texture); glBindTexture(GL_TEXTURE_2D, s->texture);
      glTexImage2D(GL_TEXTURE_2D, 0, config->format, s->width, s->height,
            0, GL_RGBA, GL_FLOAT, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   }
   cfg.scale_source = config->scale_source;
   cfg.get_proc = config->gl_proc; cfg.sequence = config->stream.sequence;
   cfg.maximum_bytes = config->stream.maximum_frame_bytes; cfg.slots = config->slots;
   cfg.bit_depth = config->stream.bit_depth; cfg.texture_mode = config->texture_mode;
   cfg.texture_width = s->width; cfg.texture_height = s->height;
   cfg.x = decode ? 0 : config->x; cfg.y = decode ? 0 : config->y;
   cfg.flip_y = !decode; cfg.decode = decode;
   s->codec = record_pyrowave_gl_new(&cfg);
   if (!s->codec) goto done;
   memset(source, 0, sizeof(*source)); source->user = s;
   record_pyrowave_gl_session_video(s->codec, &source->video);
   source->poll = record_pyrowave_source_gl_poll; source->idle = record_pyrowave_source_gl_idle;
   source->retired = record_pyrowave_source_gl_retired; source->free = record_pyrowave_source_gl_free;
   source->quiesce = record_pyrowave_source_gl_quiesce;
   source->abandon = record_pyrowave_source_gl_abandon;
   valid = true;
done:
   record_pyrowave_source_gl_state(&state, true);
   if (!valid) record_pyrowave_source_gl_free(s);
   return valid;
}

bool record_pyrowave_source_gl_new(record_pyrowave_source_t *source,
      const record_pyrowave_source_config_t *config)
{
   return record_pyrowave_source_gl_create(source, config, false);
}

record_pyrowave_player_gl_t *record_pyrowave_player_gl_new(
      const record_pyrowave_source_config_t *config)
{
   record_pyrowave_source_t source;
   memset(&source, 0, sizeof(source));
   return record_pyrowave_source_gl_create(&source, config, true)
      ? (record_pyrowave_player_gl_t*)source.user : NULL;
}

void record_pyrowave_player_gl_video(record_pyrowave_player_gl_t *p, record_pyrowave_video_t *video)
{
   if (p) record_pyrowave_gl_session_video(p->codec, video);
}
int record_pyrowave_player_gl_decode(record_pyrowave_player_gl_t *p)
{
   record_pyrowave_source_gl_state_t state;
   int result;
   if (!p) return RECORD_GPU_QUEUE_ERROR;
   record_pyrowave_source_gl_state(&state, false);
   result = record_pyrowave_gl_decode(p->codec);
   record_pyrowave_source_gl_state(&state, true);
   return result;
}
bool record_pyrowave_player_gl_poll(record_pyrowave_player_gl_t *p)
{ return p && record_pyrowave_gl_poll(p->codec); }
int record_pyrowave_player_gl_peek(record_pyrowave_player_gl_t *p, record_gpu_frame_t *frame)
{ return p ? record_pyrowave_gl_receive_peek(p->codec, frame) : RECORD_GPU_QUEUE_ERROR; }
int record_pyrowave_player_gl_discard(record_pyrowave_player_gl_t *p, record_gpu_frame_t *frame)
{ return p ? record_pyrowave_gl_receive_drop(p->codec, frame) : RECORD_GPU_QUEUE_ERROR; }
int record_pyrowave_player_gl_image(record_pyrowave_player_gl_t *p, record_pyrowave_gl_image_t *image)
{ return p ? record_pyrowave_gl_image(p->codec, image) : RECORD_GPU_QUEUE_ERROR; }
bool record_pyrowave_player_gl_drop(record_pyrowave_player_gl_t *p, unsigned slot)
{ return p && record_pyrowave_gl_image_drop(p->codec, slot); }
bool record_pyrowave_player_gl_present(record_pyrowave_player_gl_t *p, unsigned slot)
{ return p && record_pyrowave_gl_present(p->codec, slot, NULL); }
bool record_pyrowave_player_gl_idle(const record_pyrowave_player_gl_t *p)
{ return p && record_pyrowave_gl_idle(p->codec); }
bool record_pyrowave_player_gl_free(record_pyrowave_player_gl_t *p)
{ return record_pyrowave_source_gl_free(p); }

int record_pyrowave_source_gl_push(record_pyrowave_source_t *source,
      uint64_t timestamp, uint64_t duration, uint64_t *ticket)
{
   record_pyrowave_source_gl_t *s = (record_pyrowave_source_gl_t*)source->user;
   record_pyrowave_source_gl_state_t state;
   int result;
   record_pyrowave_source_gl_state(&state, false);
   glBindFramebuffer(GL_READ_FRAMEBUFFER, 0); glReadBuffer(s->read_buffer);
   glBindTexture(GL_TEXTURE_2D, s->texture);
   glBindSampler(0, 0);
   /* The default framebuffer cannot be sampled. This copy stays on GPU. */
   glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, s->width, s->height);
   result = record_pyrowave_gl_push(s->codec, s->texture, NULL, timestamp, duration, ticket);
   record_pyrowave_source_gl_state(&state, true);
   return result;
}
#endif
