/* Exercise font_renderer_create_default() itself, with the real stb
 * renderer, since that is where the read/ownership handoff lives. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#include "gfx/font_driver.h"

int read_should_fail  = 0;


static int fails = 0;
#define CHECK(c,m) do { if(!(c)) { printf("  FAIL: %s\n", m); fails++; } } while (0)

int main(void)
{
   const font_renderer_driver_t *drv = NULL;
   void *handle = NULL;
   int ok;

   /* 1. a path whose bytes are junk: stb must reject them, and
    *    create_default must free the buffer it read. */
   ok = font_renderer_create_default(&drv, &handle,
         "/tmp/san/garbage.ttf", 16, FONT_ATLAS_FORMAT_A8);
   printf("  junk font: create_default -> %d\n", ok);
   if (ok) { drv->free(handle); handle = NULL; }

   /* 2. no path: stb walks its candidate list, and with every read
    *    returning junk it should end on the built-in glyphs. */
   read_should_fail = 1;
   ok = font_renderer_create_default(&drv, &handle,
         NULL, 16, FONT_ATLAS_FORMAT_A8);
   printf("  no path:   create_default -> %d (%s)\n", ok,
          ok ? drv->ident : "none");
   CHECK(ok, "built-in fallback reached when nothing can be read");
   if (ok)
   {
      struct font_atlas *at = drv->get_atlas(handle);
      CHECK(at && at->buffer, "atlas produced");
      drv->free(handle);
   }

   /* 3. an explicit path must be honoured, not replaced by whatever
    *    the renderer would have picked on its own. Asking the
    *    resolver unconditionally once swapped the menu font for the
    *    first system font on stb's candidate list. */
   {
      extern const char *last_read_path;
      read_should_fail = 0;
      last_read_path   = NULL;
      ok = font_renderer_create_default(&drv, &handle,
            "/some/explicit/menu-font.ttf", 16, FONT_ATLAS_FORMAT_A8);
      CHECK(last_read_path
            && !strcmp(last_read_path, "/some/explicit/menu-font.ttf"),
            "explicit path is the one read, not a renderer default");
      if (ok)
         drv->free(handle);
   }

   printf("%s (%d failures)\n", fails ? "FAILURES" : "all checks passed", fails);
   return fails ? 1 : 0;
}
