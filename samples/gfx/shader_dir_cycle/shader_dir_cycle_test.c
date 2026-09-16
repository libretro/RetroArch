/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (shader_dir_cycle_test.c).
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

/* What the Next/Previous Shader hotkeys cycle through, over a shader
 * tree built here on disk. video_shader_parse.c is compiled for real;
 * only the frontend around it is stubbed.
 *
 * The tree is the one from the report on PR #19558: a core shader
 * override in the config directory whose #reference points at a
 * preset in the Video Shaders root, which is itself a simple preset
 * (#reference) pointing into a subfolder.
 *
 *   shaders/01-crt-basic.slangp      full preset
 *   shaders/02-crt-plain.slangp      full preset
 *   shaders/06-ntsc-glow.slangp      -> shaders/ntsc/06-ntsc-base.slangp
 *   shaders/07-scanline.slangp       full preset
 *   shaders/ntsc/06-ntsc-base.slangp        full preset
 *   shaders/ntsc/06-ntsc-composite.slangp   -> 06-ntsc-base.slangp
 *   shaders/ntsc/06-ntsc-svideo.slangp      -> 06-ntsc-base.slangp
 *   config/FCEUmm/FCEUmm.slangp      -> shaders/06-ntsc-glow.slangp
 *
 * The hotkeys step within the folder holding the preset they anchor
 * on. With Remember Last Used Shader Directory on, that is the end of
 * the loaded preset's #reference chain - where the shader being
 * rendered is defined - so the core override cycles ntsc/ rather than
 * the root it passes through. With the setting off the root is what
 * gets cycled, and a preset there stands for itself.
 *
 * ntsc/ holds simple presets to cover the anchor moving on: cycling
 * onto one and anchoring on it again would follow its reference back
 * to 06-ntsc-base.slangp, and the hotkey would never leave the pair.
 *
 * Fixture root: argv[1], else ./shader_dir_cycle_fixture. Removed on
 * exit, so nothing is left behind and nothing lands in /tmp.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include "../../../configuration.h"
#include "../../../runloop.h"
#include "../../../gfx/video_shader_parse.h"

/* stubs_retroarch.c */
void stub_reset(void);
void stub_set_runtime_preset(const char *path);
unsigned stub_applied_count(void);
const char *stub_applied(unsigned i);

static int failures;
static char fixture[PATH_MAX_LENGTH];
static char shader_dir[PATH_MAX_LENGTH];
static char ntsc_dir[PATH_MAX_LENGTH];
static char config_dir[PATH_MAX_LENGTH];
static char core_dir[PATH_MAX_LENGTH];

static void die(const char *what, const char *path)
{
   fprintf(stderr, "FATAL: %s: %s\n", what, path);
   exit(1);
}

static void write_file(const char *path, const char *contents)
{
   FILE *f = fopen(path, "w");
   if (!f)
      die("cannot write", path);
   fputs(contents, f);
   fclose(f);
}

static void make_dir(char *s, size_t len, const char *parent,
      const char *name)
{
   fill_pathname_join(s, parent, name, len);
   if (!path_mkdir(s))
      die("cannot create", s);
}

static void make_full_preset(char *s, size_t len, const char *dir,
      const char *name)
{
   fill_pathname_join(s, dir, name, len);
   write_file(s, "shaders = \"1\"\nshader0 = \"stock.slang\"\n");
}

static void make_simple_preset(char *s, size_t len, const char *dir,
      const char *name, const char *reference)
{
   char buf[PATH_MAX_LENGTH + 32];
   fill_pathname_join(s, dir, name, len);
   snprintf(buf, sizeof(buf), "#reference \"%s\"\n", reference);
   write_file(s, buf);
}

static void build_fixture(void)
{
   char path[PATH_MAX_LENGTH];
   char glow_ref[PATH_MAX_LENGTH];

   if (!path_mkdir(fixture))
      die("cannot create", fixture);

   make_dir(shader_dir, sizeof(shader_dir), fixture, "shaders");
   make_dir(ntsc_dir,   sizeof(ntsc_dir),   shader_dir, "ntsc");
   make_dir(config_dir, sizeof(config_dir), fixture, "config");
   make_dir(core_dir,   sizeof(core_dir),   config_dir, "FCEUmm");

   make_full_preset(path, sizeof(path), ntsc_dir, "06-ntsc-base.slangp");
   make_simple_preset(path, sizeof(path), ntsc_dir,
         "06-ntsc-composite.slangp", "06-ntsc-base.slangp");
   make_simple_preset(path, sizeof(path), ntsc_dir,
         "06-ntsc-svideo.slangp", "06-ntsc-base.slangp");

   make_full_preset(path, sizeof(path), shader_dir, "01-crt-basic.slangp");
   make_full_preset(path, sizeof(path), shader_dir, "02-crt-plain.slangp");
   make_full_preset(path, sizeof(path), shader_dir, "07-scanline.slangp");

   /* A reference is relative to the preset holding it, as the shader
    * packs write them */
   make_simple_preset(glow_ref, sizeof(glow_ref), shader_dir,
         "06-ntsc-glow.slangp", "ntsc/06-ntsc-base.slangp");

   /* RetroArch's own preset, as applying from the menu writes it */
   make_simple_preset(path, sizeof(path), shader_dir,
         "retroarch.slangp", "ntsc/06-ntsc-base.slangp");

   /* The core override names an absolute path, as ":/shaders/..."
    * expands to */
   make_simple_preset(path, sizeof(path), core_dir, "FCEUmm.slangp",
         glow_ref);
}

static void remove_file(const char *dir, const char *name)
{
   char path[PATH_MAX_LENGTH];
   fill_pathname_join(path, dir, name, sizeof(path));
   remove(path);
}

static void tear_down_fixture(void)
{
   remove_file(core_dir,   "FCEUmm.slangp");
   remove_file(ntsc_dir,   "06-ntsc-base.slangp");
   remove_file(ntsc_dir,   "06-ntsc-composite.slangp");
   remove_file(ntsc_dir,   "06-ntsc-svideo.slangp");
   remove_file(shader_dir, "01-crt-basic.slangp");
   remove_file(shader_dir, "02-crt-plain.slangp");
   remove_file(shader_dir, "06-ntsc-glow.slangp");
   remove_file(shader_dir, "07-scanline.slangp");
   remove_file(shader_dir, "retroarch.slangp");
   remove(core_dir);
   remove(config_dir);
   remove(ntsc_dir);
   remove(shader_dir);
   remove(fixture);
}

static settings_t *configure(bool remember_last_dir)
{
   settings_t *settings = config_get_ptr();

   strlcpy(settings->paths.directory_video_shader, shader_dir,
         sizeof(settings->paths.directory_video_shader));
   strlcpy(settings->paths.directory_menu_config, config_dir,
         sizeof(settings->paths.directory_menu_config));
   settings->bools.video_shader_remember_last_dir = remember_last_dir;
   settings->bools.show_hidden_files              = false;
   settings->bools.video_shader_watch_files       = false;

   return settings;
}

/* Presses next @presses times and checks the applied presets against
 * @want, each entry a basename expected inside @want_dir. */
static void run_case(const char *name, bool remember_last_dir,
      const char *runtime_preset, const char *want_dir,
      const char *const *want, unsigned presses)
{
   struct rarch_dir_shader_list dir_list;
   settings_t *settings = configure(remember_last_dir);
   unsigned i;
   bool ok              = true;

   memset(&dir_list, 0, sizeof(dir_list));
   stub_reset();
   stub_set_runtime_preset(runtime_preset);

   for (i = 0; i < presses; i++)
      video_shader_dir_check_shader(NULL, settings, &dir_list,
            true, false);

   if (stub_applied_count() != presses)
      ok = false;
   else
      for (i = 0; i < presses; i++)
      {
         char expected[PATH_MAX_LENGTH];
         fill_pathname_join(expected, want_dir, want[i], sizeof(expected));
         if (!string_is_equal(stub_applied(i), expected))
            ok = false;
      }

   printf("%-58s %s\n", name, ok ? "ok" : "FAILED");

   if (!ok)
   {
      failures++;
      for (i = 0; i < presses; i++)
      {
         char expected[PATH_MAX_LENGTH];
         fill_pathname_join(expected, want_dir, want[i], sizeof(expected));
         printf("      press %u\n         want %s\n         got  %s\n",
               i + 1, expected,
               i < stub_applied_count() ? stub_applied(i) : "(nothing)");
      }
   }

   video_shader_dir_free_shader(&dir_list, remember_last_dir);
}

/* Cycles @off_presses times with the setting off, turns it on, and
 * checks the presets the next @count presses apply, each entry a path
 * below the Video Shaders root. The list built while it was off holds
 * the loaded preset, which must not keep the cycling in the root once
 * the setting asks for the folder it points into. */
static void run_toggle_case(const char *name, const char *runtime_preset,
      unsigned off_presses, const char *const *want, unsigned count)
{
   struct rarch_dir_shader_list dir_list;
   settings_t *settings = configure(false);
   unsigned i;
   bool ok              = true;

   memset(&dir_list, 0, sizeof(dir_list));
   stub_reset();
   stub_set_runtime_preset(runtime_preset);

   for (i = 0; i < off_presses; i++)
      video_shader_dir_check_shader(NULL, settings, &dir_list, true, false);

   settings = configure(true);

   for (i = 0; i < count; i++)
      video_shader_dir_check_shader(NULL, settings, &dir_list, true, false);

   if (stub_applied_count() != off_presses + count)
      ok = false;
   else
      for (i = 0; i < count; i++)
      {
         char expected[PATH_MAX_LENGTH];
         fill_pathname_join(expected, shader_dir, want[i], sizeof(expected));
         if (!string_is_equal(stub_applied(off_presses + i), expected))
            ok = false;
      }

   printf("%-58s %s\n", name, ok ? "ok" : "FAILED");

   if (!ok)
   {
      failures++;
      for (i = 0; i < count; i++)
      {
         char expected[PATH_MAX_LENGTH];
         fill_pathname_join(expected, shader_dir, want[i], sizeof(expected));
         printf("      press %u\n         want %s\n         got  %s\n",
               i + 1, expected,
               off_presses + i < stub_applied_count()
                  ? stub_applied(off_presses + i) : "(nothing)");
      }
   }

   video_shader_dir_free_shader(&dir_list, true);
}

int main(int argc, char **argv)
{
   char auto_preset[PATH_MAX_LENGTH];
   char root_preset[PATH_MAX_LENGTH];

   /* The presets in the Video Shaders root, in listing order from the
    * one the core override resolves to. retroarch.slangp is one of
    * them: cycling the root, it stands for itself. */
   static const char * const root_cycle[] =
   {
      "07-scanline.slangp",
      "retroarch.slangp",
      "01-crt-basic.slangp",
      "02-crt-plain.slangp"
   };

   /* The presets in ntsc/, in listing order from the one the
    * #reference chain ends at */
   static const char * const ntsc_cycle[] =
   {
      "06-ntsc-composite.slangp",
      "06-ntsc-svideo.slangp",
      "06-ntsc-base.slangp",
      "06-ntsc-composite.slangp"
   };

   if (argc > 1)
      strlcpy(fixture, argv[1], sizeof(fixture));
   else
      strlcpy(fixture, "shader_dir_cycle_fixture", sizeof(fixture));

   /* Preset paths reach the cycling absolute, so the fixture is too */
   if (!path_is_absolute(fixture))
   {
      char cwd[PATH_MAX_LENGTH];
      char relative[PATH_MAX_LENGTH];

      if (!getcwd(cwd, sizeof(cwd)))
         die("cannot resolve", fixture);

      strlcpy(relative, fixture, sizeof(relative));
      fill_pathname_join(fixture, cwd, relative, sizeof(fixture));
   }

   build_fixture();
   /* Registered once the paths it removes are all set, so a failing
    * case leaves nothing behind either */
   atexit(tear_down_fixture);

   fill_pathname_join(auto_preset, core_dir, "FCEUmm.slangp",
         sizeof(auto_preset));
   fill_pathname_join(root_preset, shader_dir, "06-ntsc-glow.slangp",
         sizeof(root_preset));

   /* Remember Last Used Shader Directory on: the core override in the
    * config directory stands in for the shader it ends up loading,
    * and that shader's own folder is what gets cycled */
   run_case("remember on, core override into a subfolder",
         true, auto_preset, ntsc_dir, ntsc_cycle, 4);

   /* Loaded as itself rather than through the core override, that
    * same preset anchors where it sits: a preset saved from the menu
    * is a simple preset, and cycling its source folder instead of the
    * folder it was saved into is not what the setting asks for */
   run_case("remember on, root simple preset loaded",
         true, root_preset, shader_dir, root_cycle, 4);

   /* Remember Last Used Shader Directory off cycles the root too */
   run_case("remember off, core override into the shader root",
         false, auto_preset, shader_dir, root_cycle, 4);

   /* Cycling the root with the setting off stops on retroarch.slangp,
    * which stood for itself while it was off. Turning the setting on
    * makes it the wrapper it is again, and the cycling follows it out
    * of the root into the folder it references. */
   {
      static const char * const into_ntsc[] =
      {
         "ntsc/06-ntsc-composite.slangp"
      };
      run_toggle_case("remember turned on onto retroarch.slangp",
            auto_preset, 2, into_ntsc, 1);
   }

   /* Stopping on any other preset keeps cycling the root, and keeps
    * stepping through it: reaching retroarch.slangp mid-cycle must
    * not send the press after it into ntsc/ */
   {
      static const char * const stay_in_root[] =
      {
         "retroarch.slangp",
         "01-crt-basic.slangp"
      };
      run_toggle_case("remember turned on onto a plain root preset",
            auto_preset, 1, stay_in_root, 2);
   }

   if (failures)
      printf("%d case(s) failed\n", failures);
   else
      printf("all cases passed\n");

   return failures ? 1 : 0;
}
