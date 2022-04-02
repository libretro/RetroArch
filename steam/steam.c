#include <ctype.h>
#include <mist.h>
#include <retro_timers.h>
#include <stdio.h>
#include <string.h>

#include "../input/input_driver.h"
#include "../menu/menu_driver.h"
#include "../menu/menu_entries.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "paths.h"
#include "verbosity.h"

#include "steam.h"

static bool mist_initialized = false;
static bool mist_showing_osk = false;
static steam_core_dlc_list_t *mist_dlc_list = NULL;
static enum presence last_presence = PRESENCE_NONE;

void str_to_lower(char *str)
{
   for (size_t i = 0; str[i] != '\0'; i++)
   {
      str[i] = tolower(str[i]);
   }
}

void steam_init(void)
{
   MistResult result;

   result = mist_subprocess_init();

   if (MIST_IS_SUCCESS(result))
      mist_initialized = true;
   else
      RARCH_ERR("[Steam]: Failed to initialize mist subprocess (%d-%d)\n", MIST_UNPACK_RESULT(result));
}

void steam_poll(void)
{
   MistResult result;
   MistCallbackMsg callback;
   steam_core_dlc_list_t *core_dlc_list;
   bool has_callback = false;
   settings_t* settings = config_get_ptr();
   static bool has_poll_errored = false;
   static bool has_rich_presence_enabled = false;

   result = mist_poll();
   if (MIST_IS_ERROR(result))
   {
      if(has_poll_errored) return;

      RARCH_ERR("[Steam]: Error polling (%d-%d)\n", MIST_UNPACK_RESULT(result));

      has_poll_errored = true;
   }

   result = mist_next_callback(&has_callback, &callback);
   if(MIST_IS_ERROR(result)) return;

   while (has_callback && MIST_IS_SUCCESS(result))
   {
      switch (callback.callback)
      {
         /* Reload core info and Steam Core DLC mappings */
         case MistCallback_DlcInstalled:
            command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
            steam_get_core_dlcs(&core_dlc_list, false);
            break;
         /* The Steam OSK is dismissed */
         case MistCallback_FloatingGamepadTextInputDismissed:
            /* If we do not poll for input the callback might race condition and
               will dismiss the input even when enter is pressed */
            retro_sleep(50);
            runloop_iterate();
            menu_input_dialog_end();
            break;
      }

      result = mist_next_callback(&has_callback, &callback);
   }

   /* Ensure rich presence state is correct */
   if(settings->bools.steam_rich_presence_enable != has_rich_presence_enabled)
   {
      steam_update_presence(last_presence, true);
      has_rich_presence_enabled = settings->bools.steam_rich_presence_enable;
   }
}

steam_core_dlc_list_t *steam_core_dlc_list_new(size_t count)
{
   steam_core_dlc_list_t *core_dlc_list = (steam_core_dlc_list_t*)
      malloc(sizeof(*core_dlc_list));

   core_dlc_list->list = (steam_core_dlc_t*)
      malloc(count * sizeof(*core_dlc_list->list));

   core_dlc_list->count = 0; /* This is incremented inside the setup function */

   return core_dlc_list;
}

void steam_core_dlc_list_free(steam_core_dlc_list_t *list)
{
   if (list == NULL) return;

   for (size_t i = 0; list->count > i; i++)
   {
      if (list->list[i].name != NULL)
         free(list->list[i].name);
      if (list->list[i].name_lower != NULL)
         free(list->list[i].name_lower);
   }

   free(list->list);
   free(list);
}

steam_core_dlc_t *steam_core_dlc_list_get(steam_core_dlc_list_t *list, size_t i)
{
   if (!list || (i >= list->count))
      return NULL;

   return &list->list[i];
}

/* Sort the dlc cores alphabetically based on their name */
static int dlc_core_qsort_cmp(const void *a_, const void *b_)
{
   const steam_core_dlc_t *a = (const steam_core_dlc_t*)a_;
   const steam_core_dlc_t *b = (const steam_core_dlc_t*)b_;

   return strcasecmp(a->name, b->name);
}

/* Find core info for dlcs
 * TODO: This currently only uses core info for cores that are installed */
core_info_t* steam_find_core_info_for_dlc(const char* name)
{
   size_t name_len = strlen(name);

   core_info_list_t *core_info_list = NULL;
   core_info_get_list(&core_info_list);

   if (core_info_list == NULL) return NULL;

   for (int i = 0; core_info_list->count > i; i++)
   {
      core_info_t *core_info = core_info_get(core_info_list, i);

      char core_info_name[256] = {0};

      /* Find the opening parenthesis for the core name */
      char *start = strchr(core_info->display_name, '(');
      if (start == NULL) continue;

      /* Skip the first parenthesis and copy it to the stack */
      strncpy(core_info_name, start + 1, sizeof(core_info_name) - 1);

      /* Null terminate at the closing parenthesis. */
      char *end = strchr((const char*)&core_info_name, ')');
      if (end == NULL) continue;
      else *end = '\0';

      /* Make it lowercase */
      str_to_lower((char*)&core_info_name);

      /* Check if it matches */
      if (strcasecmp(core_info_name, name) == 0)
         return core_info;
   }

   return NULL;
}

/* Generate a list with core dlcs
 * Needs to be called after initializion because it uses core info */
MistResult steam_generate_core_dlcs_list(steam_core_dlc_list_t **list)
{
   MistResult result;
   int count;
   steam_core_dlc_list_t *dlc_list = NULL;
   char dlc_name[PATH_MAX_LENGTH] = { 0 };
   bool avaliable;

   result = mist_steam_apps_get_dlc_count(&count);
   if (MIST_IS_ERROR(result)) goto error;

   dlc_list = steam_core_dlc_list_new(count);
   for (int i = 0; count > i; i++)
   {
      steam_core_dlc_t core_dlc;

      result = mist_steam_apps_get_dlc_data_by_index(i, &core_dlc.app_id, &avaliable, (char*)&dlc_name, PATH_MAX_LENGTH);
      if (MIST_IS_ERROR(result)) goto error;

      /* Strip away the "RetroArch - " prefix if present */
      if (strncmp(dlc_name, "RetroArch - ", sizeof("RetroArch - ") - 1) == 0)
         core_dlc.name = strdup(dlc_name + sizeof("RetroArch - ") - 1);
      else
         core_dlc.name = strdup(dlc_name);

      /* Make a lower case version */
      core_dlc.name_lower = strdup(core_dlc.name);
      str_to_lower(core_dlc.name_lower);

      core_dlc.core_info = steam_find_core_info_for_dlc(core_dlc.name_lower);

      dlc_list->list[i] = core_dlc;
      dlc_list->count++;
   }

   /* Sort the list */
   qsort(dlc_list->list, dlc_list->count,
         sizeof(steam_core_dlc_t), dlc_core_qsort_cmp);

   *list = dlc_list;
   return MistResult_Success;

error:
   if (dlc_list != NULL) steam_core_dlc_list_free(dlc_list);

   return result;
}

MistResult steam_get_core_dlcs(steam_core_dlc_list_t **list, bool cached) {
   MistResult result;
   steam_core_dlc_list_t *new_list = NULL;

   if (cached && mist_dlc_list != NULL)
   {
      *list = mist_dlc_list;
      return MistResult_Success;
   }

   result = steam_generate_core_dlcs_list(&new_list);
   if (MIST_IS_ERROR(result)) return result;

   if (mist_dlc_list != NULL) steam_core_dlc_list_free(mist_dlc_list);

   mist_dlc_list = new_list;
   *list = new_list;

   return MistResult_Success;
}

steam_core_dlc_t* steam_get_core_dlc_by_name(steam_core_dlc_list_t *list, const char *name) {
   steam_core_dlc_t *core_info;

   for (int i = 0; list->count > i; i++)
   {
      core_info = steam_core_dlc_list_get(list, i);
      if (strcasecmp(core_info->name, name) == 0) return core_info;
   }

   return NULL;
}

void steam_install_core_dlc(steam_core_dlc_t *core_dlc)
{
   MistResult result;
   char msg[PATH_MAX_LENGTH] = { 0 };

   bool downloading = false;
   bool installed = false;
   uint64_t bytes_downloaded = 0;
   uint64_t bytes_total = 0;

   /* Check if the core is already being downloaded */
   result = mist_steam_apps_get_dlc_download_progress(core_dlc->app_id, &downloading, &bytes_downloaded, &bytes_total);
   if (MIST_IS_ERROR(result)) goto error;

   /* Check if the core is already installed */
   result = mist_steam_apps_is_dlc_installed(core_dlc->app_id, &installed);
   if (MIST_IS_ERROR(result)) goto error;

   if (downloading || installed)
   {
      runloop_msg_queue_push(msg_hash_to_str(MSG_CORE_STEAM_CURRENTLY_DOWNLOADING), 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);

      return;
   }

   result = mist_steam_apps_install_dlc(core_dlc->app_id);
   if (MIST_IS_ERROR(result)) goto error;

   task_push_steam_core_dlc_install(core_dlc->app_id, core_dlc->name);

   return;
error:
      snprintf(msg, sizeof(msg), "%s: (%d-%d)",
         msg_hash_to_str(MSG_ERROR),
         MIST_UNPACK_RESULT(result));

      runloop_msg_queue_push(msg, 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);

      RARCH_ERR("[Steam]: Error installing DLC %d (%d-%d)\n", core_dlc->app_id, MIST_UNPACK_RESULT(result));
   return;
}

void steam_uninstall_core_dlc(steam_core_dlc_t *core_dlc)
{
   char msg[PATH_MAX_LENGTH] = { 0 };

   MistResult result = mist_steam_apps_uninstall_dlc(core_dlc->app_id);

   if (MIST_IS_ERROR(result)) goto error;

   runloop_msg_queue_push(msg_hash_to_str(MSG_CORE_STEAM_UNINSTALLED), 1, 180, true, NULL,
      MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   bool refresh = false;

   return;
error:
      snprintf(msg, sizeof(msg), "%s: (%d-%d)",
         msg_hash_to_str(MSG_ERROR),
         MIST_UNPACK_RESULT(result));

      runloop_msg_queue_push(msg, 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);

      RARCH_ERR("[Steam]: Error uninstalling DLC %d (%d-%d)\n", core_dlc->app_id, MIST_UNPACK_RESULT(result));
   return;
}

bool steam_open_osk(void)
{
   bool shown = false;
   bool on_deck = false;
   video_driver_state_t *video_st = video_state_get_ptr();

   /* Only open the Steam OSK if running on a Steam Deck,
      as currently the Big Picture OSK seems to be semi-broken */
   mist_steam_utils_is_steam_running_on_steam_deck(&on_deck);
   if(!on_deck) return false;

   mist_steam_utils_show_floating_gamepad_text_input(
      MistFloatingGamepadTextInputMode_SingleLine,
      0,
      0,
      video_st->width,
      video_st->height / 2,
      &shown
   );

   mist_showing_osk = shown;

   return shown;
}

bool steam_has_osk_open(void)
{
   return mist_showing_osk;
}

void steam_update_presence(enum presence presence, bool force)
{
   settings_t* settings = config_get_ptr();

   if (!mist_initialized)
      return;

   /* Avoid spamming steam with presence updates */
   if (presence == last_presence && !force)
      return;
   last_presence = presence;

   /* Ensure rich presence is enabled */
   if(!settings->bools.steam_rich_presence_enable)
   {
      mist_steam_friends_clear_rich_presence();
      return;
   }

   switch (presence)
   {
   case PRESENCE_MENU:
      mist_steam_friends_set_rich_presence("steam_display", "#Status_InMenu");
      break;
   case PRESENCE_GAME_PAUSED:
      mist_steam_friends_set_rich_presence("steam_display", "#Status_Paused");
      break;
   case PRESENCE_GAME:
   {
      const char *label = NULL;
      const struct playlist_entry *entry = NULL;
      core_info_t *core_info = NULL;
      playlist_t *current_playlist = playlist_get_cached();
      char content[PATH_MAX_LENGTH] = {0};

      core_info_get_current_core(&core_info);

      if (current_playlist)
      {
         playlist_get_index_by_path(
             current_playlist,
             path_get(RARCH_PATH_CONTENT),
             &entry);

         if (entry && !string_is_empty(entry->label))
            label = entry->label;
      }

      if (!label)
         label = path_basename(path_get(RARCH_PATH_BASENAME));

      switch(settings->uints.steam_rich_presence_format)
      {
         case STEAM_RICH_PRESENCE_FORMAT_CONTENT:
            strncpy(content, label, sizeof(content) - 1);
            break;
         case STEAM_RICH_PRESENCE_FORMAT_CORE:
            strncpy(content, core_info ? core_info->core_name : "N/A", sizeof(content) - 1);
            break;
         case STEAM_RICH_PRESENCE_FORMAT_SYSTEM:
            strncpy(content, core_info ? core_info->systemname : "N/A", sizeof(content) - 1);
            break;
         case STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM:
            snprintf(content, sizeof(content) - 1, "%s (%s)",
               label,
               core_info ? core_info->systemname : "N/A");
            break;
         case STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE:
            snprintf(content, sizeof(content) - 1, "%s (%s)",
               label,
               core_info ? core_info->core_name : "N/A");
            break;
         case STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE:
            snprintf(content, sizeof(content) - 1, "%s (%s - %s)",
               label,
               core_info ? core_info->systemname : "N/A",
               core_info ? core_info->core_name : "N/A");
            break;
         case STEAM_RICH_PRESENCE_FORMAT_NONE:
         default:
            break;
      }


      mist_steam_friends_set_rich_presence("content", content);
      mist_steam_friends_set_rich_presence("steam_display",
         settings->uints.steam_rich_presence_format != STEAM_RICH_PRESENCE_FORMAT_NONE
            ? "#Status_RunningContent" : "#Status_Running" );
   }
   break;
   default:
      break;
   }
}

void steam_deinit(void)
{
   MistResult result;

   result = mist_subprocess_deinit();

   /* Free the cached dlc list */
   if (mist_dlc_list != NULL) steam_core_dlc_list_free(mist_dlc_list);

   if (MIST_IS_SUCCESS(result))
      mist_initialized = false;
   else
      RARCH_ERR("[Steam]: Failed to deinitialize mist subprocess (%d-%d)\n", MIST_UNPACK_RESULT(result));
}
