#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

#include "../file_path_special.h"
#include "../configuration.h"
#include "../verbosity.h"
#include "../network/net_http_special.h"

#include "badges.h"

badges_ctx_t badges_ctx;

bool badge_exists(const char* filepath)
{
  return filestream_exists(filepath);
}

void set_badge_menu_texture(badges_ctx_t * badges, int i)
{
   char badge_file[16];
   char fullpath[PATH_MAX_LENGTH];
   const char * locked_suffix = (badges->badge_locked[i] == true) 
      ? "_lock.png" : ".png";

   snprintf(badge_file, sizeof(badge_file), "%s", badges->badge_id_list[i]);
   strcat(badge_file, locked_suffix);

   fill_pathname_application_special(fullpath,
         PATH_MAX_LENGTH * sizeof(char),
         APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_CHEEVOS_BADGES);

#ifdef HAVE_MENU
   menu_display_reset_textures_list(badge_file, fullpath,
         &badges->menu_texture_list[i],TEXTURE_FILTER_MIPMAP_LINEAR);
#endif
}

void set_badge_info (badges_ctx_t *badge_struct, int id,
      const char *badge_id, bool active)
{
   if (!badge_struct)
      return;

   badge_struct->badge_id_list[id] = badge_id;
   badge_struct->badge_locked[id]  = active;
   set_badge_menu_texture(badge_struct, id);
}

menu_texture_item get_badge_texture(int id)
{
   settings_t *settings = config_get_ptr();
   if (!settings->bools.cheevos_badges_enable)
      return (menu_texture_item)NULL;

   return badges_ctx.menu_texture_list[id];
}
