/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <string.h>
#include "record_android_capture_ops.h"

bool record_android_capture_load_ops(record_gpu_mediacodec_config_t *config,
      record_android_capture_resolve_t resolve, void *context)
{
   record_gpu_mediacodec_config_t loaded;
   function_t proc;
   if (!config)
      return false;
   memset(&config->surface, 0, sizeof(config->surface));
   memset(&config->capture, 0, sizeof(config->capture));
   memset(&config->draw, 0, sizeof(config->draw));
   if (!resolve)
      return false;
   memset(&loaded, 0, sizeof(loaded));
   proc = resolve(context, "eglQueryString");
   if (!proc)
      return false;
   loaded.surface.query_string = (PFNEGLQUERYSTRINGPROC)proc;
   proc = resolve(context, "eglGetConfigAttrib");
   if (!proc)
      return false;
   loaded.surface.config_attrib = (PFNEGLGETCONFIGATTRIBPROC)proc;
   proc = resolve(context, "eglCreateContext");
   if (!proc)
      return false;
   loaded.surface.create_context = (PFNEGLCREATECONTEXTPROC)proc;
   proc = resolve(context, "eglCreateWindowSurface");
   if (!proc)
      return false;
   loaded.surface.create_surface = (PFNEGLCREATEWINDOWSURFACEPROC)proc;
   proc = resolve(context, "eglDestroyContext");
   if (!proc)
      return false;
   loaded.surface.destroy_context = (PFNEGLDESTROYCONTEXTPROC)proc;
   proc = resolve(context, "eglDestroySurface");
   if (!proc)
      return false;
   loaded.surface.destroy_surface = (PFNEGLDESTROYSURFACEPROC)proc;
   proc = resolve(context, "eglQuerySurface");
   if (!proc)
      return false;
   loaded.surface.query_surface = (PFNEGLQUERYSURFACEPROC)proc;
   proc = resolve(context, "eglQueryAPI");
   if (!proc)
      return false;
   loaded.surface.query_api = (PFNEGLQUERYAPIPROC)proc;
   proc = resolve(context, "eglGetCurrentContext");
   if (!proc)
      return false;
   loaded.surface.current_context = (PFNEGLGETCURRENTCONTEXTPROC)proc;
   proc = resolve(context, "eglGetCurrentDisplay");
   if (!proc)
      return false;
   loaded.surface.current_display = (PFNEGLGETCURRENTDISPLAYPROC)proc;
   proc = resolve(context, "eglGetCurrentSurface");
   if (!proc)
      return false;
   loaded.surface.current_surface = (PFNEGLGETCURRENTSURFACEPROC)proc;
   proc = resolve(context, "eglMakeCurrent");
   if (!proc)
      return false;
   loaded.surface.make_current = (PFNEGLMAKECURRENTPROC)proc;
   proc = resolve(context, "eglPresentationTimeANDROID");
   if (!proc)
      return false;
   loaded.surface.presentation_time = (PFNEGLPRESENTATIONTIMEANDROIDPROC)proc;
   proc = resolve(context, "eglSwapBuffers");
   if (!proc)
      return false;
   loaded.surface.swap = (PFNEGLSWAPBUFFERSPROC)proc;
   proc = resolve(context, "eglQueryString");
   if (!proc)
      return false;
   loaded.capture.query_string = (PFNEGLQUERYSTRINGPROC)proc;
   proc = resolve(context, "eglGetNativeClientBufferANDROID");
   if (!proc)
      return false;
   loaded.capture.native_buffer = (PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC)proc;
   proc = resolve(context, "eglCreateImageKHR");
   if (!proc)
      return false;
   loaded.capture.create_image = (PFNEGLCREATEIMAGEKHRPROC)proc;
   proc = resolve(context, "eglDestroyImageKHR");
   if (!proc)
      return false;
   loaded.capture.destroy_image = (PFNEGLDESTROYIMAGEKHRPROC)proc;
   proc = resolve(context, "eglCreateSyncKHR");
   if (!proc)
      return false;
   loaded.capture.create_sync = (PFNEGLCREATESYNCKHRPROC)proc;
   proc = resolve(context, "eglDestroySyncKHR");
   if (!proc)
      return false;
   loaded.capture.destroy_sync = (PFNEGLDESTROYSYNCKHRPROC)proc;
   proc = resolve(context, "eglWaitSyncKHR");
   if (!proc)
      return false;
   loaded.capture.wait_sync = (PFNEGLWAITSYNCKHRPROC)proc;
   proc = resolve(context, "glGenTextures");
   if (!proc)
      return false;
   loaded.capture.gen_textures = (PFNGLGENTEXTURESPROC)proc;
   proc = resolve(context, "glDeleteTextures");
   if (!proc)
      return false;
   loaded.capture.delete_textures = (PFNGLDELETETEXTURESPROC)proc;
   proc = resolve(context, "glBindTexture");
   if (!proc)
      return false;
   loaded.capture.bind_texture = (PFNGLBINDTEXTUREPROC)proc;
   proc = resolve(context, "glTexParameteri");
   if (!proc)
      return false;
   loaded.capture.texture_parameter = (PFNGLTEXPARAMETERIPROC)proc;
   proc = resolve(context, "glEGLImageTargetTexture2DOES");
   if (!proc)
      return false;
   loaded.capture.image_target = (record_capture_gl_image_target_t)proc;
   proc = resolve(context, "glGetError");
   if (!proc)
      return false;
   loaded.capture.error = (PFNGLGETERRORPROC)proc;
   proc = resolve(context, "glGetString");
   if (!proc)
      return false;
   loaded.capture.string = (PFNGLGETSTRINGPROC)proc;
   proc = resolve(context, "glFenceSync");
   if (!proc)
      return false;
   loaded.capture.fence = (PFNGLFENCESYNCPROC)proc;
   proc = resolve(context, "glClientWaitSync");
   if (!proc)
      return false;
   loaded.capture.poll = (PFNGLCLIENTWAITSYNCPROC)proc;
   proc = resolve(context, "glDeleteSync");
   if (!proc)
      return false;
   loaded.capture.delete_fence = (PFNGLDELETESYNCPROC)proc;
   proc = resolve(context, "glFlush");
   if (!proc)
      return false;
   loaded.capture.flush = (PFNGLFLUSHPROC)proc;
   proc = resolve(context, "glCreateShader");
   if (!proc)
      return false;
   loaded.draw.create_shader = (PFNGLCREATESHADERPROC)proc;
   proc = resolve(context, "glShaderSource");
   if (!proc)
      return false;
   loaded.draw.shader_source = (PFNGLSHADERSOURCEPROC)proc;
   proc = resolve(context, "glCompileShader");
   if (!proc)
      return false;
   loaded.draw.compile_shader = (PFNGLCOMPILESHADERPROC)proc;
   proc = resolve(context, "glGetShaderiv");
   if (!proc)
      return false;
   loaded.draw.shader_status = (PFNGLGETSHADERIVPROC)proc;
   proc = resolve(context, "glDeleteShader");
   if (!proc)
      return false;
   loaded.draw.delete_shader = (PFNGLDELETESHADERPROC)proc;
   proc = resolve(context, "glCreateProgram");
   if (!proc)
      return false;
   loaded.draw.create_program = (PFNGLCREATEPROGRAMPROC)proc;
   proc = resolve(context, "glAttachShader");
   if (!proc)
      return false;
   loaded.draw.attach_shader = (PFNGLATTACHSHADERPROC)proc;
   proc = resolve(context, "glLinkProgram");
   if (!proc)
      return false;
   loaded.draw.link_program = (PFNGLLINKPROGRAMPROC)proc;
   proc = resolve(context, "glGetProgramiv");
   if (!proc)
      return false;
   loaded.draw.program_status = (PFNGLGETPROGRAMIVPROC)proc;
   proc = resolve(context, "glDeleteProgram");
   if (!proc)
      return false;
   loaded.draw.delete_program = (PFNGLDELETEPROGRAMPROC)proc;
   proc = resolve(context, "glGetUniformLocation");
   if (!proc)
      return false;
   loaded.draw.uniform_location = (PFNGLGETUNIFORMLOCATIONPROC)proc;
   proc = resolve(context, "glGenVertexArrays");
   if (!proc)
      return false;
   loaded.draw.gen_arrays = (PFNGLGENVERTEXARRAYSPROC)proc;
   proc = resolve(context, "glDeleteVertexArrays");
   if (!proc)
      return false;
   loaded.draw.delete_arrays = (PFNGLDELETEVERTEXARRAYSPROC)proc;
   proc = resolve(context, "glBindVertexArray");
   if (!proc)
      return false;
   loaded.draw.bind_array = (PFNGLBINDVERTEXARRAYPROC)proc;
   proc = resolve(context, "glUseProgram");
   if (!proc)
      return false;
   loaded.draw.use_program = (PFNGLUSEPROGRAMPROC)proc;
   proc = resolve(context, "glUniform1i");
   if (!proc)
      return false;
   loaded.draw.uniform = (PFNGLUNIFORM1IPROC)proc;
   proc = resolve(context, "glActiveTexture");
   if (!proc)
      return false;
   loaded.draw.active_texture = (PFNGLACTIVETEXTUREPROC)proc;
   proc = resolve(context, "glBindTexture");
   if (!proc)
      return false;
   loaded.draw.bind_texture = (PFNGLBINDTEXTUREPROC)proc;
   proc = resolve(context, "glBindFramebuffer");
   if (!proc)
      return false;
   loaded.draw.bind_framebuffer = (PFNGLBINDFRAMEBUFFERPROC)proc;
   proc = resolve(context, "glViewport");
   if (!proc)
      return false;
   loaded.draw.viewport = (PFNGLVIEWPORTPROC)proc;
   proc = resolve(context, "glDisable");
   if (!proc)
      return false;
   loaded.draw.disable = (PFNGLDISABLEPROC)proc;
   proc = resolve(context, "glColorMask");
   if (!proc)
      return false;
   loaded.draw.color_mask = (PFNGLCOLORMASKPROC)proc;
   proc = resolve(context, "glDrawArrays");
   if (!proc)
      return false;
   loaded.draw.draw = (PFNGLDRAWARRAYSPROC)proc;
   proc = resolve(context, "glGetError");
   if (!proc)
      return false;
   loaded.draw.error = (PFNGLGETERRORPROC)proc;
   config->surface = loaded.surface;
   config->capture = loaded.capture;
   config->draw = loaded.draw;
   return true;
}
