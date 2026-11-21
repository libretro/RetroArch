/* Viewer colour conversion at frame sampling, without changing the read. */
#ifndef GLSL_COLOUR_H__
#define GLSL_COLOUR_H__

#include "glsl_rect.h"

#define GLSL_COLOUR_ROW0 "RARCH_ViewerColour0"
#define GLSL_COLOUR_ROW1 "RARCH_ViewerColour1"
#define GLSL_COLOUR_ROW2 "RARCH_ViewerColour2"

struct glsl_colour_edit
{
   size_t at;
   unsigned kind;
};

static int glsl_colour_edit_cmp(const void *a, const void *b)
{
   const struct glsl_colour_edit *x = (const struct glsl_colour_edit*)a;
   const struct glsl_colour_edit *y = (const struct glsl_colour_edit*)b;
   return x->at < y->at ? -1 : x->at > y->at ? 1
      : x->kind < y->kind ? -1 : x->kind > y->kind;
}

/* Declarations expand inside each active stage, after its extensions. */
static const char glsl_colour_prefix[] =
   "\n#ifndef RARCH_VIEWER_DECLARE\n"
   "#ifdef GL_ES\n"
   "#if !defined(GL_FRAGMENT_PRECISION_HIGH) && !defined(VERTEX)\n"
   "#error HDR viewer samples require highp\n"
   "#endif\n"
   "#define RARCH_VIEWER_HP highp\n"
   "#else\n"
   "#define RARCH_VIEWER_HP\n"
   "#endif\n"
   "#define RARCH_VIEWER_DECLARE \\\n"
   "uniform RARCH_VIEWER_HP vec4 " GLSL_COLOUR_ROW0 "; \\\n"
   "uniform RARCH_VIEWER_HP vec4 " GLSL_COLOUR_ROW1 "; \\\n"
   "uniform RARCH_VIEWER_HP vec4 " GLSL_COLOUR_ROW2 "; \\\n"
   "RARCH_VIEWER_HP vec4 RARCH_viewer_colour(RARCH_VIEWER_HP vec4 v) { \\\n"
   " if (" GLSL_COLOUR_ROW0 ".w < 0.5) return v; \\\n"
   " RARCH_VIEWER_HP vec3 x = vec3(dot(" GLSL_COLOUR_ROW0 ".xyz,v.rgb), \\\n"
   " dot(" GLSL_COLOUR_ROW1 ".xyz,v.rgb),dot(" GLSL_COLOUR_ROW2 ".xyz,v.rgb)); \\\n"
   " return vec4(sign(x)*pow(abs(x),vec3(" GLSL_COLOUR_ROW1 ".w)),v.a); }\n"
   "#endif\n";

static const char glsl_colour_declare[] =
   "\n#ifndef RARCH_VIEWER_DEFINED\n#define RARCH_VIEWER_DEFINED\n"
   "RARCH_VIEWER_DECLARE\n#endif\n";

static char *glsl_colour_rewrite(const char *src, const char *const *frame,
      unsigned num_frame, bool *changed)
{
   static const char *const reads[] = {
      "texture2D", "texture", "texture2DLod", "textureLod",
      "texture2DProj", "textureProj", "texture2DProjLod", "textureProjLod",
      "textureOffset", "textureLodOffset", "textureGrad", "textureGradOffset",
      "textureProjOffset", "textureProjLodOffset", "textureProjGrad", "textureProjGradOffset",
      "texelFetch", "texelFetchOffset", "texture2DLodEXT", "texture2DGradEXT"
   };
   static const char *const queries[] = {"textureSize", "textureQueryLevels", "textureQueryLod"};
   static const char *const text[] = {glsl_colour_prefix, glsl_colour_declare,
      "RARCH_viewer_colour(", ")"};
   struct glsl_rect_scan s;
   struct glsl_colour_edit *edit = NULL;
   size_t i, n = 0, len, out_len, at, o;
   unsigned functions = 0;
   char *buf = NULL, *out = NULL;
   *changed = false;
   if (!src) return NULL;
   len = strlen(src);
   if (!(buf = (char*)malloc(len + 1))) return NULL;
   memcpy(buf, src, len + 1);
   glsl_rect_blank_comments(buf);
   memset(&s, 0, sizeof(s));
   s.src = buf;
   glsl_rect_scan_source(&s);
   if (s.failed || s.num_tok > (((size_t)-1 / sizeof(*edit)) - 1) / 2)
      goto fail;
   edit = (struct glsl_colour_edit*)calloc(2 * s.num_tok + 1, sizeof(*edit));
   if (!edit) goto fail;
   glsl_rect_frame_params(&s, frame, num_frame);
   if (s.failed) goto fail;
   for (i = 0; i < s.num_undef; i++)
      if (glsl_rect_resolves_to(&s, s.undef[i].name, s.undef[i].len, frame, num_frame, 0)
            || glsl_rect_resolves_to(&s, s.undef[i].name, s.undef[i].len,
                  reads, sizeof(reads) / sizeof(*reads), 0)) goto fail;
   for (i = 0; i < s.num_tok; i++)
   {
      const struct glsl_rect_token *t = &s.tok[i];
      int r;
      if (!t->region && !t->brace && i && glsl_rect_tok_is(&s, i, "{")
            && glsl_rect_tok_is(&s, i - 1, ")"))
      {
         size_t j = i;
         while (j && !s.tok[j - 1].region
               && !glsl_rect_tok_is(&s, j - 1, ";")
               && !glsl_rect_tok_is(&s, j - 1, "}")
               && !glsl_rect_tok_is(&s, j - 1, "{")) j--;
         edit[n].at = s.tok[j].start;
         edit[n++].kind = 1;
         functions++;
      }
      if (t->type != GLSL_RECT_TOK_IDENT || t->alias_body || (t->flags & GLSL_RECT_FLAG_SKIP))
         continue;
      r = (t->flags & GLSL_RECT_FLAG_FRAME) ? 1
         : glsl_rect_resolves_to(&s, t->start, t->len, frame, num_frame, 0);
      if (r < 0) goto fail;
      if (!r) continue;
      if (i && glsl_rect_tok_is(&s, i - 1, "sampler2D")) continue;
      if (i < 2 || !glsl_rect_tok_is(&s, i - 1, "(")
            || !glsl_rect_tok_is(&s, i + 1, ",")
            || s.tok[i - 2].region != t->region) goto fail;
      if (glsl_rect_resolves_to(&s, s.tok[i - 2].start, s.tok[i - 2].len,
               queries, sizeof(queries) / sizeof(*queries), 0) == 1) continue;
      if (glsl_rect_resolves_to(&s, s.tok[i - 2].start, s.tok[i - 2].len,
               reads, sizeof(reads) / sizeof(*reads), 0) != 1) goto fail;
      {
         size_t end = glsl_rect_match(&s, i - 1, '(', ')');
         if (end == s.num_tok || s.tok[end].region != t->region) goto fail;
         edit[n].at = s.tok[i - 2].start;
         edit[n++].kind = 2;
         edit[n].at = s.tok[end].start + 1;
         edit[n++].kind = 3;
         *changed = true;
      }
   }
   if (!*changed)
   {
      out = (char*)malloc(len + 1);
      if (out) memcpy(out, src, len + 1);
      goto done;
   }
   if (!functions) goto fail;
   {
      const char *version = strstr(buf, "#version");
      edit[n].at = version ? glsl_rect_line_end(buf, (size_t)(version - buf)) : 0;
      edit[n++].kind = 0;
   }
   qsort(edit, n, sizeof(*edit), glsl_colour_edit_cmp);
   out_len = len + 1;
   for (i = 0; i < n; i++)
   {
      size_t add = strlen(text[edit[i].kind]);
      if (out_len > (size_t)-1 - add) goto fail;
      out_len += add;
   }
   if (!(out = (char*)malloc(out_len))) goto fail;
   at = o = 0;
   for (i = 0; i < n; i++)
   {
      size_t size = strlen(text[edit[i].kind]);
      memcpy(out + o, src + at, edit[i].at - at);
      o += edit[i].at - at;
      memcpy(out + o, text[edit[i].kind], size);
      o += size;
      at = edit[i].at;
   }
   memcpy(out + o, src + at, len - at + 1);
done:
   free(edit);
   free(s.tok);
   free(s.alias);
   free(buf);
   return out;
fail:
   free(out);
   out = NULL;
   *changed = false;
   goto done;
}

#endif
