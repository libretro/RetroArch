/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (logiqx_dat.c).
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

#include <file/file_path.h>
#include <string/stdstring.h>
#include <formats/rxml.h>

#include <formats/logiqx_dat.h>

/* Holds all internal DAT file data */
struct logiqx_dat
{
   rxml_document_t *data;
   rxml_node_t *current_node;
};

/* List of HTML formatting codes that must
 * be replaced when parsing XML data */
const char *logiqx_dat_html_code_list[][2] = {
   {"&amp;",  "&"},
   {"&apos;", "'"},
   {"&gt;",   ">"},
   {"&lt;",   "<"},
   {"&quot;", "\""}
};

#define LOGIQX_DAT_HTML_CODE_LIST_SIZE 5

/* Validation */

/* Performs rudimentary validation of the specified
 * Logiqx XML DAT file path (not rigorous - just
 * enough to prevent obvious errors).
 * Also provides access to file size (DAT files can
 * be very large, so it is useful to have this information
 * on hand - i.e. so we can check that the system has
 * enough free memory to load the file). */
bool logiqx_dat_path_is_valid(const char *path, uint64_t *file_size)
{
   const char *file_ext = NULL;
   int32_t file_size_int;

   if (string_is_empty(path))
      return false;

   /* Check file extension */
   file_ext = path_get_extension(path);

   if (string_is_empty(file_ext))
      return false;

   if (!string_is_equal_noncase(file_ext, "dat") &&
       !string_is_equal_noncase(file_ext, "xml"))
      return false;

   /* Ensure file exists */
   if (!path_is_valid(path))
      return false;

   /* Get file size */
   file_size_int = path_get_size(path);

   if (file_size_int <= 0)
      return false;

   if (file_size)
      *file_size = (uint64_t)file_size_int;

   return true;
}

/* File initialisation/de-initialisation */

/* Loads specified Logiqx XML DAT file from disk.
 * Returned logiqx_dat_t object must be free'd using
 * logiqx_dat_free().
 * Returns NULL if file is invalid or a read error
 * occurs. */
logiqx_dat_t *logiqx_dat_init(const char *path)
{
   logiqx_dat_t *dat_file = NULL;
   rxml_node_t *root_node = NULL;

   /* Check file path */
   if (!logiqx_dat_path_is_valid(path, NULL))
      goto error;

   /* Create logiqx_dat_t object */
   dat_file = (logiqx_dat_t*)calloc(1, sizeof(*dat_file));

   if (!dat_file)
      goto error;

   /* Read file from disk */
   dat_file->data = rxml_load_document(path);

   if (!dat_file->data)
      goto error;

   /* Ensure root node has the correct name */
   root_node = rxml_root_node(dat_file->data);

   if (!root_node)
      goto error;

   if (string_is_empty(root_node->name))
      goto error;

   /* > Logiqx XML uses:           'datafile'
    * > MAME List XML uses:        'mame'
    * > MAME 'Software List' uses: 'softwarelist' */
   if (!string_is_equal(root_node->name, "datafile") &&
       !string_is_equal(root_node->name, "mame") &&
       !string_is_equal(root_node->name, "softwarelist"))
      goto error;

   /* Get pointer to initial child node */
   dat_file->current_node = root_node->children;

   if (!dat_file->current_node)
      goto error;

   /* All is well - return logiqx_dat_t object */
   return dat_file;

error:
   logiqx_dat_free(dat_file);
   return NULL;
}

/* Frees specified DAT file */
void logiqx_dat_free(logiqx_dat_t *dat_file)
{
   if (!dat_file)
      return;

   dat_file->current_node = NULL;

   if (dat_file->data)
   {
      rxml_free_document(dat_file->data);
      dat_file->data = NULL;
   }

   free(dat_file);
   dat_file = NULL;
}

/* Game information access */

/* Returns true if specified node is a 'game' entry */
static bool logiqx_dat_is_game_node(rxml_node_t *node)
{
   const char *node_name = NULL;

   if (!node)
      return false;

   /* Check node name */
   node_name = node->name;

   if (string_is_empty(node_name))
      return false;

   /* > Logiqx XML uses:           'game'
    * > MAME List XML uses:        'machine'
    * > MAME 'Software List' uses: 'software' */
   return string_is_equal(node_name, "game") ||
          string_is_equal(node_name, "machine") ||
          string_is_equal(node_name, "software");
}

/* Returns true if specified node is a game
 * node containing information for a game with
 * the specified name */
static bool logiqx_dat_game_node_matches_name(
      rxml_node_t *node, const char *game_name)
{
   const char *node_game_name = NULL;

   if (!logiqx_dat_is_game_node(node) ||
       string_is_empty(game_name))
      return false;

   /* Get 'name' attribute of XML node */
   node_game_name = rxml_node_attrib(node, "name");

   if (string_is_empty(node_game_name))
      return false;

   return string_is_equal(node_game_name, game_name);
}

/* The XML element data strings returned from
 * DAT files are very 'messy'. This function
 * removes all cruft, replaces formatting strings
 * and copies the result (if valid) to 'str' */
static void logiqx_dat_sanitise_element_data(
      const char *data, char *str, size_t len)
{
   char sanitised_data[PATH_MAX_LENGTH];
   size_t i;

   sanitised_data[0] = '\0';

   if (string_is_empty(data))
      return;

   strlcpy(sanitised_data, data, sizeof(sanitised_data));

   /* Element data includes leading/trailing
    * newline characters - trim them away */
   string_trim_whitespace_right(sanitised_data);
   string_trim_whitespace_left(sanitised_data);

   if (string_is_empty(sanitised_data))
      return;

   /* XML has a number of special characters that
    * are handled using a HTML formatting codes.
    * All of these have to be replaced...
    * &amp;  -> &
    * &apos; -> '
    * &gt;   -> >
    * &lt;   -> <
    * &quot; -> "
    */
   for (i = 0; i < LOGIQX_DAT_HTML_CODE_LIST_SIZE; i++)
   {
      const char *find_string    = logiqx_dat_html_code_list[i][0];
      const char *replace_string = logiqx_dat_html_code_list[i][1];

      /* string_replace_substring() is expensive
       * > only invoke if element string contains
       *   HTML code */
      if (strstr(sanitised_data, find_string))
      {
         char *tmp = string_replace_substring(
               sanitised_data, strlen(sanitised_data),
               find_string,    strlen(find_string),
               replace_string, strlen(replace_string));

         if (!string_is_empty(tmp))
            strlcpy(sanitised_data, tmp, sizeof(sanitised_data));

         if (tmp)
            free(tmp);
      }
   }

   if (string_is_empty(sanitised_data))
      return;

   /* All is well - can copy result */
   strlcpy(str, sanitised_data, len);
}

/* Extracts game information from specified node.
 * Returns false if node is invalid */
static bool logiqx_dat_parse_game_node(
      rxml_node_t *node, logiqx_dat_game_info_t *game_info)
{
   const char *game_name   = NULL;
   const char *is_bios     = NULL;
   const char *is_runnable = NULL;
   rxml_node_t *info_node  = NULL;
   bool description_found  = false;
   bool year_found         = false;
   bool manufacturer_found = false;

   if (!logiqx_dat_is_game_node(node))
      return false;

   if (!game_info)
      return false;

   /* Initialise logiqx_dat_game_info_t object */
   game_info->name[0]         = '\0';
   game_info->description[0]  = '\0';
   game_info->year[0]         = '\0';
   game_info->manufacturer[0] = '\0';
   game_info->is_bios         = false;
   game_info->is_runnable     = true;

   /* Get game name */
   game_name = rxml_node_attrib(node, "name");

   if (!string_is_empty(game_name))
      strlcpy(game_info->name, game_name, sizeof(game_info->name));

   /* Get 'is bios' status */
   is_bios = rxml_node_attrib(node, "isbios");

   if (!string_is_empty(is_bios))
      game_info->is_bios = string_is_equal(is_bios, "yes");

   /* Get 'is runnable' status
    * > Note: This attribute only exists in MAME List
    *   XML files, but there is no harm in checking for
    *   it generally. For normal Logiqx XML files,
    *   'is runnable' is just the inverse of 'is bios' */
   is_runnable = rxml_node_attrib(node, "runnable");

   if (!string_is_empty(is_runnable))
      game_info->is_runnable = string_is_equal(is_runnable, "yes");
   else
      game_info->is_runnable = !game_info->is_bios;

   /* Loop over all game info nodes */
   for (info_node = node->children; info_node; info_node = info_node->next)
   {
      const char *info_node_name = info_node->name;
      const char *info_node_data = info_node->data;

      if (string_is_empty(info_node_name))
         continue;

      /* Check description */
      if (string_is_equal(info_node_name, "description"))
      {
         logiqx_dat_sanitise_element_data(
            info_node_data, game_info->description,
            sizeof(game_info->description));
         description_found = true;
      }
      /* Check year */
      else if (string_is_equal(info_node_name, "year"))
      {
         logiqx_dat_sanitise_element_data(
            info_node_data, game_info->year,
            sizeof(game_info->year));
         year_found = true;
      }
      /* Check manufacturer */
      else if (string_is_equal(info_node_name, "manufacturer"))
      {
         logiqx_dat_sanitise_element_data(
            info_node_data, game_info->manufacturer,
            sizeof(game_info->manufacturer));
         manufacturer_found = true;
      }

      /* If all required entries have been found,
       * can end loop */
      if (description_found && year_found && manufacturer_found)
         break;
   }

   return true;
}

/* Sets/resets internal node pointer to the first
 * entry in the DAT file */
void logiqx_dat_set_first(logiqx_dat_t *dat_file)
{
   rxml_node_t *root_node = NULL;

   if (!dat_file)
      return;

   if (!dat_file->data)
      return;

   /* Get root node */
   root_node = rxml_root_node(dat_file->data);

   if (!root_node)
   {
      dat_file->current_node = NULL;
      return;
   }

   /* Get pointer to initial child node */
   dat_file->current_node = root_node->children;
}

/* Fetches game information for the current entry
 * in the DAT file and increments the internal node
 * pointer.
 * Returns false if the end of the DAT file has been
 * reached (in which case 'game_info' will be invalid) */
bool logiqx_dat_get_next(
      logiqx_dat_t *dat_file, logiqx_dat_game_info_t *game_info)
{
   if (!dat_file || !game_info)
      return false;

   if (!dat_file->data)
      return false;

   while (dat_file->current_node)
   {
      rxml_node_t *current_node = dat_file->current_node;

      /* Whatever happens, internal node pointer must
       * be 'incremented' */
      dat_file->current_node = dat_file->current_node->next;

      /* If this is a game node, extract info
       * and return */
      if (logiqx_dat_is_game_node(current_node))
         return logiqx_dat_parse_game_node(current_node, game_info);
   }

   return false;
}

/* Fetches information for the specified game.
 * Returns false if game does not exist, or arguments
 * are invalid. */
bool logiqx_dat_search(
      logiqx_dat_t *dat_file, const char *game_name,
      logiqx_dat_game_info_t *game_info)
{
   rxml_node_t *root_node = NULL;
   rxml_node_t *game_node = NULL;

   if (!dat_file || !game_info || string_is_empty(game_name))
      return false;

   if (!dat_file->data)
      return false;

   /* Get root node */
   root_node = rxml_root_node(dat_file->data);

   if (!root_node)
      return false;

   /* Loop over all child nodes of the DAT file */
   for (game_node = root_node->children; game_node; game_node = game_node->next)
   {
      /* If this is the requested game, fetch info and return */
      if (logiqx_dat_game_node_matches_name(game_node, game_name))
         return logiqx_dat_parse_game_node(game_node, game_info);
   }

   return false;
}
