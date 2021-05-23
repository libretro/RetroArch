/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011      - Daniel De Matteis
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

#include <stdint.h>

#include <string/stdstring.h>

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "configuration.h"
#include "content.h"
#include "file_path_special.h"
#include "paths.h"
#include "retroarch.h"
#include "verbosity.h"

/**
 * runloop_game_specific_options:
 *
 * Returns: true (1) if a game specific core
 * options path has been found,
 * otherwise false (0).
 **/
static bool runloop_game_specific_options(char **output)
{
   char game_options_path[PATH_MAX_LENGTH];
   game_options_path[0] ='\0';

   if (!retroarch_validate_game_options(
            game_options_path,
            sizeof(game_options_path), false) ||
       !path_is_valid(game_options_path))
      return false;

   RARCH_LOG("%s %s\n",
         msg_hash_to_str(MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT),
         game_options_path);
   *output = strdup(game_options_path);
   return true;
}


/**
 * runloop_folder_specific_options:
 *
 * Returns: true (1) if a folder specific core
 * options path has been found,
 * otherwise false (0).
 **/
static bool runloop_folder_specific_options(char **output)
{
   char folder_options_path[PATH_MAX_LENGTH];
   folder_options_path[0] ='\0';

   if (!retroarch_validate_folder_options(
            folder_options_path,
            sizeof(folder_options_path), false) ||
       !path_is_valid(folder_options_path))
      return false;

   RARCH_LOG("%s %s\n",
         msg_hash_to_str(MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT),
         folder_options_path);
   *output = strdup(folder_options_path);
   return true;
}

static const char *core_option_manager_parse_value_label(
      const char *value, const char *value_label)
{
   /* 'value_label' may be NULL */
   const char *label = string_is_empty(value_label) ?
         value : value_label;

   if (string_is_empty(label))
      return NULL;

   /* Any label starting with a digit (or +/-)
    * cannot be a boolean string, and requires
    * no further processing */
   if (ISDIGIT((unsigned char)*label) ||
       (*label == '+') ||
       (*label == '-'))
      return label;

   /* Core devs have a habit of using arbitrary
    * strings to label boolean values (i.e. enabled,
    * Enabled, on, On, ON, true, True, TRUE, disabled,
    * Disabled, off, Off, OFF, false, False, FALSE).
    * These should all be converted to standard ON/OFF
    * strings
    * > Note: We require some duplication here
    *   (e.g. MENU_ENUM_LABEL_ENABLED *and*
    *    MENU_ENUM_LABEL_VALUE_ENABLED) in order
    *   to match both localised and non-localised
    *   strings. This function is not performance
    *   critical, so these extra comparisons do
    *   no harm */
   if (string_is_equal_noncase(label, msg_hash_to_str(MENU_ENUM_LABEL_ENABLED)) ||
       string_is_equal_noncase(label, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ENABLED)) ||
       string_is_equal_noncase(label, "enable") ||
       string_is_equal_noncase(label, "on") ||
       string_is_equal_noncase(label, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON)) ||
       string_is_equal_noncase(label, "true") ||
       string_is_equal_noncase(label, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TRUE)))
      label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON);
   else if (string_is_equal_noncase(label, msg_hash_to_str(MENU_ENUM_LABEL_DISABLED)) ||
            string_is_equal_noncase(label, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED)) ||
            string_is_equal_noncase(label, "disable") ||
            string_is_equal_noncase(label, "off") ||
            string_is_equal_noncase(label, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)) ||
            string_is_equal_noncase(label, "false") ||
            string_is_equal_noncase(label, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FALSE)))
      label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF);

   return label;
}

static bool core_option_manager_parse_variable(
      core_option_manager_t *opt, size_t idx,
      const struct retro_variable *var,
      config_file_t *config_src)
{
   size_t i;
   union string_list_elem_attr attr;
   const char *val_start      = NULL;
   char *value                = NULL;
   char *desc_end             = NULL;
   struct core_option *option = (struct core_option*)&opt->opts[idx];
   struct config_entry_list
      *entry                  = NULL;

   /* All options are visible by default */
   option->visible            = true;

   if (!string_is_empty(var->key))
      option->key             = strdup(var->key);
   if (!string_is_empty(var->value))
      value                   = strdup(var->value);

   if (!string_is_empty(value))
      desc_end                = strstr(value, "; ");

   if (!desc_end)
      goto error;

   *desc_end    = '\0';

   if (!string_is_empty(value))
      option->desc    = strdup(value);

   val_start          = desc_end + 2;
   option->vals       = string_split(val_start, "|");

   if (!option->vals)
      goto error;

   /* Legacy core option interface has no concept
    * of value labels
    * > Use actual values for display purposes */
   attr.i             = 0;
   option->val_labels = string_list_new();

   if (!option->val_labels)
      goto error;

   /* > Loop over values and 'extract' labels */
   for (i = 0; i < option->vals->size; i++)
   {
      const char *value       = option->vals->elems[i].data;
      const char *value_label = core_option_manager_parse_value_label(
            value, NULL);

      /* Redundant safely check... */
      value_label = string_is_empty(value_label) ?
            value : value_label;

      /* Append value label string */
      string_list_append(option->val_labels, value_label, attr);
   }

   /* Legacy core option interface always uses first
    * defined value as the default */
   option->default_index = 0;
   option->index         = 0;

   if (config_src)
      entry              = config_get_entry(config_src, option->key);
   else
      entry              = config_get_entry(opt->conf,  option->key);

   /* Set current config value */
   if (entry && !string_is_empty(entry->value))
   {
      for (i = 0; i < option->vals->size; i++)
      {
         if (string_is_equal(option->vals->elems[i].data, entry->value))
         {
            option->index = i;
            break;
         }
      }
   }

   free(value);

   return true;

error:
   free(value);
   return false;
}


static bool core_option_manager_parse_option(
      core_option_manager_t *opt, size_t idx,
      const struct retro_core_option_definition *option_def,
      config_file_t *config_src)
{
   size_t i;
   union string_list_elem_attr attr;
   struct config_entry_list
      *entry                  = NULL;
   size_t num_vals            = 0;
   struct core_option *option = (struct core_option*)&opt->opts[idx];
   const struct retro_core_option_value
      *values                 = option_def->values;

   /* All options are visible by default */
   option->visible            = true;

   if (!string_is_empty(option_def->key))
      option->key             = strdup(option_def->key);

   if (!string_is_empty(option_def->desc))
      option->desc            = strdup(option_def->desc);

   if (!string_is_empty(option_def->info))
      option->info            = strdup(option_def->info);

   /* Get number of values */
   for (;;)
   {
      if (string_is_empty(values[num_vals].value))
         break;
      num_vals++;
   }

   if (num_vals < 1)
      return false;

   /* Initialise string lists */
   attr.i             = 0;
   option->vals       = string_list_new();
   option->val_labels = string_list_new();

   if (!option->vals || !option->val_labels)
      return false;

   /* Initialise default value */
   option->default_index = 0;
   option->index         = 0;

   /* Extract value/label pairs */
   for (i = 0; i < num_vals; i++)
   {
      const char *value       = values[i].value;
      const char *value_label = values[i].label;

      /* Append value string
       * > We know that 'value' is always valid */
      string_list_append(option->vals, value, attr);

      /* Value label requires additional processing */
      value_label = core_option_manager_parse_value_label(
            value, value_label);

      /* > Redundant safely check... */
      value_label = string_is_empty(value_label) ?
            value : value_label;

      /* Append value label string */
      string_list_append(option->val_labels, value_label, attr);

      /* Check whether this value is the default setting */
      if (!string_is_empty(option_def->default_value))
      {
         if (string_is_equal(option_def->default_value, value))
         {
            option->default_index = i;
            option->index         = i;
         }
      }
   }

   if (config_src)
      entry              = config_get_entry(config_src, option->key);
   else
      entry              = config_get_entry(opt->conf,  option->key);

   /* Set current config value */
   if (entry && !string_is_empty(entry->value))
   {
      for (i = 0; i < option->vals->size; i++)
      {
         if (string_is_equal(option->vals->elems[i].data, entry->value))
         {
            option->index = i;
            break;
         }
      }
   }

   return true;
}


/**
 * core_option_manager_free:
 * @opt              : options manager handle
 *
 * Frees core option manager handle.
 **/
static void core_option_manager_free(core_option_manager_t *opt)
{
   size_t i;

   if (!opt)
      return;

   for (i = 0; i < opt->size; i++)
   {
      if (opt->opts[i].desc)
         free(opt->opts[i].desc);
      if (opt->opts[i].info)
         free(opt->opts[i].info);
      if (opt->opts[i].key)
         free(opt->opts[i].key);

      if (opt->opts[i].vals)
         string_list_free(opt->opts[i].vals);
      if (opt->opts[i].val_labels)
         string_list_free(opt->opts[i].val_labels);

      opt->opts[i].desc = NULL;
      opt->opts[i].info = NULL;
      opt->opts[i].key  = NULL;
      opt->opts[i].vals = NULL;
   }

   if (opt->conf)
      config_file_free(opt->conf);
   free(opt->opts);
   free(opt);
}


/**
 * core_option_manager_new:
 * @conf_path        : Filesystem path to write core option config file to.
 * @src_conf_path    : Filesystem path from which to load initial config settings.
 * @option_defs      : Pointer to variable array handle.
 *
 * Creates and initializes a core manager handle.
 *
 * Returns: handle to new core manager handle, otherwise NULL.
 **/
static core_option_manager_t *core_option_manager_new(
      const char *conf_path, const char *src_conf_path,
      const struct retro_core_option_definition *option_defs)
{
   const struct retro_core_option_definition *option_def;
   size_t size                       = 0;
   config_file_t *config_src         = NULL;
   core_option_manager_t *opt        = (core_option_manager_t*)
      malloc(sizeof(*opt));

   if (!opt)
      return NULL;

   opt->conf                         = NULL;
   opt->conf_path[0]                 = '\0';
   opt->opts                         = NULL;
   opt->size                         = 0;
   opt->updated                      = false;

   if (!string_is_empty(conf_path))
      if (!(opt->conf = config_file_new_from_path_to_string(conf_path)))
         if (!(opt->conf = config_file_new_alloc()))
            goto error;

   strlcpy(opt->conf_path, conf_path, sizeof(opt->conf_path));

   /* Load source config file, if required */
   if (!string_is_empty(src_conf_path))
      config_src = config_file_new_from_path_to_string(src_conf_path);

   /* Note: 'option_def->info == NULL' is valid */
   for (option_def = option_defs;
        option_def->key && option_def->desc && option_def->values[0].value;
        option_def++)
      size++;

   if (size == 0)
      goto error;

   opt->opts = (struct core_option*)calloc(size, sizeof(*opt->opts));
   if (!opt->opts)
      goto error;

   opt->size = size;
   size      = 0;

   /* Note: 'option_def->info == NULL' is valid */
   for (option_def = option_defs;
        option_def->key && option_def->desc && option_def->values[0].value;
        size++, option_def++)
      if (!core_option_manager_parse_option(opt, size, option_def, config_src))
         goto error;

   if (config_src)
      config_file_free(config_src);

   return opt;

error:
   if (config_src)
      config_file_free(config_src);
   core_option_manager_free(opt);
   return NULL;
}


/**
 * core_option_manager_new_vars:
 * @conf_path        : Filesystem path to write core option config file to.
 * @src_conf_path    : Filesystem path from which to load initial config settings.
 * @vars             : Pointer to variable array handle.
 *
 * Legacy version of core_option_manager_new().
 * Creates and initializes a core manager handle.
 *
 * Returns: handle to new core manager handle, otherwise NULL.
 **/
static core_option_manager_t *core_option_manager_new_vars(
      const char *conf_path, const char *src_conf_path,
      const struct retro_variable *vars)
{
   const struct retro_variable *var;
   size_t size                       = 0;
   config_file_t *config_src         = NULL;
   core_option_manager_t *opt        = (core_option_manager_t*)
      malloc(sizeof(*opt));

   if (!opt)
      return NULL;

   opt->conf                         = NULL;
   opt->conf_path[0]                 = '\0';
   opt->opts                         = NULL;
   opt->size                         = 0;
   opt->updated                      = false;

   if (!string_is_empty(conf_path))
      if (!(opt->conf = config_file_new_from_path_to_string(conf_path)))
         if (!(opt->conf = config_file_new_alloc()))
            goto error;

   strlcpy(opt->conf_path, conf_path, sizeof(opt->conf_path));

   /* Load source config file, if required */
   if (!string_is_empty(src_conf_path))
      config_src = config_file_new_from_path_to_string(src_conf_path);

   for (var = vars; var->key && var->value; var++)
      size++;

   if (size == 0)
      goto error;

   opt->opts = (struct core_option*)calloc(size, sizeof(*opt->opts));
   if (!opt->opts)
      goto error;

   opt->size = size;
   size      = 0;

   for (var = vars; var->key && var->value; size++, var++)
   {
      if (!core_option_manager_parse_variable(opt, size, var, config_src))
         goto error;
   }

   if (config_src)
      config_file_free(config_src);

   return opt;

error:
   if (config_src)
      config_file_free(config_src);
   core_option_manager_free(opt);
   return NULL;
}

#ifdef HAVE_BSV_MOVIE
/* BSV MOVIE */
static bool bsv_movie_init_playback(
      bsv_movie_t *handle, const char *path)
{
   uint32_t state_size       = 0;
   uint32_t content_crc      = 0;
   uint32_t header[4]        = {0};
   intfstream_t *file        = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open BSV file for playback, path : \"%s\".\n", path);
      return false;
   }

   handle->file              = file;
   handle->playback          = true;

   intfstream_read(handle->file, header, sizeof(uint32_t) * 4);
   /* Compatibility with old implementation that
    * used incorrect documentation. */
   if (swap_if_little32(header[MAGIC_INDEX]) != BSV_MAGIC
         && swap_if_big32(header[MAGIC_INDEX]) != BSV_MAGIC)
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE));
      return false;
   }

   content_crc               = content_get_crc();

   if (content_crc != 0)
      if (swap_if_big32(header[CRC_INDEX]) != content_crc)
         RARCH_WARN("%s.\n", msg_hash_to_str(MSG_CRC32_CHECKSUM_MISMATCH));

   state_size = swap_if_big32(header[STATE_SIZE_INDEX]);

#if 0
   RARCH_ERR("----- debug %u -----\n", header[0]);
   RARCH_ERR("----- debug %u -----\n", header[1]);
   RARCH_ERR("----- debug %u -----\n", header[2]);
   RARCH_ERR("----- debug %u -----\n", header[3]);
#endif

   if (state_size)
   {
      retro_ctx_size_info_t info;
      retro_ctx_serialize_info_t serial_info;
      uint8_t *buf       = (uint8_t*)malloc(state_size);

      if (!buf)
         return false;

      handle->state      = buf;
      handle->state_size = state_size;
      if (intfstream_read(handle->file,
               handle->state, state_size) != state_size)
      {
         RARCH_ERR("%s\n", msg_hash_to_str(MSG_COULD_NOT_READ_STATE_FROM_MOVIE));
         return false;
      }

      core_serialize_size( &info);

      if (info.size == state_size)
      {
         serial_info.data_const = handle->state;
         serial_info.size       = state_size;
         core_unserialize(&serial_info);
      }
      else
         RARCH_WARN("%s\n",
               msg_hash_to_str(MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION));
   }

   handle->min_file_pos = sizeof(header) + state_size;

   return true;
}

static bool bsv_movie_init_record(
      bsv_movie_t *handle, const char *path)
{
   retro_ctx_size_info_t info;
   uint32_t state_size       = 0;
   uint32_t content_crc      = 0;
   uint32_t header[4]        = {0};
   intfstream_t *file        = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open BSV file for recording, path : \"%s\".\n", path);
      return false;
   }

   handle->file             = file;

   content_crc              = content_get_crc();

   /* This value is supposed to show up as
    * BSV1 in a HEX editor, big-endian. */
   header[MAGIC_INDEX]      = swap_if_little32(BSV_MAGIC);
   header[CRC_INDEX]        = swap_if_big32(content_crc);

   core_serialize_size(&info);

   state_size               = (unsigned)info.size;

   header[STATE_SIZE_INDEX] = swap_if_big32(state_size);

   intfstream_write(handle->file, header, 4 * sizeof(uint32_t));

   handle->min_file_pos     = sizeof(header) + state_size;
   handle->state_size       = state_size;

   if (state_size)
   {
      retro_ctx_serialize_info_t serial_info;
      uint8_t *st      = (uint8_t*)malloc(state_size);

      if (!st)
         return false;

      handle->state    = st;

      serial_info.data = handle->state;
      serial_info.size = state_size;

      core_serialize(&serial_info);

      intfstream_write(handle->file,
            handle->state, state_size);
   }

   return true;
}

static void bsv_movie_free(bsv_movie_t *handle)
{
   if (!handle)
      return;

   intfstream_close(handle->file);
   free(handle->file);

   free(handle->state);
   free(handle->frame_pos);
   free(handle);
}

static bsv_movie_t *bsv_movie_init_internal(const char *path,
      enum rarch_movie_type type)
{
   size_t *frame_pos   = NULL;
   bsv_movie_t *handle = (bsv_movie_t*)calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   if (type == RARCH_MOVIE_PLAYBACK)
   {
      if (!bsv_movie_init_playback(handle, path))
         goto error;
   }
   else if (!bsv_movie_init_record(handle, path))
      goto error;

   /* Just pick something really large
    * ~1 million frames rewind should do the trick. */
   if (!(frame_pos = (size_t*)calloc((1 << 20), sizeof(size_t))))
      goto error;

   handle->frame_pos       = frame_pos;

   handle->frame_pos[0]    = handle->min_file_pos;
   handle->frame_mask      = (1 << 20) - 1;

   return handle;

error:
   bsv_movie_free(handle);
   return NULL;
}

bool bsv_movie_init(runloop_state_t *p_runloop)
{
   if (p_runloop->bsv_movie_state.movie_start_playback)
   {
      if (!(p_runloop->bsv_movie_state_handle = bsv_movie_init_internal(
               p_runloop->bsv_movie_state.movie_start_path,
                  RARCH_MOVIE_PLAYBACK)))
      {
         RARCH_ERR("%s: \"%s\".\n",
               msg_hash_to_str(MSG_FAILED_TO_LOAD_MOVIE_FILE),
               p_runloop->bsv_movie_state.movie_start_path);
         return false;
      }

      p_runloop->bsv_movie_state.movie_playback = true;
      runloop_msg_queue_push(msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK),
            2, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("%s.\n", msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK));

      return true;
   }
   else if (p_runloop->bsv_movie_state.movie_start_recording)
   {
      char msg[8192];

      if (!(p_runloop->bsv_movie_state_handle = bsv_movie_init_internal(
               p_runloop->bsv_movie_state.movie_start_path,
               RARCH_MOVIE_RECORD)))
      {
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD),
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_ERR("%s.\n",
               msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD));
         return false;
      }

      snprintf(msg, sizeof(msg),
            "%s \"%s\".",
            msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
            p_runloop->bsv_movie_state.movie_start_path);

      runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("%s \"%s\".\n",
            msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
            p_runloop->bsv_movie_state.movie_start_path);

      return true;
   }

   return false;
}

void bsv_movie_deinit(runloop_state_t *p_runloop)
{
   if (p_runloop->bsv_movie_state_handle)
      bsv_movie_free(p_runloop->bsv_movie_state_handle);
   p_runloop->bsv_movie_state_handle = NULL;
}

static bool runloop_check_movie_init(runloop_state_t *p_runloop,
      settings_t *settings)
{
   char msg[16384], path[8192];
   int state_slot              = settings->ints.state_slot;

   msg[0] = path[0]            = '\0';

   configuration_set_uint(settings, settings->uints.rewind_granularity, 1);

   if (state_slot > 0)
      snprintf(path, sizeof(path), "%s%d.bsv",
            p_runloop->bsv_movie_state.movie_path,
            state_slot);
   else
      snprintf(path, sizeof(path), "%s.bsv",
            p_runloop->bsv_movie_state.movie_path);

   snprintf(msg, sizeof(msg), "%s \"%s\".",
         msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
         path);

   if (!(p_runloop->bsv_movie_state_handle = bsv_movie_init_internal(
         path, RARCH_MOVIE_RECORD)))
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD),
            2, 180, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_ERR("%s\n",
            msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD));
      return false;
   }

   runloop_msg_queue_push(msg, 2, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s \"%s\".\n",
         msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
         path);

   return true;
}

bool bsv_movie_check(
      runloop_state_t *p_runloop,
      settings_t *settings)
{
   if (!p_runloop->bsv_movie_state_handle)
      return runloop_check_movie_init(p_runloop, settings);

   if (p_runloop->bsv_movie_state.movie_playback)
   {
      /* Checks if movie is being played back. */
      if (!p_runloop->bsv_movie_state.movie_end)
         return false;
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED), 2, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED));

      p_runloop->bsv_movie_state.movie_end      = false;
      p_runloop->bsv_movie_state.movie_playback = false;

      goto end;
   }

   /* Checks if movie is being recorded. */
   if (!p_runloop->bsv_movie_state_handle)
      return false;

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED), 2, 180, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED));

end:
   bsv_movie_deinit(p_runloop);
   return true;
}
#endif


/* Fetches core options path for current core/content
 * - path: path from which options should be read
 *   from/saved to
 * - src_path: in the event that 'path' file does not
 *   yet exist, provides source path from which initial
 *   options should be extracted
 *
 *   NOTE: caller must ensure 
 *   path and src_path are NULL-terminated
 *  */
void runloop_init_core_options_path(
      runloop_state_t *p_runloop,
      settings_t *settings,
      char *path, size_t len,
      char *src_path, size_t src_len)
{
   char *game_options_path        = NULL;
   char *folder_options_path      = NULL;
   bool game_specific_options     = settings->bools.game_specific_options;

   /* Check whether game-specific options exist */
   if (game_specific_options &&
       runloop_game_specific_options(&game_options_path))
   {
      /* Notify system that we have a valid core options
       * override */
      path_set(RARCH_PATH_CORE_OPTIONS, game_options_path);
      p_runloop->game_options_active   = true;
      p_runloop->folder_options_active = false;

      /* Copy options path */
      strlcpy(path, game_options_path, len);

      free(game_options_path);
   }
   /* Check whether folder-specific options exist */
   else if (game_specific_options &&
            runloop_folder_specific_options(&folder_options_path))
   {
      /* Notify system that we have a valid core options
       * override */
      path_set(RARCH_PATH_CORE_OPTIONS, folder_options_path);
      p_runloop->game_options_active   = false;
      p_runloop->folder_options_active = true;

      /* Copy options path */
      strlcpy(path, folder_options_path, len);

      free(folder_options_path);
   }
   else
   {
      char global_options_path[PATH_MAX_LENGTH];
      char per_core_options_path[PATH_MAX_LENGTH];
      bool per_core_options_exist   = false;
      bool per_core_options         = !settings->bools.global_core_options;
      const char *path_core_options = settings->paths.path_core_options;

      global_options_path[0]        = '\0';
      per_core_options_path[0]      = '\0';

      if (per_core_options)
      {
         const char *core_name      = p_runloop->system.info.library_name;
         /* Get core-specific options path
          * > if retroarch_validate_per_core_options() returns
          *   false, then per-core options are disabled (due to
          *   unknown system errors...) */
         per_core_options = retroarch_validate_per_core_options(
               per_core_options_path, sizeof(per_core_options_path), true,
               core_name, core_name);

         /* If we can use per-core options, check whether an options
          * file already exists */
         if (per_core_options)
            per_core_options_exist = path_is_valid(per_core_options_path);
      }

      /* If not using per-core options, or if a per-core options
       * file does not yet exist, must fetch 'global' options path */
      if (!per_core_options || !per_core_options_exist)
      {
         const char *options_path   = path_core_options;

         if (!string_is_empty(options_path))
            strlcpy(global_options_path,
                  options_path, sizeof(global_options_path));
         else if (!path_is_empty(RARCH_PATH_CONFIG))
            fill_pathname_resolve_relative(
                  global_options_path, path_get(RARCH_PATH_CONFIG),
                  FILE_PATH_CORE_OPTIONS_CONFIG, sizeof(global_options_path));
      }

      /* Allocate correct path/src_path strings */
      if (per_core_options)
      {
         strlcpy(path, per_core_options_path, len);

         if (!per_core_options_exist)
            strlcpy(src_path, global_options_path, src_len);
      }
      else
         strlcpy(path, global_options_path, len);

      /* Notify system that we *do not* have a valid core options
       * options override */
      p_runloop->game_options_active   = false;
      p_runloop->folder_options_active = false;
   }
}


core_option_manager_t *runloop_init_core_variables(
      runloop_state_t *p_runloop,
      settings_t *settings,
      const struct retro_variable *vars)
{
   char options_path[PATH_MAX_LENGTH];
   char src_options_path[PATH_MAX_LENGTH];

   /* Ensure these are NULL-terminated */
   options_path[0]     = '\0';
   src_options_path[0] = '\0';

   /* Get core options file path */
   runloop_init_core_options_path(
         p_runloop,
         settings,
         options_path, sizeof(options_path),
         src_options_path, sizeof(src_options_path));

   if (!string_is_empty(options_path))
      return core_option_manager_new_vars(options_path, src_options_path, vars);
   return NULL;
}


void runloop_deinit_core_options(
      runloop_state_t *p_runloop,
      const char *path_core_options)
{
   /* Check whether game-specific options file is being used */
   if (!string_is_empty(path_core_options))
   {
      config_file_t *conf_tmp = NULL;

      /* We only need to save configuration settings for
       * the current core
       * > If game-specific options file exists, have
       *   to read it (to ensure file only gets written
       *   if config values change)
       * > Otherwise, create a new, empty config_file_t
       *   object */
      if (path_is_valid(path_core_options))
         conf_tmp = config_file_new_from_path_to_string(path_core_options);

      if (!conf_tmp)
         conf_tmp = config_file_new_alloc();

      if (conf_tmp)
      {
         core_option_manager_flush(
               conf_tmp,
               p_runloop->core_options);
         RARCH_LOG("[Core Options]: Saved %s-specific core options to \"%s\"\n",
               p_runloop->game_options_active 
               ? "game" 
               : "folder",
               path_core_options);
         config_file_write(conf_tmp, path_core_options, true);
         config_file_free(conf_tmp);
         conf_tmp = NULL;
      }
      path_clear(RARCH_PATH_CORE_OPTIONS);
   }
   else
   {
      const char *path = p_runloop->core_options->conf_path;
      core_option_manager_flush(
            p_runloop->core_options->conf,
            p_runloop->core_options);
      RARCH_LOG("[Core Options]: Saved core options file to \"%s\"\n", path);
      config_file_write(p_runloop->core_options->conf, path, true);
   }

   p_runloop->game_options_active   = false;
   p_runloop->folder_options_active = false;

   if (p_runloop->core_options)
      core_option_manager_free(p_runloop->core_options);
   p_runloop->core_options          = NULL;
}


retro_time_t runloop_core_runtime_tick(
      runloop_state_t *p_runloop,
      float slowmotion_ratio,
      retro_time_t current_time)
{
   retro_time_t frame_time              =
      (1.0 / p_runloop->av_info.timing.fps) * 1000000;
   bool runloop_slowmotion              = p_runloop->slowmotion;
   bool runloop_fastmotion              = p_runloop->fastmotion;

   /* Account for slow motion */
   if (runloop_slowmotion)
      return (retro_time_t)((double)frame_time * slowmotion_ratio);

   /* Account for fast forward */
   if (runloop_fastmotion)
   {
      /* Doing it this way means we miss the first frame after
       * turning fast forward on, but it saves the overhead of
       * having to do:
       *    retro_time_t current_usec = cpu_features_get_time_usec();
       *    libretro_core_runtime_last = current_usec;
       * every frame when fast forward is off. */
      retro_time_t current_usec                = current_time;
      retro_time_t potential_frame_time        = current_usec -
         p_runloop->libretro_core_runtime_last;
      p_runloop->libretro_core_runtime_last    = current_usec;

      if (potential_frame_time < frame_time)
         return potential_frame_time;
   }

   return frame_time;
}

core_option_manager_t *runloop_init_core_options(
      runloop_state_t *p_runloop,
      settings_t *settings,
      const struct retro_core_option_definition *option_defs)
{
   char options_path[PATH_MAX_LENGTH];
   char src_options_path[PATH_MAX_LENGTH];

   /* Ensure these are NULL-terminated */
   options_path[0]                = '\0';
   src_options_path[0]            = '\0';

   /* Get core options file path */
   runloop_init_core_options_path(
         p_runloop,
         settings,
         options_path, sizeof(options_path),
         src_options_path, sizeof(src_options_path));

   if (!string_is_empty(options_path))
      return core_option_manager_new(
            options_path, src_options_path, option_defs);
   return NULL;
}
