/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* gfx_thumbnail_update_path_cfg's order: label, file name, then the
 * match the thumbnail task stored. Fixtures in ./lookup-fixture. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../../gfx/gfx_thumbnail.h"
#include "../../../gfx/gfx_thumbnail_results.h"

#define LK_ROOT "lookup-fixture"
#define LK_SYS  "Nintendo - Nintendo Entertainment System"
#define LK_PL   "/pl/lookup.lpl"

static char lk_dir[PATH_MAX_LENGTH];

static void lk_file(const char *name, bool present)
{
   char path[PATH_MAX_LENGTH];
   fill_pathname_join_special(path, lk_dir, name, sizeof(path));
   if (present)
      filestream_write_file(path, "png", 3);
   else
      filestream_delete(path);
}

static void lk_entry(gfx_thumbnail_path_data_t *pd, const char *label,
      const char *content_path)
{
   char stem[NAME_MAX_LENGTH];
   gfx_thumbnail_path_reset(pd);
   strlcpy(pd->system, LK_SYS, sizeof(pd->system));
   strlcpy(pd->content_db_name, LK_SYS, sizeof(pd->content_db_name));
   strlcpy(pd->content_path, content_path, sizeof(pd->content_path));
   strlcpy(pd->content_label, label, sizeof(pd->content_label));
   fill_pathname(stem, path_basename(content_path), "", sizeof(stem));
   gfx_thumbnail_fill_content_img(pd->content_img,
         sizeof(pd->content_img), label, false);
   gfx_thumbnail_fill_content_img(pd->content_img_full,
         sizeof(pd->content_img_full), stem, false);
   if (string_is_equal(pd->content_img, pd->content_img_full))
      pd->content_img_full[0] = '\0';
   gfx_thumbnail_fill_content_img(pd->content_img_short,
         sizeof(pd->content_img_short), label, true);
}

/* @want: the file shown, NULL for none */
static bool lk_shows(gfx_thumbnail_path_data_t *pd,
      const gfx_thumbnail_dir_config_t *cfg, const char *want)
{
   if (!gfx_thumbnail_update_path_cfg(pd, GFX_THUMBNAIL_RIGHT, cfg))
      return !want;
   if (!want)
      return !path_is_valid(pd->right_path);
   return    path_is_valid(pd->right_path)
          && string_is_equal(path_basename(pd->right_path), want);
}

static int lk_case(gfx_thumbnail_path_data_t *pd,
      const gfx_thumbnail_dir_config_t *cfg, const char *what,
      const char *want)
{
   if (lk_shows(pd, cfg, want))
   {
      printf("[ok]   lookup: %s\n", what);
      return 0;
   }
   printf("[FAIL] lookup: %s: got %s\n", what, pd->right_path);
   return 1;
}

int gt_thumbnail_lookup_test(void)
{
   gfx_thumbnail_dir_config_t cfg;
   int bad                       = 0;
   gfx_thumbnail_path_data_t *pd = gfx_thumbnail_path_init();

   if (!pd)
      return 1;
   fill_pathname_join_special(lk_dir, LK_ROOT, LK_SYS, sizeof(lk_dir));
   fill_pathname_join_special(lk_dir, lk_dir, "Named_Boxarts", sizeof(lk_dir));
   path_mkdir(lk_dir);
   lk_file("Tetris.png", true);
   lk_file("Tetris (USA).png", true);
   lk_file("Duck Hunt (USA).jpg", true);

   memset(&cfg, 0, sizeof(cfg));
   strlcpy(cfg.dir_thumbnails, LK_ROOT, sizeof(cfg.dir_thumbnails));
   cfg.gfx_thumbnails = 3; /* Named_Boxarts */

   lk_entry(pd, "Tetris (USA) (Tengen) (Unl)", "/roms/Tetris (Tengen).nes");
   bad |= lk_case(pd, &cfg, "the Tengen entry gets no untagged box", NULL);

   {
      size_t rec;
      gfx_thumbnail_results_t *t = gfx_thumbnail_results_new();
      rec = gfx_thumbnail_results_add(t, "/roms/t.nes", "Tetris (USA)", "",
            LK_SYS);
      gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS,
            "Tetris.png");
      rec = gfx_thumbnail_results_add(t, "/roms/Tetris (USA).nes",
            "Tetris [!]", "", LK_SYS);
      gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS,
            "Tetris.png");
      rec = gfx_thumbnail_results_add(t, "/roms/tet-u.nes", "Tetris (U) [!]",
            "", LK_SYS);
      gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS,
            "Tetris (USA).png");
      rec = gfx_thumbnail_results_add(t, "/roms/gone.nes", "Gone (U)", "",
            LK_SYS);
      gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS,
            "Gone (USA).png");
      rec = gfx_thumbnail_results_add(t, "/roms/dh.nes", "Duck Hunt (U)", "",
            LK_SYS);
      gfx_thumbnail_results_set(t, rec, GFX_THUMBNAIL_RESULT_BOXARTS,
            "Duck Hunt (USA).jpg");
      gfx_thumbnail_results_sort(t);
      gfx_thumbnail_results_open(LK_PL);
      gfx_thumbnail_results_install(LK_PL, t, NULL, true);
   }
   cfg.use_results = true;

   lk_entry(pd, "Tetris (USA)", "/roms/t.nes");
   bad |= lk_case(pd, &cfg, "the label comes before a stored match",
         "Tetris (USA).png");
   lk_entry(pd, "Tetris [!]", "/roms/Tetris (USA).nes");
   bad |= lk_case(pd, &cfg, "then the file name", "Tetris (USA).png");
   lk_entry(pd, "Tetris (U) [!]", "/roms/tet-u.nes");
   bad |= lk_case(pd, &cfg, "then the stored match", "Tetris (USA).png");
   lk_entry(pd, "Gone (U)", "/roms/gone.nes");
   bad |= lk_case(pd, &cfg, "a stored match whose file is gone shows nothing",
         NULL);
   lk_entry(pd, "Duck Hunt (U)", "/roms/dh.nes");
   bad |= lk_case(pd, &cfg, "a stored .jpg needs non-PNG thumbnails", NULL);
   cfg.playlist_allow_non_png = true;
   bad |= lk_case(pd, &cfg, "a stored .jpg with non-PNG thumbnails on",
         "Duck Hunt (USA).jpg");
   cfg.playlist_allow_non_png = false;
   cfg.use_results = false;
   lk_entry(pd, "Tetris (U) [!]", "/roms/tet-u.nes");
   bad |= lk_case(pd, &cfg, "a worker's config never reads the store", NULL);

   gfx_thumbnail_results_deinit();
   lk_file("Tetris.png", false);
   lk_file("Tetris (USA).png", false);
   lk_file("Duck Hunt (USA).jpg", false);
   free(pd);
   return bad;
}
