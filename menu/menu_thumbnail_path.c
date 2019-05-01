/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (runtime_file.c).
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <string/stdstring.h>
#include <file/file_path.h>
#include <lists/file_list.h>

#include "../configuration.h"
#include "../msg_hash.h"
#include "../paths.h"
#include "../file_path_special.h"

#include "menu_driver.h"

#include "menu_thumbnail_path.h"

/* Used fixed size char arrays here, just to avoid
 * the inconvenience of having to calloc()/free()
 * each individual entry by hand... */
struct menu_thumbnail_path_data
{
   char system[PATH_MAX_LENGTH];
   char content_path[PATH_MAX_LENGTH];
   char content_label[PATH_MAX_LENGTH];
   char content_core_name[PATH_MAX_LENGTH];
   char content_db_name[PATH_MAX_LENGTH];
   char content_img[PATH_MAX_LENGTH];
   char right_path[PATH_MAX_LENGTH];
   char left_path[PATH_MAX_LENGTH];
};

/* Initialisation */

/* Creates new thumbnail path data container.
 * Returns handle to new menu_thumbnail_path_data_t object.
 * on success, otherwise NULL.
 * Note: Returned object must be free()d */
menu_thumbnail_path_data_t *menu_thumbnail_path_init()
{
   menu_thumbnail_path_data_t *path_data = NULL;
   path_data = (menu_thumbnail_path_data_t*)calloc(1, sizeof(*path_data));
   if (!path_data)
      return NULL;
   return path_data;
}

/* Resets thumbnail path data
 * (blanks all internal string containers) */
void menu_thumbnail_path_reset(menu_thumbnail_path_data_t *path_data)
{
   if (!path_data)
      return;
   
   path_data->system[0] = '\0';
   path_data->content_path[0] = '\0';
   path_data->content_label[0] = '\0';
   path_data->content_core_name[0] = '\0';
   path_data->content_db_name[0] = '\0';
   path_data->content_img[0] = '\0';
   path_data->right_path[0] = '\0';
   path_data->left_path[0] = '\0';
}

/* Utility Functions */

/* Returns currently set thumbnail 'type' (Named_Snaps,
 * Named_Titles, Named_Boxarts) for specified thumbnail
 * identifier (right, left) */
const char *menu_thumbnail_get_type(enum menu_thumbnail_id thumbnail_id)
{
   settings_t *settings = config_get_ptr();
   unsigned type = 0;
   
   if (!settings)
      return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF);
   
   switch (thumbnail_id)
   {
      case MENU_THUMBNAIL_RIGHT:
         type = settings->uints.menu_thumbnails;
         break;
      case MENU_THUMBNAIL_LEFT:
         type = settings->uints.menu_left_thumbnails;
         break;
      default:
         return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF);
   }
   
   switch (type)
   {
      case 1:
         return "Named_Snaps";
      case 2:
         return "Named_Titles";
      case 3:
         return "Named_Boxarts";
      case 0:
      default:
         break;
   }
   
   return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF);
}

/* Returns true if specified thumbnail is enabled
 * (i.e. if 'type' is not equal to MENU_ENUM_LABEL_VALUE_OFF) */
bool menu_thumbnail_is_enabled(enum menu_thumbnail_id thumbnail_id)
{
   settings_t *settings = config_get_ptr();
   
   if (!settings)
      return false;
   
   switch (thumbnail_id)
   {
      case MENU_THUMBNAIL_RIGHT:
         return settings->uints.menu_thumbnails != 0;
      case MENU_THUMBNAIL_LEFT:
         return settings->uints.menu_left_thumbnails != 0;
         break;
      default:
         break;
   }
   
   return false;
}

/* Setters */

/* Fills content_img field of path_data using existing
 * content_label field (for internal use only) */
static void fill_content_img(menu_thumbnail_path_data_t *path_data)
{
   char *scrub_char_pointer = NULL;
   
   /* Copy source label string */
   strlcpy(path_data->content_img,
         path_data->content_label, sizeof(path_data->content_img));
   
   /* Scrub characters that are not cross-platform and/or violate the
    * No-Intro filename standard:
    * http://datomatic.no-intro.org/stuff/The%20Official%20No-Intro%20Convention%20(20071030).zip
    * Replace these characters in the entry name with underscores */
   while((scrub_char_pointer = strpbrk(path_data->content_img, "&*/:`\"<>?\\|")))
      *scrub_char_pointer = '_';
   
   /* Add PNG extension */
   strlcat(path_data->content_img,
         file_path_str(FILE_PATH_PNG_EXTENSION), sizeof(path_data->content_img));
}

/* Sets current 'system' (default database name).
 * Returns true if 'system' is valid.
 * > Used as a fallback when individual content lacks an
 *   associated database name */
bool menu_thumbnail_set_system(menu_thumbnail_path_data_t *path_data, const char *system)
{
   if (!path_data)
      return false;
   
   /* When system is updated, must regenerate right/left
    * thumbnail paths */
   path_data->right_path[0] = '\0';
   path_data->left_path[0] = '\0';
   
   /* 'Reset' path_data system string */
   path_data->system[0] = '\0';
   
   if (string_is_empty(system))
      return false;
   
   /* Hack: There is only one MAME thumbnail repo,
    * so filter any input starting with 'MAME...' */
   if (strncmp(system, "MAME", 4) == 0)
      strlcpy(path_data->system, "MAME", sizeof(path_data->system));
   else
      strlcpy(path_data->system, system, sizeof(path_data->system));
   
   return true;
}

/* Sets current thumbnail content according to the specified label.
 * Returns true if content is valid */
bool menu_thumbnail_set_content(menu_thumbnail_path_data_t *path_data, const char *label)
{
   if (!path_data)
      return false;
   
   /* When content is updated, must regenerate right/left
    * thumbnail paths */
   path_data->right_path[0] = '\0';
   path_data->left_path[0] = '\0';
   
   /* 'Reset' path_data content strings */
   path_data->content_path[0] = '\0';
   path_data->content_label[0] = '\0';
   path_data->content_core_name[0] = '\0';
   path_data->content_db_name[0] = '\0';
   path_data->content_img[0] = '\0';
   
   if (string_is_empty(label))
      return false;
   
   /* Cache content label */
   strlcpy(path_data->content_label, label, sizeof(path_data->content_label));
   
   /* Determine content image name */
   fill_content_img(path_data);
   
   /* Redundant error check... */
   if (string_is_empty(path_data->content_img))
      return false;
   
   return true;
}

/* Sets current thumbnail content to the specified image.
 * Returns true if content is valid */
bool menu_thumbnail_set_content_image(menu_thumbnail_path_data_t *path_data, const char *img_dir, const char *img_name)
{
   char *content_img_no_ext = NULL;
   
   if (!path_data)
      return false;
   
   /* When content is updated, must regenerate right/left
    * thumbnail paths */
   path_data->right_path[0] = '\0';
   path_data->left_path[0] = '\0';
   
   /* 'Reset' path_data content strings */
   path_data->content_path[0] = '\0';
   path_data->content_label[0] = '\0';
   path_data->content_core_name[0] = '\0';
   path_data->content_db_name[0] = '\0';
   path_data->content_img[0] = '\0';
   
   if (string_is_empty(img_dir))
      return false;
   
   if (string_is_empty(img_name))
      return false;
   
   if (path_is_media_type(img_name) != RARCH_CONTENT_IMAGE)
      return false;
   
   /* Cache content image name */
   strlcpy(path_data->content_img,
            img_name, sizeof(path_data->content_img));
   
   /* Get image label */
   content_img_no_ext = path_remove_extension(path_data->content_img);
   if (!string_is_empty(content_img_no_ext))
      strlcpy(path_data->content_label,
            content_img_no_ext, sizeof(path_data->content_label));
   else
      strlcpy(path_data->content_label,
            path_data->content_img, sizeof(path_data->content_label));
   
   /* Set file path */
   fill_pathname_join(path_data->content_path,
      img_dir, img_name, sizeof(path_data->content_path));
   
   /* Set core name to "imageviewer" */
   strlcpy(path_data->content_core_name,
            "imageviewer", sizeof(path_data->content_core_name));
   
   /* Set database name (arbitrarily) to "_images_"
    * (required for compatibility with menu_thumbnail_update_path(),
    * but not actually used...) */
   strlcpy(path_data->content_db_name,
            "_images_", sizeof(path_data->content_db_name));
   
   /* Redundant error check */
   if (string_is_empty(path_data->content_path))
      return false;
   
   return true;
}

/* Sets current thumbnail content to the specified playlist entry.
 * Returns true if content is valid.
 * > Note: It is always best to use playlists when setting
 *   thumbnail content, since there is no guarantee that the
 *   corresponding menu entry label will contain a useful
 *   identifier (it may be 'tainted', e.g. with the current
 *   core name). 'Real' labels should be extracted from source */
bool menu_thumbnail_set_content_playlist(menu_thumbnail_path_data_t *path_data, playlist_t *playlist, size_t idx)
{
   const char *content_path  = NULL;
   const char *content_label = NULL;
   const char *core_name     = NULL;
   const char *db_name       = NULL;
   const struct playlist_entry *entry = NULL;

   if (!path_data)
      return false;
   
   /* When content is updated, must regenerate right/left
    * thumbnail paths */
   path_data->right_path[0] = '\0';
   path_data->left_path[0] = '\0';
   
   /* 'Reset' path_data content strings */
   path_data->content_path[0] = '\0';
   path_data->content_label[0] = '\0';
   path_data->content_core_name[0] = '\0';
   path_data->content_db_name[0] = '\0';
   path_data->content_img[0] = '\0';
   
   if (!playlist)
      return false;
   
   if (idx >= playlist_get_size(playlist))
      return false;
   
   /* Read playlist values */
   playlist_get_index(playlist, idx, &entry);

   content_path = entry->path;
   content_label = entry->label;
   core_name = entry->core_name;
   db_name = entry->db_name;

   /* Content without a path is invalid by definition */
   if (string_is_empty(content_path))
      return false;
   
   /* Cache content path
    * (This is required for imageviewer content) */
   strlcpy(path_data->content_path,
            content_path, sizeof(path_data->content_path));
   
   /* Cache core name
    * (This is required for imageviewer content) */
   if (!string_is_empty(core_name))
      strlcpy(path_data->content_core_name,
            core_name, sizeof(path_data->content_core_name));
   
   /* Get content label */
   if (!string_is_empty(content_label))
      strlcpy(path_data->content_label,
            content_label, sizeof(path_data->content_label));
   else
      fill_short_pathname_representation(path_data->content_label,
            content_path, sizeof(path_data->content_label));
   
   /* Determine content image name */
   fill_content_img(path_data);
   
   /* Redundant error check... */
   if (string_is_empty(path_data->content_img))
      return false;
   
   /* Thumbnail image name is done -> now check if
    * per-content database name is defined */
   if (!string_is_empty(db_name))
   {
      /* Hack: There is only one MAME thumbnail repo,
       * so filter any input starting with 'MAME...' */
      if (strncmp(db_name, "MAME", 4) == 0)
         strlcpy(path_data->content_db_name,
               "MAME", sizeof(path_data->content_db_name));
      else
      {
         char *db_name_no_ext = NULL;
         char tmp_buf[PATH_MAX_LENGTH];
         tmp_buf[0] = '\0';
         
         /* Remove .lpl extension
          * > path_remove_extension() requires a char * (not const)
          *   so have to use a temporary buffer... */
         strlcpy(tmp_buf, db_name, sizeof(tmp_buf));
         db_name_no_ext = path_remove_extension(tmp_buf);
         
         if (!string_is_empty(db_name_no_ext))
            strlcpy(path_data->content_db_name,
                  db_name_no_ext, sizeof(path_data->content_db_name));
         else
            strlcpy(path_data->content_db_name,
                  tmp_buf, sizeof(path_data->content_db_name));
      }
   }
   
   return true;
}

/* Updaters */

/* Updates path for specified thumbnail identifier (right, left).
 * Must be called after:
 * - menu_thumbnail_set_system()
 * - menu_thumbnail_set_content*()
 * ...and before:
 * - menu_thumbnail_get_path()
 * Returns true if generated path is valid */
bool menu_thumbnail_update_path(menu_thumbnail_path_data_t *path_data, enum menu_thumbnail_id thumbnail_id)
{
   settings_t *settings    = config_get_ptr();
   const char *type        = menu_thumbnail_get_type(thumbnail_id);
   const char *system_name = NULL;
   char *thumbnail_path    = NULL;
   
   if (!path_data)
      return false;
   
   /* Determine which path we are updating... */
   switch (thumbnail_id)
   {
      case MENU_THUMBNAIL_RIGHT:
         thumbnail_path = path_data->right_path;
         break;
      case MENU_THUMBNAIL_LEFT:
         thumbnail_path = path_data->left_path;
         break;
      default:
         return false;
   }
   
   thumbnail_path[0] = '\0';
   
   /* Sundry error checking */
   if (!settings)
      return false;
   
   if (string_is_empty(settings->paths.directory_thumbnails))
      return false;
   
   if (!menu_thumbnail_is_enabled(thumbnail_id))
      return false;
   
   /* Generate new path */
   
   /* > Check path_data for empty strings */
   if (string_is_empty(path_data->content_img) ||
         (string_is_empty(path_data->system) &&
          string_is_empty(path_data->content_db_name)))
      return false;
   
   /* > Get current system */
   system_name = string_is_empty(path_data->content_db_name) ?
      path_data->system : path_data->content_db_name;
   
   /* > Special case: thumbnail for imageviewer content
    *   is the image file itself */
   if (string_is_equal(system_name, "images_history") ||
       string_is_equal(path_data->content_core_name, "imageviewer"))
   {
      if (string_is_empty(path_data->content_path))
         return false;
      
      /* imageviewer content is identical for left and right thumbnails */
      if (path_is_media_type(path_data->content_path) == RARCH_CONTENT_IMAGE)
         strlcpy(thumbnail_path,
            path_data->content_path, PATH_MAX_LENGTH * sizeof(char));
   }
   else
   {
      char tmp_buf[PATH_MAX_LENGTH];
      tmp_buf[0] = '\0';
      
      /* > Normal content: assemble path */
      
      /* >> Base + system name */
      fill_pathname_join(thumbnail_path, settings->paths.directory_thumbnails,
            system_name, PATH_MAX_LENGTH * sizeof(char));
      
      /* >> Add type */
      fill_pathname_join(tmp_buf, thumbnail_path, type, sizeof(tmp_buf));
      
      /* >> Add content image */
      thumbnail_path[0] = '\0';
      fill_pathname_join(thumbnail_path, tmp_buf,
            path_data->content_img, PATH_MAX_LENGTH * sizeof(char));
   }
   
   /* Final error check - is cached path empty? */
   if (string_is_empty(thumbnail_path))
      return false;
   
   return true;
}

/* Getters */

/* Fetches the current thumbnail file path of the
 * specified thumbnail 'type'.
 * Returns true if path is valid. */
bool menu_thumbnail_get_path(menu_thumbnail_path_data_t *path_data, enum menu_thumbnail_id thumbnail_id, const char **path)
{
   char *thumbnail_path = NULL;
   
   if (!path_data)
      return false;
   
   if (!path)
      return false;
   
   switch (thumbnail_id)
   {
      case MENU_THUMBNAIL_RIGHT:
         thumbnail_path = path_data->right_path;
         break;
      case MENU_THUMBNAIL_LEFT:
         thumbnail_path = path_data->left_path;
         break;
      default:
         return false;
   }
   
   if (string_is_empty(thumbnail_path))
      return false;
   
   *path = thumbnail_path;
   
   return true;
}

/* Fetches current thumbnail label.
 * Returns true if label is valid. */
bool menu_thumbnail_get_label(menu_thumbnail_path_data_t *path_data, const char **label)
{
   if (!path_data)
      return false;
   
   if (!label)
      return false;
   
   if (string_is_empty(path_data->content_label))
      return false;
   
   *label = path_data->content_label;
   
   return true;
}

/* Fetches current thumbnail core name.
 * Returns true if core name is valid. */
bool menu_thumbnail_get_core_name(menu_thumbnail_path_data_t *path_data, const char **core_name)
{
   if (!path_data)
      return false;
   
   if (!core_name)
      return false;
   
   if (string_is_empty(path_data->content_core_name))
      return false;
   
   *core_name = path_data->content_core_name;
   
   return true;
}
