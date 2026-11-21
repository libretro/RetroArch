#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../gfx/drivers_shader/glsl_colour.h"

static unsigned failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"colour FAIL %d: %s\n",__LINE__,#x);failures++; } } while (0)
static const char *names[] = {"Texture","OrigTexture","PrevTexture"};

static char *rewrite(const char *source, bool *changed)
{
   return glsl_colour_rewrite(source,names,3,changed);
}

int main(void)
{
   static const char stage[] =
      "#version 130\n#extension GL_ARB_shader_texture_lod : enable\n"
      "#if defined(VERTEX)\nvoid main(){gl_Position=vec4(0.0);}\n"
      "#elif defined(FRAGMENT)\nuniform sampler2D Texture;\n"
      "uniform sampler2D Lut;\nvoid main(){gl_FragColor=texture2DLod(Texture,vec2(.5),2.0)"
      "+texture2D(Lut,vec2(.5));}\n#endif\n";
   static const char nested[] =
      "uniform sampler2D Texture; uniform sampler2D OrigTexture;"
      "vec4 f(sampler2D s,vec2 uv){return textureLodOffset(s,uv,2.0,ivec2(1,-1));}"
      "void main(){vec4 v=texture(Texture,texture(OrigTexture,vec2(.5)).xy);"
      "v+=f(Texture,vec2(.2));}";
   static const char macro[] =
      "#define FRAME Texture\n#define READ textureGrad\n"
      "uniform sampler2D Texture; void main(){vec4 v=READ(FRAME,vec2(.5),vec2(.1),vec2(.2));}";
   static const char mixed[] =
      "uniform sampler2D Texture; uniform sampler2D Lut;"
      "vec4 f(sampler2D s,vec2 uv){return texture(s,uv);}"
      "void main(){vec4 v=f(Texture,vec2(.2))+f(Lut,vec2(.3));}";
   static const char query[] = "uniform sampler2D Texture; void main(){ivec2 s=textureSize(Texture,0);}";
   char *out;
   bool changed;
   out=rewrite(stage,&changed);
   CHECK(out && changed);
   if(out)
   {
      CHECK(!strncmp(out,"#version 130\n",13));
      CHECK(strstr(out,"RARCH_viewer_colour(texture2DLod(Texture,vec2(.5),2.0))"));
      CHECK(strstr(out,"+texture2D(Lut,vec2(.5))"));
      CHECK(strstr(out,"#extension GL_ARB_shader_texture_lod : enable"));
   }
   free(out);
   out=rewrite(nested,&changed);
   CHECK(out && changed);
   if(out)
   {
      CHECK(strstr(out,"RARCH_viewer_colour(textureLodOffset(s,uv,2.0,ivec2(1,-1)))"));
      CHECK(strstr(out,"RARCH_viewer_colour(texture(Texture,RARCH_viewer_colour(texture(OrigTexture,vec2(.5))).xy))"));
   }
   free(out);
   out=rewrite(macro,&changed);
   CHECK(out && changed);
   if(out) CHECK(strstr(out,"RARCH_viewer_colour(READ(FRAME,vec2(.5),vec2(.1),vec2(.2)))"));
   free(out);
   out=rewrite(mixed,&changed);
   CHECK(!out && !changed);
   free(out);
   out=rewrite(query,&changed);
   CHECK(out && !changed && !strcmp(out,query));
   free(out);
   out=rewrite("uniform sampler2D Texture;void main(){vec4 v=textureGather(Texture,vec2(.5));}",&changed);
   CHECK(!out && !changed);
   free(out);
   printf("GLSL viewer source: nested reads, LOD/offset/gradients, macros, LUTs and queries, %u failures\n",failures);
   return failures?1:0;
}
