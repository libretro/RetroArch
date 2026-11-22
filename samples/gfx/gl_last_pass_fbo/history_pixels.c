/* Real EGL/GL pixels through the shared GLSL/Cg viewer history code. */
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <gfx/math/matrix_4x4.h>
#include "../../../gfx/video_driver.h"
static unsigned draws, copies, program, show_previous, previous_index;
static bool smooth_mode = true;
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
#include "../../../gfx/common/gl_media_shader.h"
#undef glDrawArrays
#undef glCopyTexSubImage2D
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
static bool mipmap(void *data,unsigned pass) {(void)data;(void)pass;return false;}
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
int main(void)
{
   const char *vs="#version 120\nattribute vec2 pos,uv;varying vec2 current,previous;uniform float flip;void main(){gl_Position=vec4(pos*2.-1.,0,1);current=uv;previous=vec2(pos.x,mix(pos.y,1.-pos.y,flip));}";
   const char *fs="#version 120\nvarying vec2 current,previous;uniform sampler2D image,old;uniform bool pick;void main(){gl_FragColor=pick?texture2D(old,previous):texture2D(image,current);}";
   EGLDisplay display;EGLConfig config;EGLSurface surface;EGLContext context;EGLint count;
   const EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_NONE};
   const EGLint size[]={EGL_WIDTH,4,EGL_HEIGHT,3,EGL_NONE};GLuint vertex,fragment,target,fbo;
   PFNEGLGETPLATFORMDISPLAYEXTPROC platform=(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
   display=platform?platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL):eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if(!eglInitialize(display,NULL,NULL)){puts("No EGL OpenGL device");return 77;}
   assert(eglBindAPI(EGL_OPENGL_API));assert(eglChooseConfig(display,attributes,&config,1,&count)&&count);
   surface=eglCreatePbufferSurface(display,config,size);context=eglCreateContext(display,config,EGL_NO_CONTEXT,NULL);
   assert(eglMakeCurrent(display,surface,surface,context));
   vertex=compile(GL_VERTEX_SHADER,vs);fragment=compile(GL_FRAGMENT_SHADER,fs);program=glCreateProgram();
   glAttachShader(program,vertex);glAttachShader(program,fragment);glBindAttribLocation(program,0,"pos");glBindAttribLocation(program,1,"uv");glLinkProgram(program);
   target=texture(GL_RGBA32F,NULL);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
   glViewport(0,0,4,3);run(GL_RGBA8,0);run(GL_RGBA16F,-0.5f);
   glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&target);glDeleteProgram(program);glDeleteShader(vertex);glDeleteShader(fragment);
   eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);
   puts("GL history GPU pixels: RGBA8/FP16 rows, Prev cadence, raw mutation snapshot, samplers and retired sources passed");return 0;
}
