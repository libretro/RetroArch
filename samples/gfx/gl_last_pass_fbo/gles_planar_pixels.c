/* The same reference comparison on real GLES2 and GLES3 contexts. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#ifdef HAVE_OPENGLES3
#include <GLES3/gl3.h>
#else
#include <GLES2/gl2.h>
#endif
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/image.h>
#include <formats/image_yuv_blit.h>
static unsigned draws;
static GLuint program;
static void counted_draw(GLenum mode, GLint first, GLsizei count)
{ draws++; glDrawArrays(mode,first,count); }
#define HAVE_GL_PLANAR 1
#define glDrawArrays counted_draw
#include "planar.inc"
#undef glDrawArrays
static GLuint compile(GLenum stage,const char *source)
{
   GLuint shader=glCreateShader(stage);GLint ok;
   glShaderSource(shader,1,&source,NULL);glCompileShader(shader);
   glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);assert(ok);return shader;
}
static GLuint texture(GLint format,const float *data)
{
   GLuint name;glGenTextures(1,&name);glBindTexture(GL_TEXTURE_2D,name);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
   assert(!data);glTexImage2D(GL_TEXTURE_2D,0,format,4,3,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
   return name;
}
#include "planar_direct_pixels.h"
int main(void)
{
   const char *vs="attribute vec2 pos,uv;varying vec2 current;void main(){gl_Position=vec4(pos*2.-1.,0,1);current=uv;}";
   const char *fs="precision highp float;varying vec2 current;uniform sampler2D image;void main(){gl_FragColor=texture2D(image,current);}";
   PFNEGLGETPLATFORMDISPLAYEXTPROC platform=(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
   EGLDisplay display=platform?platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL):eglGetDisplay(EGL_DEFAULT_DISPLAY);
   EGLConfig config;EGLSurface surface;EGLContext context;EGLint count;
   EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_NONE};
   EGLint size[]={EGL_WIDTH,4,EGL_HEIGHT,3,EGL_NONE};
#ifdef HAVE_OPENGLES3
   EGLint version[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
   bool core=true;
#else
   EGLint version[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
   bool core=false;
#endif
   GLuint vertex,fragment;unsigned layout,linear;
   assert(eglInitialize(display,NULL,NULL));assert(eglBindAPI(EGL_OPENGL_ES_API));
   assert(eglChooseConfig(display,attributes,&config,1,&count)&&count);
   surface=eglCreatePbufferSurface(display,config,size);
   context=eglCreateContext(display,config,EGL_NO_CONTEXT,version);
   assert(eglMakeCurrent(display,surface,surface,context));
   vertex=compile(GL_VERTEX_SHADER,vs);fragment=compile(GL_FRAGMENT_SHADER,fs);
   program=glCreateProgram();glAttachShader(program,vertex);glAttachShader(program,fragment);
   glBindAttribLocation(program,0,"pos");glBindAttribLocation(program,1,"uv");glLinkProgram(program);
   for(layout=0;layout<3;layout++)for(linear=0;linear<2;linear++)
      run_planar_direct(core,layout,linear!=0,GL_RGBA);
   glDeleteProgram(program);glDeleteShader(vertex);glDeleteShader(fragment);
   eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
   eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);
   puts("GLES direct YUV: separate/NV12/NV21, nearest/linear, crop, updates and RGB fallback PASS");
   return 0;
}
