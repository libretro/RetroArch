/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <file/config_file.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>
#include <features/features_cpu.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/widgets/menu_input_bind_dialog.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos-new/cheevos.h"
#endif

#include "cheat_manager.h"

#include "../msg_hash.h"
#include "../retroarch.h"
#include "../dynamic.h"
#include "../core.h"
#include "../verbosity.h"
#include "../configuration.h"

cheat_manager_t cheat_manager_state;

unsigned cheat_manager_get_buf_size(void)
{
   return cheat_manager_state.buf_size;
}

unsigned cheat_manager_get_size(void)
{
   return cheat_manager_state.size;
}

void cheat_manager_apply_cheats(void)
{
#ifdef HAVE_CHEEVOS
   bool data_bool  = false;

#endif
   unsigned i, idx = 0;

   if (!cheat_manager_state.cheats)
      return;

   core_reset_cheat();

   for (i = 0; i < cheat_manager_state.size; i++)
   {
      if (cheat_manager_state.cheats[i].state && cheat_manager_state.cheats[i].handler == CHEAT_HANDLER_TYPE_EMU)
      {
         retro_ctx_cheat_info_t cheat_info;

         cheat_info.index = idx++;
         cheat_info.enabled = true;
         cheat_info.code = cheat_manager_state.cheats[i].code;

         if (!string_is_empty(cheat_info.code))
            core_set_cheat(&cheat_info);
      }
   }

   if (cheat_manager_state.size > 0)
   {
      runloop_msg_queue_push(msg_hash_to_str(MSG_APPLYING_CHEAT), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_APPLYING_CHEAT));
   }

#ifdef HAVE_CHEEVOS
   data_bool = idx != 0;
   rcheevos_apply_cheats(&data_bool);
#endif
}

void cheat_manager_set_code(unsigned i, const char *str)
{
   if (!cheat_manager_state.cheats)
      return;

   if (!string_is_empty(str))
      strcpy(cheat_manager_state.cheats[i].code, str);

   cheat_manager_state.cheats[i].state = true;
}

/**
 * cheat_manager_save:
 * @path                      : Path to cheats file (relative path).
 *
 * Saves cheats to file on disk.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool cheat_manager_save(const char *path, const char *cheat_database, bool overwrite)
{
   bool ret;
   unsigned i;
   char buf[PATH_MAX_LENGTH];
   char cheats_file[PATH_MAX_LENGTH];
   config_file_t *conf = NULL;
   unsigned int* data_ptrs[16] = { NULL};
   char* keys[16] = {
      (char*)"cheat%u_handler",
      (char*)"cheat%u_memory_search_size",
      (char*)"cheat%u_cheat_type",
      (char*)"cheat%u_value",
      (char*)"cheat%u_address",
      (char*)"cheat%u_address_bit_position",
      (char*)"cheat%u_rumble_type",
      (char*)"cheat%u_rumble_value",
      (char*)"cheat%u_rumble_port",
      (char*)"cheat%u_rumble_primary_strength",
      (char*)"cheat%u_rumble_primary_duration",
      (char*)"cheat%u_rumble_secondary_strength",
      (char*)"cheat%u_rumble_secondary_duration",
      (char*)"cheat%u_repeat_count",
      (char*)"cheat%u_repeat_add_to_value",
      (char*)"cheat%u_repeat_add_to_address"
   };

   buf[0] = cheats_file[0] = '\0';

   if ((!cheat_manager_state.cheats) || cheat_manager_state.size == 0)
      return false;

   if (!cheat_database)
      strlcpy(cheats_file, path, sizeof(cheats_file));
   else
   {
      fill_pathname_join(buf, cheat_database, path, sizeof(buf));

      fill_pathname_noext(cheats_file, buf, ".cht", sizeof(cheats_file));
   }

   if (!overwrite)
      conf = config_file_new_from_path_to_string(cheats_file);

   if (!conf)
      if (!(conf = config_file_new_alloc()))
         return false;

   conf->guaranteed_no_duplicates = true;

   config_set_int(conf, "cheats", cheat_manager_state.size);

   for (i = 0; i < cheat_manager_state.size; i++)
   {
      unsigned j;
      char endian_key[100];
      char key[256];
      char desc_key[256];
      char code_key[256];
      char enable_key[256];

      key[0] = endian_key[0] = desc_key[0] = code_key[0] = enable_key[0] = '\0';

      snprintf(endian_key, sizeof(endian_key), "cheat%u_big_endian", i);
      snprintf(desc_key, sizeof(desc_key), "cheat%u_desc", i);
      snprintf(code_key, sizeof(code_key), "cheat%u_code", i);
      snprintf(enable_key, sizeof(enable_key), "cheat%u_enable", i);

      if (!string_is_empty(cheat_manager_state.cheats[i].desc))
         config_set_string(conf, desc_key, cheat_manager_state.cheats[i].desc);
      else
         config_set_string(conf, desc_key, cheat_manager_state.cheats[i].code);

      config_set_string(conf, code_key, cheat_manager_state.cheats[i].code);
      config_set_bool(conf, enable_key, cheat_manager_state.cheats[i].state);
      config_set_bool(conf, endian_key, cheat_manager_state.cheats[i].big_endian);

      data_ptrs[0] = &cheat_manager_state.cheats[i].handler;
      data_ptrs[1] = &cheat_manager_state.cheats[i].memory_search_size;
      data_ptrs[2] = &cheat_manager_state.cheats[i].cheat_type;
      data_ptrs[3] = &cheat_manager_state.cheats[i].value;
      data_ptrs[4] = &cheat_manager_state.cheats[i].address;
      data_ptrs[5] = &cheat_manager_state.cheats[i].address_mask;
      data_ptrs[6] = &cheat_manager_state.cheats[i].rumble_type;
      data_ptrs[7] = &cheat_manager_state.cheats[i].rumble_value;
      data_ptrs[8] = &cheat_manager_state.cheats[i].rumble_port;
      data_ptrs[9] = &cheat_manager_state.cheats[i].rumble_primary_strength;
      data_ptrs[10] = &cheat_manager_state.cheats[i].rumble_primary_duration;
      data_ptrs[11] = &cheat_manager_state.cheats[i].rumble_secondary_strength;
      data_ptrs[12] = &cheat_manager_state.cheats[i].rumble_secondary_duration;
      data_ptrs[13] = &cheat_manager_state.cheats[i].repeat_count;
      data_ptrs[14] = &cheat_manager_state.cheats[i].repeat_add_to_value;
      data_ptrs[15] = &cheat_manager_state.cheats[i].repeat_add_to_address;

      for (j = 0; j < 16; j++)
      {
         key[0] = '\0';
         snprintf(key, sizeof(key), keys[j], i);
         config_set_uint(conf, key, *(data_ptrs[j]));
      }

   }

   ret = config_file_write(conf, cheats_file, true);
   config_file_free(conf);

   return ret;
}

bool cheat_manager_copy_idx_to_working(unsigned idx)
{
   if ((!cheat_manager_state.cheats) || (cheat_manager_state.size < idx + 1))
      return false;

   memcpy(&(cheat_manager_state.working_cheat), &(cheat_manager_state.cheats[idx]), sizeof(struct item_cheat));

   if (cheat_manager_state.cheats[idx].desc)
      strlcpy(cheat_manager_state.working_desc, cheat_manager_state.cheats[idx].desc, CHEAT_DESC_SCRATCH_SIZE);
   else
      cheat_manager_state.working_desc[0] = '\0';

   if (cheat_manager_state.cheats[idx].code)
      strlcpy(cheat_manager_state.working_code, cheat_manager_state.cheats[idx].code, CHEAT_CODE_SCRATCH_SIZE);
   else
      cheat_manager_state.working_code[0] = '\0';

   return true;
}

bool cheat_manager_copy_working_to_idx(unsigned idx)
{
   if ((!cheat_manager_state.cheats) || (cheat_manager_state.size < idx + 1))
      return false;

   memcpy(&(cheat_manager_state.cheats[idx]), &(cheat_manager_state.working_cheat), sizeof(struct item_cheat));

   if (cheat_manager_state.cheats[idx].desc)
      free(cheat_manager_state.cheats[idx].desc);

   cheat_manager_state.cheats[idx].desc = strdup(cheat_manager_state.working_desc);

   if (cheat_manager_state.cheats[idx].code)
      free(cheat_manager_state.cheats[idx].code);

   cheat_manager_state.cheats[idx].code = strdup(cheat_manager_state.working_code);

   return true;
}

static void cheat_manager_free(void)
{
   unsigned i = 0;

   if (cheat_manager_state.cheats)
   {
      for (i = 0; i < cheat_manager_state.size; i++)
      {
         if (cheat_manager_state.cheats[i].desc)
            free(cheat_manager_state.cheats[i].desc);
         if (cheat_manager_state.cheats[i].code)
            free(cheat_manager_state.cheats[i].code);
      }

      free(cheat_manager_state.cheats);
   }

   if (cheat_manager_state.prev_memory_buf)
      free(cheat_manager_state.prev_memory_buf);

   if (cheat_manager_state.matches)
      free(cheat_manager_state.matches);

   if (cheat_manager_state.memory_buf_list)
      free(cheat_manager_state.memory_buf_list);

   if (cheat_manager_state.memory_size_list)
      free(cheat_manager_state.memory_size_list);

   cheat_manager_state.cheats = NULL;
   cheat_manager_state.size = 0;
   cheat_manager_state.buf_size = 0;
   cheat_manager_state.prev_memory_buf = NULL;
   cheat_manager_state.curr_memory_buf = NULL;
   cheat_manager_state.memory_buf_list = NULL;
   cheat_manager_state.memory_size_list = NULL;
   cheat_manager_state.matches = NULL;
   cheat_manager_state.num_memory_buffers = 0;
   cheat_manager_state.total_memory_size = 0;
   cheat_manager_state.memory_initialized = false;
   cheat_manager_state.memory_search_initialized = false;

}


static void cheat_manager_new(unsigned size)
{
   unsigned i;

   cheat_manager_free();

   cheat_manager_state.buf_size        = size;
   cheat_manager_state.size            = size;
   cheat_manager_state.search_bit_size = 3;
   cheat_manager_state.cheats          = (struct item_cheat*)
         calloc(cheat_manager_state.buf_size, sizeof(struct item_cheat));

   if (!cheat_manager_state.cheats)
   {
      cheat_manager_state.buf_size    = 0;
      cheat_manager_state.size        = 0;
      cheat_manager_state.cheats      = NULL;
      return;
   }

   for (i = 0; i < cheat_manager_state.size; i++)
   {
      cheat_manager_state.cheats[i].desc                  = NULL;
      cheat_manager_state.cheats[i].code                  = NULL;
      cheat_manager_state.cheats[i].state                 = false;
      cheat_manager_state.cheats[i].repeat_count          = 1;
      cheat_manager_state.cheats[i].repeat_add_to_value   = 0;
      cheat_manager_state.cheats[i].repeat_add_to_address = 1;
   }
}

static void cheat_manager_load_cb_first_pass(char *key, char *value)
{
   errno = 0;

   if (string_is_equal(key, "cheats"))
   {
      cheat_manager_state.loading_cheat_size = (unsigned)strtoul(value, NULL, 0);

      if (errno != 0)
         cheat_manager_state.loading_cheat_size = 0;
   }
}

static void cheat_manager_load_cb_second_pass(char *key, char *value)
{
   char cheat_num_str[20];
   unsigned cheat_num;
   unsigned cheat_idx;
   unsigned idx = 5;
   size_t key_length = 0;
   errno = 0;

   if (strncmp(key, "cheat", 5) != 0)
      return;

   key_length = strlen((const char*)key);

   while (idx < key_length && key[idx] >= '0' && key[idx] <= '9' && idx < 24)
   {
      cheat_num_str[idx - 5] = key[idx];
      idx++;
   }

   cheat_num_str[idx - 5] = '\0';

   cheat_num = (unsigned)strtoul(cheat_num_str, NULL, 0);

   if (cheat_num + cheat_manager_state.loading_cheat_offset >= cheat_manager_state.size)
      return;

   key = key + idx + 1;

   cheat_idx = cheat_num + cheat_manager_state.loading_cheat_offset;

   if (string_is_equal(key, "address"))
      cheat_manager_state.cheats[cheat_idx].address = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "address_bit_position"))
      cheat_manager_state.cheats[cheat_idx].address_mask = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "big_endian"))
      cheat_manager_state.cheats[cheat_idx].big_endian = (string_is_equal(value, "true") || string_is_equal(value, "1"));
   else if (string_is_equal(key, "cheat_type"))
      cheat_manager_state.cheats[cheat_idx].cheat_type = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "code"))
      cheat_manager_state.cheats[cheat_idx].code = strdup(value);
   else if (string_is_equal(key, "desc"))
      cheat_manager_state.cheats[cheat_idx].desc = strdup(value);
   else if (string_is_equal(key, "enable"))
      cheat_manager_state.cheats[cheat_idx].state = (string_is_equal(value, "true") || string_is_equal(value, "1"));
   else if (string_is_equal(key, "handler"))
      cheat_manager_state.cheats[cheat_idx].handler = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "memory_search_size"))
      cheat_manager_state.cheats[cheat_idx].memory_search_size = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "repeat_add_to_address"))
      cheat_manager_state.cheats[cheat_idx].repeat_add_to_address = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "repeat_add_to_value"))
      cheat_manager_state.cheats[cheat_idx].repeat_add_to_value = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "repeat_count"))
      cheat_manager_state.cheats[cheat_idx].repeat_count = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "rumble_port"))
      cheat_manager_state.cheats[cheat_idx].rumble_port = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "rumble_primary_duration"))
      cheat_manager_state.cheats[cheat_idx].rumble_primary_duration = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "rumble_primary_strength"))
      cheat_manager_state.cheats[cheat_idx].rumble_primary_strength = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "rumble_secondary_duration"))
      cheat_manager_state.cheats[cheat_idx].rumble_secondary_duration = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "rumble_secondary_strength"))
      cheat_manager_state.cheats[cheat_idx].rumble_secondary_strength = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "rumble_type"))
      cheat_manager_state.cheats[cheat_idx].rumble_type = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "rumble_value"))
      cheat_manager_state.cheats[cheat_idx].rumble_value = (unsigned)strtoul(value, NULL, 0);
   else if (string_is_equal(key, "value"))
      cheat_manager_state.cheats[cheat_idx].value = (unsigned)strtoul(value, NULL, 0);
}

bool cheat_manager_load(const char *path, bool append)
{
   config_file_cb_t cb;
   unsigned orig_size  = 0;
   unsigned cheats     = 0;
   unsigned i          = 0;
   config_file_t *conf = NULL;

   cb.config_file_new_entry_cb = cheat_manager_load_cb_first_pass;

   cheat_manager_state.loading_cheat_size = 0;

   conf = config_file_new_with_callback(path, &cb);

   if (!conf)
      return false;

   cheats = cheat_manager_state.loading_cheat_size;

   if (cheats == 0)
      goto error;

   config_file_free(conf);
   conf = NULL;

   cheat_manager_alloc_if_empty();

   if (append)
   {
      orig_size = cheat_manager_get_size();
      if (orig_size == 0)
         cheat_manager_new(cheats);
      else
      {
         cheats = cheats + orig_size;
         if (cheat_manager_realloc(cheats, CHEAT_HANDLER_TYPE_EMU))
         {
         }
      }
   }
   else
   {
      orig_size = 0;
      cheat_manager_new(cheats);
   }

   for (i = orig_size; cheat_manager_state.cheats && i < cheats; i++)
   {
      cheat_manager_state.cheats[i].idx = i;
      cheat_manager_state.cheats[i].desc = NULL;
      cheat_manager_state.cheats[i].code = NULL;
      cheat_manager_state.cheats[i].state = false;
      cheat_manager_state.cheats[i].big_endian = false;
      cheat_manager_state.cheats[i].cheat_type = CHEAT_TYPE_SET_TO_VALUE;
      cheat_manager_state.cheats[i].memory_search_size = 3;
   }

   cheat_manager_state.loading_cheat_offset = orig_size;
   cb.config_file_new_entry_cb = cheat_manager_load_cb_second_pass;
   conf = config_file_new_with_callback(path, &cb);

   if (!conf)
      return false;

   config_file_free(conf);

   return true;

error:
   config_file_free(conf);
   return false;
}

bool cheat_manager_realloc(unsigned new_size, unsigned default_handler)
{
   unsigned i;
   unsigned orig_size = 0;

   if (!cheat_manager_state.cheats)
   {
      cheat_manager_state.cheats = (struct item_cheat*)
            calloc(new_size, sizeof(struct item_cheat));
      orig_size = 0;
   }
   else
   {
      struct item_cheat *val = NULL;
      orig_size = cheat_manager_state.size;

      /* if size is decreasing, free the items that will be lost */
      for (i = new_size; i < orig_size; i++)
      {
         if (cheat_manager_state.cheats[i].code)
            free(cheat_manager_state.cheats[i].code);
         if (cheat_manager_state.cheats[i].desc)
            free(cheat_manager_state.cheats[i].desc);
      }

      val = (struct item_cheat*)
            realloc(cheat_manager_state.cheats,
            new_size * sizeof(struct item_cheat));

      cheat_manager_state.cheats = val ? val : NULL;
   }

   if (!cheat_manager_state.cheats)
   {
      cheat_manager_state.buf_size = cheat_manager_state.size = 0;
      cheat_manager_state.cheats = NULL;
      return false;
   }

   cheat_manager_state.buf_size = new_size;
   cheat_manager_state.size = new_size;

   for (i = orig_size; i < cheat_manager_state.size; i++)
   {
      memset(&(cheat_manager_state.cheats[i]), 0, sizeof(cheat_manager_state.cheats[i]));
      cheat_manager_state.cheats[i].state = false;
      cheat_manager_state.cheats[i].handler = default_handler;
      cheat_manager_state.cheats[i].cheat_type = CHEAT_TYPE_SET_TO_VALUE;
      cheat_manager_state.cheats[i].memory_search_size = 3;
      cheat_manager_state.cheats[i].idx = i;
      cheat_manager_state.cheats[i].repeat_count = 1;
      cheat_manager_state.cheats[i].repeat_add_to_value = 0;
      cheat_manager_state.cheats[i].repeat_add_to_address = 1;
   }

   return true;
}

void cheat_manager_update(cheat_manager_t *handle, unsigned handle_idx)
{
   char msg[256];

   if (!handle || !handle->cheats || handle->size == 0)
      return;

   snprintf(msg, sizeof(msg), "Cheat: #%u [%s]: %s",
         handle_idx, handle->cheats[handle_idx].state ? "ON" : "OFF",
         (handle->cheats[handle_idx].desc != NULL) ?
         (handle->cheats[handle_idx].desc) : (handle->cheats[handle_idx].code)
         );
   runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s\n", msg);
}

void cheat_manager_toggle_index(unsigned i)
{
   settings_t *settings = config_get_ptr();
   if (!cheat_manager_state.cheats || cheat_manager_state.size == 0)
      return;

   cheat_manager_state.cheats[i].state = !cheat_manager_state.cheats[i].state;
   cheat_manager_update(&cheat_manager_state, i);

   if (!settings)
      return;

   if (settings->bools.apply_cheats_after_toggle)
      cheat_manager_apply_cheats();
}

void cheat_manager_toggle(void)
{
   if (!cheat_manager_state.cheats || cheat_manager_state.size == 0)
      return;

   cheat_manager_state.cheats[cheat_manager_state.ptr].state ^= true;
   cheat_manager_apply_cheats();
   cheat_manager_update(&cheat_manager_state, cheat_manager_state.ptr);
}

void cheat_manager_index_next(void)
{
   if (!cheat_manager_state.cheats || cheat_manager_state.size == 0)
      return;

   cheat_manager_state.ptr = (cheat_manager_state.ptr + 1) % cheat_manager_state.size;
   cheat_manager_update(&cheat_manager_state, cheat_manager_state.ptr);
}

void cheat_manager_index_prev(void)
{
   if (!cheat_manager_state.cheats || cheat_manager_state.size == 0)
      return;

   if (cheat_manager_state.ptr == 0)
      cheat_manager_state.ptr = cheat_manager_state.size - 1;
   else
      cheat_manager_state.ptr--;

   cheat_manager_update(&cheat_manager_state, cheat_manager_state.ptr);
}

const char *cheat_manager_get_code(unsigned i)
{
   if (!cheat_manager_state.cheats)
      return NULL;
   return cheat_manager_state.cheats[i].code;
}

const char *cheat_manager_get_desc(unsigned i)
{
   if (!cheat_manager_state.cheats)
      return NULL;
   return cheat_manager_state.cheats[i].desc;
}

bool cheat_manager_get_code_state(unsigned i)
{
   if (!cheat_manager_state.cheats)
      return false;
   return cheat_manager_state.cheats[i].state;
}

static bool cheat_manager_get_game_specific_filename(
      char *s, size_t len,
      bool saving)
{
   char s1[PATH_MAX_LENGTH];
   struct retro_system_info system_info;
   settings_t *settings    = config_get_ptr();
   global_t *global        = global_get_ptr();
   const char *core_name   = NULL;
   const char *game_name   = NULL;

   s1[0]                   = '\0';

   if (!settings || !global)
      return false;

   if (!core_get_system_info(&system_info))
      return false;

   core_name = system_info.library_name;
   game_name = path_basename(global->name.cheatfile);

   if (string_is_empty(settings->paths.path_cheat_database) ||
         string_is_empty(core_name) ||
         string_is_empty(game_name))
      return false;

   s[0] = '\0';

   fill_pathname_join(s1,
         settings->paths.path_cheat_database, core_name,
         sizeof(s1));

   if (saving)
   {
      /* Check if directory is valid, if not, create it */
      if (!path_is_valid(s1))
         path_mkdir(s1);
   }

   fill_pathname_join(s, s1, game_name, len);

   return true;
}

void cheat_manager_load_game_specific_cheats(void)
{
   char cheat_file[PATH_MAX_LENGTH];

   if (cheat_manager_get_game_specific_filename(
            cheat_file, sizeof(cheat_file), false))
   {
      RARCH_LOG("[Cheats]: Load game-specific cheatfile: %s\n", cheat_file);
      cheat_manager_load(cheat_file, true);
   }
}

void cheat_manager_save_game_specific_cheats(void)
{
   char cheat_file[PATH_MAX_LENGTH];

   if (cheat_manager_get_game_specific_filename(
            cheat_file, sizeof(cheat_file), true))
   {
      RARCH_LOG("[Cheats]: Save game-specific cheatfile: %s\n", cheat_file);
      cheat_manager_save(cheat_file, NULL, true);
   }
}

void cheat_manager_state_free(void)
{
   cheat_manager_free();
}

bool cheat_manager_alloc_if_empty(void)
{
   if (!cheat_manager_state.cheats)
      cheat_manager_new(0);

   return true;
}

int cheat_manager_initialize_memory(rarch_setting_t *setting, bool wraparound)
{
   unsigned i;
   retro_ctx_memory_info_t meminfo;
   bool refresh                           = false;
   bool is_search_initialization          = (setting != NULL);
   rarch_system_info_t *system            = runloop_get_system_info();
   unsigned offset                        = 0;

   cheat_manager_state.num_memory_buffers = 0;
   cheat_manager_state.total_memory_size  = 0;
   cheat_manager_state.curr_memory_buf    = NULL;

   if (cheat_manager_state.memory_buf_list)
   {
      free(cheat_manager_state.memory_buf_list);
      cheat_manager_state.memory_buf_list = NULL;
   }

   if (cheat_manager_state.memory_size_list)
   {
      free(cheat_manager_state.memory_size_list);
      cheat_manager_state.memory_size_list = NULL;
   }

   if (system && system->mmaps.num_descriptors > 0)
   {
      for (i = 0; i < system->mmaps.num_descriptors; i++)
      {
         if ((system->mmaps.descriptors[i].core.flags & RETRO_MEMDESC_SYSTEM_RAM) != 0 &&
               system->mmaps.descriptors[i].core.ptr &&
               system->mmaps.descriptors[i].core.len > 0)
         {
            cheat_manager_state.num_memory_buffers++;

            if (!cheat_manager_state.memory_buf_list)
               cheat_manager_state.memory_buf_list = (uint8_t**)calloc(1, sizeof(uint8_t *));
            else
               cheat_manager_state.memory_buf_list = (uint8_t**)realloc(
                     cheat_manager_state.memory_buf_list, sizeof(uint8_t *) * cheat_manager_state.num_memory_buffers);

            if (!cheat_manager_state.memory_size_list)
               cheat_manager_state.memory_size_list = (unsigned*)calloc(1, sizeof(unsigned));
            else
            {
               unsigned *val = (unsigned*)realloc(
                     cheat_manager_state.memory_size_list,
                     sizeof(unsigned) *
                     cheat_manager_state.num_memory_buffers);

               if (val)
                  cheat_manager_state.memory_size_list = val;
            }

            cheat_manager_state.memory_buf_list[cheat_manager_state.num_memory_buffers - 1] = (uint8_t*)system->mmaps.descriptors[i].core.ptr;
            cheat_manager_state.memory_size_list[cheat_manager_state.num_memory_buffers - 1] = (unsigned)system->mmaps.descriptors[i].core.len;
            cheat_manager_state.total_memory_size += system->mmaps.descriptors[i].core.len;

            if (!cheat_manager_state.curr_memory_buf)
               cheat_manager_state.curr_memory_buf = (uint8_t*)system->mmaps.descriptors[i].core.ptr;
         }
      }
   }

   if (cheat_manager_state.num_memory_buffers == 0)
   {
      meminfo.id = RETRO_MEMORY_SYSTEM_RAM;
      if (!core_get_memory(&meminfo))
      {
         runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_INIT_FAIL), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         return 0;
      }

      if (meminfo.size == 0)
         return 0;

      cheat_manager_state.memory_buf_list = (uint8_t**)
            calloc(1, sizeof(uint8_t *));
      cheat_manager_state.memory_size_list = (unsigned*)
            calloc(1, sizeof(unsigned));
      cheat_manager_state.num_memory_buffers = 1;
      cheat_manager_state.memory_buf_list[0] = (uint8_t*)meminfo.data;
      cheat_manager_state.memory_size_list[0] = (unsigned)meminfo.size;
      cheat_manager_state.total_memory_size = (unsigned)meminfo.size;
      cheat_manager_state.curr_memory_buf = (uint8_t*)meminfo.data;

   }

   cheat_manager_state.num_matches = (cheat_manager_state.total_memory_size * 8) / ((int)pow(2, cheat_manager_state.search_bit_size));

   /* Ensure we're aligned on 4-byte boundary */
#if 0
   if (meminfo.size % 4 > 0)
      cheat_manager_state.total_memory_size = cheat_manager_state.total_memory_size + (4 - (meminfo.size % 4));
#endif

   if (is_search_initialization)
   {
      if (cheat_manager_state.prev_memory_buf)
      {
         free(cheat_manager_state.prev_memory_buf);
         cheat_manager_state.prev_memory_buf = NULL;
      }

      cheat_manager_state.prev_memory_buf = (uint8_t*)calloc(cheat_manager_state.total_memory_size, sizeof(uint8_t));
      if (!cheat_manager_state.prev_memory_buf)
      {
         runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_INIT_FAIL), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         return 0;
      }

      if (cheat_manager_state.matches)
      {
         free(cheat_manager_state.matches);
         cheat_manager_state.matches = NULL;
      }

      cheat_manager_state.matches = (uint8_t*)calloc(cheat_manager_state.total_memory_size, sizeof(uint8_t));
      if (!cheat_manager_state.matches)
      {
         free(cheat_manager_state.prev_memory_buf);
         cheat_manager_state.prev_memory_buf = NULL;
         runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_INIT_FAIL), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         return 0;
      }

      memset(cheat_manager_state.matches, 0xFF, cheat_manager_state.total_memory_size);

      offset = 0;

      for (i = 0; i < cheat_manager_state.num_memory_buffers; i++)
      {
         memcpy(cheat_manager_state.prev_memory_buf + offset, cheat_manager_state.memory_buf_list[i], cheat_manager_state.memory_size_list[i]);
         offset += cheat_manager_state.memory_size_list[i];
      }

      cheat_manager_state.memory_search_initialized = true;
   }

   cheat_manager_state.memory_initialized = true;

   runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_INIT_SUCCESS), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

#ifdef HAVE_MENU
   if (!wraparound)
   {
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   }
#endif

   return 0;
}

static unsigned translate_address(unsigned address, unsigned char **curr)
{
   unsigned offset = 0;
   unsigned i = 0;

   for (i = 0; i < cheat_manager_state.num_memory_buffers; i++)
   {
      if ((address >= offset) && (address < offset + cheat_manager_state.memory_size_list[i]))
      {
         *curr = cheat_manager_state.memory_buf_list[i];
         break;
      }
      else
         offset += cheat_manager_state.memory_size_list[i];
   }

   return offset;
}

static void cheat_manager_setup_search_meta(unsigned int bitsize, unsigned int *bytes_per_item, unsigned int *mask, unsigned int *bits)
{
   switch (bitsize)
   {
      case 0:
         *bytes_per_item = 1;
         *bits = 1;
         *mask = 0x01;
         break;
      case 1:
         *bytes_per_item = 1;
         *bits = 2;
         *mask = 0x03;
         break;
      case 2:
         *bytes_per_item = 1;
         *bits = 4;
         *mask = 0x0F;
         break;
      case 3:
         *bytes_per_item = 1;
         *bits = 8;
         *mask = 0xFF;
         break;
      case 4:
         *bytes_per_item = 2;
         *bits = 8;
         *mask = 0xFFFF;
         break;
      case 5:
         *bytes_per_item = 4;
         *bits = 8;
         *mask = 0xFFFFFFFF;
         break;
   }
}

static int cheat_manager_search(enum cheat_search_type search_type)
{
   char msg[100];
   unsigned char *curr         = cheat_manager_state.curr_memory_buf;
   unsigned char *prev         = cheat_manager_state.prev_memory_buf;
   unsigned int idx            = 0;
   unsigned int curr_val       = 0;
   unsigned int prev_val       = 0;
   unsigned int mask           = 0;
   unsigned int bytes_per_item = 1;
   unsigned int bits           = 8;
   unsigned int offset         = 0;
   unsigned int i              = 0;
   bool refresh                = false;

   if (cheat_manager_state.num_memory_buffers == 0)
   {
      runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_SEARCH_NOT_INITIALIZED), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      return 0;
   }

   cheat_manager_setup_search_meta(cheat_manager_state.search_bit_size, &bytes_per_item, &mask, &bits);

   /* little endian FF000000 = 256 */
   for (idx = 0; idx < cheat_manager_state.total_memory_size; idx = idx + bytes_per_item)
   {
      unsigned byte_part;

      offset = translate_address(idx, &curr);

      switch (bytes_per_item)
      {
         case 2:
            curr_val = cheat_manager_state.big_endian ?
               (*(curr + idx - offset) * 256) + *(curr + idx + 1 - offset) :
               *(curr + idx - offset) + (*(curr + idx + 1 - offset) * 256);
            prev_val = cheat_manager_state.big_endian ?
               (*(prev + idx) * 256) + *(prev + idx + 1) :
               *(prev + idx) + (*(prev + idx + 1) * 256);
            break;
         case 4:
            curr_val = cheat_manager_state.big_endian ?
               (*(curr + idx - offset) * 256 * 256 * 256) + (*(curr + idx + 1 - offset) * 256 * 256) + (*(curr + idx + 2 - offset) * 256) + *(curr + idx + 3 - offset) :
               *(curr + idx - offset) + (*(curr + idx + 1 - offset) * 256) + (*(curr + idx + 2 - offset) * 256 * 256) + (*(curr + idx + 3 - offset) * 256 * 256 * 256);
            prev_val = cheat_manager_state.big_endian ?
               (*(prev + idx) * 256 * 256 * 256) + (*(prev + idx + 1) * 256 * 256) + (*(prev + idx + 2) * 256) + *(prev + idx + 3) :
               *(prev + idx) + (*(prev + idx + 1) * 256) + (*(prev + idx + 2) * 256 * 256) + (*(prev + idx + 3) * 256 * 256 * 256);
            break;
         case 1:
         default:
            curr_val = *(curr - offset + idx);
            prev_val = *(prev + idx);
            break;
      }

      for (byte_part = 0; byte_part < 8 / bits; byte_part++)
      {
         unsigned int curr_subval = (curr_val >> (byte_part * bits)) & mask;
         unsigned int prev_subval = (prev_val >> (byte_part * bits)) & mask;
         unsigned int prev_match;

         if (bits < 8)
            prev_match = *(cheat_manager_state.matches + idx) & (mask << (byte_part * bits));
         else
            prev_match = *(cheat_manager_state.matches + idx);

         if (prev_match > 0)
         {
            bool match = false;
            switch (search_type)
            {
               case CHEAT_SEARCH_TYPE_EXACT:
                  match = (curr_subval == cheat_manager_state.search_exact_value);
                  break;
               case CHEAT_SEARCH_TYPE_LT:
                  match = (curr_subval < prev_subval);
                  break;
               case CHEAT_SEARCH_TYPE_GT:
                  match = (curr_subval > prev_subval);
                  break;
               case CHEAT_SEARCH_TYPE_LTE:
                  match = (curr_subval <= prev_subval);
                  break;
               case CHEAT_SEARCH_TYPE_GTE:
                  match = (curr_subval >= prev_subval);
                  break;
               case CHEAT_SEARCH_TYPE_EQ:
                  match = (curr_subval == prev_subval);
                  break;
               case CHEAT_SEARCH_TYPE_NEQ:
                  match = (curr_subval != prev_subval);
                  break;
               case CHEAT_SEARCH_TYPE_EQPLUS:
                  match = (curr_subval == prev_subval + cheat_manager_state.search_eqplus_value);
                  break;
               case CHEAT_SEARCH_TYPE_EQMINUS:
                  match = (curr_subval == prev_subval - cheat_manager_state.search_eqminus_value);
                  break;
            }

            if (!match)
            {
               if (bits < 8)
                  *(cheat_manager_state.matches + idx) = *(cheat_manager_state.matches + idx) &
                     ((~(mask << (byte_part * bits))) & 0xFF);
               else
                  memset(cheat_manager_state.matches + idx, 0, bytes_per_item);
               if (cheat_manager_state.num_matches > 0)
                  cheat_manager_state.num_matches--;
            }
         }
      }
   }

   offset = 0;

   for (i = 0; i < cheat_manager_state.num_memory_buffers; i++)
   {
      memcpy(cheat_manager_state.prev_memory_buf + offset, cheat_manager_state.memory_buf_list[i], cheat_manager_state.memory_size_list[i]);
      offset += cheat_manager_state.memory_size_list[i];
   }

   snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_CHEAT_SEARCH_FOUND_MATCHES), cheat_manager_state.num_matches);
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

#ifdef HAVE_MENU
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
#endif
   return 0;
}


int cheat_manager_search_exact(rarch_setting_t *setting, bool wraparound)
{
   return cheat_manager_search(CHEAT_SEARCH_TYPE_EXACT);
}

int cheat_manager_search_lt(rarch_setting_t *setting, bool wraparound)
{
   return cheat_manager_search(CHEAT_SEARCH_TYPE_LT);
}

int cheat_manager_search_gt(rarch_setting_t *setting, bool wraparound)
{
   return cheat_manager_search(CHEAT_SEARCH_TYPE_GT);
}

int cheat_manager_search_lte(rarch_setting_t *setting, bool wraparound)
{
   return cheat_manager_search(CHEAT_SEARCH_TYPE_LTE);
}

int cheat_manager_search_gte(rarch_setting_t *setting, bool wraparound)
{
   return cheat_manager_search(CHEAT_SEARCH_TYPE_GTE);
}

int cheat_manager_search_eq(rarch_setting_t *setting, bool wraparound)
{
   return cheat_manager_search(CHEAT_SEARCH_TYPE_EQ);
}

int cheat_manager_search_neq(rarch_setting_t *setting, bool wraparound)
{
   return cheat_manager_search(CHEAT_SEARCH_TYPE_NEQ);
}

int cheat_manager_search_eqplus(rarch_setting_t *setting, bool wraparound)
{
   return cheat_manager_search(CHEAT_SEARCH_TYPE_EQPLUS);
}

int cheat_manager_search_eqminus(rarch_setting_t *setting, bool wraparound)
{
   return cheat_manager_search(CHEAT_SEARCH_TYPE_EQMINUS);
}

bool cheat_manager_add_new_code(unsigned int memory_search_size, unsigned int address, unsigned int address_mask,
      bool big_endian, unsigned int value)
{
   int new_size = cheat_manager_get_size() + 1;

   if (!cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_RETRO))
      return false;

   cheat_manager_state.cheats[cheat_manager_state.size - 1].address = address;
   cheat_manager_state.cheats[cheat_manager_state.size - 1].address_mask = address_mask;
   cheat_manager_state.cheats[cheat_manager_state.size - 1].memory_search_size = memory_search_size;
   cheat_manager_state.cheats[cheat_manager_state.size - 1].value = value;
   cheat_manager_state.cheats[cheat_manager_state.size - 1].big_endian = big_endian;

   return true;
}
int cheat_manager_add_matches(const char *path,
      const char *label, unsigned type, size_t menuidx, size_t entry_idx)
{
   char msg[100];
   bool refresh = false;
   unsigned byte_part = 0;
   unsigned int idx = 0;
   unsigned int mask = 0;
   unsigned int bytes_per_item = 1;
   unsigned int bits = 8;
   unsigned int curr_val = 0;
   unsigned int num_added = 0;
   unsigned int offset = 0;
   unsigned char *curr = cheat_manager_state.curr_memory_buf;

   if (cheat_manager_state.num_matches + cheat_manager_state.size > 100)
   {
      runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      return 0;
   }
   cheat_manager_setup_search_meta(cheat_manager_state.search_bit_size, &bytes_per_item, &mask, &bits);

   for (idx = 0; idx < cheat_manager_state.total_memory_size; idx = idx + bytes_per_item)
   {
      offset = translate_address(idx, &curr);

      switch (bytes_per_item)
      {
      case 2:
         curr_val = cheat_manager_state.big_endian ?
               (*(curr + idx - offset) * 256) + *(curr + idx + 1 - offset) :
               *(curr + idx - offset) + (*(curr + idx + 1 - offset) * 256);
         break;
      case 4:
         curr_val = cheat_manager_state.big_endian ?
               (*(curr + idx - offset) * 256 * 256 * 256) + (*(curr + idx + 1 - offset) * 256 * 256) + (*(curr + idx + 2 - offset) * 256) + *(curr + idx + 3 - offset) :
               *(curr + idx - offset) + (*(curr + idx + 1 - offset) * 256) + (*(curr + idx + 2 - offset) * 256 * 256) + (*(curr + idx + 3 - offset) * 256 * 256 * 256);
         break;
      case 1:
      default:
         curr_val = *(curr - offset + idx);
         break;
      }
      for (byte_part = 0; byte_part < 8 / bits; byte_part++)
      {
         unsigned int prev_match;

         if (bits < 8)
         {
            prev_match = *(cheat_manager_state.matches + idx) & (mask << (byte_part * bits));
            if (prev_match)
            {
               if (!cheat_manager_add_new_code(cheat_manager_state.search_bit_size, idx, (mask << (byte_part * bits)),
                     cheat_manager_state.big_endian, curr_val))
               {
                  runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  return 0;
               }
               num_added++;
            }
         }
         else
         {
            prev_match = *(cheat_manager_state.matches + idx);
            if (prev_match)
            {
               if (!cheat_manager_add_new_code(cheat_manager_state.search_bit_size, idx, 0xFF,
                     cheat_manager_state.big_endian, curr_val))
               {
                  runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  return 0;
               }
               num_added++;
            }
         }

      }
   }

   snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS), cheat_manager_state.num_matches);
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

#ifdef HAVE_MENU
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
#endif

   return 0;
}

void cheat_manager_apply_rumble(struct item_cheat *cheat, unsigned int curr_value)
{
   bool rumble = false;

   switch (cheat->rumble_type)
   {
      case RUMBLE_TYPE_DISABLED:
         return;
      case RUMBLE_TYPE_CHANGES:
         rumble = (curr_value != cheat->rumble_prev_value);
         break;
      case RUMBLE_TYPE_DOES_NOT_CHANGE:
         rumble = (curr_value == cheat->rumble_prev_value);
         break;
      case RUMBLE_TYPE_INCREASE:
         rumble = (curr_value > cheat->rumble_prev_value);
         break;
      case RUMBLE_TYPE_DECREASE:
         rumble = (curr_value < cheat->rumble_prev_value);
         break;
      case RUMBLE_TYPE_EQ_VALUE:
         rumble = (curr_value == cheat->rumble_value);
         break;
      case RUMBLE_TYPE_NEQ_VALUE:
         rumble = (curr_value != cheat->rumble_value);
         break;
      case RUMBLE_TYPE_LT_VALUE:
         rumble = (curr_value < cheat->rumble_value);
         break;
      case RUMBLE_TYPE_GT_VALUE:
         rumble = (curr_value > cheat->rumble_value);
         break;
      case RUMBLE_TYPE_INCREASE_BY_VALUE:
         rumble = (curr_value == cheat->rumble_prev_value + cheat->rumble_value);
         break;
      case RUMBLE_TYPE_DECREASE_BY_VALUE:
         rumble = (curr_value == cheat->rumble_prev_value - cheat->rumble_value);
         break;
   }

   cheat->rumble_prev_value = curr_value;

   /* Give the emulator enough time
    * to initialize, load state, etc */
   if (cheat->rumble_initialized > 300)
   {
      if (rumble)
      {
         cheat->rumble_primary_end_time = cpu_features_get_time_usec() + (cheat->rumble_primary_duration * 1000);
         cheat->rumble_secondary_end_time = cpu_features_get_time_usec() + (cheat->rumble_secondary_duration * 1000);
         input_driver_set_rumble_state(cheat->rumble_port, RETRO_RUMBLE_STRONG, cheat->rumble_primary_strength);
         input_driver_set_rumble_state(cheat->rumble_port, RETRO_RUMBLE_WEAK, cheat->rumble_secondary_strength);
      }
   }
   else
   {
      cheat->rumble_initialized++;
      return;
   }

   if (cheat->rumble_primary_end_time <= cpu_features_get_time_usec())
   {
      if (cheat->rumble_primary_end_time != 0)
         input_driver_set_rumble_state(cheat->rumble_port, RETRO_RUMBLE_STRONG, 0);
      cheat->rumble_primary_end_time = 0;
   }
   else
   {
      input_driver_set_rumble_state(cheat->rumble_port, RETRO_RUMBLE_STRONG, cheat->rumble_primary_strength);
   }

   if (cheat->rumble_secondary_end_time <= cpu_features_get_time_usec())
   {
      if (cheat->rumble_secondary_end_time != 0)
         input_driver_set_rumble_state(cheat->rumble_port, RETRO_RUMBLE_WEAK, 0);
      cheat->rumble_secondary_end_time = 0;
   }
   else
      input_driver_set_rumble_state(cheat->rumble_port, RETRO_RUMBLE_WEAK, cheat->rumble_secondary_strength);
}

void cheat_manager_apply_retro_cheats(void)
{
   unsigned i;
   unsigned int offset;
   unsigned int mask           = 0;
   unsigned int bytes_per_item = 1;
   unsigned int bits           = 8;
   unsigned int curr_val       = 0;
   bool run_cheat              = true;

   if ((!cheat_manager_state.cheats))
      return;

   for (i = 0; i < cheat_manager_state.size; i++)
   {
      unsigned char *curr       = NULL;
      bool set_value            = false;
      unsigned int idx          = 0;
      unsigned int value_to_set = 0;
      unsigned int repeat_iter  = 0;
      unsigned int address_mask = cheat_manager_state.cheats[i].address_mask;

      if (cheat_manager_state.cheats[i].handler != CHEAT_HANDLER_TYPE_RETRO || !cheat_manager_state.cheats[i].state)
         continue;
      if (!cheat_manager_state.memory_initialized)
         cheat_manager_initialize_memory(NULL, false);

      /* If we're still not initialized, something
       * must have gone wrong - just bail */
      if (!cheat_manager_state.memory_initialized)
         return;

      if (!run_cheat)
      {
         run_cheat = true;
         continue;
      }
      cheat_manager_setup_search_meta(cheat_manager_state.cheats[i].memory_search_size, &bytes_per_item, &mask, &bits);

      curr   = cheat_manager_state.curr_memory_buf;
      idx    = cheat_manager_state.cheats[i].address;

      offset = translate_address(idx, &curr);

      switch (bytes_per_item)
      {
         case 2:
            curr_val = cheat_manager_state.big_endian ?
               (*(curr + idx - offset) * 256) + *(curr + idx + 1 - offset) :
               *(curr + idx - offset) + (*(curr + idx + 1 - offset) * 256);
            break;
         case 4:
            curr_val = cheat_manager_state.big_endian ?
               (*(curr + idx - offset) * 256 * 256 * 256) + (*(curr + idx + 1 - offset) * 256 * 256) + (*(curr + idx + 2 - offset) * 256) + *(curr + idx + 3 - offset) :
               *(curr + idx - offset) + (*(curr + idx + 1 - offset) * 256) + (*(curr + idx + 2 - offset) * 256 * 256) + (*(curr + idx + 3 - offset) * 256 * 256 * 256);
            break;
         case 1:
         default:
            curr_val = *(curr + idx - offset);
            break;
      }

      cheat_manager_apply_rumble(&cheat_manager_state.cheats[i], curr_val);

      switch (cheat_manager_state.cheats[i].cheat_type)
      {
         case CHEAT_TYPE_SET_TO_VALUE:
            set_value = true;
            value_to_set = cheat_manager_state.cheats[i].value;
            break;
         case CHEAT_TYPE_INCREASE_VALUE:
            set_value = true;
            value_to_set = curr_val + cheat_manager_state.cheats[i].value;
            break;
         case CHEAT_TYPE_DECREASE_VALUE:
            set_value = true;
            value_to_set = curr_val - cheat_manager_state.cheats[i].value;
            break;
         case CHEAT_TYPE_RUN_NEXT_IF_EQ:
            if (!(curr_val == cheat_manager_state.cheats[i].value))
               run_cheat = false;
            break;
         case CHEAT_TYPE_RUN_NEXT_IF_NEQ:
            if (!(curr_val != cheat_manager_state.cheats[i].value))
               run_cheat = false;
            break;
         case CHEAT_TYPE_RUN_NEXT_IF_LT:
            if (!(cheat_manager_state.cheats[i].value < curr_val))
               run_cheat = false;
            break;
         case CHEAT_TYPE_RUN_NEXT_IF_GT:
            if (!(cheat_manager_state.cheats[i].value > curr_val))
               run_cheat = false;
            break;
      }

      if (set_value)
      {
         for (repeat_iter = 1; repeat_iter <= cheat_manager_state.cheats[i].repeat_count; repeat_iter++)
         {
            switch (bytes_per_item)
            {
               case 2:
                  if (cheat_manager_state.cheats[i].big_endian)
                  {
                     *(curr + idx - offset) = (value_to_set >> 8) & 0xFF;
                     *(curr + idx + 1 - offset) = value_to_set & 0xFF;
                  }
                  else
                  {
                     *(curr + idx - offset) = value_to_set & 0xFF;
                     *(curr + idx + 1 - offset) = (value_to_set >> 8) & 0xFF;
                  }
                  break;
               case 4:
                  if (cheat_manager_state.cheats[i].big_endian)
                  {
                     *(curr + idx - offset) = (value_to_set >> 24) & 0xFF;
                     *(curr + idx + 1 - offset) = (value_to_set >> 16) & 0xFF;
                     *(curr + idx + 2 - offset) = (value_to_set >> 8) & 0xFF;
                     *(curr + idx + 3 - offset) = value_to_set & 0xFF;
                  }
                  else
                  {
                     *(curr + idx - offset) = value_to_set & 0xFF;
                     *(curr + idx + 1 - offset) = (value_to_set >> 8) & 0xFF;
                     *(curr + idx + 2 - offset) = (value_to_set >> 16) & 0xFF;
                     *(curr + idx + 3 - offset) = (value_to_set >> 24) & 0xFF;
                  }
                  break;
               case 1:
                  if (bits < 8)
                  {
                     unsigned bitpos;
                     unsigned char val = *(curr + idx - offset);

                     for (bitpos = 0; bitpos < 8; bitpos++)
                     {
                        if ((address_mask >> bitpos) & 0x01)
                        {
                           mask = (~(1 << bitpos) & 0xFF);
                           /* Clear current bit value */
                           val = val & mask;
                           /* Inject cheat bit value */
                           val = val | (((value_to_set >> bitpos) & 0x01) << bitpos);
                        }
                     }

                     *(curr + idx - offset) = val;
                  }
                  else
                     *(curr + idx - offset) = value_to_set & 0xFF;
                  break;
               default:
                  *(curr + idx - offset) = value_to_set & 0xFF;
                  break;
            }

            value_to_set += cheat_manager_state.cheats[i].repeat_add_to_value;

            if (mask != 0)
               value_to_set = value_to_set % mask;

            if (bits < 8)
            {
               unsigned int bit_iter;
               for (bit_iter = 0; bit_iter < cheat_manager_state.cheats[i].repeat_add_to_address; bit_iter++)
               {
                  address_mask = (address_mask << mask) & 0xFF;

                  if (address_mask == 0)
                  {
                     address_mask = mask;
                     idx++;
                  }
               }
            }
            else
               idx += (cheat_manager_state.cheats[i].repeat_add_to_address * bytes_per_item);

            idx = idx % cheat_manager_state.total_memory_size;

            offset = translate_address(idx, &curr);
         }
      }
   }
}

void cheat_manager_match_action(enum cheat_match_action_type match_action, unsigned int target_match_idx, unsigned int *address, unsigned int *address_mask,
      unsigned int *prev_value, unsigned int *curr_value)
{
   unsigned int byte_part;
   unsigned int idx;
   unsigned int start_idx;
   unsigned int mask = 0;
   unsigned int bytes_per_item = 1;
   unsigned int bits = 8;
   unsigned int curr_val = 0;
   unsigned int prev_val = 0;
   unsigned int offset = 0;
   unsigned char *curr = cheat_manager_state.curr_memory_buf;
   unsigned char *prev = cheat_manager_state.prev_memory_buf;
   unsigned int curr_match_idx = 0;

   if (target_match_idx > cheat_manager_state.num_matches - 1)
      return;

   if (cheat_manager_state.num_memory_buffers == 0)
      return;

   cheat_manager_setup_search_meta(cheat_manager_state.search_bit_size, &bytes_per_item, &mask, &bits);

   if (match_action == CHEAT_MATCH_ACTION_TYPE_BROWSE)
      start_idx = *address;
   else
      start_idx = 0;

   for (idx = start_idx; idx < cheat_manager_state.total_memory_size; idx = idx + bytes_per_item)
   {
      offset = translate_address(idx, &curr);

      switch (bytes_per_item)
      {
      case 2:
         curr_val = cheat_manager_state.big_endian ?
               (*(curr + idx - offset) * 256) + *(curr + idx + 1 - offset) :
               *(curr + idx - offset) + (*(curr + idx + 1 - offset) * 256);
         if (prev)
            prev_val = cheat_manager_state.big_endian ?
                  (*(prev + idx) * 256) + *(prev + idx + 1) :
                  *(prev + idx) + (*(prev + idx + 1) * 256);
         break;
      case 4:
         curr_val = cheat_manager_state.big_endian ?
               (*(curr + idx - offset) * 256 * 256 * 256) + (*(curr + idx + 1 - offset) * 256 * 256) + (*(curr + idx + 2 - offset) * 256) + *(curr + idx + 3 - offset) :
               *(curr + idx - offset) + (*(curr + idx + 1 - offset) * 256) + (*(curr + idx + 2 - offset) * 256 * 256) + (*(curr + idx + 3 - offset) * 256 * 256 * 256);
         if (prev)
            prev_val = cheat_manager_state.big_endian ?
                  (*(prev + idx) * 256 * 256 * 256) + (*(prev + idx + 1) * 256 * 256) + (*(prev + idx + 2) * 256) + *(prev + idx + 3) :
                  *(prev + idx) + (*(prev + idx + 1) * 256) + (*(prev + idx + 2) * 256 * 256) + (*(prev + idx + 3) * 256 * 256 * 256);
         break;
      case 1:
      default:
         curr_val = *(curr + idx - offset);
         if (prev)
            prev_val = *(prev + idx);
         break;
      }

      if (match_action == CHEAT_MATCH_ACTION_TYPE_BROWSE)
      {
         *curr_value = curr_val;
         *prev_value = prev_val;
         return;
      }

      if (!prev)
         return;

      for (byte_part = 0; byte_part < 8 / bits; byte_part++)
      {
         unsigned int prev_match;

         if (bits < 8)
         {
            prev_match = *(cheat_manager_state.matches + idx) & (mask << (byte_part * bits));
            if (prev_match)
            {
               if (target_match_idx == curr_match_idx)
               {
                  switch (match_action)
                  {
                  case CHEAT_MATCH_ACTION_TYPE_BROWSE:
                     return;
                  case CHEAT_MATCH_ACTION_TYPE_VIEW:
                     *address = idx;
                     *address_mask = (mask << (byte_part * bits));
                     *curr_value = curr_val;
                     *prev_value = prev_val;
                     return;
                  case CHEAT_MATCH_ACTION_TYPE_COPY:
                     if (!cheat_manager_add_new_code(cheat_manager_state.search_bit_size, idx, (mask << (byte_part * bits)),
                           cheat_manager_state.big_endian, curr_val))
                        runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_SEARCH_ADD_MATCH_FAIL), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                     else
                        runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                     return;
                  case CHEAT_MATCH_ACTION_TYPE_DELETE:
                     if (bits < 8)
                        *(cheat_manager_state.matches + idx) = *(cheat_manager_state.matches + idx) &
                              ((~(mask << (byte_part * bits))) & 0xFF);
                     else
                        memset(cheat_manager_state.matches + idx, 0, bytes_per_item);
                     if (cheat_manager_state.num_matches > 0)
                        cheat_manager_state.num_matches--;
                     runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                     return;
                  }
                  return;
               }
               curr_match_idx++;
            }
         }
         else
         {
            prev_match = *(cheat_manager_state.matches + idx);
            if (prev_match)
            {
               if (target_match_idx == curr_match_idx)
               {
                  switch (match_action)
                  {
                  case CHEAT_MATCH_ACTION_TYPE_BROWSE:
                     return;
                  case CHEAT_MATCH_ACTION_TYPE_VIEW:
                     *address = idx;
                     *address_mask = 0xFF;
                     *curr_value = curr_val;
                     *prev_value = prev_val;
                     return;
                  case CHEAT_MATCH_ACTION_TYPE_COPY:
                     if (!cheat_manager_add_new_code(cheat_manager_state.search_bit_size, idx, 0xFF,
                           cheat_manager_state.big_endian, curr_val))
                        runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_SEARCH_ADD_MATCH_FAIL), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                     else
                        runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                     return;
                  case CHEAT_MATCH_ACTION_TYPE_DELETE:
                     if (bits < 8)
                        *(cheat_manager_state.matches + idx) = *(cheat_manager_state.matches + idx) &
                              ((~(mask << (byte_part * bits))) & 0xFF);
                     else
                        memset(cheat_manager_state.matches + idx, 0, bytes_per_item);
                     if (cheat_manager_state.num_matches > 0)
                        cheat_manager_state.num_matches--;
                     runloop_msg_queue_push(msg_hash_to_str(MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                     return;
                  }
               }

               curr_match_idx++;
            }
         }
      }
   }

}
int cheat_manager_copy_match(rarch_setting_t *setting, bool wraparound)
{
   cheat_manager_match_action(CHEAT_MATCH_ACTION_TYPE_COPY,
         cheat_manager_state.match_idx, NULL, NULL, NULL, NULL);
   return 0;
}

int cheat_manager_delete_match(rarch_setting_t *setting, bool wraparound)
{
   bool refresh = false;
   cheat_manager_match_action(CHEAT_MATCH_ACTION_TYPE_DELETE,
         cheat_manager_state.match_idx, NULL, NULL, NULL, NULL);
#ifdef HAVE_MENU
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
#endif
   return 0;
}
