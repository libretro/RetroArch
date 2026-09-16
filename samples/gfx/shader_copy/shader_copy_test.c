/* video_shader_copy_for_menu() is a drop-in for the disk re-parse.
 *
 * Links the shipping objects with only main() replaced. A fixture
 * preset chain exercises what the copy replaces: a child preset
 * referencing a parent, whose pass source carries a
 * #pragma parameter, so the parse walks the reference chain and
 * resolves parameters exactly as the driver's own load did.
 *
 * Lanes:
 *  - parse the chain into "driver" and plant heap source strings in
 *    a pass, the way glsl/d3d compilation does;
 *  - copy_for_menu into "menu": every byte equal except the two
 *    driver-owned string pointers, which must be NULL in the copy
 *    while the originals stay intact (freed once at the end - the
 *    run being ASan-clean is the no-aliasing proof);
 *  - the copy is independent: mutating a menu parameter leaves the
 *    driver struct untouched.
 *
 *   ./configure --disable-qt && make
 *   samples/gfx/shader_copy/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <streams/file_stream.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../gfx/video_shader_parse.h"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

static struct video_shader drv;
static struct video_shader menu;

int main(int argc, char *argv[])
{
   char dir[512];
   char child[640];
   char parent[640];
   char slang[640];
   char cmd[700];
   FILE *f;

   (void)argc;
   (void)argv;

   snprintf(dir, sizeof(dir), "/tmp/shader_copy_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", dir);
   if (system(cmd) != 0)
      return 1;
   snprintf(child,  sizeof(child),  "%s/child.slangp",  dir);
   snprintf(parent, sizeof(parent), "%s/parent.slangp", dir);
   snprintf(slang,  sizeof(slang),  "%s/pass.slang",    dir);

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();

   if ((f = fopen(slang, "wb")))
   {
      fputs("#version 450\n"
            "#pragma parameter COPY_GAIN \"Gain\" 1.5 0.0 3.0 0.1\n"
            "#pragma stage vertex\nvoid main(){}\n"
            "#pragma stage fragment\nvoid main(){}\n", f);
      fclose(f);
   }
   if ((f = fopen(parent, "wb")))
   {
      fputs("shaders = 1\nshader0 = \"pass.slang\"\n"
            "COPY_GAIN = \"2.5\"\n", f);
      fclose(f);
   }
   if ((f = fopen(child, "wb")))
   {
      fprintf(f, "#reference \"parent.slangp\"\n");
      fclose(f);
   }

   /* The parse the driver would have done. */
   CHECK(video_shader_load_preset_into_shader(child, &drv),
         "fixture: reference-chain parse failed");
   CHECK(drv.passes == 1, "fixture: passes %u", drv.passes);
   CHECK(drv.num_parameters >= 1,
         "fixture: no parameters resolved from the pass source");
   if (drv.num_parameters >= 1)
      CHECK(drv.parameters[0].current > 2.4f
            && drv.parameters[0].current < 2.6f,
            "fixture: referenced value not applied (%f)",
            (double)drv.parameters[0].current);

   /* Driver-side compilation state: heap source strings. */
   drv.pass[0].source.string.vertex   = strdup("void main(){}");
   drv.pass[0].source.string.fragment = strdup("void main(){}");

   video_shader_copy_for_menu(&menu, &drv);

   /* Byte parity, modulo the two cleared pointers. */
   CHECK(menu.pass[0].source.string.vertex   == NULL
      && menu.pass[0].source.string.fragment == NULL,
         "copy aliases the driver-owned source strings");
   CHECK(drv.pass[0].source.string.vertex != NULL,
         "copy stole the driver's source strings");
   {
      struct video_shader drv_view = drv;
      drv_view.pass[0].source.string.vertex   = NULL;
      drv_view.pass[0].source.string.fragment = NULL;
      CHECK(memcmp(&drv_view, &menu, sizeof(drv_view)) == 0,
            "copy differs from the parse beyond the cleared strings");
   }

   /* Independence. */
   if (menu.num_parameters >= 1)
   {
      menu.parameters[0].current = 0.25f;
      CHECK(drv.parameters[0].current > 2.4f,
            "menu edit reached the driver struct");
   }

   free(drv.pass[0].source.string.vertex);
   free(drv.pass[0].source.string.fragment);

   snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
   if (system(cmd) != 0) { /* fixture dir left behind; harmless */ }

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("shader_copy: all lanes passed\n");
   return 0;
}
