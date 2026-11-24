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
static const char fusion_shader[]=
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
static char *fusion_input(const char *src,const char *name)
{
   size_t count=0,n=strlen(name),used=0;
   const char *p=src,*next;
   char *out;
   while((next=strstr(p,"Texture"))){count++;p=next+7;}
   out=(char*)malloc(strlen(src)+1+count*n);
   p=src;
   while((next=strstr(p,"Texture")))
   {
      size_t span=(size_t)(next-p);
      memcpy(out+used,p,span);used+=span;
      memcpy(out+used,name,n);used+=n;p=next+7;
   }
   strcpy(out+used,p);return out;
}
static char *fusion_replace(const char *src,const char *needle,const char *replacement)
{
   const char *point=strstr(src,needle);
   size_t prefix=(size_t)(point-src),count=strlen(replacement);
   char *out=(char*)malloc(strlen(src)+count+1);
   memcpy(out,src,prefix);memcpy(out+prefix,replacement,count);
   strcpy(out+prefix+count,point+strlen(needle));return out;
}
static void test_planar_dependencies(void)
{
   static const char *const names[]={"OrigTexture","rubyOrigTexture",
      "PassPrev2Texture","PrevTexture","UnboundTexture"};
   unsigned i;
   for(i=0;i<sizeof(names)/sizeof(names[0]);i++)
   {
      char *late=fusion_input(fusion_shader,names[i]);
      const char *src[]={fusion_shader,late};
      glsl_shader_data_t *glsl=make_glsl(src,2);
      glsl->shader->flags |= SHDR_FLAG_MODERN;
      glsl->shader->feedback_pass=-1;glsl->shader->pass[0].wrap=RARCH_WRAP_EDGE;
      gl_glsl_planar_prepare(glsl);
      if(i<3)CHECK(glsl->planar && glsl->planar->pass[1].prg.id,"later original aliases receive a private planar program");
      else CHECK(!glsl->planar,"temporal and unresolved inputs retain RGB fallback");
      free_glsl(glsl);free(late);
   }
   for(i=0;i<3;i++)
   {
      char *original=fusion_input(fusion_shader,"OrigTexture");
      char *first=i==0?fusion_replace(fusion_shader,"return SAMPLE(s,p);","return vec4(1.0);"):NULL;
      char *late=i==1?fusion_replace(original,"return SAMPLE(s,p);","return SAMPLE(s,p,1.0);")
         :i==2?fusion_replace(fusion_shader,"uniform sampler2D Texture;","uniform sampler2D Texture,OrigTexture;"):NULL;
      const char *src[2];
      glsl_shader_data_t *glsl;
      if(i==2)
      {
         char *mixed=fusion_replace(late,"read_frame(Texture,uv)*.5","read_frame(OrigTexture,uv)*.5");
         free(late);late=mixed;
      }
      src[0]=first?first:fusion_shader;src[1]=late?late:original;
      glsl=make_glsl(src,2);
      CHECK(glsl->prg[1].id && glsl->prg[2].id,"dependency fixture links both core programs");
      glsl->shader->flags |= SHDR_FLAG_MODERN;
      glsl->shader->feedback_pass=-1;glsl->shader->pass[0].wrap=RARCH_WRAP_EDGE;
      gl_glsl_planar_prepare(glsl);
      if(!i)CHECK(glsl->planar && !glsl->planar->pass[0].prg.id
            && glsl->planar->pass[1].prg.id,"constant first pass needs no private program before a later original read");
      else CHECK(!glsl->planar,"unsupported later sampling retires all partially compiled variants");
      free_glsl(glsl);free(original);free(first);free(late);
   }
}
static char *fusion_lookup_shader(void)
{
   static const char declaration[]="uniform sampler2D LookupA,LookupB;";
   static const char operation[]="FragColor *= SAMPLE(LookupA,vec2(uv.x,.5))*SAMPLE(LookupB,vec2(uv.y,.5));";
   const char *point=strstr(fusion_shader,"uniform sampler2D Texture;");
   const char *end=strstr(point,"}\n#endif");
   size_t prefix=(size_t)(point-fusion_shader),body=(size_t)(end-point);
   char *out=(char*)malloc(strlen(fusion_shader)+sizeof(declaration)+sizeof(operation));
   memcpy(out,fusion_shader,prefix);memcpy(out+prefix,declaration,strlen(declaration));
   prefix+=strlen(declaration);memcpy(out+prefix,point,body);prefix+=body;
   strcpy(out+prefix,operation);strcat(out+prefix,end);return out;
}
static const char fusion_zero_sampler[]=
      "#version 130\n#if defined(VERTEX)\n"
      "#if __VERSION__ >= 130\nin vec4 VertexCoord;in vec4 TexCoord;out vec2 uv;\n"
      "#else\nattribute vec4 VertexCoord;attribute vec4 TexCoord;varying vec2 uv;\n#endif\n"
      "void main(){gl_Position=vec4(VertexCoord.xy*2.-1.,0.,1.);uv=TexCoord.xy;}\n"
      "#elif defined(FRAGMENT)\n#ifdef GL_ES\nprecision highp float;\n#endif\n"
      "#if __VERSION__ >= 130\nin vec2 uv;out vec4 FragColor;\n#define SAMPLE texture\n"
      "#else\nvarying vec2 uv;\n#define FragColor gl_FragColor\n#define SAMPLE texture2D\n#endif\n"
      "uniform sampler2D Pass1Texture;void main(){FragColor=SAMPLE(Pass1Texture,uv);}\n#endif\n";
static char *fusion_original_shader(const char *src,const char *name,bool hdr)
{
   const char *point=strstr(src,"uniform sampler2D Texture;");
   const char *end=strstr(point,"}\n#endif");
   char declaration[160],operation[256];
   size_t prefix=(size_t)(point-src),body=(size_t)(end-point);
   char *out;
   snprintf(declaration,sizeof(declaration),"uniform sampler2D %s,Pass1Texture,PassPrev1Texture;",name);
   snprintf(operation,sizeof(operation),"FragColor=FragColor*.5+SAMPLE(%s,uv)*.25+SAMPLE(Pass1Texture,uv)*.125+SAMPLE(PassPrev1Texture,uv)*.125;%s",name,hdr?"FragColor.rb=FragColor.rb*.5+vec2(-.75,1.5);":"");
   out=(char*)malloc(strlen(src)+strlen(declaration)+strlen(operation)+1);
   memcpy(out,src,prefix);memcpy(out+prefix,declaration,strlen(declaration));prefix+=strlen(declaration);
   memcpy(out+prefix,point,body);prefix+=body;
   strcpy(out+prefix,operation);strcat(out+prefix,end);return out;
}
static void test_planar_fusion(unsigned layout,bool smooth,bool hdr,unsigned passes,unsigned lookups,bool original)
{
   char *lookup_source=lookups?fusion_lookup_shader():NULL;
   const char *base=lookups?lookup_source:fusion_shader;
   char name[32];
   char *late,*middle=NULL;
   const char *src[3];
   glsl_shader_data_t *glsl;
   gl_planar_t planar;
   struct texture_planar planes;
   struct texture_image image;
   struct gl_planar_sample sample;
   gl_media_shader_t *chain=NULL;
   uint8_t y[24],u[8],v[8],pair[16],expected[9*7*4],actual[9*7*4];
   uint8_t first_expected[10*6*4],first_actual[10*6*4];
   GLuint source,reference,target,fbo,sentinel[3],core;
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
   GLuint samplers[3];
#endif
#ifdef HAVE_OPENGLES2
   bool core_planes=false;
#else
   bool core_planes=true;
#endif
   unsigned frame,i,before,allocated,unit=lookups+1;
   float *hdr_expected=hdr?(float*)malloc(9*7*4*sizeof(float)):NULL;
   float *hdr_actual=hdr?(float*)malloc(9*7*4*sizeof(float)):NULL;
   bool negative=false,over_one=false;
   bool zero_final=!original && passes==3 && !lookups && !layout;
   snprintf(name,sizeof(name),"PassPrev%uTexture",passes);
   late=original?fusion_original_shader(base,layout==0?"OrigTexture":layout==1?"rubyOrigTexture":name,hdr)
      :fusion_input(base,"Pass1Texture");
   if(original && passes==3 && layout==1)middle=fusion_original_shader(base,"OrigTexture",hdr);
   src[0]=base;src[1]=passes==2 && original?late:middle?middle:base;src[2]=zero_final?fusion_zero_sampler:late;
   glsl=make_glsl(src,passes);
   printf("Planar preset: %u passes, layout %u, linear %u, HDR %u, %u lookup tables, original %u\n",passes,layout,smooth,hdr,lookups,original);
   glDisable(GL_DITHER); /* match the viewer drivers before comparing RGB8 taps */
   memset(&planar,0,sizeof(planar));memset(&planes,0,sizeof(planes));memset(&image,0,sizeof(image));
   glsl->shader->luts=lookups;
   if(lookups)
   {
      uint8_t table[16*4];
      strcpy(glsl->shader->lut[0].id,"LookupA");strcpy(glsl->shader->lut[1].id,"LookupB");
      for(i=0;i<16;i++){table[i*4]=(uint8_t)(128+i*7);table[i*4+1]=(uint8_t)(200-i*5);table[i*4+2]=table[i*4+3]=255;}
      glsl->lut_textures[0]=make_texture(16,1,table);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
      for(i=0;i<16;i++){table[i*4]=(uint8_t)(220-i*3);table[i*4+1]=(uint8_t)(140+i*5);}
      glsl->lut_textures[1]=make_texture(16,1,table);
      for(i=1;i<=passes;i++)gl_glsl_find_uniforms(glsl,i,glsl->prg[i].id,&glsl->uniforms[i]);
   }
   glsl->shader->flags |= SHDR_FLAG_MODERN;
   glGenBuffers(1,&glsl->vbo[0].vbo_primary);glGenBuffers(1,&glsl->vbo[0].vbo_secondary);
   for(i=1;i<=passes;i++)
   {
      glGenBuffers(1,&glsl->vbo[i].vbo_primary);glGenBuffers(1,&glsl->vbo[i].vbo_secondary);
      glsl->shader->pass[i-1].wrap=RARCH_WRAP_EDGE;
      glsl->shader->pass[i-1].filter=smooth?RARCH_FILTER_LINEAR:RARCH_FILTER_NEAREST;
   }
   if(passes>1)
   {
      struct gfx_fbo_scale *scale=&glsl->shader->pass[0].fbo;
      scale->flags=FBO_SCALE_FLAG_VALID | (hdr?FBO_SCALE_FLAG_FP_FBO:0);
      glsl->shader->pass[1].mipmap=!hdr && passes==3;
      glsl->shader->pass[1].wrap=RARCH_WRAP_REPEAT;
      scale->type_x=scale->type_y=RARCH_SCALE_INPUT;
      scale->scale_x=scale->scale_y=2.0f;
   }
   glGenBuffers(1,&glsl->vbo[VIDEO_SHADER_STOCK_BLEND].vbo_primary);
   glGenBuffers(1,&glsl->vbo[VIDEO_SHADER_STOCK_BLEND].vbo_secondary);
   glsl->shader->feedback_pass=-1;glsl->shader->pass[0].wrap=RARCH_WRAP_EDGE;
   glsl->shader->pass[0].filter=smooth?RARCH_FILTER_LINEAR:RARCH_FILTER_NEAREST;
   glsl->prg[0]=glsl->prg[1];glsl->uniforms[0]=glsl->uniforms[1];
   glsl->prg[VIDEO_SHADER_STOCK_BLEND]=glsl->prg[1];
   glsl->uniforms[VIDEO_SHADER_STOCK_BLEND]=glsl->uniforms[1];
   core=glsl->prg[1].id;gl_glsl_planar_prepare(glsl);
   CHECK(glsl->planar && glsl->prg[1].id==core,"planar variant compiled without changing the core program");
   if(!glsl->planar){glDeleteTextures(lookups,glsl->lut_textures);free(lookup_source);free(middle);free(late);free(hdr_expected);free(hdr_actual);free_glsl(glsl);return;}
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
      for(i=0;i<unit+3;i++)glBindSampler(i,0);
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
      if(passes>1 && !hdr)
      {
         glBindFramebuffer(GL_FRAMEBUFFER,chain->pass[0].fbo);
         glReadPixels(0,0,10,6,GL_RGBA,GL_UNSIGNED_BYTE,first_expected);
         glBindFramebuffer(GL_FRAMEBUFFER,fbo);
      }
      CHECK(gl_planar_sample_info(&planar,source,&sample),"borrow resident GPU planes");
      memset(y,0x11,sizeof(y));
      for(i=0;i<3;i++)
      {
         glActiveTexture(GL_TEXTURE0+unit+i);glBindTexture(GL_TEXTURE_2D,sentinel[i]);
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
         glBindSampler(unit+i,samplers[i]);
#endif
      }
      for(i=0;i<lookups;i++)
      {
         glActiveTexture(GL_TEXTURE1+i);glBindTexture(GL_TEXTURE_2D,sentinel[0]);
      }
      CHECK(gl_glsl_set_media_planar(glsl,&sample),"select the precompiled sampling variant");
      CHECK(glsl->planar->unit==unit,"plane units follow the preset's lookup bindings");
      if(original)
      {
         CHECK(glsl->planar->pass[passes-1].prg.id
               && gl_glsl_program(glsl,passes)->id==glsl->planar->pass[passes-1].prg.id,
               "later original reads borrow their own cached planar program");
         if(passes==3 && !middle)CHECK(!glsl->planar->pass[1].prg.id
               && gl_glsl_program(glsl,2)->id==glsl->prg[2].id,
               "an intermediate pass without original reads keeps its core program");
      }
      if(frame==3){glDeleteTextures(1,&sentinel[2]);sentinel[2]=0;}
      before=fusion_draws;allocated=fusion_allocations;
      CHECK(gl_media_shader_draw(&chain,NULL,&gl_glsl_backend,glsl,source,VIDEO_SCALE_PACK(5,3),0,VIDEO_SCALE_PACK(9,7),frame,NULL,false),"run the shipping viewer chain directly from planes");
      CHECK(fusion_draws==before+passes && fusion_allocations==allocated,"only preset draws, no input RGB conversion or allocation");
      if(zero_final)
      {
         GLint bound=-1,location=glGetUniformLocation(glsl->prg[3].id,"Pass1Texture");
         CHECK(location==0,"pass-output sampler fixture has uniform location zero");
         glGetUniformiv(glsl->prg[3].id,location,&bound);
         CHECK(bound==(GLint)(unit+3),"location-zero sampler binds its pass output beyond the plane units");
      }
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
         {
            unsigned j,maximum=0,bound=passes>1?2:1;
            for(j=0;j<sizeof(actual);j++)
            {unsigned delta=(unsigned)abs((int)actual[j]-(int)expected[j]);if(delta>maximum)maximum=delta;}
            /* The final 1.5x contrast amplifies one-step input rounding. */
            if(maximum>bound)printf("Pixel difference: frame %u, maximum %u, bound %u\n",frame,maximum,bound);
            CHECK(maximum<=bound,"final pixels stay within the preset's propagated rounding bound");
         }
      }
      if(passes>1 && !hdr)
      {
         glBindFramebuffer(GL_FRAMEBUFFER,chain->pass[0].fbo);
         glReadPixels(0,0,10,6,GL_RGBA,GL_UNSIGNED_BYTE,first_actual);
         CHECK(same_linear(first_actual,first_expected,sizeof(first_actual)),"first-pass pixels match RGB input within one rounding step");
         glBindFramebuffer(GL_FRAMEBUFFER,fbo);
      }
      CHECK(gl_glsl_set_media_planar(glsl,NULL),"restore the core shader metadata");
      for(i=0;i<3;i++)
      {
         GLint bound;glActiveTexture(GL_TEXTURE0+unit+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);
         CHECK((GLuint)bound==sentinel[i],"restore the caller's plane-unit binding");
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
         glGetIntegerv(GL_SAMPLER_BINDING,&bound);
         CHECK((GLuint)bound==samplers[i],"restore the caller's sampler object");
#endif
      }
      for(i=0;i<lookups;i++)
      {
         GLint bound;
         glActiveTexture(GL_TEXTURE1+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);
         CHECK((GLuint)bound==glsl->lut_textures[i],"lookup tables retain the normal backend bindings");
         glGetUniformiv(glsl->planar->pass[0].prg.id,glsl->planar->pass[0].uni.lut_texture[i],&bound);
         CHECK(bound==(GLint)(1+i),"lookup uniforms never alias the borrowed planes");
      }
      glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,source);
      CHECK(gl_planar_pending(&planar,source,NULL,NULL),"source RGB storage remains absent");
      CHECK(gl_glsl_program(glsl,1)->id==core && !glsl->current_mat_data_pointer[1],"leave core programs and MVP caching ready for normal drawing");
      if(original)CHECK(gl_glsl_program(glsl,passes)->id==glsl->prg[passes].id
            && !glsl->current_mat_data_pointer[passes],"restore later core programs and their MVP caches");
   }
   if(hdr)CHECK(negative && over_one,"HDR oracle exercised negative and over-white shader pixels");
   glsl->shader->luts++;
   CHECK(!gl_glsl_set_media_planar(glsl,&sample),"live lookup count changes decline stale plane bindings");
   glsl->shader->luts--;
   glsl->shader->pass[0].wrap=RARCH_WRAP_REPEAT;
   CHECK(!gl_glsl_set_media_planar(glsl,&sample),"live wrap changes decline fusion without recompiling");
   glsl->shader->pass[0].wrap=RARCH_WRAP_EDGE;
   glsl->shader->pass[0].fbo.flags |= FBO_SCALE_FLAG_SRGB_FBO;
   CHECK(!gl_glsl_set_media_planar(glsl,&sample),"first-pass sRGB targets retain their existing RGB fallback");
   glsl->shader->pass[0].fbo.flags &= ~FBO_SCALE_FLAG_SRGB_FBO;
   glsl->shader->pass[0].mipmap=true;gl_glsl_planar_prepare(glsl);
   CHECK(!glsl->planar && !gl_glsl_set_media_planar(glsl,&sample),"mipmapped sampling retains the RGB fallback");
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
   for(i=0;i<unit+3;i++)glBindSampler(i,0);
#endif
   CHECK(gl_planar_resolve(&planar,source)==source,"RGB fallback remains available after fused draws");
   {
      GLint limit=0;
      glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&limit);
      glsl->shader->pass[0].mipmap=false;
      glsl->shader->luts=limit>2?(unsigned)limit-2:0;
      gl_glsl_planar_prepare(glsl);
      CHECK(!glsl->planar,"insufficient texture units retain RGB fallback without invalid bindings");
      glsl->shader->luts=lookups;
   }
   gl_media_shader_free(&chain);gl_planar_forget(&planar,source);gl_planar_forget(&planar,reference);
   glDeleteTextures(1,&source);glDeleteTextures(1,&reference);gl_planar_free(&planar);
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
   for(i=0;i<3;i++)glBindSampler(unit+i,0);
   glDeleteSamplers(3,samplers);
#endif
   glActiveTexture(GL_TEXTURE0);glDeleteTextures(3,sentinel);glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&target);
   CHECK(glGetError()==GL_NO_ERROR,"no GL state or lifetime error after fusion and fallback");
   glDeleteTextures(lookups,glsl->lut_textures);free(lookup_source);
   free(middle);free(late);free(hdr_expected);free(hdr_actual);free_glsl(glsl);
}
#endif
