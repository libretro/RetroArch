/* Compiled with UUT_CFLAGS so settings_t here has the exact layout
 * gfx/gfx_thumbnail.c sees.  The stub pokes its config blob through
 * these exports, keeping the offset tied to configuration.h by the
 * compiler rather than by hand. */
#include <stddef.h>

#include "configuration.h"

const size_t settings_layout_sizeof = sizeof(settings_t);
const size_t settings_layout_preview_audio_off =
      offsetof(settings_t, bools.menu_thumbnail_preview_audio);
const size_t settings_layout_preview_threads_off =
      offsetof(settings_t, uints.menu_thumbnail_preview_threads);
const size_t settings_layout_gfx_thumbnails_off =
      offsetof(settings_t, uints.gfx_thumbnails);
const size_t settings_layout_dir_thumbnails_off =
      offsetof(settings_t, paths.directory_thumbnails);

/* The menu, runloop and playlist entry gfx_savestate_thumbnail_get_path
 * reads when the menu is on a playlist entry and no core is running,
 * laid into the stubs' blobs with the UUT's own struct layouts. Returns
 * the entry the stub playlist hands out, or NULL when a blob is too
 * small for its struct. */
#include <string.h>
#include <compat/strl.h>
#include "menu/menu_driver.h"
#include "runloop.h"
#include "playlist.h"

const void *layout_savestate_entry_setup(void *menu_blob, size_t menu_size,
      void *runloop_blob, size_t runloop_size,
      const char *savestate_dir, const char *entry_path)
{
   static menu_handle_t         handle;
   static struct playlist_entry entry;
   static char                  path[256];
   struct menu_state *menu_st  = (struct menu_state*)menu_blob;
   runloop_state_t *runloop_st = (runloop_state_t*)runloop_blob;

   if (     menu_size    < sizeof(*menu_st)
         || runloop_size < sizeof(*runloop_st))
      return NULL;

   memset(&handle, 0, sizeof(handle));
   memset(&entry, 0, sizeof(entry));
   strlcpy(path, entry_path, sizeof(path));
   entry.path           = path;
   menu_st->driver_data = &handle;
   runloop_st->flags   &= ~RUNLOOP_FLAG_CORE_RUNNING;
   strlcpy(runloop_st->savestate_dir, savestate_dir,
         sizeof(runloop_st->savestate_dir));
   return &entry;
}
