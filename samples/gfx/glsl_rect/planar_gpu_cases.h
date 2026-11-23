#ifdef GLSL_PLANAR_ORACLE
#include <formats/image_yuv_blit.h>
static unsigned fusion_draws, fusion_allocations;
static void fusion_image(GLenum target,GLint level,GLint internal,GLsizei w,
      GLsizei h,GLint border,GLenum format,GLenum type,const void *pixels)
{if(format==GL_RGBA)fusion_allocations++;glTexImage2D(target,level,internal,w,h,border,format,type,pixels);}
#define glTexImage2D fusion_image
static void fusion_draw(GLenum mode,GLint first,GLsizei count)
{fusion_draws++;glDrawArrays(mode,first,count);}
#define glDrawArrays fusion_draw
#include "planar-source.inc"
#define MEDIA_GEN_FB glGenFramebuffers
#define MEDIA_DELETE_FB glDeleteFramebuffers
#define MEDIA_BIND_FB(id) glBindFramebuffer(GL_FRAMEBUFFER,id)
#define MEDIA_FB_BINDING GL_FRAMEBUFFER_BINDING
#define MEDIA_ATTACH(tex) glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0)
#define MEDIA_FB_COMPLETE() (glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE)
#include "../../../gfx/common/gl_media_shader.h"
#undef glDrawArrays
#undef glTexImage2D
static void test_planar_fusion(unsigned layout,bool smooth,bool hdr)
{
   static const char shader[]=
      "#version 130\n#if defined(VERTEX)\n"
      "#if __VERSION__ >= 130\n"
      "in vec4 VertexCoord;in vec4 TexCoord;out vec2 uv;\n"
      "#else\nattribute vec4 VertexCoord;attribute vec4 TexCoord;varying vec2 uv;\n#endif\n"
      "uniform mat4 MVPMatrix;"
      "void main(){gl_Position=MVPMatrix*VertexCoord;uv=TexCoord.xy;}\n"
      "#elif defined(FRAGMENT)\n#ifdef GL_ES\nprecision highp float;\n#endif\n"
      "#if __VERSION__ >= 130\nin vec2 uv;out vec4 FragColor;\n#define SAMPLE texture\n"
      "#else\nvarying vec2 uv;\n#define FragColor gl_FragColor\n#define SAMPLE texture2D\n#endif\n"
      "uniform sampler2D Texture;uniform vec2 TextureSize;"
      "vec4 read_frame(sampler2D s,vec2 p){return SAMPLE(s,p);}"
      "void main(){vec2 d=vec2(.6)/TextureSize;"
      "FragColor=(read_frame(Texture,uv)*.5+read_frame(Texture,uv+d)*.25"
      "+read_frame(Texture,uv-d)*.25)*vec4(.75,.5,1,1);FragColor.rgb=FragColor.rgb*1.5-.25;}\n#endif\n";
   const char *src[]={shader};
   glsl_shader_data_t *glsl=make_glsl(src,1);
   gl_planar_t planar;
   struct texture_planar planes;
   struct texture_image image;
   struct gl_planar_sample sample;
   gl_media_shader_t *chain=NULL;
   uint8_t y[24],u[8],v[8],pair[16],expected[9*7*4],actual[9*7*4];
   GLuint source,reference,target,fbo,sentinel[3],core;
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
   GLuint samplers[3];
#endif
#ifdef HAVE_OPENGLES2
   bool core_planes=false;
#else
   bool core_planes=true;
#endif
   unsigned frame,i,before,allocated;
   float *hdr_expected=hdr?(float*)malloc(9*7*4*sizeof(float)):NULL;
   float *hdr_actual=hdr?(float*)malloc(9*7*4*sizeof(float)):NULL;
   bool negative=false,over_one=false;
   glDisable(GL_DITHER); /* match the viewer drivers before comparing RGB8 taps */
   memset(&planar,0,sizeof(planar));memset(&planes,0,sizeof(planes));memset(&image,0,sizeof(image));
   glsl->shader->flags |= SHDR_FLAG_MODERN;
   glGenBuffers(1,&glsl->vbo[0].vbo_primary);glGenBuffers(1,&glsl->vbo[0].vbo_secondary);
   glGenBuffers(1,&glsl->vbo[1].vbo_primary);glGenBuffers(1,&glsl->vbo[1].vbo_secondary);
   glGenBuffers(1,&glsl->vbo[VIDEO_SHADER_STOCK_BLEND].vbo_primary);
   glGenBuffers(1,&glsl->vbo[VIDEO_SHADER_STOCK_BLEND].vbo_secondary);
   glsl->shader->feedback_pass=-1;glsl->shader->pass[0].wrap=RARCH_WRAP_EDGE;
   glsl->shader->pass[0].filter=smooth?RARCH_FILTER_LINEAR:RARCH_FILTER_NEAREST;
   glsl->prg[0]=glsl->prg[1];glsl->uniforms[0]=glsl->uniforms[1];
   glsl->prg[VIDEO_SHADER_STOCK_BLEND]=glsl->prg[1];
   glsl->uniforms[VIDEO_SHADER_STOCK_BLEND]=glsl->uniforms[1];
   core=glsl->prg[1].id;gl_glsl_planar_prepare(glsl);
   CHECK(glsl->planar && glsl->prg[1].id==core,"planar variant compiled without changing the core program");
   if(!glsl->planar){free(hdr_expected);free(hdr_actual);free_glsl(glsl);return;}
   for(i=0;i<8;i++){u[i]=(uint8_t)(i*31);v[i]=(uint8_t)(255-i*27);}
   for(i=0;i<8;i++){pair[i*2]=layout==2?v[i]:u[i];pair[i*2+1]=layout==2?u[i]:v[i];}
   planes.planes[0]=y;planes.planes[1]=layout?pair+(layout==2):u;
   planes.planes[2]=layout?pair+(layout==1):v;
   planes.strides[0]=8;planes.strides[1]=planes.strides[2]=layout?8:4;
   planes.chroma_step=layout?2:1;
   image.width=5;image.height=3;image.planar=&planes;
   memset(y,128,sizeof(y));
   before=fusion_draws;allocated=fusion_allocations;source=gl_planar_load_deferred(&planar,&image,smooth,core_planes);
   CHECK(source && fusion_draws==before && fusion_allocations==allocated,"upload does not allocate an RGB conversion pass");
   reference=gl_planar_load(&planar,&image,smooth,core_planes);
   target=make_texture(9,7,NULL);
   if(hdr){glBindTexture(GL_TEXTURE_2D,target);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,9,7,0,GL_RGBA,GL_FLOAT,NULL);}
   glGenFramebuffers(1,&fbo);
   glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
   glGenSamplers(3,samplers);
   for(i=0;i<3;i++)
   {
      glSamplerParameteri(samplers[i],GL_TEXTURE_MIN_FILTER,GL_LINEAR);
      glSamplerParameteri(samplers[i],GL_TEXTURE_MAG_FILTER,GL_LINEAR);
   }
#endif
   for(i=0;i<3;i++)sentinel[i]=make_texture(1,1,NULL);
   for(frame=0;frame<4;frame++)
   {
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
   #if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
   for(i=0;i<4;i++)glBindSampler(i,0);
#endif
#endif
      for(i=0;i<24;i++)y[i]=(uint8_t)(frame*17+i*11);
      planes.yuv=(frame&1?IMAGE_YUV_FLAG_FULL_RANGE:0)|(frame&2?IMAGE_YUV_FLAG_BT709:0);
      CHECK(gl_planar_update_deferred(&planar,source,&image),"upload next shader input planes");
      CHECK(gl_planar_update(&planar,reference,&image),"convert comparison input using the shipping RGB path");
      CHECK(gl_media_shader_draw(&chain,NULL,&gl_glsl_backend,glsl,reference,VIDEO_SCALE_PACK(5,3),0,VIDEO_SCALE_PACK(9,7),frame,NULL,false),"draw original preset from RGB");
      if(hdr)
      {
         unsigned opaque=0;
         glReadPixels(0,0,9,7,GL_RGBA,GL_FLOAT,hdr_expected);
         for(i=0;i<9*7;i++)opaque+=hdr_expected[i*4+3]>.99f;
         for(i=0;i<9*7*4;i++)if((i&3)!=3){negative|=hdr_expected[i]<0;over_one|=hdr_expected[i]>1;}
         CHECK(opaque==9*7,"comparison preset draws opaque HDR pixels");
      }
      else
      {
         unsigned opaque=0,sum=0;
         glReadPixels(0,0,9,7,GL_RGBA,GL_UNSIGNED_BYTE,expected);
         for(i=0;i<9*7;i++){opaque+=expected[i*4+3]==255;sum+=expected[i*4];}
         CHECK(opaque==9*7 && sum>32,"comparison preset draws opaque, nonblank pixels");
      }
      CHECK(gl_planar_sample_info(&planar,source,&sample),"borrow resident GPU planes");
      memset(y,0x11,sizeof(y));
      for(i=0;i<3;i++)
      {
         glActiveTexture(GL_TEXTURE1+i);glBindTexture(GL_TEXTURE_2D,sentinel[i]);
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
         glBindSampler(1+i,samplers[i]);
#endif
      }
      CHECK(gl_glsl_set_media_planar(glsl,&sample),"select the precompiled sampling variant");
      if(frame==3){glDeleteTextures(1,&sentinel[2]);sentinel[2]=0;}
      before=fusion_draws;allocated=fusion_allocations;
      CHECK(gl_media_shader_draw(&chain,NULL,&gl_glsl_backend,glsl,source,VIDEO_SCALE_PACK(5,3),0,VIDEO_SCALE_PACK(9,7),frame,NULL,false),"run the shipping viewer chain directly from planes");
      CHECK(fusion_draws==before+1 && fusion_allocations==allocated,"one shader draw, no RGB conversion or copy draw");
      if(hdr)
      {
         unsigned different=0;
         glReadPixels(0,0,9,7,GL_RGBA,GL_FLOAT,hdr_actual);
         for(i=0;i<9*7*4;i++)different+=fabsf(hdr_actual[i]-hdr_expected[i])>.007f;
         CHECK(!different,"fused shader preserves signed and over-white HDR output");
      }
      else
      {
         glReadPixels(0,0,9,7,GL_RGBA,GL_UNSIGNED_BYTE,actual);
         CHECK(same_linear(actual,expected,sizeof(actual)),"fused filtered shader pixels match the RGB-input preset");
      }
      CHECK(gl_glsl_set_media_planar(glsl,NULL),"restore the core shader metadata");
      for(i=0;i<3;i++)
      {
         GLint bound;glActiveTexture(GL_TEXTURE1+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);
         CHECK((GLuint)bound==sentinel[i],"restore the caller's plane-unit binding");
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
         glGetIntegerv(GL_SAMPLER_BINDING,&bound);
         CHECK((GLuint)bound==samplers[i],"restore the caller's sampler object");
#endif
      }
      glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,source);
      CHECK(gl_planar_pending(&planar,source,NULL,NULL),"source RGB storage remains absent");
      CHECK(gl_glsl_program(glsl,1)->id==core && !glsl->current_mat_data_pointer[1],"leave core programs and MVP caching ready for normal drawing");
   }
   if(hdr)CHECK(negative && over_one,"HDR oracle exercised negative and over-white shader pixels");
   glsl->shader->pass[0].wrap=RARCH_WRAP_REPEAT;
   CHECK(!gl_glsl_set_media_planar(glsl,&sample),"live wrap changes decline fusion without recompiling");
   glsl->shader->pass[0].wrap=RARCH_WRAP_EDGE;
   glsl->shader->pass[0].mipmap=true;gl_glsl_planar_prepare(glsl);
   CHECK(!glsl->planar && !gl_glsl_set_media_planar(glsl,&sample),"mipmapped sampling retains the RGB fallback");
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
   for(i=0;i<4;i++)glBindSampler(i,0);
#endif
   CHECK(gl_planar_resolve(&planar,source)==source,"RGB fallback remains available after fused draws");
   gl_media_shader_free(&chain);gl_planar_forget(&planar,source);gl_planar_forget(&planar,reference);
   glDeleteTextures(1,&source);glDeleteTextures(1,&reference);gl_planar_free(&planar);
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
   for(i=0;i<3;i++)glBindSampler(1+i,0);
   glDeleteSamplers(3,samplers);
#endif
   glActiveTexture(GL_TEXTURE0);glDeleteTextures(3,sentinel);glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&target);
   CHECK(glGetError()==GL_NO_ERROR,"no GL state or lifetime error after fusion and fallback");
   free(hdr_expected);free(hdr_actual);free_glsl(glsl);
}
#endif
