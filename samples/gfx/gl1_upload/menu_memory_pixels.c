#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#include <boolean.h>
#include <memalign.h>
#include <retro_math.h>
#include <gfx/video_frame.h>
#include <gfx/scaler/pixconv.h>
#include "../../../gfx/video_defines.h"
#include "memory_type.inc"
#include "flags.inc"
#include "source_format.inc"
typedef struct {
   gl1_menu_mem_t *menu_mem;
   unsigned char *menu_frame,*menu_video_buf;
   const unsigned char *menu_source;
   uint8_t *swizzle_buf;
   size_t menu_frame_cap,swizzle_cap;
   unsigned flags,menu_dims,menu_pitch,menu_bits,rotation;
   unsigned tex_store_dims,tex_store_fmt,menu_tex_store_dims,menu_tex_store_fmt;
   GLuint tex,menu_tex;
   struct {unsigned pos,dims;} vp;
   bool frame_menu_linear_filter;
} gl1_t;
typedef struct { unsigned dims; } video_frame_info_t;
static bool gl1_source_10bit_native(gl1_t *gl1){(void)gl1;return false;}
#define GET_POT(x) next_pow2(x)
#include "memory.inc"
#include "upload.inc"
static void draw(gl1_t *gl1)
{
   const void *frame_to_copy=NULL;
   bool menu_is_alive=true,do_swap=false;
   unsigned width=0,height=0,pitch=0,bits=0,pot_width=0,pot_height=0,src_row=0;
   video_frame_info_t info,*video_info=&info;
   info.dims=VIDEO_SCALE_PACK(8,8);
#include "draw.inc"
   (void)do_swap;
}
static void pattern(uint16_t *pixels,unsigned phase)
{unsigned i;for(i=0;i<15;i++)pixels[i]=(uint16_t)((((i+phase)&15)<<12)|(((i*3+phase)&15)<<8)|(((i*7+phase)&15)<<4)|(i&15));}
static void expect(gl1_t *gl1,const uint16_t *pixels,bool packed)
{
   uint8_t result[8*4*4];unsigned x,y;
   glBindTexture(GL_TEXTURE_2D,gl1->menu_tex);
   glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,result);
   for(y=0;y<4;y++)for(x=0;x<8;x++)
   {
      const uint8_t *p=result+(y*8+x)*4;
      uint16_t value=y<3&&x<5?pixels[y*5+x]:0;
      assert(p[0]==((value>>12)&15)*17&&p[1]==((value>>8)&15)*17&&p[2]==((value>>4)&15)*17);
      assert(p[3]==(packed?(value&15)*17:255));
   }
   assert(glGetError()==GL_NO_ERROR);
}
int main(void)
{
   unsigned mode,frame;gl1_t gl;uintptr_t ids[2];uint16_t *pixels[2],copy[15];
#ifdef _WIN32
   HWND window;HDC dc;HGLRC context;PIXELFORMATDESCRIPTOR format;
   window=CreateWindowExA(0,"STATIC","GL1 menu test",WS_POPUP,0,0,8,8,NULL,NULL,GetModuleHandle(NULL),NULL);
   assert(window);dc=GetDC(window);memset(&format,0,sizeof(format));format.nSize=sizeof(format);
   format.nVersion=1;format.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;format.iPixelType=PFD_TYPE_RGBA;format.cColorBits=32;
   assert(SetPixelFormat(dc,ChoosePixelFormat(dc,&format),&format));context=wglCreateContext(dc);
   assert(context&&wglMakeCurrent(dc,context));
#else
   EGLDisplay display;EGLConfig config;EGLSurface surface;EGLContext context;EGLint count;
   const EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_NONE};
   const EGLint size[]={EGL_WIDTH,8,EGL_HEIGHT,8,EGL_NONE};
   PFNEGLGETPLATFORMDISPLAYEXTPROC platform=(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
   display=platform?platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL):eglGetDisplay(EGL_DEFAULT_DISPLAY);
   assert(eglInitialize(display,NULL,NULL)&&eglBindAPI(EGL_OPENGL_API));
   assert(eglChooseConfig(display,attributes,&config,1,&count)&&count);
   surface=eglCreatePbufferSurface(display,config,size);context=eglCreateContext(display,config,EGL_NO_CONTEXT,NULL);
   assert(eglMakeCurrent(display,surface,surface,context));
#endif
   printf("GL1 menu GPU: %s\n",glGetString(GL_RENDERER));
   for(mode=0;mode<3;mode++)
   {
      memset(&gl,0,sizeof(gl));gl.vp.dims=VIDEO_SCALE_PACK(8,8);
      gl.flags=(mode==0?GL1_FLAG_SUPPORTS_PACKED_PIXELS:0)|(mode<2?GL1_FLAG_SUPPORTS_BGRA:0);
      glGenTextures(1,&gl.menu_tex);
      pixels[0]=(uint16_t*)gl1_menu_mem_alloc(&gl,60,&ids[0]);
      pixels[1]=(uint16_t*)gl1_menu_mem_alloc(&gl,60,&ids[1]);assert(pixels[0]&&pixels[1]);
      for(frame=0;frame<24;frame++)
      {
         unsigned slot=frame&1;assert(gl1_menu_mem_ready(&gl,ids[slot]));
         pattern(pixels[slot],frame);memcpy(copy,pixels[slot],sizeof(copy));
         gl1_set_texture_frame(&gl,pixels[slot],false,VIDEO_SCALE_PACK(5,3),1);
         assert(gl.menu_source==(uint8_t*)pixels[slot]&&!gl.menu_frame);
         assert(!gl1_menu_mem_ready(&gl,ids[slot]));draw(&gl);expect(&gl,copy,mode==0);
         draw(&gl);expect(&gl,copy,mode==0);assert(!gl1_menu_mem_ready(&gl,ids[slot]));
      }
      pattern(copy,7);gl1_set_texture_frame(&gl,copy,false,VIDEO_SCALE_PACK(5,3),1);
      assert(gl.menu_source==gl.menu_frame);draw(&gl);expect(&gl,copy,mode==0);
      assert(gl1_menu_mem_ready(&gl,ids[0])&&gl1_menu_mem_ready(&gl,ids[1]));
      gl1_set_texture_frame(&gl,pixels[0],false,VIDEO_SCALE_PACK(5,3),1);
      gl1_menu_mem_free(&gl,ids[0]);assert(!gl.menu_source);draw(&gl);
      gl1_menu_mem_free(&gl,ids[1]);assert(!gl.menu_mem);glDeleteTextures(1,&gl.menu_tex);
      free(gl.menu_frame);free(gl.menu_video_buf);free(gl.swizzle_buf);assert(glGetError()==GL_NO_ERROR);
   }
#ifdef _WIN32
   wglMakeCurrent(NULL,NULL);wglDeleteContext(context);ReleaseDC(window,dc);DestroyWindow(window);
#else
   eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);
#endif
   puts("PASS GL1 menu GPU pixels: borrowed 4444, expansion, swizzle, retained redraws and retirement");return 0;
}
