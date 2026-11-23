/* Conservative GLSL source adaptation for normalized, base-level frame reads.
 * Unknown uses decline the variant; the existing RGB path remains available. */
#ifndef GLSL_PLANAR_H
#define GLSL_PLANAR_H
#include "glsl_rect.h"
#include "../common/gl_planar_sample.h"
struct glsl_planar_edit { size_t at, remove; unsigned kind; };
static int glsl_planar_order(const void *a,const void *b)
{
   const struct glsl_planar_edit *x=(const struct glsl_planar_edit*)a;
   const struct glsl_planar_edit *y=(const struct glsl_planar_edit*)b;
   return x->at<y->at?-1:x->at>y->at?1:0;
}
static char *glsl_planar_rewrite(const char *src,const char *const *frame,
      unsigned num_frame,bool *changed)
{
   static const char *const reads[]={"texture2D","texture"};
   static const char declare[]=GL_PLANAR_SAMPLE_SOURCE
      "\n#ifndef RARCH_P_WRAPPER\n#define RARCH_P_WRAPPER\n"
      "RARCH_P_HP vec4 RARCH_P_read(sampler2D unused,RARCH_P_HP vec2 uv)"
      "{return RARCH_P_sample(uv);}\n#endif\n";
   static const char *const text[]={declare,"RARCH_P_read"};
   struct glsl_rect_scan s;
   struct glsl_planar_edit *edit=NULL;
   char *buf=NULL,*out=NULL;
   size_t len,i,n=0,at,o,out_len;
   unsigned functions=0;
   *changed=false;
   if(!src)return NULL;
   len=strlen(src);buf=(char*)malloc(len+1);if(!buf)return NULL;
   memcpy(buf,src,len+1);glsl_rect_blank_comments(buf);
   memset(&s,0,sizeof(s));s.src=buf;glsl_rect_scan_source(&s);
   if(s.failed || s.num_tok>((size_t)-1/sizeof(*edit)-1)/2)goto done;
   edit=(struct glsl_planar_edit*)calloc(s.num_tok*2+1,sizeof(*edit));
   if(!edit)goto done;
   glsl_rect_frame_params(&s,frame,num_frame);if(s.failed)goto done;
   for(i=0;i<s.num_undef;i++)
      if(glsl_rect_resolves_to(&s,s.undef[i].name,s.undef[i].len,frame,num_frame,0)
            || glsl_rect_resolves_to(&s,s.undef[i].name,s.undef[i].len,reads,2,0))goto done;
   for(i=0;i<s.num_tok;i++)
   {
      const struct glsl_rect_token *t=&s.tok[i];int r;
      if(t->type==GLSL_RECT_TOK_IDENT && t->len>=7
            && !memcmp(s.src+t->start,"sampler",7)
            && !(t->len==9 && !memcmp(s.src+t->start,"sampler2D",9)))goto done;
      if(t->len>=8 && !memcmp(s.src+t->start,"RARCH_P_",8))goto done;
      if(!t->region && !t->brace && i && glsl_rect_tok_is(&s,i,"{")
            && glsl_rect_tok_is(&s,i-1,")"))
      {
         size_t j=i,k=i-1;int depth=1;
         while(k && depth){k--;if(glsl_rect_tok_is(&s,k,")"))depth++;
            else if(glsl_rect_tok_is(&s,k,"("))depth--;}
         if(k && glsl_rect_resolves_to(&s,s.tok[k-1].start,s.tok[k-1].len,reads,2,0))goto done;
         while(j && !s.tok[j-1].region && !glsl_rect_tok_is(&s,j-1,";")
               && !glsl_rect_tok_is(&s,j-1,"}") && !glsl_rect_tok_is(&s,j-1,"{"))j--;
         edit[n].at=s.tok[j].start;edit[n++].kind=0;functions++;
      }
      if(t->type!=GLSL_RECT_TOK_IDENT || t->alias_body || (t->flags&GLSL_RECT_FLAG_SKIP))continue;
      r=(t->flags&GLSL_RECT_FLAG_FRAME)?1:glsl_rect_resolves_to(&s,t->start,t->len,frame,num_frame,0);
      if(r<0)goto done;
      if(!r)continue;
      if(i && glsl_rect_tok_is(&s,i-1,"sampler2D"))continue;
      if(i<2 || !glsl_rect_tok_is(&s,i-1,"(") || !glsl_rect_tok_is(&s,i+1,",")
            || s.tok[i-2].region!=t->region)goto done;
      if(glsl_rect_resolves_to(&s,s.tok[i-2].start,s.tok[i-2].len,reads,2,0)!=1)goto done;
      {
         size_t end=glsl_rect_match(&s,i-1,'(',')'),k;unsigned commas=0;int depth=0;
         if(end==s.num_tok || s.tok[end].region!=t->region)goto done;
         for(k=i;k<end;k++)
         {
            if(glsl_rect_tok_is(&s,k,"(") || glsl_rect_tok_is(&s,k,"["))depth++;
            else if(glsl_rect_tok_is(&s,k,")") || glsl_rect_tok_is(&s,k,"]"))depth--;
            else if(!depth && glsl_rect_tok_is(&s,k,","))commas++;
         }
         if(commas!=1)goto done; /* bias/LOD/gradient forms keep RGB */
         edit[n].at=s.tok[i-2].start;edit[n].remove=s.tok[i-2].len;edit[n++].kind=1;
         *changed=true;
      }
   }
   if(!*changed)
   {
      out=(char*)malloc(len+1);if(out)memcpy(out,src,len+1);goto done;
   }
   if(!functions)goto done;
   qsort(edit,n,sizeof(*edit),glsl_planar_order);out_len=len+1;
   for(i=0;i<n;i++)
   {
      size_t add=strlen(text[edit[i].kind]);
      if(out_len>(size_t)-1-add)goto done;
      out_len+=add;
   }
   out=(char*)malloc(out_len);if(!out)goto done;
   at=o=0;
   for(i=0;i<n;i++)
   {
      size_t add=strlen(text[edit[i].kind]);
      if(edit[i].at<at){free(out);out=NULL;goto done;}
      memcpy(out+o,src+at,edit[i].at-at);o+=edit[i].at-at;
      memcpy(out+o,text[edit[i].kind],add);o+=add;at=edit[i].at+edit[i].remove;
   }
   memcpy(out+o,src+at,len-at+1);
done:
   free(edit);free(s.tok);free(s.alias);free(buf);
   if(!out)*changed=false;
   return out;
}
#endif
