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

#ifndef CORE_OPTION_MANAGER_H__
#define CORE_OPTION_MANAGER_H__

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <lists/string_list.h>

#include "../../retroarch.h"

RETRO_BEGIN_DECLS

struct core_option
{
   char *desc;
   char *info;
   char *key;
   struct string_list *vals;
   struct string_list *val_labels;
   size_t default_index;
   size_t index;
   bool visible;
};

struct core_option_manager
{
   config_file_t *conf;
   char conf_path[PATH_MAX_LENGTH];

   struct core_option *opts;
   size_t size;
   bool updated;
};

typedef struct core_option_manager core_option_manager_t;

/**
 * core_option_manager_set_default:
 * @opt                   : pointer to core option manager object.
 * @idx                   : index of core option to be reset to defaults.
 *
 * Reset core option specified by @idx and sets default value for option.
 **/
void core_option_manager_set_default(core_option_manager_t *opt, size_t idx);

/**
 * core_option_manager_get_desc:
 * @opt              : options manager handle
 * @idx              : idx identifier of the option
 *
 * Gets description for an option.
 *
 * Returns: Description for an option.
 **/
const char *core_option_manager_get_desc(core_option_manager_t *opt,
      size_t idx);

/**
 * core_option_manager_get_info:
 * @opt              : options manager handle
 * @idx              : idx identifier of the option
 *
 * Gets information text for an option.
 *
 * Returns: Information text for an option.
 **/
const char *core_option_manager_get_info(core_option_manager_t *opt,
      size_t idx);

/**
 * core_option_manager_get_val:
 * @opt              : options manager handle
 * @idx              : idx identifier of the option
 *
 * Gets value for an option.
 *
 * Returns: Value for an option.
 **/
const char *core_option_manager_get_val(core_option_manager_t *opt,
      size_t idx);

/**
 * core_option_manager_get_val_label:
 * @opt              : options manager handle
 * @idx              : idx identifier of the option
 *
 * Gets value label for an option.
 *
 * Returns: Value label for an option.
 **/
const char *core_option_manager_get_val_label(core_option_manager_t *opt,
      size_t idx);

/**
 * core_option_manager_get_visible:
 * @opt              : options manager handle
 * @idx              : idx identifier of the option
 *
 * Gets whether option should be visible when displaying
 * core options in the frontend
 *
 * Returns: 'true' if option should be displayed by the frontend.
 **/
bool core_option_manager_get_visible(core_option_manager_t *opt,
      size_t idx);

void core_option_manager_set_val(core_option_manager_t *opt,
      size_t idx, size_t val_idx);

RETRO_END_DECLS

#endif
