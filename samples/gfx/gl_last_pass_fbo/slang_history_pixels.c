/* Real GL textures through the shipping GLCore Slang history lifecycle. */
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/common/media_history.h"
#include "gfx/common/media_history_ring.h"
#include "gfx/video_defines.h"
#include "texture.inc"
typedef struct gl3_filter_chain gl3_filter_chain_t;
typedef struct gl3_filter_chain_texture gl3_filter_chain_texture;
typedef struct { struct gl3_filter_chain_texture texture;unsigned filter,mip_filter,address; } gl3_texture_t;
struct gl3_framebuffer { GLuint image;unsigned size_dims;GLenum format;GLuint framebuffer;bool complete; };
struct gl3_history_lease { struct gl3_filter_chain_texture texture;bool retired; };
struct gl3_filter_chain {
   struct gl3_framebuffer **original_history,*copy_framebuffer;
   size_t num_original_history;
   gfx_media_history_ring_t history_ring;
   gfx_media_history_t *media_history;
   struct gl3_filter_chain_texture input_texture;
   struct gl3_history_lease *history_leases;
   bool borrow_input;
   void *passes[1];
   struct { gl3_texture_t original_history[3];size_t num_original_history;
      unsigned quad_program,quad_vbo,partial_vbo;struct { unsigned flat_ubo_vertex; } quad_loc; } common;
};
static unsigned copies;
static void counted_copy(GLenum t,GLint l,GLint xo,GLint yo,GLint x,GLint y,GLsizei w,GLsizei h)
{ copies++;glCopyTexSubImage2D(t,l,xo,yo,x,y,w,h); }
static void gl3_framebuffer_set_size(struct gl3_framebuffer *fb,unsigned dims,GLenum format)
{
   if(fb->image)glDeleteTextures(1,&fb->image);
   if(!fb->framebuffer)glGenFramebuffers(1,&fb->framebuffer);
   glGenTextures(1,&fb->image);glBindTexture(GL_TEXTURE_2D,fb->image);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D,0,format,VIDEO_SCALE_W(dims),VIDEO_SCALE_H(dims),0,GL_RGBA,GL_FLOAT,NULL);
   glBindFramebuffer(GL_FRAMEBUFFER,fb->framebuffer);
   glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,fb->image,0);
   fb->complete=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
   fb->size_dims=dims;fb->format=format;
}
static unsigned gl3_pass_get_source_filter(void *p){(void)p;return 0;}
static unsigned gl3_pass_get_mip_filter(void *p){(void)p;return 0;}
static unsigned gl3_pass_get_address_mode(void *p){(void)p;return 0;}
static void gl3_framebuffer_copy(GLuint f,GLuint p,GLuint v,GLint u,unsigned w,unsigned h,GLuint s)
{ (void)f;(void)p;(void)v;(void)u;(void)w;(void)h;(void)s;assert(!"unexpected shader snapshot"); }
static void gl3_framebuffer_copy_partial(GLuint f,GLuint p,GLuint v,GLint u,unsigned w,unsigned h,GLuint s,float x,float y,float rw,float rh)
{ (void)x;(void)y;(void)rw;(void)rh;gl3_framebuffer_copy(f,p,v,u,w,h,s); }
#define glCopyTexSubImage2D counted_copy
#include "slang_history.inc"
#undef glCopyTexSubImage2D
#include "gfx/video_shader_parse.h"
#include "gfx/common/gl_media_pool.h"
static GLuint make_texture(GLenum format,const float *pixels)
{
   GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D,0,format,4,3,0,GL_RGBA,GL_FLOAT,pixels);return t;
}
static void expect(GLuint image,const float *pixels)
{
   GLuint fb;float seen[48];unsigned i;
   glGenFramebuffers(1,&fb);glBindFramebuffer(GL_FRAMEBUFFER,fb);
   glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,image,0);
   assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
   glReadPixels(0,0,4,3,GL_RGBA,GL_FLOAT,seen);
   for(i=0;i<48;i++)assert(fabsf(seen[i]-pixels[i])<0.005f);
   glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&fb);
}
static void draw(struct gl3_filter_chain *c,gfx_media_history_t *h,const struct gl3_filter_chain_texture *t)
{ c->input_texture=*t;c->media_history=h;gl3_chain_end_frame(c);gl3_chain_update_history_info(c); }
static void run(GLenum format)
{
   struct gl3_filter_chain c;struct gl3_framebuffer frames[3],*slots[3];
   struct gl3_history_lease leases[3];struct gl3_filter_chain_texture t;
   gfx_media_history_t history;unsigned storage[6],i,j,before;GLuint source,next;
   float first[48],second[48];GLint read,draw_fb,bound,active;GLuint sentinels[2];
   memset(&c,0,sizeof(c));memset(frames,0,sizeof(frames));memset(leases,0,sizeof(leases));
   memset(&t,0,sizeof(t));memset(&history,0,sizeof(history));history.managed=true;
   for(i=0;i<48;i++){first[i]=(i%4==3)?1.0f:(float)(i%7)/8.0f;second[i]=first[i]*0.5f;}
   if(format==GL_RGBA16F){first[0]=-0.5f;first[1]=1.5f;second[8]=-0.25f;second[9]=3.0f;}
   for(i=0;i<3;i++)slots[i]=&frames[i];
   c.original_history=slots;c.num_original_history=c.common.num_original_history=3;c.history_leases=leases;
   gfx_media_history_ring_init(&c.history_ring,storage,3);gl3_filter_chain_set_history_borrow(&c,true);
   t.image=source=make_texture(format,first);t.dims=t.padded_dims=VIDEO_SCALE_PACK(4,3);t.format=format;
   before=copies;
   for(i=0;i<5;i++)draw(&c,&history,&t);
   assert(copies==before);
   for(i=0;i<3;i++){assert(c.common.original_history[i].texture.image==source);expect(source,first);}
   glGenFramebuffers(2,sentinels);glBindFramebuffer(GL_READ_FRAMEBUFFER,sentinels[0]);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,sentinels[1]);
   glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,source);
   assert(gl3_filter_chain_preserve_history(&c,source));assert(copies==before+1);
   glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw_fb);
   glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);
   assert((GLuint)read==sentinels[0] && (GLuint)draw_fb==sentinels[1] && active==GL_TEXTURE3 && (GLuint)bound==source);
   glTexSubImage2D(GL_TEXTURE_2D,0,0,0,4,3,GL_RGBA,GL_FLOAT,second);
   gl3_chain_update_history_info(&c);
   for(i=0;i<3;i++)expect(c.common.original_history[i].texture.image,first);
   history.copies=0;before=copies;
   for(i=0;i<3;i++)
   {
      draw(&c,&history,&t);assert(copies==before);
      for(j=0;j<3;j++)expect(c.common.original_history[j].texture.image,j<=i?second:first);
   }
   assert(gl3_filter_chain_retire_history(&c,source) && glIsTexture(source));
   t.image=next=make_texture(format,first);history.copies=0;
   for(i=0;i<3;i++)draw(&c,&history,&t);
   assert(copies==before && !glIsTexture(source));
   gl3_chain_release_history_leases(&c,true);assert(glIsTexture(next));
   glDeleteTextures(1,&next);glDeleteFramebuffers(2,sentinels);
   for(i=0;i<3;i++){if(frames[i].image)glDeleteTextures(1,&frames[i].image);if(frames[i].framebuffer)glDeleteFramebuffers(1,&frames[i].framebuffer);}
   assert(glGetError()==GL_NO_ERROR);
}
static bool pool_reference(void *context,GLuint image)
{return gl3_filter_chain_history_references((gl3_filter_chain_t*)context,image);}
static bool pool_retire(void *context,GLuint image)
{return gl3_filter_chain_retire_history((gl3_filter_chain_t*)context,image);}
static void run_pool(GLenum format)
{
   struct gl3_filter_chain c;struct gl3_framebuffer frames[3],*slots[3];
   struct gl3_history_lease leases[3];struct gl3_filter_chain_texture t;
   gfx_media_history_t history;gl_media_pool_t *pool=NULL;
   unsigned storage[6],i,j,frame,before=copies,names=0,count;
   GLuint handle,target,versions[GFX_MAX_FRAME_HISTORY+2];float fresh[48],expected[3][48];
   memset(&c,0,sizeof(c));memset(frames,0,sizeof(frames));memset(leases,0,sizeof(leases));
   memset(&t,0,sizeof(t));memset(&history,0,sizeof(history));history.managed=true;
   for(i=0;i<48;i++)fresh[i]=(i%4==3)?1.0f:(float)(i%7)/8.0f;
   if(format==GL_RGBA16F){fresh[0]=-0.5f;fresh[1]=1.5f;}
   for(i=0;i<3;i++){slots[i]=&frames[i];memcpy(expected[i],fresh,sizeof(fresh));}
   c.original_history=slots;c.num_original_history=c.common.num_original_history=3;c.history_leases=leases;
   gfx_media_history_ring_init(&c.history_ring,storage,3);gl3_filter_chain_set_history_borrow(&c,true);
   t.image=handle=make_texture(format,fresh);t.dims=t.padded_dims=VIDEO_SCALE_PACK(4,3);t.format=format;
   for(i=0;i<3;i++)draw(&c,&history,&t);
   for(frame=1;frame<=24;frame++)
   {
      fresh[4]=(float)(frame%5)*0.125f;
      if(frame==12)
      {
         const GLint filter[4]={GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT};
         glBindTexture(GL_TEXTURE_2D,gl_media_pool_resolve(pool,handle));
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filter[0]);
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filter[1]);
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,filter[2]);
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,filter[3]);
         gl_media_pool_sync_sampler(pool,filter);
      }

      target=gl_media_pool_prepare(&pool,handle,4,3,pool_reference,&c);assert(target);
      if(frame>=12)
      {
         GLint mag;glBindTexture(GL_TEXTURE_2D,target);
         glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&mag);assert(mag==GL_NEAREST);
      }

      glBindTexture(GL_TEXTURE_2D,target);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,4,3,GL_RGBA,GL_FLOAT,fresh);
      gl_media_pool_publish(pool,handle,target);t.image=gl_media_pool_resolve(pool,handle);history.copies=0;
      for(j=2;j;j--)memmove(expected[j],expected[j-1],sizeof(fresh));
      memcpy(expected[0],fresh,sizeof(fresh));
      draw(&c,&history,&t);assert(copies==before);
      for(j=0;j<3;j++)expect(c.common.original_history[j].texture.image,expected[j]);
      if(frame==8)names=pool->count;
      if(frame>8)assert(pool->count==names);
      assert(pool->count<=4);
   }
   count=pool->count;for(i=0;i<count;i++)versions[i]=pool->versions[i];
   assert(gl_media_pool_forget(&pool,handle,pool_retire,&c));
   gl3_chain_release_history_leases(&c,true);gl_media_pool_free(&pool);
   for(i=0;i<count;i++)assert(!glIsTexture(versions[i]));
   for(i=0;i<3;i++){if(frames[i].image)glDeleteTextures(1,&frames[i].image);if(frames[i].framebuffer)glDeleteFramebuffers(1,&frames[i].framebuffer);}
   assert(glGetError()==GL_NO_ERROR);
}
int main(void)
{
   EGLDisplay d;EGLConfig cfg;EGLSurface s;EGLContext c;EGLint count;
   const EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_NONE};
   const EGLint size[]={EGL_WIDTH,4,EGL_HEIGHT,3,EGL_NONE};
   PFNEGLGETPLATFORMDISPLAYEXTPROC platform=(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
   d=platform?platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL):eglGetDisplay(EGL_DEFAULT_DISPLAY);
   assert(eglInitialize(d,NULL,NULL));assert(eglBindAPI(EGL_OPENGL_API));assert(eglChooseConfig(d,attrs,&cfg,1,&count)&&count);
   s=eglCreatePbufferSurface(d,cfg,size);c=eglCreateContext(d,cfg,EGL_NO_CONTEXT,NULL);assert(eglMakeCurrent(d,s,s,c));
   run(GL_RGBA8);run(GL_RGBA16F);run_pool(GL_RGBA8);run_pool(GL_RGBA16F);
   eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(d,c);eglDestroySurface(d,s);eglTerminate(d);
   puts("GLCore Slang history GPU pixels: direct leases, raw preservation, HDR precision, GL state and deferred deletion passed");return 0;
}
