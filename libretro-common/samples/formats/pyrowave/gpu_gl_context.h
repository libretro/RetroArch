#ifndef RPYROWAVE_GPU_GL_CONTEXT_H
#define RPYROWAVE_GPU_GL_CONTEXT_H
/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 * Blocking readback is confined to this offline validation executable.
 */
#define _CRT_SECURE_NO_WARNINGS
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#ifndef APIENTRY
#define APIENTRY
#endif
#endif
#include <stddef.h>
#include "gpu_vectors.h"

#define RPY_GL_STORAGE 0x90d2
#define RPY_GL_UNIFORM 0x8a11
#define RPY_GL_DYNAMIC_COPY 0x88ea
#define RPY_GL_COMPUTE 0x91b9
#define RPY_GL_COMPILE_STATUS 0x8b81
#define RPY_GL_LINK_STATUS 0x8b82
#define RPY_GL_STORAGE_BARRIER 0x2000

typedef struct rpy_gl_api
{
   GLuint (APIENTRY *CreateShader)(GLenum);
   void (APIENTRY *ShaderSource)(GLuint, GLsizei, const char *const*, const GLint*);
   void (APIENTRY *CompileShader)(GLuint);
   void (APIENTRY *GetShaderiv)(GLuint, GLenum, GLint*);
   void (APIENTRY *GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, char*);
   GLuint (APIENTRY *CreateProgram)(void);
   void (APIENTRY *AttachShader)(GLuint, GLuint);
   void (APIENTRY *LinkProgram)(GLuint);
   void (APIENTRY *GetProgramiv)(GLuint, GLenum, GLint*);
   void (APIENTRY *GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, char*);
   void (APIENTRY *UseProgram)(GLuint);
   void (APIENTRY *DeleteShader)(GLuint);
   void (APIENTRY *DeleteProgram)(GLuint);
   void (APIENTRY *GenBuffers)(GLsizei, GLuint*);
   void (APIENTRY *BindBuffer)(GLenum, GLuint);
   void (APIENTRY *BufferData)(GLenum, ptrdiff_t, const void*, GLenum);
   void (APIENTRY *BindBufferBase)(GLenum, GLuint, GLuint);
   void (APIENTRY *BindBufferRange)(GLenum, GLuint, GLuint, ptrdiff_t, ptrdiff_t);
   void (APIENTRY *DispatchCompute)(GLuint, GLuint, GLuint);
   void (APIENTRY *MemoryBarrier)(GLenum);
   void (APIENTRY *GetBufferSubData)(GLenum, ptrdiff_t, ptrdiff_t, void*);
   void (APIENTRY *CopyBufferSubData)(GLenum, GLenum, ptrdiff_t, ptrdiff_t, ptrdiff_t);
   void (APIENTRY *DeleteBuffers)(GLsizei, const GLuint*);
} rpy_gl_api_t;

typedef struct rpy_gl_test
{
   rpy_gl_api_t api;
   uint8_t *parameters;
   size_t parameter_stride;
   unsigned cursor;
   GLuint buffers[3];
   GLuint program;
#if defined(_WIN32)
   HWND window;
   HDC dc;
   HGLRC context;
#else
   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;
#endif
} rpy_gl_test_t;

static int rpy_gl_context(rpy_gl_test_t *test)
{
#if defined(_WIN32)
   WNDCLASSA window_class;
   PIXELFORMATDESCRIPTOR format;
   HGLRC legacy;
   PROC entry;
   HGLRC (WINAPI *create_context)(HDC, HGLRC, const int*);
   static const int attributes[] =
   { 0x2091, 4, 0x2092, 3, 0x9126, 1, 0 };
   int selected;
   window_class.style         = CS_OWNDC;
   window_class.lpfnWndProc   = DefWindowProcA;
   window_class.cbClsExtra    = 0;
   window_class.cbWndExtra    = 0;
   window_class.hInstance     = GetModuleHandleA(NULL);
   window_class.hIcon         = NULL;
   window_class.hCursor       = NULL;
   window_class.hbrBackground = NULL;
   window_class.lpszMenuName  = NULL;
   window_class.lpszClassName = "RpyrowaveComputeValidation";
   if (!RegisterClassA(&window_class))
      return 0;
   test->window = CreateWindowA(window_class.lpszClassName, "", WS_POPUP,
         0, 0, 1, 1, NULL, NULL, window_class.hInstance, NULL);
   if (!test->window || !(test->dc = GetDC(test->window)))
      return 0;
   format.nSize           = sizeof(format);
   format.nVersion        = 1;
   format.dwFlags         = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
   format.iPixelType      = PFD_TYPE_RGBA;
   format.cColorBits      = 32;
   format.cRedBits        = 0;
   format.cRedShift       = 0;
   format.cGreenBits      = 0;
   format.cGreenShift     = 0;
   format.cBlueBits       = 0;
   format.cBlueShift      = 0;
   format.cAlphaBits      = 0;
   format.cAlphaShift     = 0;
   format.cAccumBits      = 0;
   format.cAccumRedBits   = 0;
   format.cAccumGreenBits = 0;
   format.cAccumBlueBits  = 0;
   format.cAccumAlphaBits = 0;
   format.cDepthBits      = 0;
   format.cStencilBits    = 0;
   format.cAuxBuffers     = 0;
   format.iLayerType      = PFD_MAIN_PLANE;
   format.bReserved       = 0;
   format.dwLayerMask     = 0;
   format.dwVisibleMask   = 0;
   format.dwDamageMask    = 0;
   selected = ChoosePixelFormat(test->dc, &format);
   if (!selected || !SetPixelFormat(test->dc, selected, &format))
      return 0;
   legacy = wglCreateContext(test->dc);
   if (!legacy)
      return 0;
   if (!wglMakeCurrent(test->dc, legacy))
   {
      wglDeleteContext(legacy);
      return 0;
   }
   entry = wglGetProcAddress("wglCreateContextAttribsARB");
   memcpy(&create_context, &entry, sizeof(entry));
   test->context = entry ? create_context(test->dc, NULL, attributes) : NULL;
   wglMakeCurrent(NULL, NULL);
   wglDeleteContext(legacy);
   if (!test->context || !wglMakeCurrent(test->dc, test->context))
      return 0;
#else
   EGLint count;
   EGLint major;
   EGLint minor;
   EGLConfig config;
   static const EGLint config_attributes[] =
   { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE };
   static const EGLint context_attributes[] =
   { EGL_CONTEXT_MAJOR_VERSION_KHR, 4, EGL_CONTEXT_MINOR_VERSION_KHR, 3,
      EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
      EGL_NONE };
   static const EGLint surface_attributes[] =
   { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
   test->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (test->display == EGL_NO_DISPLAY || !eglInitialize(test->display, &major, &minor)
         || !eglBindAPI(EGL_OPENGL_API)
         || !eglChooseConfig(test->display, config_attributes, &config, 1, &count)
         || !count)
      return 0;
   test->surface = eglCreatePbufferSurface(test->display, config, surface_attributes);
   test->context = eglCreateContext(test->display, config, EGL_NO_CONTEXT, context_attributes);
   if (test->surface == EGL_NO_SURFACE || test->context == EGL_NO_CONTEXT
         || !eglMakeCurrent(test->display, test->surface, test->surface, test->context))
      return 0;
#endif
   printf("OpenGL: %s; %s\n", (const char*)glGetString(GL_VERSION),
         (const char*)glGetString(GL_RENDERER));
   return 1;
}

#if defined(_WIN32)
#define RPY_GL_LOAD(member) do { \
   PROC entry = wglGetProcAddress("gl" #member); \
   if (!entry) return 0; \
   memcpy(&test->api.member, &entry, sizeof(entry)); \
} while (0)
#else
#define RPY_GL_LOAD(member) do { \
   __eglMustCastToProperFunctionPointerType entry = eglGetProcAddress("gl" #member); \
   if (!entry) return 0; \
   memcpy(&test->api.member, &entry, sizeof(entry)); \
} while (0)
#endif

static int rpy_gl_functions(rpy_gl_test_t *test)
{
   RPY_GL_LOAD(CreateShader);
   RPY_GL_LOAD(ShaderSource);
   RPY_GL_LOAD(CompileShader);
   RPY_GL_LOAD(GetShaderiv);
   RPY_GL_LOAD(GetShaderInfoLog);
   RPY_GL_LOAD(CreateProgram);
   RPY_GL_LOAD(AttachShader);
   RPY_GL_LOAD(LinkProgram);
   RPY_GL_LOAD(GetProgramiv);
   RPY_GL_LOAD(GetProgramInfoLog);
   RPY_GL_LOAD(UseProgram);
   RPY_GL_LOAD(DeleteShader);
   RPY_GL_LOAD(DeleteProgram);
   RPY_GL_LOAD(GenBuffers);
   RPY_GL_LOAD(BindBuffer);
   RPY_GL_LOAD(BufferData);
   RPY_GL_LOAD(BindBufferBase);
   RPY_GL_LOAD(BindBufferRange);
   RPY_GL_LOAD(DispatchCompute);
   RPY_GL_LOAD(MemoryBarrier);
   RPY_GL_LOAD(GetBufferSubData);
   RPY_GL_LOAD(CopyBufferSubData);
   RPY_GL_LOAD(DeleteBuffers);
   return 1;
}

static int rpy_gl_program(rpy_gl_test_t *test, const char *path)
{
   size_t length = 0;
   uint8_t *text = rpy_gpu_test_file(path, &length);
   const char *source = (const char*)text;
   GLint size = (GLint)length;
   GLint status;
   GLuint shader;
   char log[1024];
   if (!text)
      return 0;
   shader = test->api.CreateShader(RPY_GL_COMPUTE);
   test->api.ShaderSource(shader, 1, &source, &size);
   test->api.CompileShader(shader);
   free(text);
   test->api.GetShaderiv(shader, RPY_GL_COMPILE_STATUS, &status);
   if (!status)
   {
      test->api.GetShaderInfoLog(shader, sizeof(log), NULL, log);
      fprintf(stderr, "Compute compile: %s\n", log);
      test->api.DeleteShader(shader);
      return 0;
   }
   test->program = test->api.CreateProgram();
   test->api.AttachShader(test->program, shader);
   test->api.LinkProgram(test->program);
   test->api.DeleteShader(shader);
   test->api.GetProgramiv(test->program, RPY_GL_LINK_STATUS, &status);
   if (!status)
   {
      test->api.GetProgramInfoLog(test->program, sizeof(log), NULL, log);
      fprintf(stderr, "Compute link: %s\n", log);
      return 0;
   }
   test->api.UseProgram(test->program);
   return 1;
}

static int rpy_gl_parameters(void *userdata, const rpyrowave_dwt_parameters_t *parameters,
      unsigned source, unsigned x, unsigned y, unsigned z)
{
   rpy_gl_test_t *test = (rpy_gl_test_t*)userdata;
   unsigned slot = (parameters->inverse ? 50 : 0) + test->cursor++;
   (void)source;
   (void)x;
   (void)y;
   (void)z;
   memcpy(test->parameters + slot * test->parameter_stride, parameters, sizeof(*parameters));
   return 1;
}

static int rpy_gl_dispatch(void *userdata, const rpyrowave_dwt_parameters_t *parameters,
      unsigned source, unsigned x, unsigned y, unsigned z)
{
   rpy_gl_test_t *test = (rpy_gl_test_t*)userdata;
   unsigned slot = (parameters->inverse ? 50 : 0) + test->cursor++;
   test->api.BindBufferBase(RPY_GL_STORAGE, 0, test->buffers[source]);
   test->api.BindBufferBase(RPY_GL_STORAGE, 1, test->buffers[source ^ 1]);
   test->api.BindBufferRange(RPY_GL_UNIFORM, 2, test->buffers[2],
         (ptrdiff_t)(slot * test->parameter_stride), sizeof(*parameters));
   test->api.DispatchCompute(x, y, z);
   test->api.MemoryBarrier(RPY_GL_STORAGE_BARRIER);
   return 1;
}

static int rpy_gl_execute(rpy_gl_test_t *test, const rpy_gpu_vector_t *vector, unsigned inverse)
{
   test->cursor = 0;
   return rpyrowave_dwt_record(vector->aligned_width, vector->aligned_height,
         vector->sequence.chroma_444, inverse, rpy_gl_dispatch, test) == 0
      && glGetError() == GL_NO_ERROR;
}

static int rpy_gl_setup(rpy_gl_test_t *test, const rpy_gpu_vector_t *vector, const char *shader)
{
   GLint alignment;
   unsigned mode;
   if (!rpy_gl_context(test) || !rpy_gl_functions(test) || !rpy_gl_program(test, shader))
      return 0;
   glGetIntegerv(0x8a34, &alignment);
   if (alignment < 1)
      return 0;
   test->parameter_stride = (sizeof(rpyrowave_dwt_parameters_t) + alignment - 1)
      / alignment * alignment;
   test->parameters = (uint8_t*)calloc(100, test->parameter_stride);
   if (!test->parameters)
      return 0;
   for (mode = 0; mode < 2; mode++)
   {
      test->cursor = 0;
      if (rpyrowave_dwt_record(vector->aligned_width, vector->aligned_height,
               vector->sequence.chroma_444, mode, rpy_gl_parameters, test))
         return 0;
   }
   test->api.GenBuffers(3, test->buffers);
   for (mode = 0; mode < 2; mode++)
   {
      test->api.BindBuffer(RPY_GL_STORAGE, test->buffers[mode]);
      test->api.BufferData(RPY_GL_STORAGE, (ptrdiff_t)vector->bytes,
            mode ? NULL : vector->atlas, RPY_GL_DYNAMIC_COPY);
   }
   test->api.BindBuffer(RPY_GL_UNIFORM, test->buffers[2]);
   test->api.BufferData(RPY_GL_UNIFORM, (ptrdiff_t)(test->parameter_stride * 100),
         test->parameters, RPY_GL_DYNAMIC_COPY);
   return glGetError() == GL_NO_ERROR;
}

static void rpy_gl_free(rpy_gl_test_t *test)
{
   if (test->api.DeleteBuffers && test->buffers[0])
      test->api.DeleteBuffers(3, test->buffers);
   if (test->api.DeleteProgram && test->program)
      test->api.DeleteProgram(test->program);
#if defined(_WIN32)
   wglMakeCurrent(NULL, NULL);
   if (test->context)
      wglDeleteContext(test->context);
   if (test->dc)
      ReleaseDC(test->window, test->dc);
   if (test->window)
      DestroyWindow(test->window);
#else
   if (test->display != EGL_NO_DISPLAY)
   {
      eglMakeCurrent(test->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (test->context != EGL_NO_CONTEXT)
         eglDestroyContext(test->display, test->context);
      if (test->surface != EGL_NO_SURFACE)
         eglDestroySurface(test->display, test->surface);
      eglTerminate(test->display);
   }
#endif
   free(test->parameters);
}


#endif
