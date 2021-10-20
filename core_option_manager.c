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

#include <string/stdstring.h>

#ifdef HAVE_CHEEVOS
#include "cheevos/cheevos.h"
#endif

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#endif

#include "core_option_manager.h"
#include "msg_hash.h"

#define CORE_OPTION_MANAGER_MAP_TAG "#"
#define CORE_OPTION_MANAGER_MAP_DELIM ":"

/*********************/
/* Option Conversion */
/*********************/

/**
 * core_option_manager_convert_v1:
 *
 * @options_v1 : an array of retro_core_option_definition
 *               structs
 *
 * Converts an array of core option v1 definitions into
 * a v2 core options struct. Returned pointer must be
 * freed using core_option_manager_free_converted().
 *
 * Returns: Valid pointer to a new v2 core options struct
 * if successful, otherwise NULL.
 **/
struct retro_core_options_v2 *core_option_manager_convert_v1(
      const struct retro_core_option_definition *options_v1)
{
   size_t i;
   size_t num_options                                     = 0;
   struct retro_core_options_v2 *options_v2               = NULL;
   struct retro_core_option_v2_definition *option_v2_defs = NULL;

   if (!options_v1)
      return NULL;

   /* Determine number of options */
   for (;;)
   {
      if (string_is_empty(options_v1[num_options].key))
         break;
      num_options++;
   }

   if (num_options < 1)
      return NULL;

   /* Allocate output retro_core_options_v2 struct */
   options_v2 = (struct retro_core_options_v2 *)
         malloc(sizeof(*options_v2));
   if (!options_v2)
      return NULL;

   /* Note: v1 options have no concept of
    * categories, so this field will be left
    * as NULL */
   options_v2->categories  = NULL;
   options_v2->definitions = NULL;

   /* Allocate output option_v2_defs array
    * > One extra entry required for terminating NULL entry
    * > Note that calloc() sets terminating NULL entry and
    *   correctly 'nullifies' each values array */
   option_v2_defs = (struct retro_core_option_v2_definition *)
         calloc(num_options + 1, sizeof(*option_v2_defs));

   if (!option_v2_defs)
   {
      free(options_v2);
      return NULL;
   }

   options_v2->definitions = option_v2_defs;

   /* Loop through options... */
   for (i = 0; i < num_options; i++)
   {
      size_t j;
      size_t num_values = 0;

      /* Set key */
      option_v2_defs[i].key = options_v1[i].key;

      /* Set default value */
      option_v2_defs[i].default_value = options_v1[i].default_value;

      /* Set desc and info strings */
      option_v2_defs[i].desc = options_v1[i].desc;
      option_v2_defs[i].info = options_v1[i].info;

      /* v1 options have no concept of categories
       * (Note: These are already nullified by
       * the preceding calloc(), but we do it
       * explicitly here for code clarity) */
      option_v2_defs[i].desc_categorized = NULL;
      option_v2_defs[i].info_categorized = NULL;
      option_v2_defs[i].category_key     = NULL;

      /* Determine number of values */
      for (;;)
      {
         if (string_is_empty(options_v1[i].values[num_values].value))
            break;
         num_values++;
      }

      /* Copy values */
      for (j = 0; j < num_values; j++)
      {
         /* Set value string */
         option_v2_defs[i].values[j].value = options_v1[i].values[j].value;

         /* Set value label string */
         option_v2_defs[i].values[j].label = options_v1[i].values[j].label;
      }
   }

   return options_v2;
}

/**
 * core_option_manager_convert_v1_intl:
 *
 * @options_v1_intl : pointer to a retro_core_options_intl
 *                    struct
 *
 * Converts a v1 'international' core options definition
 * struct into a v2 core options struct. Returned pointer
 * must be freed using core_option_manager_free_converted().
 *
 * Returns: Valid pointer to a new v2 core options struct
 * if successful, otherwise NULL.
 **/
struct retro_core_options_v2 *core_option_manager_convert_v1_intl(
      const struct retro_core_options_intl *options_v1_intl)
{
   size_t i;
   size_t num_options                                     = 0;
   struct retro_core_option_definition *option_defs_us    = NULL;
   struct retro_core_option_definition *option_defs_local = NULL;
   struct retro_core_options_v2 *options_v2               = NULL;
   struct retro_core_option_v2_definition *option_v2_defs = NULL;

   if (!options_v1_intl)
      return NULL;

   option_defs_us    = options_v1_intl->us;
   option_defs_local = options_v1_intl->local;

   if (!option_defs_us)
      return NULL;

   /* Determine number of options */
   for (;;)
   {
      if (string_is_empty(option_defs_us[num_options].key))
         break;
      num_options++;
   }

   if (num_options < 1)
      return NULL;

   /* Allocate output retro_core_options_v2 struct */
   options_v2 = (struct retro_core_options_v2 *)
         malloc(sizeof(*options_v2));
   if (!options_v2)
      return NULL;

   /* Note: v1 options have no concept of
    * categories, so this field will be left
    * as NULL */
   options_v2->categories  = NULL;
   options_v2->definitions = NULL;

   /* Allocate output option_v2_defs array
    * > One extra entry required for terminating NULL entry
    * > Note that calloc() sets terminating NULL entry and
    *   correctly 'nullifies' each values array */
   option_v2_defs = (struct retro_core_option_v2_definition *)
         calloc(num_options + 1, sizeof(*option_v2_defs));

   if (!option_v2_defs)
   {
      core_option_manager_free_converted(options_v2);
      return NULL;
   }

   options_v2->definitions = option_v2_defs;

   /* Loop through options... */
   for (i = 0; i < num_options; i++)
   {
      size_t j;
      size_t num_values                            = 0;
      const char *key                              = option_defs_us[i].key;
      const char *local_desc                       = NULL;
      const char *local_info                       = NULL;
      struct retro_core_option_value *local_values = NULL;

      /* Key is always taken from us english defs */
      option_v2_defs[i].key = key;

      /* Default value is always taken from us english defs */
      option_v2_defs[i].default_value = option_defs_us[i].default_value;

      /* Try to find corresponding entry in local defs array */
      if (option_defs_local)
      {
         size_t index = 0;

         for (;;)
         {
            const char *local_key = option_defs_local[index].key;

            if (string_is_empty(local_key))
               break;

            if (string_is_equal(key, local_key))
            {
               local_desc   = option_defs_local[index].desc;
               local_info   = option_defs_local[index].info;
               local_values = option_defs_local[index].values;
               break;
            }

            index++;
         }
      }

      /* Set desc and info strings */
      option_v2_defs[i].desc = string_is_empty(local_desc) ?
            option_defs_us[i].desc : local_desc;
      option_v2_defs[i].info = string_is_empty(local_info) ?
            option_defs_us[i].info : local_info;

      /* v1 options have no concept of categories
       * (Note: These are already nullified by
       * the preceding calloc(), but we do it
       * explicitly here for code clarity) */
      option_v2_defs[i].desc_categorized = NULL;
      option_v2_defs[i].info_categorized = NULL;
      option_v2_defs[i].category_key     = NULL;

      /* Determine number of values
       * (always taken from us english defs) */
      for (;;)
      {
         if (string_is_empty(option_defs_us[i].values[num_values].value))
            break;
         num_values++;
      }

      /* Copy values */
      for (j = 0; j < num_values; j++)
      {
         const char *value       = option_defs_us[i].values[j].value;
         const char *local_label = NULL;

         /* Value string is always taken from us english defs */
         option_v2_defs[i].values[j].value = value;

         /* Try to find corresponding entry in local defs values array */
         if (local_values)
         {
            size_t value_index = 0;

            for (;;)
            {
               const char *local_value = local_values[value_index].value;

               if (string_is_empty(local_value))
                  break;

               if (string_is_equal(value, local_value))
               {
                  local_label = local_values[value_index].label;
                  break;
               }

               value_index++;
            }
         }

         /* Set value label string */
         option_v2_defs[i].values[j].label = string_is_empty(local_label) ?
               option_defs_us[i].values[j].label : local_label;
      }
   }

   return options_v2;
}

/**
 * core_option_manager_convert_v2_intl:
 *
 * @options_v2_intl : pointer to a retro_core_options_v2_intl
 *                    struct
 *
 * Converts a v2 'international' core options struct
 * into a regular v2 core options struct. Returned pointer
 * must be freed using core_option_manager_free_converted().
 *
 * Returns: Valid pointer to a new v2 core options struct
 * if successful, otherwise NULL.
 **/
struct retro_core_options_v2 *core_option_manager_convert_v2_intl(
      const struct retro_core_options_v2_intl *options_v2_intl)
{
   size_t i;
   size_t num_categories                                  = 0;
   size_t num_options                                     = 0;
   struct retro_core_options_v2 *options_v2_us            = NULL;
   struct retro_core_options_v2 *options_v2_local         = NULL;
   struct retro_core_options_v2 *options_v2               = NULL;
   struct retro_core_option_v2_category *option_v2_cats   = NULL;
   struct retro_core_option_v2_definition *option_v2_defs = NULL;

   if (!options_v2_intl)
      return NULL;

   options_v2_us    = options_v2_intl->us;
   options_v2_local = options_v2_intl->local;

   if (!options_v2_us ||
       !options_v2_us->definitions)
      return NULL;

   /* Determine number of categories
    * (Note: zero categories are permitted) */
   if (options_v2_us->categories)
   {
      for (;;)
      {
         if (string_is_empty(options_v2_us->categories[num_categories].key))
            break;
         num_categories++;
      }
   }

   /* Determine number of options */
   for (;;)
   {
      if (string_is_empty(options_v2_us->definitions[num_options].key))
         break;
      num_options++;
   }

   if (num_options < 1)
      return NULL;

   /* Allocate output retro_core_options_v2 struct */
   options_v2 = (struct retro_core_options_v2 *)
         malloc(sizeof(*options_v2));
   if (!options_v2)
      return NULL;

   options_v2->categories  = NULL;
   options_v2->definitions = NULL;

   /* Allocate output option_v2_cats array
    * > One extra entry required for terminating NULL entry
    * > Note that calloc() sets terminating NULL entry */
   if (num_categories > 0)
   {
      option_v2_cats = (struct retro_core_option_v2_category *)
            calloc(num_categories + 1, sizeof(*option_v2_cats));

      if (!option_v2_cats)
      {
         core_option_manager_free_converted(options_v2);
         return NULL;
      }
   }

   options_v2->categories = option_v2_cats;

   /* Allocate output option_v2_defs array
    * > One extra entry required for terminating NULL entry
    * > Note that calloc() sets terminating NULL entry and
    *   correctly 'nullifies' each values array */
   option_v2_defs = (struct retro_core_option_v2_definition *)
         calloc(num_options + 1, sizeof(*option_v2_defs));

   if (!option_v2_defs)
   {
      core_option_manager_free_converted(options_v2);
      return NULL;
   }

   options_v2->definitions = option_v2_defs;

   /* Loop through categories...
    * (Note: This loop will not execute if
    * options_v2_us->categories is NULL) */
   for (i = 0; i < num_categories; i++)
   {
      const char *key        = options_v2_us->categories[i].key;
      const char *local_desc = NULL;
      const char *local_info = NULL;

      /* Key is always taken from us english
       * categories */
      option_v2_cats[i].key = key;

      /* Try to find corresponding entry in local
       * categories array */
      if (options_v2_local &&
          options_v2_local->categories)
      {
         size_t index = 0;

         for (;;)
         {
            const char *local_key = options_v2_local->categories[index].key;

            if (string_is_empty(local_key))
               break;

            if (string_is_equal(key, local_key))
            {
               local_desc = options_v2_local->categories[index].desc;
               local_info = options_v2_local->categories[index].info;
               break;
            }

            index++;
         }
      }

      /* Set desc and info strings */
      option_v2_cats[i].desc = string_is_empty(local_desc) ?
            options_v2_us->categories[i].desc : local_desc;
      option_v2_cats[i].info = string_is_empty(local_info) ?
            options_v2_us->categories[i].info : local_info;
      
   }

   /* Loop through options... */
   for (i = 0; i < num_options; i++)
   {
      size_t j;
      size_t num_values                            = 0;
      const char *key                              = options_v2_us->definitions[i].key;
      const char *local_desc                       = NULL;
      const char *local_desc_categorized           = NULL;
      const char *local_info                       = NULL;
      const char *local_info_categorized           = NULL;
      struct retro_core_option_value *local_values = NULL;

      /* Key is always taken from us english defs */
      option_v2_defs[i].key = key;

      /* Default value is always taken from us english defs */
      option_v2_defs[i].default_value = options_v2_us->definitions[i].default_value;

      /* Try to find corresponding entry in local defs array */
      if (options_v2_local &&
          options_v2_local->definitions)
      {
         size_t index = 0;

         for (;;)
         {
            const char *local_key = options_v2_local->definitions[index].key;

            if (string_is_empty(local_key))
               break;

            if (string_is_equal(key, local_key))
            {
               local_desc             = options_v2_local->definitions[index].desc;
               local_desc_categorized = options_v2_local->definitions[index].desc_categorized;
               local_info             = options_v2_local->definitions[index].info;
               local_info_categorized = options_v2_local->definitions[index].info_categorized;
               local_values           = options_v2_local->definitions[index].values;
               break;
            }

            index++;
         }
      }

      /* Set desc and info strings */
      option_v2_defs[i].desc             = string_is_empty(local_desc) ?
            options_v2_us->definitions[i].desc : local_desc;
      option_v2_defs[i].desc_categorized = string_is_empty(local_desc_categorized) ?
            options_v2_us->definitions[i].desc_categorized : local_desc_categorized;
      option_v2_defs[i].info             = string_is_empty(local_info) ?
            options_v2_us->definitions[i].info : local_info;
      option_v2_defs[i].info_categorized = string_is_empty(local_info_categorized) ?
            options_v2_us->definitions[i].info_categorized : local_info_categorized;

      /* Category key is always taken from us english defs */
      option_v2_defs[i].category_key = options_v2_us->definitions[i].category_key;

      /* Determine number of values
       * (always taken from us english defs) */
      for (;;)
      {
         if (string_is_empty(
               options_v2_us->definitions[i].values[num_values].value))
            break;
         num_values++;
      }

      /* Copy values */
      for (j = 0; j < num_values; j++)
      {
         const char *value       = options_v2_us->definitions[i].values[j].value;
         const char *local_label = NULL;

         /* Value string is always taken from us english defs */
         option_v2_defs[i].values[j].value = value;

         /* Try to find corresponding entry in local defs values array */
         if (local_values)
         {
            size_t value_index = 0;

            for (;;)
            {
               const char *local_value = local_values[value_index].value;

               if (string_is_empty(local_value))
                  break;

               if (string_is_equal(value, local_value))
               {
                  local_label = local_values[value_index].label;
                  break;
               }

               value_index++;
            }
         }

         /* Set value label string */
         option_v2_defs[i].values[j].label = string_is_empty(local_label) ?
               options_v2_us->definitions[i].values[j].label : local_label;
      }
   }

   return options_v2;
}

/**
 * core_option_manager_convert_v2_intl:
 *
 * @options_v2 : pointer to a retro_core_options_v2
 *               struct
 *
 * Frees the pointer returned by any
 * core_option_manager_convert_*() function.
 **/
void core_option_manager_free_converted(
      struct retro_core_options_v2 *options_v2)
{
   if (!options_v2)
      return;

   if (options_v2->categories)
   {
      free(options_v2->categories);
      options_v2->categories = NULL;
   }

   if (options_v2->definitions)
   {
      free(options_v2->definitions);
      options_v2->definitions = NULL;
   }

   free(options_v2);
}

/**************************************/
/* Initialisation / De-Initialisation */
/**************************************/

/* Generates a hash key for the specified string */
static uint32_t core_option_manager_hash_string(const char *str)
{
   unsigned char c;
   uint32_t hash = (uint32_t)0x811c9dc5;
   while ((c = (unsigned char)*(str++)) != '\0')
      hash = ((hash * (uint32_t)0x01000193) ^ (uint32_t)c);
   return (hash ? hash : 1);
}

/* Sanitises a core option value label, handling the case
 * where an explicit label is not provided and performing
 * conversion of various true/false identifiers to
 * ON/OFF strings */
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

/* Parses a single legacy core options interface
 * variable, extracting all present core_option
 * information */
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
         *entry               = NULL;

   /* Record option index (required to facilitate
    * option map handling) */
   option->opt_idx            = idx;

   /* All options are visible by default */
   option->visible            = true;

   if (!string_is_empty(var->key))
   {
      option->key             = strdup(var->key);
      option->key_hash        = core_option_manager_hash_string(var->key);
   }

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

   /* > Loop over values and:
    *   - Set value hashes
    *   - 'Extract' labels */
   for (i = 0; i < option->vals->size; i++)
   {
      const char *value       = option->vals->elems[i].data;
      uint32_t *value_hash    = (uint32_t *)malloc(sizeof(uint32_t));
      const char *value_label = core_option_manager_parse_value_label(
            value, NULL);

      /* Set value hash */
      *value_hash = core_option_manager_hash_string(value);
      option->vals->elems[i].userdata = (void*)value_hash;

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
      uint32_t entry_value_hash = core_option_manager_hash_string(entry->value);

      for (i = 0; i < option->vals->size; i++)
      {
         const char *value   = option->vals->elems[i].data;
         uint32_t value_hash = *((uint32_t*)option->vals->elems[i].userdata);

         if ((value_hash == entry_value_hash) &&
             string_is_equal(value, entry->value))
         {
            option->index = i;
            break;
         }
      }
   }

   /* Legacy core option interface has no concept
    * of categories */
   option->desc_categorized = NULL;
   option->info_categorized = NULL;
   option->category_key     = NULL;

   free(value);
   return true;

error:
   free(value);
   return false;
}

/**
 * core_option_manager_new_vars:
 *
 * @conf_path     : Filesystem path to write core option
 *                  config file to
 * @src_conf_path : Filesystem path from which to load
 *                  initial config settings.
 * @vars          : Pointer to core option variable array
 *                  handle
 *
 * Legacy version of core_option_manager_new().
 * Creates and initializes a core manager handle.
 *
 * Returns: handle to new core manager handle if successful,
 * otherwise NULL.
 **/
core_option_manager_t *core_option_manager_new_vars(
      const char *conf_path, const char *src_conf_path,
      const struct retro_variable *vars)
{
   const struct retro_variable *var = NULL;
   size_t size                      = 0;
   config_file_t *config_src        = NULL;
   core_option_manager_t *opt       = NULL;

   if (!vars)
      return NULL;

   opt = (core_option_manager_t*)malloc(sizeof(*opt));

   if (!opt)
      return NULL;

   opt->conf                        = NULL;
   opt->conf_path[0]                = '\0';
   /* Legacy core option interface has no concept
    * of categories, so leave opt->cats as NULL
    * and opt->cats_size as zero */
   opt->cats                        = NULL;
   opt->cats_size                   = 0;
   opt->opts                        = NULL;
   opt->size                        = 0;
   opt->option_map                  = nested_list_init();
   opt->updated                     = false;

   if (!opt->option_map)
      goto error;

   /* Open 'output' config file */
   if (!string_is_empty(conf_path))
      if (!(opt->conf = config_file_new_from_path_to_string(conf_path)))
         if (!(opt->conf = config_file_new_alloc()))
            goto error;

   strlcpy(opt->conf_path, conf_path, sizeof(opt->conf_path));

   /* Load source config file, if required */
   if (!string_is_empty(src_conf_path))
      config_src = config_file_new_from_path_to_string(src_conf_path);

   /* Get number of variables */
   for (var = vars; var->key && var->value; var++)
      size++;

   if (size == 0)
      goto error;

   /* Create options array */
   opt->opts = (struct core_option*)calloc(size, sizeof(*opt->opts));
   if (!opt->opts)
      goto error;

   opt->size = size;
   size      = 0;

   /* Parse each variable */
   for (var = vars; var->key && var->value; size++, var++)
   {
      if (core_option_manager_parse_variable(opt, size, var, config_src))
      {
         /* If variable is read correctly, add it to
          * the map */
         char address[256];

         address[0] = '\0';

         /* Address string is normally:
          *    <category_key><delim><tag><option_key>
          * ...where <tag> is prepended to the option
          * key in order to avoid category/option key
          * collisions. Legacy options have no categories,
          * so we could just set the address to
          * <option_key> - but for consistency with
          * 'modern' options, we apply the tag regardless */
         snprintf(address, sizeof(address),
               CORE_OPTION_MANAGER_MAP_TAG "%s", var->key);

         if (!nested_list_add_item(opt->option_map,
               address, NULL, (const void*)&opt->opts[size]))
            goto error;
      }
      else
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

/* Parses a single v2 core options interface
 * option, extracting all present core_option
 * information */
static bool core_option_manager_parse_option(
      core_option_manager_t *opt, size_t idx,
      const struct retro_core_option_v2_definition *option_def,
      config_file_t *config_src)
{
   size_t i;
   union string_list_elem_attr attr;
   struct config_entry_list
      *entry                  = NULL;
   size_t num_vals            = 0;
   struct core_option *option = (struct core_option*)&opt->opts[idx];
   const char *key            = option_def->key;
   const char *category_key   = option_def->category_key;
   const struct retro_core_option_value
         *values              = option_def->values;

   /* Record option index (required to facilitate
    * option map handling) */
   option->opt_idx            = idx;

   /* All options are visible by default */
   option->visible            = true;

   if (!string_is_empty(option_def->desc))
      option->desc            = strdup(option_def->desc);

   if (!string_is_empty(option_def->info))
      option->info            = strdup(option_def->info);

   /* Set category-related parameters
    * > Ignore if specified category key does not
    *   match an entry in the categories array
    * > Category key cannot contain a map delimiter
    *   character */
   if (opt->cats &&
       !string_is_empty(category_key) &&
       !strstr(category_key, CORE_OPTION_MANAGER_MAP_DELIM))
   {
      for (i = 0; i < opt->cats_size; i++)
      {
         const char *search_key = opt->cats[i].key;

         if (string_is_empty(search_key))
            break;

         if (string_is_equal(search_key, category_key))
         {
            option->category_key        = strdup(category_key);

            if (!string_is_empty(option_def->desc_categorized))
               option->desc_categorized = strdup(option_def->desc_categorized);

            if (!string_is_empty(option_def->info_categorized))
               option->info_categorized = strdup(option_def->info_categorized);

            break;
         }
      }
   }

   /* Have to set key *after* checking for option
    * categories */
   if (!string_is_empty(option_def->key))
   {
      /* If option has a category, option key
       * cannot contain a map delimiter character */
      if (!string_is_empty(option->category_key) &&
          strstr(key, CORE_OPTION_MANAGER_MAP_DELIM))
         return false;

      option->key      = strdup(key);
      option->key_hash = core_option_manager_hash_string(key);
   }

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
      uint32_t *value_hash    = (uint32_t *)malloc(sizeof(uint32_t));
      const char *value_label = values[i].label;

      /* Append value string
       * > We know that 'value' is always valid */
      string_list_append(option->vals, value, attr);

      /* > Set value hash */
      *value_hash = core_option_manager_hash_string(value);
      option->vals->elems[option->vals->size - 1].userdata = (void*)value_hash;

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
      uint32_t entry_value_hash = core_option_manager_hash_string(entry->value);

      for (i = 0; i < option->vals->size; i++)
      {
         const char *value   = option->vals->elems[i].data;
         uint32_t value_hash = *((uint32_t*)option->vals->elems[i].userdata);

         if ((value_hash == entry_value_hash) &&
             string_is_equal(value, entry->value))
         {
            option->index = i;
            break;
         }
      }
   }

   return true;
}

/**
 * core_option_manager_new:
 *
 * @conf_path     : Filesystem path to write core option
 *                  config file to
 * @src_conf_path : Filesystem path from which to load
 *                  initial config settings.
 * @options_v2    : Pointer to retro_core_options_v2 struct
 * @categorized   : Flag specifying whether core option
 *                  category information should be read
 *                  from @options_v2
 *
 * Creates and initializes a core manager handle. Parses
 * information from a retro_core_options_v2 struct.
 * If @categorized is false, all option category
 * assignments will be ignored.
 *
 * Returns: handle to new core manager handle if successful,
 * otherwise NULL.
 **/
core_option_manager_t *core_option_manager_new(
      const char *conf_path, const char *src_conf_path,
      const struct retro_core_options_v2 *options_v2,
      bool categorized)
{
   const struct retro_core_option_v2_category *option_cat   = NULL;
   const struct retro_core_option_v2_definition *option_def = NULL;
   struct retro_core_option_v2_category *option_cats        = NULL;
   struct retro_core_option_v2_definition *option_defs      = NULL;
   size_t cats_size                                         = 0;
   size_t size                                              = 0;
   config_file_t *config_src                                = NULL;
   core_option_manager_t *opt                               = NULL;

   if (!options_v2 ||
       !options_v2->definitions)
      return NULL;

   option_cats = options_v2->categories;
   option_defs = options_v2->definitions;

   opt = (core_option_manager_t*)malloc(sizeof(*opt));

   if (!opt)
      return NULL;

   opt->conf                         = NULL;
   opt->conf_path[0]                 = '\0';
   opt->cats                         = NULL;
   opt->cats_size                    = 0;
   opt->opts                         = NULL;
   opt->size                         = 0;
   opt->option_map                   = nested_list_init();
   opt->updated                      = false;

   if (!opt->option_map)
      goto error;

   /* Open 'output' config file */
   if (!string_is_empty(conf_path))
      if (!(opt->conf = config_file_new_from_path_to_string(conf_path)))
         if (!(opt->conf = config_file_new_alloc()))
            goto error;

   strlcpy(opt->conf_path, conf_path, sizeof(opt->conf_path));

   /* Load source config file, if required */
   if (!string_is_empty(src_conf_path))
      config_src = config_file_new_from_path_to_string(src_conf_path);

   /* Get number of categories, if required
    * > Note: 'option_cat->info == NULL' is valid */
   if (categorized && option_cats)
   {
      for (option_cat = option_cats;
           !string_is_empty(option_cat->key) &&
                  !string_is_empty(option_cat->desc);
           option_cat++)
         cats_size++;
   }

   /* Get number of options
    * > Note: 'option_def->info == NULL' is valid */
   for (option_def = option_defs;
        option_def->key && option_def->desc && option_def->values[0].value;
        option_def++)
      size++;

   if (size == 0)
      goto error;

   /* Create categories array */
   if (cats_size > 0)
   {
      opt->cats = (struct core_catagory*)calloc(size, sizeof(*opt->cats));
      if (!opt->cats)
         goto error;

      opt->cats_size = cats_size;
      cats_size      = 0;

      /* Parse each category
       * > Note: 'option_cat->info == NULL' is valid */
      for (option_cat = option_cats;
           !string_is_empty(option_cat->key) &&
                  !string_is_empty(option_cat->desc);
           cats_size++, option_cat++)
      {
         opt->cats[cats_size].key      = strdup(option_cat->key);
         opt->cats[cats_size].key_hash = core_option_manager_hash_string(option_cat->key);
         opt->cats[cats_size].desc     = strdup(option_cat->desc);

         if (!string_is_empty(option_cat->info))
            opt->cats[cats_size].info  = strdup(option_cat->info);
      }
   }

   /* Create options array */
   opt->opts = (struct core_option*)calloc(size, sizeof(*opt->opts));
   if (!opt->opts)
      goto error;

   opt->size = size;
   size      = 0;

   /* Parse each option
    * > Note: 'option_def->info == NULL' is valid */
   for (option_def = option_defs;
        option_def->key && option_def->desc && option_def->values[0].value;
        size++, option_def++)
   {
      if (core_option_manager_parse_option(opt, size, option_def, config_src))
      {
         /* If option is read correctly, add it to
          * the map */
         const char *category_key = opt->opts[size].category_key;
         char address[256];

         address[0] = '\0';

         /* Address string is nominally:
          *    <category_key><delim><tag><option_key>
          * ...where <tag> is prepended to the option
          * key in order to avoid category/option key
          * collisions */
         if (string_is_empty(category_key))
            snprintf(address, sizeof(address),
                  CORE_OPTION_MANAGER_MAP_TAG "%s", option_def->key);
         else
            snprintf(address, sizeof(address),
                  "%s" CORE_OPTION_MANAGER_MAP_DELIM CORE_OPTION_MANAGER_MAP_TAG "%s",
                  category_key, option_def->key);

         if (!nested_list_add_item(opt->option_map,
               address, CORE_OPTION_MANAGER_MAP_DELIM,
               (const void*)&opt->opts[size]))
            goto error;
      }
      else
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

/**
 * core_option_manager_free:
 *
 * @opt : options manager handle
 *
 * Frees specified core options manager handle.
 **/
void core_option_manager_free(core_option_manager_t *opt)
{
   size_t i;

   if (!opt)
      return;

   for (i = 0; i < opt->cats_size; i++)
   {
      if (opt->cats[i].key)
         free(opt->cats[i].key);
      if (opt->cats[i].desc)
         free(opt->cats[i].desc);
      if (opt->cats[i].info)
         free(opt->cats[i].info);

      opt->cats[i].key  = NULL;
      opt->cats[i].desc = NULL;
      opt->cats[i].info = NULL;
   }

   for (i = 0; i < opt->size; i++)
   {
      if (opt->opts[i].desc)
         free(opt->opts[i].desc);
      if (opt->opts[i].desc_categorized)
         free(opt->opts[i].desc_categorized);
      if (opt->opts[i].info)
         free(opt->opts[i].info);
      if (opt->opts[i].info_categorized)
         free(opt->opts[i].info_categorized);
      if (opt->opts[i].key)
         free(opt->opts[i].key);
      if (opt->opts[i].category_key)
         free(opt->opts[i].category_key);

      if (opt->opts[i].vals)
         string_list_free(opt->opts[i].vals);
      if (opt->opts[i].val_labels)
         string_list_free(opt->opts[i].val_labels);

      opt->opts[i].desc             = NULL;
      opt->opts[i].desc_categorized = NULL;
      opt->opts[i].info             = NULL;
      opt->opts[i].info_categorized = NULL;
      opt->opts[i].key              = NULL;
      opt->opts[i].category_key     = NULL;
      opt->opts[i].vals             = NULL;
   }

   if (opt->option_map)
      nested_list_free(opt->option_map);

   if (opt->conf)
      config_file_free(opt->conf);

   free(opt->cats);
   free(opt->opts);
   free(opt);
}

/********************/
/* Category Getters */
/********************/

/**
 * core_option_manager_get_category_desc:
 *
 * @opt : options manager handle
 * @key : core option category id string
 *
 * Fetches the 'description' text of the core option
 * category identified by @key (used as the
 * category label in the menu).
 *
 * Returns: description string (menu label) of the
 * specified option category if successful,
 * otherwise NULL.
 **/
const char *core_option_manager_get_category_desc(core_option_manager_t *opt,
      const char *key)
{
   uint32_t key_hash;
   size_t i;

   if (!opt ||
       string_is_empty(key))
      return NULL;

   key_hash = core_option_manager_hash_string(key);

   for (i = 0; i < opt->cats_size; i++)
   {
      struct core_catagory *catagory = &opt->cats[i];

      if ((key_hash == catagory->key_hash) &&
          !string_is_empty(catagory->key) &&
          string_is_equal(key, catagory->key))
      {
         return catagory->desc;
      }
   }

   return NULL;
}

/**
 * core_option_manager_get_category_info:
 *
 * @opt : options manager handle
 * @key : core option category id string
 *
 * Fetches the 'info' text of the core option
 * category identified by @key (used as the category
 * sublabel in the menu).
 *
 * Returns: information string (menu sublabel) of
 * the specified option category if successful,
 * otherwise NULL.
 **/
const char *core_option_manager_get_category_info(core_option_manager_t *opt,
      const char *key)
{
   uint32_t key_hash;
   size_t i;

   if (!opt ||
       string_is_empty(key))
      return NULL;

   key_hash = core_option_manager_hash_string(key);

   for (i = 0; i < opt->cats_size; i++)
   {
      struct core_catagory *catagory = &opt->cats[i];

      if ((key_hash == catagory->key_hash) &&
          !string_is_empty(catagory->key) &&
          string_is_equal(key, catagory->key))
      {
         return catagory->info;
      }
   }

   return NULL;
}

/**
 * core_option_manager_get_category_visible:
 *
 * @opt : options manager handle
 * @key : core option category id string
 *
 * Queries whether the core option category
 * identified by @key should be displayed in
 * the frontend menu. (A category is deemed to
 * be visible if at least one of the options
 * in the category is visible)
 *
 * Returns: true if option category should be
 * displayed by the frontend, otherwise false.
 **/
bool core_option_manager_get_category_visible(core_option_manager_t *opt,
      const char *key)
{
   nested_list_item_t *category_item = NULL;
   nested_list_t *option_list        = NULL;
   nested_list_item_t *option_item   = NULL;
   const struct core_option *option  = NULL;
   size_t i;

   if (!opt ||
       string_is_empty(key))
      return false;

   /* Fetch category item from map */
   category_item = nested_list_get_item(opt->option_map,
         key, NULL);

   if (!category_item)
      return false;

   /* Get child options of specified category */
   option_list = nested_list_item_get_children(category_item);

   if (!option_list)
      return false;

   /* Loop over child options */
   for (i = 0; i < nested_list_get_size(option_list); i++)
   {
      option_item = nested_list_get_item_idx(option_list, i);
      option      = (const struct core_option *)
            nested_list_item_get_value(option_item);

      /* Check if current option is visible */
      if (option && option->visible)
         return true;
   }

   return false;
}

/******************/
/* Option Getters */
/******************/

/**
 * core_option_manager_get_idx:
 *
 * @opt : options manager handle
 * @key : core option key string (variable to query
 *        in RETRO_ENVIRONMENT_GET_VARIABLE)
 * @idx : index of core option corresponding
 *        to @key
 *
 * Fetches the index of the core option identified
 * by the specified @key.
 *
 * Returns: true if option matching the specified
 * key was found, otherwise false.
 **/
bool core_option_manager_get_idx(core_option_manager_t *opt,
      const char *key, size_t *idx)
{
   uint32_t key_hash;
   size_t i;

   if (!opt ||
       string_is_empty(key) ||
       !idx)
      return false;

   key_hash = core_option_manager_hash_string(key);

   for (i = 0; i < opt->size; i++)
   {
      struct core_option *option = &opt->opts[i];

      if ((key_hash == option->key_hash) &&
          !string_is_empty(option->key) &&
          string_is_equal(key, option->key))
      {
         *idx = i;
         return true;
      }
   }

   return false;
}

/**
 * core_option_manager_get_val_idx:
 *
 * @opt     : options manager handle
 * @idx     : core option index
 * @val     : string representation of the
 *            core option value
 * @val_idx : index of core option value
 *            corresponding to @val
 *
 * Fetches the index of the core option value
 * identified by the specified core option @idx
 * and @val string.
 *
 * Returns: true if option value matching the
 * specified option index and value string
 * was found, otherwise false.
 **/
bool core_option_manager_get_val_idx(core_option_manager_t *opt,
      size_t idx, const char *val, size_t *val_idx)
{
   struct core_option *option = NULL;
   uint32_t val_hash;
   size_t i;

   if (!opt ||
       (idx >= opt->size) ||
       string_is_empty(val) ||
       !val_idx)
      return false;

   val_hash = core_option_manager_hash_string(val);
   option   = (struct core_option*)&opt->opts[idx];

   for (i = 0; i < option->vals->size; i++)
   {
      const char *option_val   = option->vals->elems[i].data;
      uint32_t option_val_hash = *((uint32_t*)option->vals->elems[i].userdata);

      if ((val_hash == option_val_hash) &&
          !string_is_empty(option_val) &&
          string_is_equal(val, option_val))
      {
         *val_idx = i;
         return true;
      }
   }

   return false;
}

/**
 * core_option_manager_get_desc:
 *
 * @opt         : options manager handle
 * @idx         : core option index
 * @categorized : flag specifying whether to
 *                fetch the categorised description
 *                or the legacy fallback
 *
 * Fetches the 'description' of the core option at
 * index @idx (used as the option label in the menu).
 * If menu has option category support, @categorized
 * should be true. (At present, only the Qt interface
 * requires @categorized to be false)
 *
 * Returns: description string (menu label) of the
 * specified option if successful, otherwise NULL.
 **/
const char *core_option_manager_get_desc(core_option_manager_t *opt,
      size_t idx, bool categorized)
{
   const char *desc = NULL;

   if (!opt ||
       (idx >= opt->size))
      return NULL;

   /* Try categorised description first,
    * if requested */
   if (categorized)
      desc = opt->opts[idx].desc_categorized;

   /* Fall back to legacy description, if
    * required */
   if (string_is_empty(desc))
      desc = opt->opts[idx].desc;

   return desc;
}

/**
 * core_option_manager_get_info:
 *
 * @opt         : options manager handle
 * @idx         : core option index
 * @categorized : flag specifying whether to
 *                fetch the categorised information
 *                or the legacy fallback
 *
 * Fetches the 'info' text of the core option at
 * index @idx (used as the option sublabel in the
 * menu). If menu has option category support,
 * @categorized should be true. (At present, only
 * the Qt interface requires @categorized to be false)
 *
 * Returns: information string (menu sublabel) of the
 * specified option if successful, otherwise NULL.
 **/
const char *core_option_manager_get_info(core_option_manager_t *opt,
      size_t idx, bool categorized)
{
   const char *info = NULL;

   if (!opt ||
       (idx >= opt->size))
      return NULL;

   /* Try categorised information first,
    * if requested */
   if (categorized)
      info = opt->opts[idx].info_categorized;

   /* Fall back to legacy information, if
    * required */
   if (string_is_empty(info))
      info = opt->opts[idx].info;

   return info;
}

/**
 * core_option_manager_get_val:
 *
 * @opt : options manager handle
 * @idx : core option index
 *
 * Fetches the string representation of the current
 * value of the core option at index @idx.
 *
 * Returns: core option value string if successful,
 * otherwise NULL.
 **/
const char *core_option_manager_get_val(core_option_manager_t *opt,
      size_t idx)
{
   struct core_option *option = NULL;

   if (!opt ||
       (idx >= opt->size))
      return NULL;

   option = (struct core_option*)&opt->opts[idx];

   return option->vals->elems[option->index].data;
}

/**
 * core_option_manager_get_val_label:
 *
 * @opt : options manager handle
 * @idx : core option index
 *
 * Fetches the 'label' text (used for display purposes
 * in the menu) for the current value of the core
 * option at index @idx.
 *
 * Returns: core option value label string if
 * successful, otherwise NULL.
 **/
const char *core_option_manager_get_val_label(core_option_manager_t *opt,
      size_t idx)
{
   struct core_option *option = NULL;

   if (!opt ||
       (idx >= opt->size))
      return NULL;

   option = (struct core_option*)&opt->opts[idx];

   return option->val_labels->elems[option->index].data;
}

/**
 * core_option_manager_get_visible:
 *
 * @opt : options manager handle
 * @idx : core option index
 *
 * Queries whether the core option at index @idx
 * should be displayed in the frontend menu.
 *
 * Returns: true if option should be displayed by
 * the frontend, otherwise false.
 **/
bool core_option_manager_get_visible(core_option_manager_t *opt,
      size_t idx)
{
   if (!opt ||
       (idx >= opt->size))
      return false;

   return opt->opts[idx].visible;
}

/******************/
/* Option Setters */
/******************/

/**
 * core_option_manager_set_val:
 *
 * @opt          : options manager handle
 * @idx          : core option index
 * @val_idx      : index of the value to set
 * @refresh_menu : flag specifying whether menu
 *                 should be refreshed if changes
 *                 to option visibility are detected
 *
 * Sets the core option at index @idx to the
 * option value corresponding to @val_idx.
 * After setting the option value, a request
 * will be made for the core to update the
 * in-menu visibility of all options; if
 * visibility changes are detected and
 * @refresh_menu is true, the menu will be
 * redrawn.
 **/
void core_option_manager_set_val(core_option_manager_t *opt,
      size_t idx, size_t val_idx, bool refresh_menu)
{
   struct core_option *option = NULL;

   if (!opt ||
       (idx >= opt->size))
      return;

   option        = (struct core_option*)&opt->opts[idx];
   option->index = val_idx % option->vals->size;
   opt->updated  = true;

#ifdef HAVE_CHEEVOS
   rcheevos_validate_config_settings();
#endif

#ifdef HAVE_MENU
   /* Refresh menu (if required) if core option
    * visibility has changed as a result of modifying
    * the current option value */
   if (retroarch_ctl(RARCH_CTL_CORE_OPTION_UPDATE_DISPLAY, NULL) &&
       refresh_menu)
   {
      bool refresh = false;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   }
#endif
}

/**
 * core_option_manager_adjust_val:
 *
 * @opt          : options manager handle
 * @idx          : core option index
 * @adjustment   : offset to apply from current
 *                 value index
 * @refresh_menu : flag specifying whether menu
 *                 should be refreshed if changes
 *                 to option visibility are detected
 *
 * Modifies the value of the core option at
 * index @idx by incrementing the current option
 * value index by @adjustment.
 * After setting the option value, a request
 * will be made for the core to update the
 * in-menu visibility of all options; if
 * visibility changes are detected and
 * @refresh_menu is true, the menu will be
 * redrawn.
 **/
void core_option_manager_adjust_val(core_option_manager_t* opt,
      size_t idx, int adjustment, bool refresh_menu)
{
   struct core_option* option = NULL;

   if (!opt ||
       (idx >= opt->size))
      return;

   option        = (struct core_option*)&opt->opts[idx];
   option->index = (option->index + option->vals->size + adjustment) % option->vals->size;
   opt->updated  = true;

#ifdef HAVE_CHEEVOS
   rcheevos_validate_config_settings();
#endif

#ifdef HAVE_MENU
   /* Refresh menu (if required) if core option
    * visibility has changed as a result of modifying
    * the current option value */
   if (retroarch_ctl(RARCH_CTL_CORE_OPTION_UPDATE_DISPLAY, NULL) &&
       refresh_menu)
   {
      bool refresh = false;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   }
#endif
}

/**
 * core_option_manager_set_default:
 *
 * @opt          : options manager handle
 * @idx          : core option index
 * @refresh_menu : flag specifying whether menu
 *                 should be refreshed if changes
 *                 to option visibility are detected
 *
 * Resets the core option at index @idx to
 * its default value.
 * After setting the option value, a request
 * will be made for the core to update the
 * in-menu visibility of all options; if
 * visibility changes are detected and
 * @refresh_menu is true, the menu will be
 * redrawn.
 **/
void core_option_manager_set_default(core_option_manager_t *opt,
      size_t idx, bool refresh_menu)
{
   if (!opt ||
       (idx >= opt->size))
      return;

   opt->opts[idx].index = opt->opts[idx].default_index;
   opt->updated         = true;

#ifdef HAVE_CHEEVOS
   rcheevos_validate_config_settings();
#endif

#ifdef HAVE_MENU
   /* Refresh menu (if required) if core option
    * visibility has changed as a result of modifying
    * the current option value */
   if (retroarch_ctl(RARCH_CTL_CORE_OPTION_UPDATE_DISPLAY, NULL) &&
       refresh_menu)
   {
      bool refresh = false;
      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
      menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   }
#endif
}

/**
 * core_option_manager_set_visible:
 *
 * @opt     : options manager handle
 * @key     : core option key string (variable to query
 *            in RETRO_ENVIRONMENT_GET_VARIABLE)
 * @visible : flag specifying whether option should
 *            be shown in the menu
 *
 * Sets the in-menu visibility of the core option
 * identified by the specified @key.
 **/
void core_option_manager_set_visible(core_option_manager_t *opt,
      const char *key, bool visible)
{
   uint32_t key_hash;
   size_t i;

   if (!opt || string_is_empty(key))
      return;

   key_hash = core_option_manager_hash_string(key);

   for (i = 0; i < opt->size; i++)
   {
      struct core_option *option = &opt->opts[i];

      if ((key_hash == option->key_hash) &&
          !string_is_empty(option->key) &&
          string_is_equal(key, option->key))
      {
         option->visible = visible;
         return;
      }
   }
}

/**********************/
/* Configuration File */
/**********************/

/**
 * core_option_manager_flush:
 *
 * @opt  : options manager handle
 * @conf : configuration file handle
 *
 * Writes all core option key-pair values from the
 * specified core option manager handle to the
 * specified configuration file struct.
 **/
void core_option_manager_flush(core_option_manager_t *opt,
      config_file_t *conf)
{
   size_t i;

   for (i = 0; i < opt->size; i++)
   {
      struct core_option *option = (struct core_option*)&opt->opts[i];

      if (option)
         config_set_string(conf, option->key,
               opt->opts[i].vals->elems[opt->opts[i].index].data);
   }
}
