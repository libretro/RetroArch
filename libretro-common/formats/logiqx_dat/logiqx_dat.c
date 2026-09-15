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

#include <stdint.h>

#include <string/stdstring.h>
#include <formats/rxml.h>

#include <formats/logiqx_dat.h>

/* One slot of the search index: a game node keyed by its 'name'
 * attribute.  A NULL name is an empty slot.  Duplicate names keep
 * the first occurrence - the same answer the linear scan gives. */
typedef struct
{
   const char *name;   /* points into the document's own storage */
   rxml_node_t *node;
} logiqx_dat_index_entry_t;

/* Holds all internal DAT file data */
struct logiqx_dat
{
   rxml_document_t *data;
   rxml_node_t *current_node;
   /* Search index, built on the first logiqx_dat_search() (or
    * incrementally by logiqx_dat_parse_step()): without it every
    * search walks every child of the root, which is
    * O(files x games) for the scanner's per-file label lookups and
    * quadratic into MAME-sized lists.  Open-addressing hash keyed
    * on the name, power-of-two capacity, load factor at most 1/2,
    * first occurrence wins on insert.  Exact lookup is all
    * logiqx_dat_search() needs, and unlike the sorted array this
    * replaces, the build is a chunkable O(n) fill with no qsort
    * spike.  On any allocation failure index_failed is set and the
    * linear walk remains the answer. */
   logiqx_dat_index_entry_t *index;
   size_t index_cap;
   bool index_built;
   bool index_failed;
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

/* Initialisation/de-initialisation */

/* Parses the specified Logiqx XML DAT document held in
 * memory.  Takes ownership of @xml_data (heap, len + 1
 * bytes, NUL-terminated at len); it is released by
 * logiqx_dat_free(), or here on failure.  Reading the
 * document from disk is the caller's job - this module
 * performs no file I/O.
 * Returns NULL if the document is not valid Logiqx/MAME
 * XML. */
logiqx_dat_t *logiqx_dat_init_owned(char *xml_data, size_t len)
{
   logiqx_dat_t *dat_file = NULL;
   rxml_node_t *root_node = NULL;

   if (!xml_data)
      return NULL;

   /* Create logiqx_dat_t object.  On this one failure
    * path the buffer has not yet been handed to rxml,
    * so it is freed here to honour the ownership
    * contract. */
   if (!(dat_file = (logiqx_dat_t*)calloc(1, sizeof(*dat_file))))
   {
      free(xml_data);
      return NULL;
   }

   /* Parse document.  rxml_load_document_owned takes
    * the buffer: the tree points into it on success,
    * and it is freed on failure. */
   dat_file->data = rxml_load_document_owned(xml_data, len);

   if (!dat_file->data)
      goto error;

   /* Ensure root node has the correct name */
   root_node = rxml_root_node(dat_file->data);

   if (!root_node)
      goto error;

   if (!root_node->name || !*root_node->name)
      goto error;

   /* > Logiqx XML uses:           'datafile'
    * > MAME List XML uses:        'mame'
    * > MAME 'Software List' uses: 'softwarelist' */
   if (   !string_is_equal(root_node->name, "datafile")
       && !string_is_equal(root_node->name, "mame")
       && !string_is_equal(root_node->name, "softwarelist"))
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

   if (dat_file->index)
   {
      free(dat_file->index);
      dat_file->index = NULL;
   }

   if (dat_file->data)
   {
      rxml_free_document(dat_file->data);
      dat_file->data = NULL;
   }

   free(dat_file);
   dat_file = NULL;
}

/* ------------------------------------------------------------------ */
/* Incremental initialisation                                          */
/* ------------------------------------------------------------------ */

static bool logiqx_dat_is_game_node(rxml_node_t *node);
static size_t logiqx_dat_index_count_chunk(rxml_node_t **cursor,
      size_t max_nodes);
static bool logiqx_dat_index_alloc(logiqx_dat_t *dat_file, size_t count);
static void logiqx_dat_index_fill_chunk(logiqx_dat_t *dat_file,
      rxml_node_t **cursor, size_t max_nodes);

enum
{
   LOGIQX_PARSE_PHASE_XML = 0,   /* stepping the rxml parse */
   LOGIQX_PARSE_PHASE_COUNT,     /* counting named game nodes */
   LOGIQX_PARSE_PHASE_FILL,      /* filling the hash index */
   LOGIQX_PARSE_PHASE_DONE,
   LOGIQX_PARSE_PHASE_FAILED
};

struct logiqx_dat_parse
{
   rxml_parse_t *xml;        /* live during the XML phase */
   logiqx_dat_t *dat_file;   /* built at the XML verdict */
   rxml_node_t *cursor;      /* count/fill resume position */
   size_t count;
   unsigned phase;
};

/* An index visit (name-attribute walk plus one table probe) costs
 * far less than parsing a byte of XML; mapping the caller's byte
 * budget to nodes at this ratio keeps index steps comparable in
 * cost to parse steps.  The floor guarantees forward progress. */
#define LOGIQX_INDEX_NODES_PER_BYTE_SHIFT 4
#define LOGIQX_INDEX_NODES_FLOOR          256

logiqx_dat_parse_t *logiqx_dat_parse_begin_owned(char *xml_data,
      size_t len)
{
   logiqx_dat_parse_t *parse;

   if (!xml_data)
      return NULL;

   if (!(parse = (logiqx_dat_parse_t*)calloc(1, sizeof(*parse))))
   {
      free(xml_data);
      return NULL;
   }

   /* rxml_parse_begin_owned() takes the buffer under the same
    * contract as the one-shot path: it is freed on failure. */
   if (!(parse->xml = rxml_parse_begin_owned(xml_data, len, 0)))
   {
      free(parse);
      return NULL;
   }

   parse->phase = LOGIQX_PARSE_PHASE_XML;
   return parse;
}

/* Validation logiqx_dat_init_owned() performs at the parse verdict:
 * root element name and a non-empty child list.  On success the
 * logiqx_dat_t takes ownership of @doc; on failure @doc is freed. */
static logiqx_dat_t *logiqx_dat_wrap_document(rxml_document_t *doc)
{
   logiqx_dat_t *dat_file = NULL;
   rxml_node_t *root_node = NULL;

   if (!(dat_file = (logiqx_dat_t*)calloc(1, sizeof(*dat_file))))
   {
      rxml_free_document(doc);
      return NULL;
   }
   dat_file->data = doc;

   if (!(root_node = rxml_root_node(doc)))
      goto error;
   if (!root_node->name || !*root_node->name)
      goto error;
   if (   !string_is_equal(root_node->name, "datafile")
       && !string_is_equal(root_node->name, "mame")
       && !string_is_equal(root_node->name, "softwarelist"))
      goto error;
   if (!(dat_file->current_node = root_node->children))
      goto error;

   return dat_file;

error:
   logiqx_dat_free(dat_file);
   return NULL;
}

int logiqx_dat_parse_step(logiqx_dat_parse_t *parse, size_t max_work)
{
   size_t max_nodes;

   if (!parse)
      return -1;

   max_nodes = max_work >> LOGIQX_INDEX_NODES_PER_BYTE_SHIFT;
   if (!max_work)
      max_nodes = 0;                        /* unbudgeted */
   else if (max_nodes < LOGIQX_INDEX_NODES_FLOOR)
      max_nodes = LOGIQX_INDEX_NODES_FLOOR;

   switch (parse->phase)
   {
      case LOGIQX_PARSE_PHASE_XML:
      {
         int r = rxml_parse_step(parse->xml, max_work);
         if (r == 0)
            return 0;
         if (r < 0)
         {
            rxml_parse_end(parse->xml);     /* discards; NULL back */
            parse->xml   = NULL;
            parse->phase = LOGIQX_PARSE_PHASE_FAILED;
            return -1;
         }
         {
            rxml_document_t *doc = rxml_parse_end(parse->xml);
            parse->xml = NULL;
            if (!doc || !(parse->dat_file = logiqx_dat_wrap_document(doc)))
            {
               parse->phase = LOGIQX_PARSE_PHASE_FAILED;
               return -1;
            }
         }
         parse->cursor = rxml_root_node(parse->dat_file->data)->children;
         parse->count  = 0;
         parse->phase  = LOGIQX_PARSE_PHASE_COUNT;
         return 0;
      }

      case LOGIQX_PARSE_PHASE_COUNT:
         parse->count += logiqx_dat_index_count_chunk(&parse->cursor,
               max_nodes);
         if (parse->cursor)
            return 0;
         /* The incremental path is the index's one build attempt:
          * mirror the lazy build's accounting so a later search
          * neither rebuilds nor bypasses it. */
         parse->dat_file->index_built  = true;
         parse->dat_file->index_failed = false;
         if (!logiqx_dat_index_alloc(parse->dat_file, parse->count))
         {
            /* Fallback preserved: searches take the linear walk. */
            parse->dat_file->index_failed = true;
            parse->phase = LOGIQX_PARSE_PHASE_DONE;
            return 1;
         }
         parse->cursor = rxml_root_node(parse->dat_file->data)->children;
         parse->phase  = LOGIQX_PARSE_PHASE_FILL;
         return 0;

      case LOGIQX_PARSE_PHASE_FILL:
         logiqx_dat_index_fill_chunk(parse->dat_file, &parse->cursor,
               max_nodes);
         if (parse->cursor)
            return 0;
         parse->phase = LOGIQX_PARSE_PHASE_DONE;
         return 1;

      case LOGIQX_PARSE_PHASE_DONE:
         return 1;

      default:
         return -1;
   }
}

logiqx_dat_t *logiqx_dat_parse_end(logiqx_dat_parse_t *parse)
{
   logiqx_dat_t *dat_file = NULL;

   if (!parse)
      return NULL;

   if (parse->phase == LOGIQX_PARSE_PHASE_DONE)
      dat_file = parse->dat_file;
   else
   {
      if (parse->xml)
         rxml_parse_abort(parse->xml);
      logiqx_dat_free(parse->dat_file);   /* NULL-safe */
   }
   free(parse);
   return dat_file;
}

void logiqx_dat_parse_abort(logiqx_dat_parse_t *parse)
{
   if (!parse)
      return;
   if (parse->xml)
      rxml_parse_abort(parse->xml);
   logiqx_dat_free(parse->dat_file);
   free(parse);
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

   if (!node_name || !*node_name)
      return false;

   /* > Logiqx XML uses:           'game'
    * > MAME List XML uses:        'machine'
    * > MAME 'Software List' uses: 'software' */
   return    string_is_equal(node_name, "game")
          || string_is_equal(node_name, "machine")
          || string_is_equal(node_name, "software");
}

/* Returns true if specified node is a game
 * node containing information for a game with
 * the specified name */
static bool logiqx_dat_game_node_matches_name(
      rxml_node_t *node, const char *game_name)
{
   const char *node_game_name = NULL;
   if (  !logiqx_dat_is_game_node(node)
       || (!game_name || !*game_name))
      return false;
   /* Get 'name' attribute of XML node */
   node_game_name = rxml_node_attrib(node, "name");
   if (!node_game_name || !*node_game_name)
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
   if (!data || !*data)
      return;
   strlcpy(sanitised_data, data, sizeof(sanitised_data));
   /* Element data includes leading/trailing
    * newline characters - trim them away */
   string_trim_whitespace_right(sanitised_data);
   string_trim_whitespace_left(sanitised_data);
   if (!*sanitised_data)
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

         if (tmp)
         {
            if (*tmp)
               strlcpy(sanitised_data, tmp, sizeof(sanitised_data));
            free(tmp);
         }
      }
   }

   /* All is well - can copy result */
   if (*sanitised_data)
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

   if (game_name && *game_name)
      strlcpy(game_info->name, game_name, sizeof(game_info->name));

   /* Get 'is bios' status */
   is_bios = rxml_node_attrib(node, "isbios");

   if (is_bios && *is_bios)
      game_info->is_bios = string_is_equal(is_bios, "yes");

   /* Get 'is runnable' status
    * > Note: This attribute only exists in MAME List
    *   XML files, but there is no harm in checking for
    *   it generally. For normal Logiqx XML files,
    *   'is runnable' is just the inverse of 'is bios' */
   is_runnable = rxml_node_attrib(node, "runnable");

   if (is_runnable && *is_runnable)
      game_info->is_runnable = string_is_equal(is_runnable, "yes");
   else
      game_info->is_runnable = !game_info->is_bios;

   /* Loop over all game info nodes */
   for (info_node = node->children; info_node; info_node = info_node->next)
   {
      const char *info_node_name = info_node->name;
      const char *info_node_data = info_node->data;

      if (!info_node_name || !*info_node_name)
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
static uint32_t logiqx_dat_name_hash(const char *s)
{
   uint32_t h = 5381;
   while (*s)
      h = ((h << 5) + h) ^ (uint32_t)(unsigned char)*s++;
   return h;
}

/* Walk up to @max_nodes children from *@cursor, counting game nodes
 * that carry a non-empty 'name' attribute - the only nodes the
 * linear scan could ever match.  @max_nodes of 0 walks to the end.
 * Advances *@cursor; returns the count for this chunk. */
static size_t logiqx_dat_index_count_chunk(rxml_node_t **cursor,
      size_t max_nodes)
{
   size_t count   = 0;
   size_t visited = 0;
   rxml_node_t *n = *cursor;

   for (; n && (!max_nodes || visited < max_nodes); n = n->next, visited++)
   {
      if (logiqx_dat_is_game_node(n))
      {
         const char *name = rxml_node_attrib(n, "name");
         if (name && *name)
            count++;
      }
   }
   *cursor = n;
   return count;
}

/* Size the table for @count names at a load factor of at most 1/2.
 * A count of zero leaves the capacity zero: an empty index is a
 * valid one - every search misses, which is what the linear scan
 * would conclude too.  Returns false on allocation failure. */
static bool logiqx_dat_index_alloc(logiqx_dat_t *dat_file, size_t count)
{
   size_t cap;

   if (!count)
      return true;

   for (cap = 64; cap < count * 2; cap <<= 1)
      ;

   if (!(dat_file->index = (logiqx_dat_index_entry_t*)
         calloc(cap, sizeof(*dat_file->index))))
      return false;

   dat_file->index_cap = cap;
   return true;
}

/* Insert up to @max_nodes children from *@cursor into the table.
 * First occurrence wins: an equal name already present is left
 * alone, preserving the leftmost-match answer of the linear scan.
 * @max_nodes of 0 walks to the end.  Advances *@cursor. */
static void logiqx_dat_index_fill_chunk(logiqx_dat_t *dat_file,
      rxml_node_t **cursor, size_t max_nodes)
{
   size_t visited = 0;
   rxml_node_t *n = *cursor;

   for (; n && (!max_nodes || visited < max_nodes); n = n->next, visited++)
   {
      const char *name;
      size_t idx;

      if (!logiqx_dat_is_game_node(n))
         continue;
      if (!(name = rxml_node_attrib(n, "name")) || !*name)
         continue;

      idx = (size_t)logiqx_dat_name_hash(name)
            & (dat_file->index_cap - 1);
      for (;;)
      {
         if (!dat_file->index[idx].name)
         {
            dat_file->index[idx].name = name;
            dat_file->index[idx].node = n;
            break;
         }
         if (string_is_equal(dat_file->index[idx].name, name))
            break;   /* duplicate: first occurrence stays */
         idx = (idx + 1) & (dat_file->index_cap - 1);
      }
   }
   *cursor = n;
}

static rxml_node_t *logiqx_dat_index_lookup(logiqx_dat_t *dat_file,
      const char *game_name)
{
   size_t idx;

   if (!dat_file->index_cap)
      return NULL;

   idx = (size_t)logiqx_dat_name_hash(game_name)
         & (dat_file->index_cap - 1);
   while (dat_file->index[idx].name)
   {
      if (string_is_equal(dat_file->index[idx].name, game_name))
         return dat_file->index[idx].node;
      idx = (idx + 1) & (dat_file->index_cap - 1);
   }
   return NULL;
}

/* Build the whole index in one call: the lazy path taken by the
 * first logiqx_dat_search() when the incremental parse did not
 * already build it. */
static void logiqx_dat_index_build(logiqx_dat_t *dat_file)
{
   rxml_node_t *root_node = NULL;
   rxml_node_t *cursor    = NULL;
   size_t count           = 0;

   dat_file->index_failed = true;   /* cleared on success */
   dat_file->index_built  = true;   /* one attempt only, either way */

   if (!(root_node = rxml_root_node(dat_file->data)))
      return;

   cursor = root_node->children;
   count  = logiqx_dat_index_count_chunk(&cursor, 0);

   if (!logiqx_dat_index_alloc(dat_file, count))
      return;

   cursor = root_node->children;
   logiqx_dat_index_fill_chunk(dat_file, &cursor, 0);

   dat_file->index_failed = false;
}

bool logiqx_dat_search(
      logiqx_dat_t *dat_file, const char *game_name,
      logiqx_dat_game_info_t *game_info)
{
   rxml_node_t *root_node = NULL;
   rxml_node_t *game_node = NULL;

   if (!dat_file || !game_info || (!game_name || !*game_name))
      return false;

   if (!dat_file->data)
      return false;

   /* Build the name index on first use; on failure fall through to
    * the linear walk below, which remains authoritative. */
   if (!dat_file->index_built)
      logiqx_dat_index_build(dat_file);

   if (!dat_file->index_failed)
   {
      rxml_node_t *game = logiqx_dat_index_lookup(dat_file, game_name);
      if (game)
         return logiqx_dat_parse_game_node(game, game_info);
      return false;
   }

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
