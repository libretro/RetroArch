/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (shader_preset_combine_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Prepend/append of a preset, through the real preset parser with
 * gfx/video_shader_parse.c compiled into this unit so the static
 * combine step is reachable. The second preset is appended to the
 * first in place:
 *
 *  - its first pass stops taking the simulated SourceSize;
 *  - LUTs keep their filter, wrap and mipmap settings, and a LUT id
 *    the first preset already has keeps the first preset's entry;
 *  - the parameter list merged from the two resolved lists matches a
 *    fresh resolve of every pass source, with the second preset's
 *    values winning;
 *  - passes past the limit are dropped with the parameters they own,
 *    and the HDR mode follows the second preset only when all of its
 *    passes fit. */

#include "../../../gfx/video_shader_parse.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static char fixture[PATH_MAX_LENGTH];

#define CHECK(cond, what) do { if (!(cond)) { \
   fprintf(stderr, "[FAIL] %s (line %d)\n", what, __LINE__); \
   failures++; } } while (0)

static void write_file(const char *name, const char *contents,
      char *s, size_t len)
{
   FILE *f;
   fill_pathname_join(s, fixture, name, len);
   if (!(f = fopen(s, "wb")))
   {
      fprintf(stderr, "cannot write %s\n", s);
      exit(1);
   }
   fputs(contents, f);
   fclose(f);
}

static struct video_shader *load(const char *path)
{
   struct video_shader *shader = (struct video_shader*)
      calloc(1, sizeof(*shader));
   if (!shader || !video_shader_load_preset_into_shader(path, shader))
   {
      fprintf(stderr, "cannot load %s\n", path);
      exit(1);
   }
   return shader;
}

static const struct video_shader_parameter *find_param(
      const struct video_shader *s, const char *id)
{
   unsigned i;
   for (i = 0; i < s->num_parameters; i++)
      if (string_is_equal(s->parameters[i].id, id))
         return &s->parameters[i];
   return NULL;
}

static const struct video_shader_lut *find_lut(
      const struct video_shader *s, const char *id)
{
   unsigned i;
   for (i = 0; i < s->luts; i++)
      if (string_is_equal(s->lut[i].id, id))
         return &s->lut[i];
   return NULL;
}

/* What the combine produced, against a resolve of every pass source
 * with the two presets' values laid over it, first then second. */
static void check_matches_resolve(const struct video_shader *combined,
      const struct video_shader *first, const struct video_shader *second)
{
   unsigned i;
   struct video_shader *ref = (struct video_shader*)
      calloc(1, sizeof(*ref));

   ref->passes = combined->passes;
   for (i = 0; i < combined->passes; i++)
      ref->pass[i] = combined->pass[i];
   video_shader_resolve_parameters(ref);

   /* The slang resolver keeps the first declaration of an id; the plain
    * reader this unit builds keeps repeats, so they are dropped here. */
   {
      unsigned j, n = 0;
      for (i = 0; i < ref->num_parameters; i++)
      {
         for (j = 0; j < n; j++)
            if (string_is_equal(ref->parameters[j].id, ref->parameters[i].id))
               break;
         if (j < n)
            continue;
         if (n != i)
         {
            struct video_shader_parameter *d = &ref->parameters[n];
            struct video_shader_parameter *c = &ref->parameters[i];
            d->pass    = c->pass;
            d->minimum = c->minimum;
            d->initial = c->initial;
            d->maximum = c->maximum;
            d->step    = c->step;
            strlcpy(d->id,   c->id,   sizeof(d->id));
            strlcpy(d->desc, c->desc, sizeof(d->desc));
            video_shader_param_set(d, video_shader_param_get(c));
         }
         n++;
      }
      ref->num_parameters = n;
   }

   for (i = 0; i < ref->num_parameters; i++)
   {
      const struct video_shader_parameter *p;
      if ((p = find_param(first, ref->parameters[i].id)))
         video_shader_param_set(&ref->parameters[i],
               video_shader_param_get(p));
      if ((p = find_param(second, ref->parameters[i].id)))
         video_shader_param_set(&ref->parameters[i],
               video_shader_param_get(p));
   }

   CHECK(ref->num_parameters == combined->num_parameters,
         "the merged list has as many parameters as a fresh resolve");
   for (i = 0; i < ref->num_parameters && i < combined->num_parameters; i++)
   {
      const struct video_shader_parameter *a = &ref->parameters[i];
      const struct video_shader_parameter *b = &combined->parameters[i];
      CHECK(string_is_equal(a->id, b->id),
            "the merged list is in resolve order");
      CHECK(a->pass == b->pass, "a merged parameter keeps its owning pass");
      CHECK(video_shader_param_get(a) == video_shader_param_get(b),
            "a merged parameter has the value a resolve gives it");
      CHECK(a->initial == b->initial && a->minimum == b->minimum
            && a->maximum == b->maximum && a->step == b->step,
            "a merged parameter keeps its declared range");
   }
   free(ref);
}

static void check_append(const char *first_path, const char *second_path)
{
   const struct video_shader_lut *lut;
   const struct video_shader_parameter *p;
   struct video_shader *a      = load(first_path);
   struct video_shader *b      = load(second_path);
   struct video_shader *a_orig = load(first_path);

   video_shader_append_shader(a, b);

   CHECK(a->passes == 4, "the combined preset has four passes");
   CHECK( a->pass[0].use_simulated_res,
         "the first preset's first pass keeps the simulated size");
   CHECK(!a->pass[2].use_simulated_res,
         "the second preset's first pass no longer reads the core frame");
   CHECK(a->hdr_mode == b->hdr_mode,
         "the HDR mode follows the second preset when all of it fits");

   CHECK(a->luts == 2, "the duplicate LUT id is kept once");
   lut = find_lut(a, "lutA");
   CHECK(lut && strstr(lut->path, "a.png"),
         "a LUT id both presets have keeps the first preset's entry");
   CHECK(lut && lut->filter == RARCH_FILTER_LINEAR
         && lut->wrap == RARCH_WRAP_REPEAT && lut->mipmap,
         "the first preset's LUT keeps its filter, wrap and mipmap");
   lut = find_lut(a, "lutB");
   CHECK(lut && lut->filter == RARCH_FILTER_NEAREST
         && lut->wrap == RARCH_WRAP_MIRRORED_REPEAT && lut->mipmap,
         "an appended LUT keeps its filter, wrap and mipmap");

   p = find_param(a, "S");
   CHECK(p && video_shader_param_get(p) == 0.75f,
         "a parameter both presets set takes the second preset's value");
   p = find_param(a, "A");
   CHECK(p && video_shader_param_get(p) == 2.0f,
         "a parameter only the first preset sets keeps its value");
   p = find_param(a, "C");
   CHECK(p && p->pass == 3,
         "an appended parameter is owned by its pass in the combined chain");

   check_matches_resolve(a, a_orig, b);
   free(a_orig);
   free(b);
   free(a);
}

static void check_limit(const char *first_path, const char *second_path)
{
   unsigned i;
   struct video_shader *a = load(first_path);
   struct video_shader *b = load(second_path);
   struct video_shader *a_orig;

   /* Fill the first preset to one pass short of the limit. */
   for (i = a->passes; i < GFX_MAX_SHADERS - 1; i++)
   {
      a->pass[i] = a->pass[1];
      a->pass[i].use_simulated_res = false;
   }
   a->passes = GFX_MAX_SHADERS - 1;
   a_orig    = (struct video_shader*)malloc(sizeof(*a_orig));
   memcpy(a_orig, a, sizeof(*a));

   video_shader_append_shader(a, b);

   CHECK(a->passes == GFX_MAX_SHADERS, "the combined chain stops at the limit");
   CHECK(a->hdr_mode == 0,
         "the HDR mode is cleared when the second preset is cut short");
   CHECK(find_param(a, "B") != NULL,
         "a parameter owned by a kept pass is merged");
   CHECK(find_param(a, "C") == NULL,
         "a parameter owned by a dropped pass is left out with it");

   check_matches_resolve(a, a_orig, b);
   free(a_orig);
   free(b);
   free(a);
}

int main(int argc, char **argv)
{
   char first[PATH_MAX_LENGTH];
   char second[PATH_MAX_LENGTH];
   char tmp[PATH_MAX_LENGTH];

   if (argc < 2)
   {
      fprintf(stderr, "usage: %s <scratch-dir>\n", argv[0]);
      return 2;
   }
   strlcpy(fixture, argv[1], sizeof(fixture));

   write_file("pa.slang",
         "#version 450\n"
         "#pragma parameter A \"A\" 1.0 0.0 4.0 1.0\n"
         "#pragma parameter S \"S\" 0.5 0.0 1.0 0.25\n", tmp, sizeof(tmp));
   write_file("pa2.slang", "#version 450\n", tmp, sizeof(tmp));
   write_file("pb.slang",
         "#version 450\n"
         "#pragma parameter B \"B\" 1.0 0.0 4.0 1.0\n"
         "#pragma parameter S \"S\" 0.5 0.0 1.0 0.25\n", tmp, sizeof(tmp));
   write_file("pc.slang",
         "#version 450\n"
         "#pragma parameter C \"C\" 0.0 0.0 1.0 1.0\n", tmp, sizeof(tmp));
   write_file("a.png", "", tmp, sizeof(tmp));
   write_file("a2.png", "", tmp, sizeof(tmp));
   write_file("b.png", "", tmp, sizeof(tmp));

   write_file("first.slangp",
         "shaders = \"2\"\n"
         "shader0 = \"pa.slang\"\n"
         "shader1 = \"pa2.slang\"\n"
         "textures = \"lutA\"\n"
         "lutA = \"a.png\"\n"
         "lutA_linear = \"true\"\n"
         "lutA_wrap_mode = \"repeat\"\n"
         "lutA_mipmap = \"true\"\n"
         "parameters = \"A;S\"\n"
         "A = \"2.0\"\n"
         "S = \"0.25\"\n", first, sizeof(first));
   write_file("second.slangp",
         "shaders = \"2\"\n"
         "shader0 = \"pb.slang\"\n"
         "shader1 = \"pc.slang\"\n"
         "hdr_mode = \"1\"\n"
         "textures = \"lutA;lutB\"\n"
         "lutA = \"a2.png\"\n"
         "lutB = \"b.png\"\n"
         "lutB_linear = \"false\"\n"
         "lutB_wrap_mode = \"mirrored_repeat\"\n"
         "lutB_mipmap = \"true\"\n"
         "parameters = \"S;B\"\n"
         "S = \"0.75\"\n"
         "B = \"3.0\"\n", second, sizeof(second));

   check_append(first, second);
   check_limit(first, second);

   if (failures)
   {
      fprintf(stderr, "%d check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] shader_preset_combine_test\n");
   return 0;
}
