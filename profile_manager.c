#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "profile_manager.h"
#include <file/config_file.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include "file_path_special.h"
#include "paths.h"
#include "verbosity.h"

/* Directory containing retroarch.cfg (and profiles.cfg) */
static char g_config_dir[PATH_MAX_LENGTH]         = {0};
/* Full path to the MAIN retroarch.cfg */
static char g_main_config_path[PATH_MAX_LENGTH]   = {0};

static rarch_profile_list_t g_profile_list = {0};

static void sanitize_filename(const char *src, char *dst, size_t len)
{
   size_t i, j = 0;
   bool last_was_underscore = false;

   for (i = 0; src[i] && j < len - 1; i++)
   {
      unsigned char c = (unsigned char)src[i];

      /* Skip non printable bytes */
      if (c < 0x20 || c >= 0x80)
         continue;

      if (  (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
         || (c >= '0' && c <= '9') || c == '-')
      {
         dst[j++]            = (char)c;
         last_was_underscore = false;
      }
      else
      {
         if (!last_was_underscore && j > 0)
         {
            dst[j++]            = '_';
            last_was_underscore = true;
         }
      }
   }

   if (j > 0 && dst[j - 1] == '_')
      j--;

   if (j == 0)
   {
      dst[0] = 'p';
      j      = 1;
   }

   dst[j] = '\0';
}

static void sanitize_display_name(const char *src, char *dst, size_t len)
{
   size_t i, j = 0;

   for (i = 0; src[i] && j < len - 1; i++)
   {
      unsigned char c = (unsigned char)src[i];

      if (c == 0x7F)
         continue;           
      if (c < 0x20)
         continue;           
      dst[j++] = (char)c;   
   }

   /* Trim whitespace */
   while (j > 0 && dst[j - 1] == ' ')
      j--;

   {
      size_t start = 0;
      while (start < j && dst[start] == ' ')
         start++;
      if (start > 0)
      {
         memmove(dst, dst + start, j - start);
         j -= start;
      }
   }

   if (j == 0)
   {
      strlcpy(dst, "Profile", len);
      return;
   }

   dst[j] = '\0';
}

/* Write the whole profile list to profiles.cfg */
static void profile_manager_flush(void)
{
   char profiles_cfg_path[PATH_MAX_LENGTH];
   config_file_t *conf = NULL;
   int i;

   if (!g_config_dir[0])
   {
      RARCH_WARN("[Profile] flush: g_config_dir not set, skipping.\n");
      return;
   }

   fill_pathname_join(profiles_cfg_path, g_config_dir,
         "profiles.cfg", sizeof(profiles_cfg_path));

   conf = config_file_new_alloc();
   if (!conf)
   {
      RARCH_ERR("[Profile] flush: could not allocate config.\n");
      return;
   }

   config_set_string(conf, "active_profile",
         g_profile_list.profiles[g_profile_list.active_index].name);
   config_set_int(conf, "profiles_count", g_profile_list.count);

   for (i = 0; i < g_profile_list.count; i++)
   {
      char key[64];
      snprintf(key, sizeof(key), "profile_name_%d",   i);
      config_set_string(conf, key, g_profile_list.profiles[i].name);
      snprintf(key, sizeof(key), "profile_image_%d",  i);
      config_set_string(conf, key, g_profile_list.profiles[i].image_path);
      snprintf(key, sizeof(key), "profile_config_%d", i);
      config_set_string(conf, key, g_profile_list.profiles[i].config_name);
   }

   if (config_file_write(conf, profiles_cfg_path, true))
      RARCH_LOG("[Profile] Flushed %d profile(s) to \"%s\".\n",
            g_profile_list.count, profiles_cfg_path);
   else
      RARCH_ERR("[Profile] Failed to write \"%s\".\n", profiles_cfg_path);

   config_file_free(conf);
}

static void profile_manager_ensure_init(void)
{
   if (!g_config_dir[0])
   {
      const char *config_path = path_get(RARCH_PATH_CONFIG);
      if (config_path && *config_path)
      {
         char config_dir[PATH_MAX_LENGTH];
         fill_pathname_basedir(config_dir, config_path, sizeof(config_dir));
         RARCH_LOG("[Profile] Lazy initialization triggered.\n");
         profile_manager_init(config_dir, config_path);
      }
   }
}

void profile_manager_init(const char *config_dir, const char *main_config_path)
{
   char profiles_cfg_path[PATH_MAX_LENGTH];
   config_file_t *conf = NULL;
   char active_profile_name[128];
   int i;

   strlcpy(g_config_dir,       config_dir,       sizeof(g_config_dir));
   strlcpy(g_main_config_path, main_config_path, sizeof(g_main_config_path));
   fill_pathname_join(profiles_cfg_path, g_config_dir,
         "profiles.cfg", sizeof(profiles_cfg_path));

   RARCH_LOG("[Profile] init — config_dir:  \"%s\"\n", g_config_dir);
   RARCH_LOG("[Profile] init — main_config: \"%s\"\n", g_main_config_path);

   memset(&g_profile_list, 0, sizeof(g_profile_list));
   strlcpy(active_profile_name, "Default", sizeof(active_profile_name));

   /* Create a default profiles.cfg if it doesn't exist yet */
   if (!path_is_valid(profiles_cfg_path))
   {
      RARCH_LOG("[Profile] profiles.cfg not found — creating default.\n");
      conf = config_file_new_alloc();
      if (conf)
      {
         config_set_string(conf, "active_profile",   "Default");
         config_set_int   (conf, "profiles_count",   1);
         config_set_string(conf, "profile_name_0",   "Default");
         config_set_string(conf, "profile_image_0",  "");
         config_set_string(conf, "profile_config_0", "retroarch.cfg");
         config_file_write(conf, profiles_cfg_path, true);
         config_file_free(conf);
      }
   }

   conf = config_file_new(profiles_cfg_path);
   if (conf)
   {
      char *active = NULL;
      int count    = 0;

      if (config_get_string(conf, "active_profile", &active))
      {
         strlcpy(active_profile_name, active, sizeof(active_profile_name));
         free(active);
      }

      if (config_get_int(conf, "profiles_count", &count))
      {
         if (count > MAX_PROFILES)
            count = MAX_PROFILES;
         g_profile_list.count = count;

         for (i = 0; i < count; i++)
         {
            char key[64];
            char *val = NULL;

            snprintf(key, sizeof(key), "profile_name_%d", i);
            if (config_get_string(conf, key, &val))
            { strlcpy(g_profile_list.profiles[i].name, val,
                     sizeof(g_profile_list.profiles[i].name)); free(val); }

            snprintf(key, sizeof(key), "profile_image_%d", i);
            if (config_get_string(conf, key, &val))
            { strlcpy(g_profile_list.profiles[i].image_path, val,
                     sizeof(g_profile_list.profiles[i].image_path)); free(val); }

            snprintf(key, sizeof(key), "profile_config_%d", i);
            if (config_get_string(conf, key, &val))
            { strlcpy(g_profile_list.profiles[i].config_name, val,
                     sizeof(g_profile_list.profiles[i].config_name)); free(val); }
         }
      }
      config_file_free(conf);
   }
   else
      RARCH_WARN("[Profile] Could not read \"%s\".\n", profiles_cfg_path);

   /* Ensure at least Default exists */
   if (g_profile_list.count == 0)
   {
      RARCH_WARN("[Profile] No profiles found — adding built-in Default.\n");
      strlcpy(g_profile_list.profiles[0].name,        "Default",
            sizeof(g_profile_list.profiles[0].name));
      strlcpy(g_profile_list.profiles[0].config_name, "retroarch.cfg",
            sizeof(g_profile_list.profiles[0].config_name));
      g_profile_list.count = 1;
      profile_manager_flush();
   }

   /* Resolve active index */
   g_profile_list.active_index = 0;
   for (i = 0; i < g_profile_list.count; i++)
   {
      if (string_is_equal(g_profile_list.profiles[i].name, active_profile_name))
      {
         g_profile_list.active_index = i;
         break;
      }
   }

   RARCH_LOG("[Profile] Loaded %d profile(s). Active: \"%s\" (index %d).\n",
         g_profile_list.count,
         g_profile_list.profiles[g_profile_list.active_index].name,
         g_profile_list.active_index);
}

/*
 * Startup redirect always starts by loading retroarch.cfg.
 * If a non default profile is active redirect RARCH_PATH_CONFIG to
 * that profile's cfg BEFORE config_parse_file() parses anything.
 */
bool profile_manager_get_startup_redirect(const char *loaded_path,
      char *out_path, size_t len)
{
   int idx;
   profile_manager_ensure_init();

   /* Only redirect when loading the base retroarch.cfg */
   if (!string_is_equal(loaded_path, g_main_config_path))
      return false;

   idx = g_profile_list.active_index;

   /* Default profile (index 0) is retroarch.cfg */
   if (idx <= 0 || idx >= g_profile_list.count)
      return false;

   fill_pathname_join(out_path, g_config_dir,
         g_profile_list.profiles[idx].config_name, len);

   RARCH_LOG("[Profile] Startup redirect: \"%s\" to \"%s\".\n",
         loaded_path, out_path);
   return true;
}

int profile_manager_get_active_index(void)
{
   if (g_profile_list.count == 0)
      profile_manager_init(NULL, NULL);

   return g_profile_list.active_index;
}

void profile_manager_get_active(char *name, size_t name_len,
      char *image_path, size_t image_path_len)
{
   profile_manager_ensure_init();
   int idx = g_profile_list.active_index;
   if (idx >= 0 && idx < g_profile_list.count)
   {
      strlcpy(name,       g_profile_list.profiles[idx].name,       name_len);
      strlcpy(image_path, g_profile_list.profiles[idx].image_path, image_path_len);
   }
   else
   {
      strlcpy(name, "Default", name_len);
      if (image_path_len > 0)
         image_path[0] = '\0';
   }
}

bool profile_manager_get_config_path_for_index(int index, char *path, size_t len)
{
   profile_manager_ensure_init();
   if (index < 0 || index >= g_profile_list.count)
      return false;

   if (index == 0)
   {
      /* Default profile always uses the main retroarch.cfg */
      strlcpy(path, g_main_config_path, len);
   }
   else
   {
      fill_pathname_join(path, g_config_dir,
            g_profile_list.profiles[index].config_name, len);
   }
   return true;
}

bool profile_manager_create(const char *name, const char *image_name)
{
   profile_manager_ensure_init();
   char sanitized[128];
   char display_name[128];
   char config_name[128];
   char dst_path[PATH_MAX_LENGTH];
   int new_idx;

   if (!g_config_dir[0])
   {
      RARCH_ERR("[Profile] create: g_config_dir not set!\n");
      return false;
   }

   if (g_profile_list.count >= MAX_PROFILES)
   {
      RARCH_WARN("[Profile] create: MAX_PROFILES reached.\n");
      return false;
   }

   /* Clean up the display name */
   sanitize_display_name(name, display_name, sizeof(display_name));

   /* Make a filesystem safe filename: displayname.cfg */
   sanitize_filename(display_name, sanitized, sizeof(sanitized));
   snprintf(config_name, sizeof(config_name), "%s.cfg", sanitized);
   fill_pathname_join(dst_path, g_config_dir, config_name, sizeof(dst_path));

   /* Avoid overwriting Default */
   if (string_is_equal_case_insensitive(config_name, "retroarch.cfg"))
   {
      RARCH_WARN("[Profile] create: name would collide with retroarch.cfg, appending suffix.\n");
      snprintf(config_name, sizeof(config_name), "%s_profile.cfg", sanitized);
      fill_pathname_join(dst_path, g_config_dir, config_name, sizeof(dst_path));
   }

   RARCH_LOG("[Profile] Creating profile \"%s\" to \"%s\".\n", display_name, dst_path);

   /* Save current settings snapshot into the new profile cfg */
   if (!config_save_file(dst_path))
      RARCH_WARN("[Profile] config_save_file failed for \"%s\" — continuing.\n", dst_path);

   new_idx = g_profile_list.count;
   strlcpy(g_profile_list.profiles[new_idx].name,
         display_name, sizeof(g_profile_list.profiles[new_idx].name));
   strlcpy(g_profile_list.profiles[new_idx].image_path,
         image_name ? image_name : "",
         sizeof(g_profile_list.profiles[new_idx].image_path));
   strlcpy(g_profile_list.profiles[new_idx].config_name,
         config_name, sizeof(g_profile_list.profiles[new_idx].config_name));
   g_profile_list.count++;

   RARCH_LOG("[Profile] Profile \"%s\" added. Total: %d.\n",
         display_name, g_profile_list.count);

   profile_manager_flush();
   return true;
}

bool profile_manager_set_active(int index)
{
   profile_manager_ensure_init();
   if (index < 0 || index >= g_profile_list.count)
   {
      RARCH_WARN("[Profile] set_active: index %d out of range (count=%d).\n",
            index, g_profile_list.count);
      return false;
   }

   g_profile_list.active_index = index;
   RARCH_LOG("[Profile] Active profile  \"%s\" (index %d).\n",
         g_profile_list.profiles[index].name, index);

   profile_manager_flush();
   return true;
}

bool profile_manager_save_current(void)
{
   profile_manager_ensure_init();
   char dst_path[PATH_MAX_LENGTH];
   int idx = g_profile_list.active_index;

   if (!g_config_dir[0])
   {
      RARCH_ERR("[Profile] save_current: g_config_dir not set!\n");
      return false;
   }
   if (idx < 0 || idx >= g_profile_list.count)
      return false;

   if (idx == 0)
   {
      /* Default profile save to main retroarch.cfg */
      strlcpy(dst_path, g_main_config_path, sizeof(dst_path));
   }
   else
   {
      fill_pathname_join(dst_path, g_config_dir,
            g_profile_list.profiles[idx].config_name, sizeof(dst_path));
   }

   RARCH_LOG("[Profile] Saving current settings to \"%s\".\n", dst_path);
   return config_save_file(dst_path);
}

bool profile_manager_delete(int index)
{
   profile_manager_ensure_init();
   int i;

   if (index <= 0 || index >= g_profile_list.count)
   {
      RARCH_WARN("[Profile] delete: cannot delete index %d.\n", index);
      return false;
   }

   RARCH_LOG("[Profile] Deleting profile \"%s\" (index %d).\n",
         g_profile_list.profiles[index].name, index);

   for (i = index; i < g_profile_list.count - 1; i++)
      g_profile_list.profiles[i] = g_profile_list.profiles[i + 1];
   memset(&g_profile_list.profiles[g_profile_list.count - 1], 0,
         sizeof(rarch_profile_t));
   g_profile_list.count--;

   if (g_profile_list.active_index == index)
      g_profile_list.active_index = 0; /* Active profile deleted back to default */
   else if (g_profile_list.active_index > index)
      g_profile_list.active_index--; /* Active profile down by 1 */

   RARCH_LOG("[Profile] After delete: %d profile(s), active index %d.\n",
         g_profile_list.count, g_profile_list.active_index);

   profile_manager_flush();
   return true;
}

const rarch_profile_list_t *profile_manager_get_list(void)
{
   profile_manager_ensure_init();
   return &g_profile_list;
}

void profile_manager_get_sysicons_dir(char *dir, size_t len)
{
   fill_pathname_application_special(dir, len,
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_SYSICONS);
}
