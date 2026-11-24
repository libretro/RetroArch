#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../gfx/drivers_shader/glsl_planar.h"
static unsigned failures;
#define CHECK(x) do { if(!(x)){fprintf(stderr,"planar FAIL %d: %s\n",__LINE__,#x);failures++;} } while(0)
static const char *names[]={"Texture","OrigTexture"};
static void valid(const char *s,bool changed)
{
   bool c=false;char *out=glsl_planar_rewrite(s,names,2,&c);
   CHECK(out && c==changed);
   if(out && changed)CHECK(strstr(out,"RARCH_P_read(") && strstr(out,"RARCH_P_sample"));
   free(out);
}
static void invalid(const char *s)
{
   bool c=true;char *out=glsl_planar_rewrite(s,names,2,&c);
   CHECK(!out && !c);free(out);
}
int main(void)
{
   valid("#version 130\nuniform sampler2D Texture;void main(){vec4 c=texture(Texture,vec2(.5));}",true);
   valid("uniform sampler2D Texture;uniform sampler2D OrigTexture;void main(){vec4 c=texture2D(Texture,texture2D(OrigTexture,vec2(.5)).xy);}",true);
   valid("uniform sampler2D Texture;vec4 f(sampler2D s,vec2 uv){return texture2D(s,uv);}void main(){vec4 v=f(Texture,vec2(.5));}",true);
   valid("#define FETCH texture2D\n#define FRAME Texture\nuniform sampler2D Texture;void main(){vec4 v=FETCH(FRAME,vec2(.5));}",true);
   {
      bool changed=false;
      char *out=glsl_planar_rewrite("uniform sampler2D Texture,LutA,LutB;void main(){vec4 v=texture2D(Texture,vec2(.5))*texture2D(LutA,vec2(.3))*texture2D(LutB,vec2(.8));}",names,2,&changed);
      CHECK(out && changed);
      if(out)CHECK(strstr(out,"texture2D(LutA,") && strstr(out,"texture2D(LutB,"));
      free(out);
   }
   valid("void main(){vec4 v=vec4(1.);}",false);
   invalid("uniform sampler2D Texture;void main(){vec4 v=texture2D(Texture,vec2(.5),1.);}");
   invalid("uniform sampler2D Texture;void main(){ivec2 s=textureSize(Texture,0);}");
   invalid("uniform sampler2D Texture;void main(){vec4 v=texelFetch(Texture,ivec2(0),0);}");
   invalid("uniform sampler2D Texture;void main(){vec4 v=textureGrad(Texture,vec2(.5),vec2(1),vec2(1));}");
   invalid("uniform sampler2D Texture,Lut;vec4 f(sampler2D s,vec2 uv){return texture2D(s,uv);}void main(){vec4 v=f(Texture,vec2(.5))+f(Lut,vec2(.5));}");
   invalid("uniform sampler2D Texture;uniform float RARCH_P_bad;void main(){vec4 v=texture2D(Texture,vec2(.5));}");
   invalid("uniform sampler2D Texture;void main(){vec4 v=texture2D(Texture[0],vec2(.5));}");
   invalid("uniform sampler2D Texture;uniform samplerCube cube;void main(){vec4 v=texture2D(Texture,vec2(.5));}");
   invalid("vec4 texture2D(sampler2D s,vec2 uv){return vec4(1.);}uniform sampler2D Texture;void main(){vec4 v=texture2D(Texture,vec2(.5));}");
   if(failures)return 1;
   puts("PASS: conservative planar GLSL sampling rewrite and unsupported-read fallbacks");return 0;
}
