/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "core_options.h"
#include "general.h"
#include "file.h"
#include "compat/posix_string.h"

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
   char conf_path[PATH_MAX];

   struct core_option *opts;
   size_t size;
   bool updated;
};

void core_option_free(core_option_manager_t *opt)
{
   size_t i;
   if (!opt)
      return;

   for (i = 0; i < opt->size; i++)
   {
      free(opt->opts[i].desc);
      free(opt->opts[i].key);
      string_list_free(opt->opts[i].vals);
   }

   if (opt->conf)
      config_file_free(opt->conf);
   free(opt->opts);
   free(opt);
}

void core_option_get(core_option_manager_t *opt, struct retro_variable *var)
{
   size_t i;
   opt->updated = false;
   for (i = 0; i < opt->size; i++)
   {
      if (strcmp(opt->opts[i].key, var->key) == 0)
      {
         var->value = core_option_get_val(opt, i);
         return;
      }
   }

   var->value = NULL;
}

static bool parse_variable(core_option_manager_t *opt, size_t index, const struct retro_variable *var)
{
   size_t i;
   struct core_option *option = &opt->opts[index];
   option->key = strdup(var->key);

   char *value = strdup(var->value);
   char *desc_end = strstr(value, "; ");

   if (!desc_end)
   {
      free(value);
      return false;
   }

   *desc_end = '\0';
   option->desc = strdup(value);

   const char *val_start = desc_end + 2;
   option->vals = string_split(val_start, "|");

   if (!option->vals)
   {
      free(value);
      return false;
   }

   char *config_val = NULL;
   if (config_get_string(opt->conf, option->key, &config_val))
   {
      for (i = 0; i < option->vals->size; i++)
      {
         if (strcmp(option->vals->elems[i].data, config_val) == 0)
         {
            option->index = i;
            break;
         }
      }

      free(config_val);
   }

   free(value);

   RARCH_LOG("Core option:\n");
   RARCH_LOG("\tDescription: %s\n", option->desc);
   RARCH_LOG("\tKey: %s\n", option->key);
   RARCH_LOG("\tCurrent value: %s\n", core_option_get_val(opt, index));
   RARCH_LOG("\tPossible values:\n");
   for (i = 0; i < option->vals->size; i++)
      RARCH_LOG("\t\t%s\n", option->vals->elems[i].data);

   return true;
}

core_option_manager_t *core_option_new(const char *conf_path, const struct retro_variable *vars)
{
   const struct retro_variable *var;
   core_option_manager_t *opt = (core_option_manager_t*)calloc(1, sizeof(*opt));
   if (!opt)
      return NULL;

   size_t size = 0;

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

   size = 0;
   for (var = vars; var->key && var->value; size++, var++)
   {
      if (!parse_variable(opt, size, var))
         goto error;
   }

   return opt;

error:
   core_option_free(opt);
   return NULL;
}

bool core_option_updated(core_option_manager_t *opt)
{
   return opt->updated;
}

void core_option_flush(core_option_manager_t *opt)
{
   size_t i;
   for (i = 0; i < opt->size; i++)
   {
      struct core_option *option = &opt->opts[i];
      config_set_string(opt->conf, option->key, core_option_get_val(opt, i));
   }
   config_file_write(opt->conf, opt->conf_path);
}

size_t core_option_size(core_option_manager_t *opt)
{
   return opt->size;
}

const char *core_option_get_desc(core_option_manager_t *opt, size_t index)
{
   return opt->opts[index].desc;
}

const char *core_option_get_val(core_option_manager_t *opt, size_t index)
{
   struct core_option *option = &opt->opts[index];
   return option->vals->elems[option->index].data;
}

struct string_list *core_option_get_vals(core_option_manager_t *opt, size_t index)
{
   return opt->opts[index].vals;
}

void core_option_set_val(core_option_manager_t *opt, size_t index, size_t val_index)
{
   struct core_option *option= (struct core_option*)&opt->opts[index];
   option->index = val_index % option->vals->size;
   opt->updated = true;
}

void core_option_next(core_option_manager_t *opt, size_t index)
{
   struct core_option *option = &opt->opts[index];
   option->index = (option->index + 1) % option->vals->size;
   opt->updated = true;
}

void core_option_prev(core_option_manager_t *opt, size_t index)
{
   struct core_option *option = &opt->opts[index];
   option->index = (option->index + option->vals->size - 1) % option->vals->size;
   opt->updated = true;
}

void core_option_set_default(core_option_manager_t *opt, size_t index)
{
   opt->opts[index].index = 0;
   opt->updated = true;
}


