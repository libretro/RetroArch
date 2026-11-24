/* Real EGL/GL pixels through the shared GLSL/Cg viewer history code. */
#ifdef _WIN32
#include <windows.h>
#include <glsym/glsym.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <gfx/math/matrix_4x4.h>
#include "../../../gfx/video_driver.h"
static unsigned draws, copies, program, show_previous, previous_index;
static bool smooth_mode = true;
static bool mipmap_mode;
static void counted_draw(GLenum mode, GLint first, GLsizei count)
{ draws++; glDrawArrays(mode,first,count); }
static void counted_copy(GLenum target, GLint level, GLint xoff, GLint yoff,
      GLint x, GLint y, GLsizei width, GLsizei height)
{ copies++; glCopyTexSubImage2D(target,level,xoff,yoff,x,y,width,height); }
#define MEDIA_GEN_FB glGenFramebuffers
#define MEDIA_DELETE_FB glDeleteFramebuffers
#define MEDIA_BIND_FB(id) glBindFramebuffer(GL_FRAMEBUFFER,id)
#define MEDIA_FB_BINDING GL_FRAMEBUFFER_BINDING
#define MEDIA_ATTACH(tex) glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0)
#define MEDIA_FB_COMPLETE() (glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE)
#define glDrawArrays counted_draw
#define glCopyTexSubImage2D counted_copy
#define gl_media_shader_references gl_media_shader_references
#include "../../../gfx/common/gl_media_shader.h"
#ifdef GL_POOL_LEGACY
static void legacy_pool_integer(GLenum name,GLint *value)
{assert(name!=GL_PIXEL_UNPACK_BUFFER_BINDING);glGetIntegerv(name,value);}
static void legacy_pool_parameter(GLenum target,GLenum name,GLint *value)
{assert(name!=GL_TEXTURE_SWIZZLE_R && name!=GL_TEXTURE_SWIZZLE_G && name!=GL_TEXTURE_SWIZZLE_B && name!=GL_TEXTURE_SWIZZLE_A);glGetTexParameteriv(target,name,value);}
static void legacy_pool_buffer(GLenum target,GLuint buffer)
{(void)target;(void)buffer;assert(!"legacy pool used a pixel buffer API");}
#define glGetIntegerv legacy_pool_integer
#define glGetTexParameteriv legacy_pool_parameter
#undef glBindBuffer
#define glBindBuffer legacy_pool_buffer
#endif
#include "../../../gfx/common/gl_media_pool.h"
#ifdef GL_POOL_LEGACY
#undef glGetIntegerv
#undef glGetTexParameteriv
#undef glBindBuffer
#ifdef _WIN32
#define glBindBuffer __rglgen_glBindBuffer
#endif
#endif
#undef glDrawArrays
#undef glCopyTexSubImage2D
#include <formats/image.h>
#include <formats/image_yuv_blit.h>
#define HAVE_GL_PLANAR 1
#define glDrawArrays counted_draw
#include "planar.inc"
#undef glDrawArrays
static GLuint compile(GLenum type,const char *source)
{
   GLint ok;GLuint shader=glCreateShader(type);glShaderSource(shader,1,&source,NULL);glCompileShader(shader);
   glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);assert(ok);return shader;
}
static void use(void *driver,void *data,unsigned pass,bool active)
{
   (void)driver;(void)data;(void)active;glUseProgram(program);
   glUniform1i(glGetUniformLocation(program,"pick"),pass==1 && show_previous);
}
static unsigned passes(void *data) {(void)data;return 1;}
static unsigned history(void *data) {(void)data;return 2;}
static bool filter(void *data,unsigned pass,bool *smooth) {(void)data;(void)pass;*smooth=smooth_mode;return true;}
static enum gfx_wrap_type wrap(void *data,unsigned pass) {(void)data;(void)pass;return RARCH_WRAP_EDGE;}
static void scale(void *data,unsigned pass,struct gfx_fbo_scale *out) {(void)data;(void)pass;memset(out,0,sizeof(*out));}
static bool mipmap(void *data,unsigned pass) {(void)data;(void)pass;return mipmap_mode;}
static bool feedback(void *data,unsigned *pass) {(void)data;(void)pass;return false;}
static bool mvp(void *data,const void *matrix) {(void)data;(void)matrix;return true;}
static bool colour(void *data,const float *rows) {(void)data;(void)rows;return true;}
static bool coords(void *data,const struct video_coords *c)
{
   (void)data;glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,c->vertex);glEnableVertexAttribArray(0);
   glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,0,c->tex_coord);glEnableVertexAttribArray(1);return true;
}
static void params(void *data,void *backend)
{
   const video_shader_ctx_params_t *p=(const video_shader_ctx_params_t*)data;
   const struct video_tex_info *prev=(const struct video_tex_info*)p->prev_info;
   (void)backend;glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,prev[previous_index].tex);
   glUniform1i(glGetUniformLocation(program,"old"),1);
   glUniform1f(glGetUniformLocation(program,"flip"),prev[previous_index].coord[1]);
   glActiveTexture(GL_TEXTURE0);glUniform1i(glGetUniformLocation(program,"image"),0);
}
static void pixels(float *out,float phase)
{
   unsigned x,y;for(y=0;y<3;y++)for(x=0;x<4;x++)
   { unsigned i=(y*4+x)*4;out[i]=phase+x*0.125f;out[i+1]=y*0.25f;out[i+2]=phase < 0 ? 1.5f : 0.5f;out[i+3]=1; }
}
static void expect(const float *source)
{
   float output[48];unsigned x,y,c;glReadPixels(0,0,4,3,GL_RGBA,GL_FLOAT,output);
   for(y=0;y<3;y++)for(x=0;x<4;x++)for(c=0;c<4;c++)
      assert(fabsf(output[(y*4+x)*4+c]-source[((2-y)*4+x)*4+c])<0.005f);
   assert(glGetError()==GL_NO_ERROR);
}
static GLuint texture(GLint format,const float *data)
{
   GLuint tex;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D,0,format,4,3,0,GL_RGBA,GL_FLOAT,data);return tex;
}
static void run(GLint format,float phase)
{
   shader_backend_t backend;gl_media_shader_t *state=NULL;GLuint src,next;unsigned i,before,copy_before;
   GLint sampler;float first[48],second[48],third[48];
   const float rows[12]={1,0,0,0,0,1,0,0,0,0,1,0};
   const float *hdr=format==GL_RGBA16F ? rows : NULL;
   memset(&backend,0,sizeof(backend));backend.use=use;backend.num_shaders=passes;backend.get_prev_textures=history;
   backend.filter_type=filter;backend.wrap_type=wrap;backend.shader_scale=scale;backend.mipmap_input=mipmap;
   backend.get_feedback_pass=feedback;backend.set_coords=coords;backend.set_mvp=mvp;backend.set_params=params;
   backend.set_media_colour=colour;
   pixels(first,phase);pixels(second,phase+0.125f);pixels(third,phase+0.25f);
   src=texture(format,first);show_previous=0;previous_index=0;
   for(i=0;i<4;i++)
   {
      before=draws;assert(gl_media_shader_draw(&state,&backend,&backend,&backend,src,
            VIDEO_SCALE_PACK(4,3),0,VIDEO_SCALE_PACK(4,3),i,hdr,true));
      assert(draws==before+1);expect(first);
   }
   show_previous=1;
   assert(gl_media_shader_draw(&state,&backend,&backend,&backend,src,VIDEO_SCALE_PACK(4,3),0,VIDEO_SCALE_PACK(4,3),5,hdr,true));expect(first);
   glBindTexture(GL_TEXTURE_2D,src);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&sampler);assert(sampler==GL_NEAREST);
   copy_before=copies;assert(gl_media_shader_preserve(state,src));assert(copies==copy_before+1);
   glBindTexture(GL_TEXTURE_2D,src);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,4,3,GL_RGBA,GL_FLOAT,second);
   gl_media_shader_changed(state,src);
   assert(gl_media_shader_draw(&state,&backend,&backend,&backend,src,VIDEO_SCALE_PACK(4,3),0,VIDEO_SCALE_PACK(4,3),6,hdr,true));expect(first);
   previous_index=1;
   assert(gl_media_shader_draw(&state,&backend,&backend,&backend,src,VIDEO_SCALE_PACK(4,3),0,VIDEO_SCALE_PACK(4,3),7,hdr,true));expect(first);
   previous_index=0;
   assert(gl_media_shader_draw(&state,&backend,&backend,&backend,src,VIDEO_SCALE_PACK(4,3),0,VIDEO_SCALE_PACK(4,3),8,hdr,true));expect(second);
   assert(gl_media_shader_retire(state,src) && glIsTexture(src));
   next=texture(format,third);
   glBindTexture(GL_TEXTURE_2D,src); /* deletion must not resurrect a saved binding */
   assert(gl_media_shader_draw(&state,&backend,&backend,&backend,next,VIDEO_SCALE_PACK(4,3),0,VIDEO_SCALE_PACK(4,3),9,hdr,true));expect(second);
   assert(gl_media_shader_draw(&state,&backend,&backend,&backend,next,VIDEO_SCALE_PACK(4,3),0,VIDEO_SCALE_PACK(4,3),10,hdr,true));expect(third);
   assert(!glIsTexture(src));
   copy_before=copies;smooth_mode=false;
   assert(gl_media_shader_draw(&state,&backend,&backend,&backend,next,VIDEO_SCALE_PACK(4,3),0,VIDEO_SCALE_PACK(4,3),11,hdr,true));expect(third);
   assert(copies==copy_before+1);smooth_mode=true;
   gl_media_shader_free(&state);assert(glIsTexture(next));glDeleteTextures(1,&next);
}
static bool pool_reference(void *context,GLuint image)
{return gl_media_shader_references(*(gl_media_shader_t**)context,image);}
static bool pool_retire(void *context,GLuint image)
{return gl_media_shader_retire(*(gl_media_shader_t**)context,image);}
static GLuint prepare_pool(gl_media_pool_t **pool,GLuint handle,gl_media_shader_t **state)
{
#ifdef GL_POOL_LEGACY
   return gl_media_pool_prepare_ex(pool,handle,4,3,pool_reference,state,GL_MEDIA_POOL_MUTABLE_MIPS);
#else
   return gl_media_pool_prepare(pool,handle,4,3,pool_reference,state);
#endif
}
static void run_pool(GLint format,float phase)
{
   shader_backend_t backend;gl_media_shader_t *state=NULL;gl_media_pool_t *pool=NULL;
   float old[48],fresh[48],sampled[48];GLuint handle,target,pbo,versions[GFX_MAX_FRAME_HISTORY+2];
   unsigned frame,i,names=0,before=copies,count;
   GLint unpack,bound;
   const float rows[12]={1,0,0,0,0,1,0,0,0,0,1,0};
   const float *hdr=format==GL_RGBA16F?rows:NULL;
   memset(&backend,0,sizeof(backend));backend.use=use;backend.num_shaders=passes;backend.get_prev_textures=history;
   backend.filter_type=filter;backend.wrap_type=wrap;backend.shader_scale=scale;backend.mipmap_input=mipmap;
   backend.get_feedback_pass=feedback;backend.set_mvp=mvp;backend.set_media_colour=colour;
   backend.set_coords=coords;backend.set_params=params;
   pixels(old,phase);handle=texture(format,old);show_previous=0;previous_index=0;smooth_mode=true;
#ifndef GL_POOL_LEGACY
   /* GLCore packs BGRA words via a swizzle; old GL uploads BGRA natively. */
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_R,GL_BLUE);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_B,GL_RED);
#endif
   for(i=0;i<3;i++)assert(gl_media_shader_draw(&state,&backend,&backend,&backend,handle,VIDEO_SCALE_PACK(4,3),0,VIDEO_SCALE_PACK(4,3),i,hdr,true));
   assert(copies==before);
#ifndef GL_POOL_LEGACY
   glGenBuffers(1,&pbo);
#else
   pbo=0;
#endif
   for(frame=1;frame<=24;frame++)
   {
      pixels(fresh,phase+(float)(frame%3)*0.0625f);
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

#ifndef GL_POOL_LEGACY
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);glBufferData(GL_PIXEL_UNPACK_BUFFER,sizeof(fresh),fresh,GL_STREAM_DRAW);
#endif
      glBindTexture(GL_TEXTURE_2D,gl_media_pool_resolve(pool,handle));
      target=prepare_pool(&pool,handle,&state);assert(target);
      if(frame>=12)
      {
         GLint mag,saved;glGetIntegerv(GL_TEXTURE_BINDING_2D,&saved);glBindTexture(GL_TEXTURE_2D,target);
         glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&mag);assert(mag==GL_NEAREST);
         glBindTexture(GL_TEXTURE_2D,(GLuint)saved);
      }

#ifndef GL_POOL_LEGACY
      glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack);
#else
      unpack=0;
#endif
      glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);
      assert((GLuint)unpack==pbo && (GLuint)bound==gl_media_pool_resolve(pool,handle));
      /* A refused/dropped upload does not publish the reserved destination. */
      if(frame==1){assert(gl_media_pool_resolve(pool,handle)==handle);assert(prepare_pool(&pool,handle,&state)==target);}
      glBindTexture(GL_TEXTURE_2D,target);
#ifdef GL_POOL_LEGACY
      glTexSubImage2D(GL_TEXTURE_2D,0,0,0,4,3,GL_RGBA,GL_FLOAT,fresh);
#else
      glTexSubImage2D(GL_TEXTURE_2D,0,0,0,4,3,GL_RGBA,GL_FLOAT,NULL);
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
#endif
      gl_media_pool_publish(pool,handle,target);
      show_previous=1;
      assert(gl_media_shader_draw(&state,&backend,&backend,&backend,gl_media_pool_resolve(pool,handle),VIDEO_SCALE_PACK(4,3),0,VIDEO_SCALE_PACK(4,3),frame+3,hdr,true));
      memcpy(sampled,old,sizeof(old));
#ifndef GL_POOL_LEGACY
      for(i=0;i<12;i++){sampled[i*4]=old[i*4+2];sampled[i*4+2]=old[i*4];}
#endif
      expect(sampled);assert(copies==before);memcpy(old,fresh,sizeof(old));
      if(frame==8)names=pool->count;
      if(frame>8)assert(pool->count==names);
      assert(pool->count<=3);
   }
#ifdef GL_POOL_LEGACY
   /* A later mipmapped preset must still generate levels, just as it
    * could on the original legacy mutable texture. */
   mipmap_mode=true;
   assert(gl_media_shader_draw(&state,&backend,&backend,&backend,gl_media_pool_resolve(pool,handle),VIDEO_SCALE_PACK(4,3),0,VIDEO_SCALE_PACK(4,3),80,hdr,true));
   glBindTexture(GL_TEXTURE_2D,gl_media_pool_resolve(pool,handle));
   glGetTexLevelParameteriv(GL_TEXTURE_2D,1,GL_TEXTURE_WIDTH,&bound);assert(bound==2);
   mipmap_mode=false;
#endif
   count=pool->count;for(i=0;i<count;i++)versions[i]=pool->versions[i];
   assert(gl_media_pool_forget(&pool,handle,pool_retire,&state) && !pool);
   gl_media_shader_free(&state);gl_media_pool_free(&pool);
   for(i=0;i<count;i++)assert(!glIsTexture(versions[i]));
   glDeleteBuffers(1,&pbo);assert(glGetError()==GL_NO_ERROR);
}
/* Exercise the shipping converter and history together: every new output
 * must leave the preceding RGB output intact, without a preservation draw. */
static void run_planar(bool core, bool interleaved, bool deferred)
{
   static const float vertex[8]={0,0,1,0,0,1,1,1};
   static const float uv[8]={0,1,1,1,0,0,1,0};
   static const float transform[16]={2,0,0,0,0,2,0,0,0,0,1,0,-1,-1,0,1};
   gl_planar_t planar;
   struct texture_planar planes;
   struct texture_image image;
   gl_media_shader_t *state=NULL;
   gl_media_pool_t *pool=NULL;
   shader_backend_t backend;
   uint8_t y[12], chroma[8];
   float expected[48];
   GLuint handle,target;
   unsigned frame,i,before=copies,names=0,draw_before;
   GLint binding;
   memset(&planar,0,sizeof(planar));memset(&planes,0,sizeof(planes));
   memset(&image,0,sizeof(image));memset(&backend,0,sizeof(backend));
   backend.use=use;backend.num_shaders=passes;backend.get_prev_textures=history;
   backend.filter_type=filter;backend.wrap_type=wrap;backend.shader_scale=scale;
   backend.mipmap_input=mipmap;backend.get_feedback_pass=feedback;
   backend.set_mvp=mvp;backend.set_media_colour=colour;
   backend.set_coords=coords;backend.set_params=params;
   memset(chroma,128,sizeof(chroma));
   for(i=0;i<12;i++)y[i]=(uint8_t)(32+i);
   planes.planes[0]=y;planes.planes[1]=chroma;
   planes.planes[2]=chroma+(interleaved?1:4);
   planes.strides[0]=4;planes.strides[1]=planes.strides[2]=interleaved?4:2;
   planes.chroma_step=interleaved?2:1;planes.yuv=IMAGE_YUV_FLAG_FULL_RANGE;
   image.width=4;image.height=3;image.planar=&planes;
   handle=deferred?gl_planar_load_deferred(&planar,&image,true,core)
      :gl_planar_load(&planar,&image,true,core);assert(handle);
   if(deferred)assert(gl_planar_resolve(&planar,handle)==handle);
   show_previous=0;previous_index=0;smooth_mode=true;
   assert(gl_media_shader_draw(&state,&backend,&backend,&backend,handle,VIDEO_SCALE_PACK(4,3),0,VIDEO_SCALE_PACK(4,3),0,NULL,true));
   for(frame=1;frame<=24;frame++)
   {
      unsigned prior=32+(frame-1)*4;
      if(deferred)
      {
         unsigned skip, width=0, height=0;
         GLuint retained=gl_media_pool_resolve(pool,handle), read_fbo;
         float old[48];GLint saved;
         unsigned count=pool?pool->count:0;
         for(skip=0;skip<4;skip++)
         {
            for(i=0;i<12;i++)y[i]=(uint8_t)(frame+skip+i);
            draw_before=draws;
            assert(gl_planar_update_deferred(&planar,handle,&image));assert(draws==draw_before);
            assert(gl_planar_pending(&planar,handle,&width,&height)&&width==4&&height==3);
            assert(gl_planar_render(&planar,handle,vertex,uv,NULL,transform));
            assert(draws==draw_before+1);
            for(i=0;i<12;i++){expected[i*4]=expected[i*4+1]=expected[i*4+2]=(float)y[i]/255;expected[i*4+3]=1;}
            expect(expected);assert((pool?pool->count:0)==count);
         }
         /* The held shader input must still contain the last RGB frame. */
         glGetIntegerv(GL_FRAMEBUFFER_BINDING,&saved);glGenFramebuffers(1,&read_fbo);
         glBindFramebuffer(GL_FRAMEBUFFER,read_fbo);
         glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,retained,0);
         assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
         glReadPixels(0,0,4,3,GL_RGBA,GL_FLOAT,old);
         for(i=0;i<12;i++)assert(fabsf(old[i*4]-(float)(prior+i)/255)<0.005f);
         glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)saved);glDeleteFramebuffers(1,&read_fbo);
      }
      target=prepare_pool(&pool,handle,&state);assert(target && target!=gl_media_pool_resolve(pool,handle));
      for(i=0;i<12;i++)y[i]=(uint8_t)(32+frame*4+i);
      glGetIntegerv(GL_FRAMEBUFFER_BINDING,&binding);
      if(deferred)
      {
         draw_before=draws;
         assert(gl_planar_update_deferred(&planar,handle,&image));assert(draws==draw_before);
         assert(gl_planar_resolve_target(&planar,handle,target));assert(draws==draw_before+1);
         assert(!gl_planar_pending(&planar,handle,NULL,NULL));
         assert(gl_planar_resolve_target(&planar,handle,target));assert(draws==draw_before+1);
      }
      else assert(gl_planar_update_target(&planar,handle,target,&image));
      { GLint restored;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&restored);assert(restored==binding); }
      gl_media_pool_publish(pool,handle,target);
      show_previous=1;draw_before=draws;
      assert(gl_media_shader_draw(&state,&backend,&backend,&backend,target,VIDEO_SCALE_PACK(4,3),0,VIDEO_SCALE_PACK(4,3),frame,NULL,true));
      for(i=0;i<12;i++){expected[i*4]=expected[i*4+1]=expected[i*4+2]=(float)(prior+i)/255;expected[i*4+3]=1;}
      expect(expected);assert(copies==before);assert(draws==draw_before+1);
      if(frame==8)names=pool->count;
      if(frame>8)assert(pool->count==names);
   }
   /* Rejected frames neither publish a reservation nor disturb old pixels. */
   target=prepare_pool(&pool,handle,&state);assert(target);
   image.width=3;assert(!gl_planar_update_target(&planar,handle,target,&image));
   assert(gl_media_pool_resolve(pool,handle)!=target);image.width=4;
   gl_planar_forget(&planar,handle);
   assert(gl_media_pool_forget(&pool,handle,pool_retire,&state));
   gl_media_shader_free(&state);gl_planar_free(&planar);
   assert(glGetError()==GL_NO_ERROR);
}
#include "planar_direct_pixels.h"
#ifdef _WIN32
static rglgen_func_t resolve_gl(const char *name)
{
   PROC proc=wglGetProcAddress(name);
   rglgen_func_t result;
   if(!proc || proc==(PROC)1 || proc==(PROC)2 || proc==(PROC)3 || proc==(PROC)-1)
      proc=GetProcAddress(GetModuleHandleA("opengl32.dll"),name);
   memcpy(&result,&proc,sizeof(result));return result;
}
#endif
int main(void)
{
   const char *vs="#version 120\nattribute vec2 pos,uv;varying vec2 current,previous;uniform float flip;void main(){gl_Position=vec4(pos*2.-1.,0,1);current=uv;previous=vec2(pos.x,mix(pos.y,1.-pos.y,flip));}";
   const char *fs="#version 120\nvarying vec2 current,previous;uniform sampler2D image,old;uniform bool pick;void main(){gl_FragColor=pick?texture2D(old,previous):texture2D(image,current);}";
#ifdef _WIN32
   HWND window;HDC dc;HGLRC context;PIXELFORMATDESCRIPTOR descriptor;
   GLuint vertex,fragment,target,fbo;
   window=CreateWindowExA(0,"STATIC","GL viewer regression",WS_POPUP,0,0,16,16,NULL,NULL,GetModuleHandle(NULL),NULL);
   assert(window);dc=GetDC(window);assert(dc);
   memset(&descriptor,0,sizeof(descriptor));descriptor.nSize=sizeof(descriptor);
   descriptor.nVersion=1;descriptor.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;
   descriptor.iPixelType=PFD_TYPE_RGBA;descriptor.cColorBits=32;
   assert(SetPixelFormat(dc,ChoosePixelFormat(dc,&descriptor),&descriptor));
   context=wglCreateContext(dc);assert(context && wglMakeCurrent(dc,context));
   rglgen_resolve_symbols(resolve_gl);
#else
   EGLDisplay display;EGLConfig config;EGLSurface surface;EGLContext context;EGLint count;
   const EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_NONE};
   const EGLint size[]={EGL_WIDTH,4,EGL_HEIGHT,3,EGL_NONE};GLuint vertex,fragment,target,fbo;
   PFNEGLGETPLATFORMDISPLAYEXTPROC platform=(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
   display=platform?platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL):eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if(!eglInitialize(display,NULL,NULL)){puts("No EGL OpenGL device");return 77;}
   assert(eglBindAPI(EGL_OPENGL_API));assert(eglChooseConfig(display,attributes,&config,1,&count)&&count);
   surface=eglCreatePbufferSurface(display,config,size);context=eglCreateContext(display,config,EGL_NO_CONTEXT,NULL);
   assert(eglMakeCurrent(display,surface,surface,context));
#endif
   printf("GPU reference: %s / %s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER));
   vertex=compile(GL_VERTEX_SHADER,vs);fragment=compile(GL_FRAGMENT_SHADER,fs);program=glCreateProgram();
   glAttachShader(program,vertex);glAttachShader(program,fragment);glBindAttribLocation(program,0,"pos");glBindAttribLocation(program,1,"uv");glLinkProgram(program);
   target=texture(GL_RGBA32F,NULL);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
   glViewport(0,0,4,3);run(GL_RGBA8,0);run(GL_RGBA16F,-0.5f);
   run_pool(GL_RGBA8,0);run_pool(GL_RGBA16F,-0.5f);
#ifdef GL_POOL_LEGACY
   run_pool(GL_RGBA,0);
#endif
   run_planar(false,false,false);run_planar(false,true,false);
   run_planar(false,false,true);run_planar(false,true,true);
   {unsigned layout,linear;for(layout=0;layout<3;layout++)for(linear=0;linear<2;linear++)
    {run_planar_direct(false,layout,linear!=0,GL_RGBA8);run_planar_direct(false,layout,linear!=0,GL_RGBA16F);}}
   puts("Direct YUV viewer pixels: odd padded planes, NV12/NV21, filtering, crop, HDR storage, lazy RGB and state passed");
   puts("Planar GL GPU pixels: separate/interleaved planes, on-demand RGB, retained history, direct draws after shaders and no snapshots passed");
#ifndef GL_POOL_LEGACY
   run_planar(true,false,false);run_planar(true,true,false);
   run_planar(true,false,true);run_planar(true,true,true);
   {unsigned layout,linear;for(layout=0;layout<3;layout++)for(linear=0;linear<2;linear++)
    {run_planar_direct(true,layout,linear!=0,GL_RGBA8);run_planar_direct(true,layout,linear!=0,GL_RGBA16F);}}
#endif
   glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&target);glDeleteProgram(program);glDeleteShader(vertex);glDeleteShader(fragment);
#ifdef _WIN32
   wglMakeCurrent(NULL,NULL);wglDeleteContext(context);ReleaseDC(window,dc);DestroyWindow(window);
#else
   eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);
#endif
#ifdef GL_POOL_LEGACY
   puts("GL 2.1 pool GPU pixels: RGBA/RGBA8/FP16 uploads, no swizzle/PBO queries, history and retirement passed");
#else
   puts("GL history GPU pixels: RGBA8/FP16 rows, pooled PBO uploads without snapshots, Prev cadence, samplers and retirement passed");
#endif
   return 0;
}
