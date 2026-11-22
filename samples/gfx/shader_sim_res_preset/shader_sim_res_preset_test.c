/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (shader_sim_res_preset_test.c).
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

/* The per-pass use_simulated_res key through the preset parser and
 * writers, with gfx/video_shader_parse.c compiled into this unit:
 *
 *  - a preset without the key tells the first pass only;
 *  - the key overrides that either way;
 *  - a full preset carries the key only where it differs from that;
 *  - a simple (#reference) preset is refused when the flag differs
 *    from the referenced preset, so the change is not lost.
 * Combining presets is covered by shader_preset_combine. */

/* The shared stub reports no config directory, which makes every
 * preset look like it lives there and so refuses all references;
 * this unit's parser asks a local one for a scratch folder instead. */
#define fill_pathname_application_special test_application_special
#include "../../../gfx/video_shader_parse.c"
#undef fill_pathname_application_special

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static char fixture[PATH_MAX_LENGTH];
static char config_dir_path[PATH_MAX_LENGTH];

size_t test_application_special(char *s, size_t len,
      enum application_special_type type)
{
   (void)type;
   return strlcpy(s, config_dir_path, len);
}

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

static char *read_file(const char *path)
{
   long size;
   char *buf;
   FILE *f = fopen(path, "rb");
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   size = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (!(buf = (char*)calloc(1, (size_t)size + 1)))
   {
      fclose(f);
      return NULL;
   }
   if (fread(buf, 1, (size_t)size, f) != (size_t)size)
      buf[0] = '\0';
   fclose(f);
   return buf;
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

static void check_defaults_and_overrides(const char *plain,
      const char *keyed)
{
   struct video_shader *s = load(plain);
   CHECK(s->passes == 3, "the plain preset has three passes");
   CHECK( s->pass[0].use_simulated_res, "pass 0 defaults on");
   CHECK(!s->pass[1].use_simulated_res, "pass 1 defaults off");
   CHECK(!s->pass[2].use_simulated_res, "pass 2 defaults off");
   free(s);

   s = load(keyed);
   CHECK(!s->pass[0].use_simulated_res, "pass 0 turned off by its key");
   CHECK( s->pass[1].use_simulated_res, "pass 1 turned on by its key");
   CHECK(!s->pass[2].use_simulated_res, "pass 2 keeps its default");
   free(s);
}

static void check_full_preset_keys(const char *plain)
{
   char out[PATH_MAX_LENGTH];
   char *text;
   struct video_shader *s = load(plain);
   struct video_shader *back;

   fill_pathname_join(out, fixture, "full_default.slangp", sizeof(out));
   CHECK(video_shader_write_preset(out, s, false), "default preset written");
   text = read_file(out);
   CHECK(text && !strstr(text, "use_simulated_res"),
         "a preset at the defaults carries no use_simulated_res key");
   free(text);

   s->pass[0].use_simulated_res = false;
   s->pass[2].use_simulated_res = true;
   fill_pathname_join(out, fixture, "full_changed.slangp", sizeof(out));
   CHECK(video_shader_write_preset(out, s, false), "changed preset written");
   text = read_file(out);
   CHECK(text && strstr(text, "use_simulated_res0"),
         "pass 0 turned off is written");
   CHECK(text && !strstr(text, "use_simulated_res1"),
         "pass 1 at its default is not written");
   CHECK(text && strstr(text, "use_simulated_res2"),
         "pass 2 turned on is written");
   free(text);

   back = load(out);
   CHECK(!back->pass[0].use_simulated_res
         && !back->pass[1].use_simulated_res
         &&  back->pass[2].use_simulated_res,
         "the written flags read back as saved");
   free(back);
   free(s);
}

static void check_simple_preset_refused(const char *plain)
{
   char out[PATH_MAX_LENGTH];
   char *text;
   struct video_shader *s = load(plain);

   fill_pathname_join(out, fixture, "simple_same.slangp", sizeof(out));
   CHECK(video_shader_write_preset(out, s, true), "unchanged simple written");
   text = read_file(out);
   CHECK(text && strstr(text, "#reference"),
         "an unchanged preset is saved as a reference");
   free(text);

   s->pass[1].use_simulated_res = true;
   fill_pathname_join(out, fixture, "simple_changed.slangp", sizeof(out));
   CHECK(video_shader_write_preset(out, s, true), "changed simple written");
   text = read_file(out);
   CHECK(text && !strstr(text, "#reference")
         && strstr(text, "use_simulated_res1"),
         "a toggled flag forces a full preset instead of a reference");
   free(text);
   free(s);
}

int main(int argc, char **argv)
{
   char plain[PATH_MAX_LENGTH];
   char keyed[PATH_MAX_LENGTH];
   char tmp[PATH_MAX_LENGTH];

   if (argc < 2)
   {
      fprintf(stderr, "usage: %s <scratch-dir>\n", argv[0]);
      return 2;
   }
   strlcpy(fixture, argv[1], sizeof(fixture));
   fill_pathname_join(config_dir_path, fixture, "config",
         sizeof(config_dir_path));

   write_file("stock.slang", "#version 450\n", tmp, sizeof(tmp));
   write_file("plain.slangp",
         "shaders = \"3\"\n"
         "shader0 = \"stock.slang\"\n"
         "shader1 = \"stock.slang\"\n"
         "shader2 = \"stock.slang\"\n", plain, sizeof(plain));
   write_file("keyed.slangp",
         "shaders = \"3\"\n"
         "shader0 = \"stock.slang\"\n"
         "shader1 = \"stock.slang\"\n"
         "shader2 = \"stock.slang\"\n"
         "use_simulated_res0 = \"false\"\n"
         "use_simulated_res1 = \"true\"\n", keyed, sizeof(keyed));

   check_defaults_and_overrides(plain, keyed);
   check_full_preset_keys(plain);
   check_simple_preset_refused(plain);

   if (failures)
   {
      fprintf(stderr, "%d check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] shader_sim_res_preset_test\n");
   return 0;
}
