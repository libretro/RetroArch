#ifndef SHADER_MEDIA_TEST_H
#define SHADER_MEDIA_TEST_H

#include "../../../menu/menu_shader.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)

static bool media_shader_colour(unsigned red, unsigned green, unsigned blue)
{
   video_driver_state_t *video = video_state_get_ptr();
   struct video_viewport vp;
   uint8_t *pixels;
   unsigned bpp, width, height;
   size_t at;
   bool ok, bottom_up = true;
   memset(&vp, 0, sizeof(vp));
   video_driver_get_viewport_info(&vp);
   width = VIDEO_SCALE_W(vp.dims);
   height = VIDEO_SCALE_H(vp.dims);
   if (!width || !height) return false;
   bpp = video->current_video->read_viewport_bgrx ? 4 : 3;
   pixels = (uint8_t*)malloc((size_t)width * height * bpp);
   if (!pixels) return false;
   ok = bpp == 4
      ? video->current_video->read_viewport_bgrx(video->data, pixels, false, &bottom_up)
      : video->current_video->read_viewport
         && video->current_video->read_viewport(video->data, pixels, false);
   at = ((size_t)(height / 2) * width + width / 2) * bpp;
   if (ok)
      ok = abs((int)pixels[at + 2] - (int)red) < 24
         && abs((int)pixels[at + 1] - (int)green) < 24
         && abs((int)pixels[at] - (int)blue) < 24;
   free(pixels);
   return ok;
}

static bool media_shader_wait(struct menu_state *menu_st,
      unsigned red, unsigned green, unsigned blue)
{
   unsigned tick;
   for (tick = 0; tick < 300; tick++)
   {
      task_queue_check();
      menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
            config_get_ptr(), MENU_ACTION_NOOP, cpu_features_get_time_usec());
      render(menu_st);
      if (media_shader_colour(red, green, blue)) return true;
      retro_sleep(1);
   }
   return false;
}

static void media_shader_check(struct menu_state *menu_st, test_viewer_t *viewer)
{
   const char *reload = getenv("MEDIA_TEST_SHADER_RELOAD");
   gfx_thumbnail_t *thumbnail = test_thumbnail(viewer);
   uintptr_t texture = thumbnail->texture;
   menu_entry_t entry;
   CHECK("CLI shader applies to a coreless red image", media_shader_wait(menu_st, 0, 255, 255));
   menu_st->flags &= ~MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   CHECK("external preset application succeeds", reload
         && menu_shader_manager_set_preset(menu_shader_get(),
            video_shader_parse_type(reload), reload, true));
   CHECK("external shader does not refresh unrelated browser entries",
         !(menu_st->flags & MENU_ST_FLAG_ENTRIES_NEED_REFRESH));
   MENU_ENTRY_INITIALIZE(entry);
   menu_entry_get(&entry, 0, menu_st->selection_ptr, NULL, true);
   generic_menu_entry_action(menu_st->userdata, &entry,
         menu_st->selection_ptr, MENU_ACTION_NOOP);
   CHECK("external shader samples the image instead of the core frame",
         media_shader_wait(menu_st, 0, 0, 255));
   CHECK("preset reload preserves the visible image texture without a copy",
         test_showing(viewer) && thumbnail->texture == texture
         && thumbnail->status == GFX_THUMBNAIL_STATUS_AVAILABLE);
   CHECK("disabling the preset succeeds", menu_shader_manager_set_preset(
            menu_shader_get(), video_shader_parse_type(reload), NULL, true));
   CHECK("disabling the preset preserves browser entries",
         !(menu_st->flags & MENU_ST_FLAG_ENTRIES_NEED_REFRESH));
   CHECK("disabling the preset restores the original red image",
         media_shader_wait(menu_st, 255, 0, 0));
   CHECK("disabling the preset keeps the existing image texture",
         test_showing(viewer) && thumbnail->texture == texture);
   {
      file_list_t *stack = MENU_LIST_GET(menu_st->entries.list, 0);
      char *label = stack->list[stack->size - 1].label;
      /* Exercise refresh bookkeeping without leaving the image browser. */
      stack->list[stack->size - 1].label = (char*)MENU_ENUM_LABEL_SHADER_OPTIONS_STR;
      CHECK("shader options accept a preset", menu_shader_manager_set_preset(
               menu_shader_get(), video_shader_parse_type(reload), reload, true));
      CHECK("shader options still request an entry refresh",
            menu_st->flags & MENU_ST_FLAG_ENTRIES_NEED_REFRESH);
      stack->list[stack->size - 1].label = label;
      menu_st->flags &= ~MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   }
   CHECK("menu toggle dismisses the fullscreen viewer", command_event(CMD_EVENT_MENU_TOGGLE, NULL));
   CHECK("menu toggle reveals the browser without closing the menu",
         !test_showing(viewer) && (menu_st->flags & MENU_ST_FLAG_ALIVE));
   CHECK("ordinary menu keeps normal toggle behavior", !menu_driver_dismiss_fullscreen());
}
#else
static void media_shader_check(struct menu_state *menu_st, test_viewer_t *viewer)
{
   (void)menu_st;
   (void)viewer;
   CHECK("shader backend is available", false);
}
#endif
#endif
