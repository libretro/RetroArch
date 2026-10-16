/* Ozone list scroll with sublabels shown for the selected entry only.
 *
 * Links the shipping RetroArch objects with only main() replaced and
 * includes ozone.c itself, so the lanes can read the handle. The
 * animation clock only moves when the harness moves it, so every
 * lane sees exactly what one runloop iteration leaves behind.
 *
 *   jump       a selection change that asks for no animation moves
 *              the list at once and shows the cursor fully
 *   animated   a selection change that asks for animation leaves the
 *              list where it was and fades the cursor in, and the
 *              tween ends on the selected entry
 *   later      a compute in a later iteration, with that fade still
 *              under way, does not start another one
 *   sidebar    with the cursor in the sidebar, a new selection in the
 *              list behind it re-centres that list
 *   past end   a selection past the end of the list, behind the
 *              sidebar, is not taken for a new one by a later
 *              compute (the render pass keeps the unclamped value)
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/ozone_sublabel_scroll/build.sh
 */

#include "../../../menu/drivers/ozone.c"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <boolean.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../menu/menu_driver.h"
#include "../../../gfx/gfx_animation.h"
#include "../../../frontend/frontend_driver.h"

#ifdef _WIN32
#include <process.h>
#define harness_pid() ((long)_getpid())
#else
#include <unistd.h>
#define harness_pid() ((long)getpid())
#endif

static unsigned failures       = 0;
static retro_time_t anim_clock = 1000000;
static unsigned harness_dims   = 0;

#define CHECK(cond, msg, a, b) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s (%f, %f)\n", \
               __FILE__, __LINE__, msg, (double)(a), (double)(b)); \
         failures++; \
      } \
   } while (0)

#define SCROLL_NEAR(a, b) (fabs((double)(a) - (double)(b)) < 0.5)

static ozone_handle_t *handle(void)
{
   return (ozone_handle_t*)menu_state_get_ptr()->userdata;
}

static file_list_t *entries(void)
{
   return MENU_LIST_GET_SELECTION(menu_state_get_ptr()->entries.list, 0);
}

/* One runloop iteration's worth of menu: input already applied, then
 * the render slot. */
static void render(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_st->driver_ctx->render(menu_st->userdata, harness_dims, false);
}

static void advance_animations(void)
{
   unsigned i;
   for (i = 0; i < 20; i++)
   {
      anim_clock += 50000;
      gfx_animation_update(anim_clock, false, 1.0f, harness_dims);
   }
}

/* Where the list stands once the selected entry is centred, from the
 * geometry the compute pass left on the nodes. */
static float centred_scroll(ozone_handle_t *ozone, size_t idx)
{
   ozone_node_t *node = (ozone_node_t*)entries()->list[idx].userdata;
   float height       = (float)VIDEO_SCALE_H(ozone->last_dims);
   float bottom       = height
         - ozone->dimensions.header_height
         - ozone->dimensions.spacer_1px
         - ozone->dimensions.footer_height;
   float middle       = ozone->dimensions.header_height
         + ozone->dimensions.entry_padding_vertical
         + node->position_y
         + OZONE_NODE_HEIGHT(node) / 2;
   float scroll       = -(middle - height / 2);

   if (scroll + ozone->entries_height < bottom)
      scroll = bottom - ozone->entries_height
            - ozone->dimensions.entry_padding_vertical * 2;
   if (scroll > 0)
      scroll = 0;
   return scroll;
}

/* Back to the first entry with nothing in flight. */
static void settle_at_top(void)
{
   ozone_handle_t *ozone = handle();
   menu_state_get_ptr()->selection_ptr = 0;
   ozone_selection_changed(ozone, false);
   render();
   advance_animations();
   render();
}

int main(int argc, char *argv[])
{
   char fixture_dir[512];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc = 0;
   size_t far_idx, size;
   float before;
   ozone_handle_t *ozone;
   struct menu_state *menu_st;

   (void)argc;
   (void)argv;

   snprintf(fixture_dir, sizeof(fixture_dir),
         "ozone_sublabel_scroll_%ld", harness_pid());
   path_mkdir(fixture_dir);

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   {
      FILE *cfg;
      snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", fixture_dir);
      if ((cfg = fopen(cfg_path, "wb")))
      {
         fprintf(cfg, "video_driver = \"null\"\n");
         fprintf(cfg, "audio_driver = \"null\"\n");
         fprintf(cfg, "input_driver = \"null\"\n");
         fprintf(cfg, "input_joypad_driver = \"null\"\n");
         fprintf(cfg, "menu_driver = \"ozone\"\n");
         fprintf(cfg, "video_threaded = \"false\"\n");
         fprintf(cfg, "menu_show_sublabels = \"true\"\n");
         fprintf(cfg, "menu_show_sublabels_current_selection_only = \"true\"\n");
         fprintf(cfg, "ozone_collapse_sidebar = \"false\"\n");
         fclose(cfg);
      }
   }

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   retroarch_menu_running();

   menu_st      = menu_state_get_ptr();
   /* Short enough that the main menu has to scroll. */
   harness_dims = VIDEO_SCALE_PACK(640, 240);

   if (     !menu_st->driver_ctx || !menu_st->driver_ctx->ident
         || !string_is_equal(menu_st->driver_ctx->ident, "ozone")
         || !menu_st->userdata)
   {
      fprintf(stderr, "FAIL: fixture: ozone is not the running menu\n");
      return 1;
   }
   ozone = handle();
   render();
   advance_animations();
   render();

   /* The Settings tab: long enough to scroll at this height. */
   {
      size_t tab;
      for (tab = 0; tab <= ozone->system_tab_end; tab++)
         if (ozone->tabs[tab] == OZONE_SYSTEM_TAB_SETTINGS)
            break;
      ozone_go_to_sidebar(ozone, false, (uintptr_t)ozone);
      render();
      ozone_sidebar_goto(ozone, tab);
      render();
      ozone_leave_sidebar(ozone, false, (uintptr_t)ozone,
            MENU_REMEMBER_SELECTION_OFF);
      render();
      advance_animations();
      render();
   }

   size    = entries()->size;
   far_idx = size - 2;
   printf("fixture: %u entries, list %u px, view %u px, depth %d\n",
         (unsigned)size, ozone->entries_height,
         VIDEO_SCALE_H(ozone->last_dims), ozone->depth);
   CHECK(size >= 6, "fixture: main menu too short", size, 6);
   CHECK(!(ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR),
         "fixture: cursor starts in the sidebar", 0, 0);
   CHECK(centred_scroll(ozone, far_idx) < -1.0f,
         "fixture: list does not need to scroll", centred_scroll(ozone, far_idx), 0);

   /* jump */
   settle_at_top();
   menu_st->selection_ptr = far_idx;
   ozone_selection_changed(ozone, false);
   render();
   CHECK(SCROLL_NEAR(ozone->animations.scroll_y, centred_scroll(ozone, far_idx)),
         "jump: list not moved to the selection",
         ozone->animations.scroll_y, centred_scroll(ozone, far_idx));
   CHECK(ozone->animations.cursor_alpha == 1.0f,
         "jump: cursor not fully shown", ozone->animations.cursor_alpha, 1);

   /* animated */
   settle_at_top();
   before                 = ozone->animations.scroll_y;
   menu_st->selection_ptr = far_idx;
   menu_st->driver_ctx->navigation_set(menu_st->userdata, true);
   render();
   CHECK(SCROLL_NEAR(ozone->animations.scroll_y, before),
         "animated: list jumped instead of tweening",
         ozone->animations.scroll_y, before);
   CHECK(ozone->animations.cursor_alpha == 0.0f,
         "animated: cursor fade not running", ozone->animations.cursor_alpha, 0);

   /* later: same fade, clock not moved, another compute */
   ozone->flags |= OZONE_FLAG_NEED_COMPUTE;
   render();
   CHECK(SCROLL_NEAR(ozone->animations.scroll_y, centred_scroll(ozone, far_idx)),
         "later: a compute after the iteration that started the fade tweened again",
         ozone->animations.scroll_y, centred_scroll(ozone, far_idx));
   CHECK(ozone->animations.cursor_alpha == 1.0f,
         "later: cursor not fully shown", ozone->animations.cursor_alpha, 1);

   /* animated, run to the end */
   settle_at_top();
   menu_st->selection_ptr = far_idx;
   menu_st->driver_ctx->navigation_set(menu_st->userdata, true);
   render();
   advance_animations();
   render();
   CHECK(SCROLL_NEAR(ozone->animations.scroll_y, centred_scroll(ozone, far_idx)),
         "animated: tween did not end on the selection",
         ozone->animations.scroll_y, centred_scroll(ozone, far_idx));
   CHECK(ozone->animations.cursor_alpha == 1.0f,
         "animated: cursor fade did not finish", ozone->animations.cursor_alpha, 1);

   /* sidebar */
   settle_at_top();
   ozone_go_to_sidebar(ozone, false, (uintptr_t)ozone);
   render();
   advance_animations();
   render();
   CHECK((ozone->flags & OZONE_FLAG_CURSOR_IN_SIDEBAR) != 0,
         "fixture: cursor did not reach the sidebar", 0, 0);
   before                 = ozone->animations.scroll_y;
   menu_st->selection_ptr = far_idx;
   render();
   CHECK(SCROLL_NEAR(ozone->animations.scroll_y, centred_scroll(ozone, far_idx)),
         "sidebar: list behind the sidebar not re-centred",
         ozone->animations.scroll_y, centred_scroll(ozone, far_idx));
   CHECK(!SCROLL_NEAR(before, centred_scroll(ozone, far_idx)),
         "fixture: sidebar lane does not move the list", before, 0);

   /* past end: still in the sidebar */
   menu_st->selection_ptr = size + 3;
   render();
   CHECK(SCROLL_NEAR(ozone->animations.scroll_y, centred_scroll(ozone, size - 1)),
         "past end: list not centred on the last entry",
         ozone->animations.scroll_y, centred_scroll(ozone, size - 1));
   ozone->animations.scroll_y = 0.0f;
   ozone->flags              |= OZONE_FLAG_NEED_COMPUTE;
   render();
   CHECK(ozone->animations.scroll_y == 0.0f,
         "past end: an unchanged selection re-centred the list",
         ozone->animations.scroll_y, 0);
   menu_st->selection_ptr = 0;

   {
      static const char *const written[] = {
         "harness.cfg", "retroarch-keychain.cfg", "retroarch-keychain.key"
      };
      char path[640];
      unsigned i;
      for (i = 0; i < ARRAY_SIZE(written); i++)
      {
         fill_pathname_join_special(path, fixture_dir, written[i],
               sizeof(path));
         filestream_delete(path);
      }
      path_rmdir(fixture_dir);
   }

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("ozone_sublabel_scroll: all lanes passed\n");
   return 0;
}
