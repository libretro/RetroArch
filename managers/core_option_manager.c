/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <string.h>

#include <file/config_file.h>
#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <compat/posix_string.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include <libretro.h>

#include "core_option_manager.h"

struct core_option
{
   char *desc;
   char *key;
   struct string_list *vals;
   size_t index;
};

struct core_option_manager
{
   config_file_t *conf;
   char conf_path[PATH_MAX_LENGTH];

   struct core_option *opts;
   size_t size;
   bool updated;
};

static bool core_option_manager_parse_variable(core_option_manager_t *opt, size_t idx,
      const struct retro_variable *var)
{
   const char *val_start      = NULL;
   char *value                = NULL;
   char *desc_end             = NULL;
   char *config_val           = NULL;
   struct core_option *option = (struct core_option*)&opt->opts[idx];

   if (!option)
      return false;

   option->key = strdup(var->key);
   value       = strdup(var->value);
   desc_end    = strstr(value, "; ");

   if (!desc_end)
   {
      free(value);
      return false;
   }

   *desc_end    = '\0';
   option->desc = strdup(value);

   val_start    = desc_end + 2;
   option->vals = string_split(val_start, "|");

   if (!option->vals)
   {
      free(value);
      return false;
   }

   if (config_get_string(opt->conf, option->key, &config_val))
   {
      size_t i;

      for (i = 0; i < option->vals->size; i++)
      {
         if (string_is_equal(option->vals->elems[i].data, config_val))
         {
            option->index = i;
            break;
         }
      }

      free(config_val);
   }

   free(value);

   return true;
}

/**
 * core_option_manager_free:
 * @opt              : options manager handle
 *
 * Frees core option manager handle.
 **/
void core_option_manager_free(core_option_manager_t *opt)
{
   size_t i;

   if (!opt)
      return;

   for (i = 0; i < opt->size; i++)
   {
      if (opt->opts[i].desc)
         free(opt->opts[i].desc);
      if (opt->opts[i].key)
         free(opt->opts[i].key);

      if (opt->opts[i].vals)
         string_list_free(opt->opts[i].vals);

      opt->opts[i].desc = NULL;
      opt->opts[i].key  = NULL;
      opt->opts[i].vals = NULL;
   }

   if (opt->conf)
      config_file_free(opt->conf);
   free(opt->opts);
   free(opt);
}

void core_option_manager_get(core_option_manager_t *opt, void *data)
{
   size_t i;
   struct retro_variable *var = (struct retro_variable*)data;

   if (!opt)
      return;

   opt->updated = false;

   for (i = 0; i < opt->size; i++)
   {
      if (string_is_empty(opt->opts[i].key))
         continue;

      if (string_is_equal(opt->opts[i].key, var->key))
      {
         var->value = core_option_manager_get_val(opt, i);
         return;
      }
   }

   var->value = NULL;
}


/**
 * core_option_manager_new:
 * @conf_path        : Filesystem path to write core option config file to.
 * @vars             : Pointer to variable array handle.
 *
 * Creates and initializes a core manager handle.
 *
 * Returns: handle to new core manager handle, otherwise NULL.
 **/
core_option_manager_t *core_option_manager_new(const char *conf_path,
      const void *data)
{
   const struct retro_variable *var;
   const struct retro_variable *vars = (const struct retro_variable*)data;
   size_t size                       = 0;
   core_option_manager_t *opt        = (core_option_manager_t*)
      calloc(1, sizeof(*opt));

   if (!opt)
      return NULL;

   if (*conf_path)
      opt->conf = config_file_new(conf_path);
   if (!opt->conf)
      opt->conf = config_file_new(NULL);

   strlcpy(opt->conf_path, conf_path, sizeof(opt->conf_path));

   if (!opt->conf)
      goto error;

   for (var = vars; var->key && var->value; var++)
      size++;

   opt->opts = (struct core_option*)calloc(size, sizeof(*opt->opts));
   if (!opt->opts)
      goto error;

   opt->size = size;
   size      = 0;

   for (var = vars; var->key && var->value; size++, var++)
   {
      if (!core_option_manager_parse_variable(opt, size, var))
         goto error;
   }

   return opt;

error:
   core_option_manager_free(opt);
   return NULL;
}

/**
 * core_option_manager_updated:
 * @opt              : options manager handle
 *
 * Has a core option been updated?
 *
 * Returns: true (1) if a core option has been updated,
 * otherwise false (0).
 **/
bool core_option_manager_updated(core_option_manager_t *opt)
{
   if (!opt)
      return false;
   return opt->updated;
}

/**
 * core_option_manager_flush:
 * @opt              : options manager handle
 *
 * Writes core option key-pair values to file.
 *
 * Returns: true (1) if core option values could be
 * successfully saved to disk, otherwise false (0).
 **/
bool core_option_manager_flush(core_option_manager_t *opt)
{
   size_t i;

   for (i = 0; i < opt->size; i++)
   {
      struct core_option *option = (struct core_option*)&opt->opts[i];

      if (option)
         config_set_string(opt->conf, option->key,
               core_option_manager_get_val(opt, i));
   }

   return config_file_write(opt->conf, opt->conf_path);
}

/**
 * core_option_manager_flush_game_specific:
 * @opt              : options manager handle
 * @path             : path for the core options file
 *
 * Writes core option key-pair values to a custom file.
 *
 * Returns: true (1) if core option values could be
 * successfully saved to disk, otherwise false (0).
 **/
bool core_option_manager_flush_game_specific(core_option_manager_t *opt, char* path)
{
   size_t i;
   for (i = 0; i < opt->size; i++)
   {
      struct core_option *option = (struct core_option*)&opt->opts[i];

      if (option)
         config_set_string(opt->conf, option->key,
               core_option_manager_get_val(opt, i));
}

   return config_file_write(opt->conf, path);
}

/**
 * core_option_manager_size:
 * @opt              : options manager handle
 *
 * Gets total number of options.
 *
 * Returns: Total number of options.
 **/
size_t core_option_manager_size(core_option_manager_t *opt)
{
   if (!opt)
      return 0;
   return opt->size;
}

/**
 * core_option_manager_get_desc:
 * @opt              : options manager handle
 * @index            : index identifier of the option
 *
 * Gets description for an option.
 *
 * Returns: Description for an option.
 **/
const char *core_option_manager_get_desc(core_option_manager_t *opt, size_t idx)
{
   if (!opt)
      return NULL;
   return opt->opts[idx].desc;
}

/**
 * core_option_manager_get_val:
 * @opt              : options manager handle
 * @index            : index identifier of the option
 *
 * Gets value for an option.
 *
 * Returns: Value for an option.
 **/
const char *core_option_manager_get_val(core_option_manager_t *opt, size_t idx)
{
   struct core_option *option = NULL;
   if (!opt)
      return NULL;
   option = (struct core_option*)&opt->opts[idx];
   if (!option)
      return NULL;
   return option->vals->elems[option->index].data;
}

void core_option_manager_set_val(core_option_manager_t *opt,
      size_t idx, size_t val_idx)
{
   struct core_option *option= NULL;

   if (!opt)
      return;
   
   option = (struct core_option*)&opt->opts[idx];

   if (!option)
      return;

   option->index = val_idx % option->vals->size;
   opt->updated  = true;
}

/**
 * core_option_manager_next:
 * @opt                   : pointer to core option manager object.
 * @idx                   : index of core option to be reset to defaults.
 *
 * Get next value for core option specified by @idx.
 * Options wrap around.
 **/
void core_option_manager_next(core_option_manager_t *opt, size_t idx)
{
   struct core_option *option = NULL;

   if (!opt)
      return;
   
   option = (struct core_option*)&opt->opts[idx];

   if (!option)
      return;

   option->index = (option->index + 1) % option->vals->size;
   opt->updated  = true;
}

/**
 * core_option_manager_prev:
 * @opt                   : pointer to core option manager object.
 * @idx                   : index of core option to be reset to defaults.
 * Options wrap around.
 *
 * Get previous value for core option specified by @idx.
 * Options wrap around.
 **/
void core_option_manager_prev(core_option_manager_t *opt, size_t idx)
{
   struct core_option *option = NULL;

   if (!opt)
      return;
   
   option = (struct core_option*)&opt->opts[idx];

   if (!option)
      return;

   option->index = (option->index + option->vals->size - 1) %
      option->vals->size;
   opt->updated  = true;
}

/**
 * core_option_manager_set_default:
 * @opt                   : pointer to core option manager object.
 * @idx                   : index of core option to be reset to defaults.
 *
 * Reset core option specified by @idx and sets default value for option.
 **/
void core_option_manager_set_default(core_option_manager_t *opt, size_t idx)
{
   if (!opt)
      return;

   opt->opts[idx].index = 0;
   opt->updated         = true;
}
