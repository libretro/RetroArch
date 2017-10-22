#include <file/file_path.h>
#include <streams/file_stream.h>

#include "../verbosity.h"
#include "../network/net_http_special.h"

#include "badges.h"

badges_ctx_t badges_ctx;

bool download_badge(const char* filename)
{
  char fullpath[PATH_MAX_LENGTH];

  strcpy(fullpath, "badges/");
  strcat(fullpath, filename);

  if(path_file_exists(fullpath))
  {
    return true;
  }
  else
  {
    size_t mysize = 1024 * 100;
    size_t *size;
    size = &mysize;

    const char **buffer = malloc(sizeof(*buffer) * mysize);
    char url[PATH_MAX_LENGTH];
    strcpy(url, "http://i.retroachievements.org/Badge/");
    strcat(url, filename);

    retro_time_t *timeout;
    retro_time_t timesecs = 10000000; //10 seconds
    timeout = &timesecs;

    if(net_http_get(buffer, size, url, timeout) != NET_HTTP_GET_OK)
    {
      printf("[CHEEVOS]: Download to %s failed.\n", fullpath);
      return false;
    }

    if (!filestream_write_file(fullpath, *buffer, *size))
    {
      printf("[CHEEVOS]: Write to %s failed.\n", fullpath);
      return false;
    }
    else
    {
      printf("[CHEEVOS]: %s downloaded.\n", fullpath);
      return true;
    }
  }
}

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
