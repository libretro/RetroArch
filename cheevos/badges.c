#include "badges.h"
#include "../menu/menu_driver.h"
#include "../verbosity.h"

badges_ctx_t badges_ctx;

void set_badge_menu_texture(badges_ctx_t * badges, int i)
{
  const char * locked_suffix = (badges->badge_locked[i] == true) ? "_lock.png" : ".png";

  unsigned int bufferSize = 16;
  char badge_file[bufferSize];

  snprintf(badge_file, bufferSize, "%s", badges->badge_id_list[i]);
  strcat(badge_file, locked_suffix);

  // Badge directory should probably use a definition
  menu_display_reset_textures_list(badge_file, "badges", &badges->menu_texture_list[i],TEXTURE_FILTER_MIPMAP_LINEAR);
}

void set_badge_info (badges_ctx_t *badge_struct, int id, const char *badge_id, bool active)
{
  badge_struct->badge_id_list[id] = badge_id;
  badge_struct->badge_locked[id] = active;
  set_badge_menu_texture(badge_struct, id);
}

menu_texture_item get_badge_texture (int id)
{
  return badges_ctx.menu_texture_list[id];
}
