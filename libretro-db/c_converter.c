/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (c_converter.c).
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <rhash.h>

#include <retro_assert.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>

#include "libretrodb.h"

static void dat_converter_exit(int rc)
{
   fflush(stdout);
   exit(rc);
}

typedef enum
{
   DAT_CONVERTER_STRING_MAP,
   DAT_CONVERTER_LIST_MAP,
} dat_converter_map_enum;

typedef enum
{
   DAT_CONVERTER_TOKEN_LIST,
   DAT_CONVERTER_STRING_LIST,
   DAT_CONVERTER_MAP_LIST,
   DAT_CONVERTER_LIST_LIST,
} dat_converter_list_enum;

typedef struct
{
   const char* label;
   int line_no;
   int column;
   const char* fname;
} dat_converter_token_t;

typedef struct dat_converter_map_t dat_converter_map_t;
typedef struct dat_converter_list_t dat_converter_list_t;
typedef union dat_converter_list_item_t dat_converter_list_item_t;
typedef struct dat_converter_search_tree_t dat_converter_search_tree_t;
typedef struct dat_converter_bt_node_t dat_converter_bt_node_t;

struct dat_converter_map_t
{
   const char* key;
   uint32_t hash;
   dat_converter_map_enum type;
   union
   {
      const char* string;
      dat_converter_list_t* list;
   } value;
};

struct dat_converter_list_t
{
   dat_converter_list_enum type;
   dat_converter_list_item_t* values;
   dat_converter_bt_node_t* bt_root;
   int count;
   int capacity;
};

union dat_converter_list_item_t
{
   const char* string;
   dat_converter_map_t map;
   dat_converter_token_t token;
   dat_converter_list_t* list;
};

struct dat_converter_bt_node_t
{
   int index;
   uint32_t hash;
   dat_converter_bt_node_t* right;
   dat_converter_bt_node_t* left;
};

static dat_converter_list_t* dat_converter_list_create(
      dat_converter_list_enum type)
{
   dat_converter_list_t* list = malloc(sizeof(*list));

   list->type                 = type;
   list->count                = 0;
   list->capacity             = (1 << 2);
   list->bt_root              = NULL;
   list->values               = (dat_converter_list_item_t*)malloc(
         sizeof(*list->values) * list->capacity);

   return list;
}

static void dat_converter_bt_node_free(dat_converter_bt_node_t* node)
{
   if (!node)
      return;

   dat_converter_bt_node_free(node->left);
   dat_converter_bt_node_free(node->right);
   free(node);
}

static void dat_converter_list_free(dat_converter_list_t* list)
{
   if (!list)
      return;
   switch (list->type)
   {
   case DAT_CONVERTER_LIST_LIST:
      while (list->count--)
         dat_converter_list_free(list->values[list->count].list);
      break;
   case DAT_CONVERTER_MAP_LIST:
      while (list->count--)
      {
         if (list->values[list->count].map.type == DAT_CONVERTER_LIST_MAP)
            dat_converter_list_free(list->values[list->count].map.value.list);
      }
      dat_converter_bt_node_free(list->bt_root);
      break;
   default:
      break;
   }

   free(list->values);
   free(list);
}
static void dat_converter_list_append(dat_converter_list_t* dst, void* item);

static dat_converter_bt_node_t* dat_converter_bt_node_insert(
      dat_converter_list_t* list,
      dat_converter_bt_node_t** node,
      dat_converter_map_t* map)
{
   retro_assert(map->key);
   retro_assert(list->type == DAT_CONVERTER_MAP_LIST);

   if (!*node)
   {
      *node = calloc(1, sizeof(dat_converter_bt_node_t));
      return *node;
   }

   int diff = (*node)->hash - map->hash;

   if (!diff)
      diff = strcmp(list->values[(*node)->index].map.key, map->key);

   if (diff < 0)
      return dat_converter_bt_node_insert(list, &(*node)->left, map);
   else if (diff > 0)
      return dat_converter_bt_node_insert(list, &(*node)->right, map);

   /* found match */

   if (list->values[(*node)->index].map.type == DAT_CONVERTER_LIST_MAP)
   {
      if (map->type == DAT_CONVERTER_LIST_MAP)
      {
         int i;

         retro_assert(list->values[(*node)->index].map.value.list->type
               == map->value.list->type);

         for (i = 0; i < map->value.list->count; i++)
            dat_converter_list_append(
                  list->values[(*node)->index].map.value.list,
                  &map->value.list->values[i]);

         /* set count to 0 to prevent freeing the child nodes */
         map->value.list->count = 0;
         dat_converter_list_free(map->value.list);
      }
   }
   else
      list->values[(*node)->index].map = *map;

   return NULL;
}

static void dat_converter_list_append(dat_converter_list_t* dst, void* item)
{
   if (dst->count == dst->capacity)
   {
      dst->capacity <<= 1;
      dst->values = realloc(dst->values, sizeof(*dst->values) * dst->capacity);
   }
   switch (dst->type)
   {
   case DAT_CONVERTER_TOKEN_LIST:
   {
      dat_converter_token_t* token = (dat_converter_token_t*) item;
      dst->values[dst->count].token = *token;
      break;
   }
   case DAT_CONVERTER_STRING_LIST:
   {
      char* str = (char*) item;
      dst->values[dst->count].string = str;
      break;
   }
   case DAT_CONVERTER_MAP_LIST:
   {
      dat_converter_map_t* map = (dat_converter_map_t*) item;
      if (!map->key)
         dst->values[dst->count].map = *map;
      else
      {
         map->hash = djb2_calculate(map->key);
         dat_converter_bt_node_t* new_node =
            dat_converter_bt_node_insert(dst, &dst->bt_root, map);

         if (!new_node)
            return;

         dst->values[dst->count].map = *map;
         new_node->index = dst->count;
         new_node->hash = map->hash;
      }
      break;
   }
   case DAT_CONVERTER_LIST_LIST:
   {
      dat_converter_list_t* list = (dat_converter_list_t*) item;
      dst->values[dst->count].list = list;
      break;
   }
   default:
      return;
   }
   dst->count++;
}

static dat_converter_list_t* dat_converter_lexer(
      char* src, const char* dat_path)
{
   dat_converter_list_t* token_list =
      dat_converter_list_create(DAT_CONVERTER_TOKEN_LIST);
   dat_converter_token_t token      = {NULL, 1, 1, dat_path};
   bool quoted_token                = false;

   while (*src)
   {
      if ((!quoted_token && (*src == '\t' || *src == ' ')) || (*src == '\r'))
      {
         *src = '\0';
         src++;
         token.column++;
         token.label = NULL;
         quoted_token = false;
         continue;
      }

      if (*src == '\n')
      {
         *src = '\0';
         src++;
         token.column = 1;
         token.line_no++;
         token.label = NULL;
         quoted_token = false;
         continue;
      }

      if (*src == '\"')
      {
         *src = '\0';
         src++;
         token.column++;
         quoted_token = !quoted_token;
         token.label = NULL;

         if (quoted_token)
         {
            token.label = src;
            dat_converter_list_append(token_list, &token);
         }

         continue;
      }

      if (!token.label)
      {
         token.label = src;
         dat_converter_list_append(token_list, &token);
      }

      src++;
      token.column++;
   }

   token.label = NULL;
   dat_converter_list_append(token_list, &token);

   return token_list;
}

static dat_converter_list_t* dat_parser_table(
      dat_converter_list_item_t** start_token)
{
   dat_converter_list_t* parsed_table =
      dat_converter_list_create(DAT_CONVERTER_MAP_LIST);
   dat_converter_map_t map            = {0};
   dat_converter_list_item_t* current = *start_token;

   while (current->token.label)
   {

      if (!map.key)
      {
         if (string_is_equal(current->token.label, ")"))
         {
            current++;
            *start_token = current;
            return parsed_table;
         }
         else if (string_is_equal(current->token.label, "("))
         {
            printf("%s:%d:%d: fatal error: Unexpected '(' instead of key\n",
                   current->token.fname,
                   current->token.line_no,
                   current->token.column);
            dat_converter_exit(1);
         }
         else
         {
            map.key = current->token.label;
            current++;
         }
      }
      else
      {
         if (string_is_equal(current->token.label, "("))
         {
            current++;
            map.type = DAT_CONVERTER_LIST_MAP;
            map.value.list = dat_parser_table(&current);
            dat_converter_list_append(parsed_table, &map);
         }
         else if (string_is_equal(current->token.label, ")"))
         {
            printf("%s:%d:%d: fatal error: Unexpected ')' instead of value\n",
                   current->token.fname,
                   current->token.line_no,
                   current->token.column);
            dat_converter_exit(1);
         }
         else
         {
            map.type = DAT_CONVERTER_STRING_MAP;
            map.value.string = current->token.label;
            dat_converter_list_append(parsed_table, &map);
            current++;
         }
         map.key = NULL;
      }
   }

   printf("%s:%d:%d: fatal error: Missing ')' for '('\n",
          (*start_token)->token.fname,
          (*start_token)->token.line_no,
          (*start_token)->token.column);
   dat_converter_exit(1);

   /* unreached */
   dat_converter_list_free(parsed_table);
   return NULL;
}

typedef struct dat_converter_match_key_t dat_converter_match_key_t;
struct dat_converter_match_key_t
{
   char* value;
   uint32_t hash;
   dat_converter_match_key_t* next;
};

static dat_converter_match_key_t* dat_converter_match_key_create(
      const char* format)
{
   dat_converter_match_key_t* match_key;
   dat_converter_match_key_t* current_mk;
   char* dot;

   match_key = malloc(sizeof(*match_key));
   match_key->value = strdup(format);
   match_key->next = NULL;
   current_mk = match_key;

   dot = match_key->value;
   while (*dot++)
   {
      if (*dot == '.')
      {
         *dot++ = '\0';
         current_mk->hash = djb2_calculate(current_mk->value);
         current_mk->next = malloc(sizeof(*match_key));
         current_mk = current_mk->next;
         current_mk->value = dot;
         current_mk->next = NULL;
      }
   }
   current_mk->hash = djb2_calculate(current_mk->value);

   return match_key;
}

static void dat_converter_match_key_free(dat_converter_match_key_t* match_key)
{
   if (!match_key)
      return;

   free(match_key->value);

   while (match_key)
   {
      dat_converter_match_key_t* next = match_key->next;
      free(match_key);
      match_key = next;
   }
}

static const char* dat_converter_get_match(
      dat_converter_list_t* list,
      dat_converter_match_key_t* match_key)
{
   int i;

   retro_assert(match_key);

   if (list->type != DAT_CONVERTER_MAP_LIST)
      return NULL;

   for (i = 0; i < list->count; i++)
   {
      if (list->values[i].map.hash == match_key->hash)
      {
         retro_assert(string_is_equal(list->values[i].map.key, match_key->value));

         if (match_key->next)
            return dat_converter_get_match(
                  list->values[i].map.value.list, match_key->next);

         if ((list->values[i].map.type == DAT_CONVERTER_STRING_MAP))
            return list->values[i].map.value.string;

         return NULL;

      }
   }
   return NULL;
}

static dat_converter_list_t* dat_converter_parser(
      dat_converter_list_t* target,
      dat_converter_list_t* lexer_list,
      dat_converter_match_key_t* match_key)
{
   dat_converter_map_t map;
   dat_converter_list_item_t* current = lexer_list->values;
   bool skip                          = true;
   bool warning_displayed             = false;

   map.key                            = NULL;
   map.type                           = DAT_CONVERTER_LIST_MAP;

   if (!target)
   {
      dat_converter_map_t map;
      target = dat_converter_list_create(DAT_CONVERTER_MAP_LIST);
      map.key = NULL;
      map.type = DAT_CONVERTER_LIST_MAP;
      map.value.list = NULL;
      dat_converter_list_append(target, &map);
   }

   while (current->token.label)
   {
      if (!map.key)
      {
         if (string_is_equal(current->token.label, "game"))
            skip = false;
         map.key = current->token.label;
         current++;
      }
      else
      {
         if (string_is_equal(current->token.label, "("))
         {
            current++;
            map.value.list = dat_parser_table(&current);
            if (!skip)
            {
               if (match_key)
               {
                  map.key = dat_converter_get_match(map.value.list, match_key);
                  // If the key is not found, report, and mark it to be skipped.
                  if (!map.key)
                  {
                     if (warning_displayed == false)
                     {
                        printf("    - Missing match key '");
                        while (match_key->next)
                        {
                           printf("%s.", match_key->value);
                           match_key = match_key->next;
                        }
                        printf("%s' on line %d\n", match_key->value, current->token.line_no);
                        warning_displayed = true;
                     }
                     skip = true;
                  }
               }
               else
                  map.key = NULL;

               // If we are still not to skip the entry, append it to the list.
               if (!skip) {
                  dat_converter_list_append(target, &map);
                  skip = true;
               }
            }
            else
               dat_converter_list_free(map.value.list);

            map.key = NULL;
         }
         else
         {
            printf("%s:%d:%d: fatal error: Expected '(' found '%s'\n",
                   current->token.fname,
                   current->token.line_no,
                   current->token.column,
                   current->token.label);
            dat_converter_exit(1);
         }
      }
   }
   return target;
}

typedef enum
{
   DAT_CONVERTER_RDB_TYPE_STRING,
   DAT_CONVERTER_RDB_TYPE_UINT,
   DAT_CONVERTER_RDB_TYPE_BINARY,
   DAT_CONVERTER_RDB_TYPE_HEX
} dat_converter_rdb_format_enum;

typedef struct
{
   const char* dat_key;
   const char* rdb_key;
   dat_converter_rdb_format_enum format;
} dat_converter_rdb_mappings_t;

dat_converter_rdb_mappings_t rdb_mappings[] =
{
   {"name",           "name",           DAT_CONVERTER_RDB_TYPE_STRING},
   {"description",    "description",    DAT_CONVERTER_RDB_TYPE_STRING},
   {"genre",          "genre",          DAT_CONVERTER_RDB_TYPE_STRING},
   {"rom.name",       "rom_name",       DAT_CONVERTER_RDB_TYPE_STRING},
   {"rom.size",       "size",           DAT_CONVERTER_RDB_TYPE_UINT},
   {"users",          "users",          DAT_CONVERTER_RDB_TYPE_UINT},
   {"releasemonth",   "releasemonth",   DAT_CONVERTER_RDB_TYPE_UINT},
   {"releaseyear",    "releaseyear",    DAT_CONVERTER_RDB_TYPE_UINT},
   {"rumble",         "rumble",         DAT_CONVERTER_RDB_TYPE_UINT},
   {"analog",         "analog",         DAT_CONVERTER_RDB_TYPE_UINT},

   {"famitsu_rating", "famitsu_rating", DAT_CONVERTER_RDB_TYPE_UINT},
   {"edge_rating",    "edge_rating",    DAT_CONVERTER_RDB_TYPE_UINT},
   {"edge_issue",     "edge_issue",     DAT_CONVERTER_RDB_TYPE_UINT},
   {"edge_review",    "edge_review",    DAT_CONVERTER_RDB_TYPE_STRING},

   {"enhancement_hw", "enhancement_hw", DAT_CONVERTER_RDB_TYPE_STRING},
   {"barcode",        "barcode",        DAT_CONVERTER_RDB_TYPE_STRING},
   {"esrb_rating",    "esrb_rating",    DAT_CONVERTER_RDB_TYPE_STRING},
   {"elspa_rating",   "elspa_rating",   DAT_CONVERTER_RDB_TYPE_STRING},
   {"pegi_rating",    "pegi_rating",    DAT_CONVERTER_RDB_TYPE_STRING},
   {"cero_rating",    "cero_rating",    DAT_CONVERTER_RDB_TYPE_STRING},
   {"franchise",      "franchise",      DAT_CONVERTER_RDB_TYPE_STRING},

   {"developer",      "developer",      DAT_CONVERTER_RDB_TYPE_STRING},
   {"publisher",      "publisher",      DAT_CONVERTER_RDB_TYPE_STRING},
   {"origin",         "origin",         DAT_CONVERTER_RDB_TYPE_STRING},

   {"coop",           "coop",           DAT_CONVERTER_RDB_TYPE_UINT},
   {"tgdb_rating",    "tgdb_rating",    DAT_CONVERTER_RDB_TYPE_UINT},

   {"rom.crc",        "crc",            DAT_CONVERTER_RDB_TYPE_HEX},
   {"rom.md5",        "md5",            DAT_CONVERTER_RDB_TYPE_HEX},
   {"rom.sha1",       "sha1",           DAT_CONVERTER_RDB_TYPE_HEX},
   {"serial",         "serial",         DAT_CONVERTER_RDB_TYPE_BINARY},
   {"rom.serial",     "serial",         DAT_CONVERTER_RDB_TYPE_BINARY}
};

dat_converter_match_key_t* rdb_mappings_mk[(sizeof(rdb_mappings)
      / sizeof(*rdb_mappings))] = {0};

static void dat_converter_value_provider_init(void)
{
   int i;
   for (i = 0; i < (sizeof(rdb_mappings) / sizeof(*rdb_mappings)); i++)
      rdb_mappings_mk[i] = dat_converter_match_key_create(rdb_mappings[i].dat_key);
}
static void dat_converter_value_provider_free(void)
{
   int i;
   for (i = 0; i < (sizeof(rdb_mappings) / sizeof(*rdb_mappings)); i++)
   {
      dat_converter_match_key_free(rdb_mappings_mk[i]);
      rdb_mappings_mk[i] = NULL;
   }
}
static int dat_converter_value_provider(
      dat_converter_list_item_t** current_item, struct rmsgpack_dom_value* out)
{
   int i;
   struct rmsgpack_dom_pair* current = NULL;

   out->type          = RDT_MAP;
   out->val.map.len   = 0;
   out->val.map.items = calloc((sizeof(rdb_mappings) / sizeof(*rdb_mappings)),
         sizeof(struct rmsgpack_dom_pair));

   (*current_item)--;

   retro_assert((*current_item)->map.type == DAT_CONVERTER_LIST_MAP);

   dat_converter_list_t* list = (*current_item)->map.value.list;

   if (!list)
      return 1;

   retro_assert(list->type == DAT_CONVERTER_MAP_LIST);

   current = out->val.map.items;

   for (i = 0; i < (sizeof(rdb_mappings) / sizeof(*rdb_mappings)); i++)
   {
      const char* value = dat_converter_get_match(list, rdb_mappings_mk[i]);
      if (!value)
         continue;

      current->key.type = RDT_STRING;
      current->key.val.string.len = strlen(rdb_mappings[i].rdb_key);
      current->key.val.string.buff = strdup(rdb_mappings[i].rdb_key);

      switch (rdb_mappings[i].format)
      {
      case DAT_CONVERTER_RDB_TYPE_STRING:
         current->value.type = RDT_STRING;
         current->value.val.string.len = strlen(value);
         current->value.val.string.buff = strdup(value);
         break;
      case DAT_CONVERTER_RDB_TYPE_UINT:
         current->value.type = RDT_UINT;
         if (value[strlen(value) - 1] == '\?')
         {
            free(current->key.val.string.buff);
            continue;
         }
         current->value.val.uint_ = (uint64_t)atoll(value);
         break;
      case DAT_CONVERTER_RDB_TYPE_BINARY:
         current->value.type = RDT_BINARY;
         current->value.val.binary.len = strlen(value);
         current->value.val.binary.buff = strdup(value);
         break;
      case DAT_CONVERTER_RDB_TYPE_HEX:
         current->value.type = RDT_BINARY;
         current->value.val.binary.len  = strlen(value) / 2;
         current->value.val.binary.buff =
            malloc(current->value.val.binary.len);
         {
            const char* hex_char = value;
            char* out_buff       = current->value.val.binary.buff;
            while (*hex_char && *(hex_char + 1))
            {
               char val = 0;
               if (*hex_char >= 'A' && *hex_char <= 'F')
                  val = *hex_char + 0xA - 'A';
               else if (*hex_char >= 'a' && *hex_char <= 'f')
                  val = *hex_char + 0xA - 'a';
               else if (*hex_char >= '0' && *hex_char <= '9')
                  val = *hex_char - '0';
               else
                  val = 0;
               val <<= 4;
               hex_char++;
               if (*hex_char >= 'A' && *hex_char <= 'F')
                  val |= *hex_char + 0xA - 'A';
               else if (*hex_char >= 'a' && *hex_char <= 'f')
                  val |= *hex_char + 0xA - 'a';
               else if (*hex_char >= '0' && *hex_char <= '9')
                  val |= *hex_char - '0';

               *out_buff++ = val;
               hex_char++;
            }
         }
         break;
      default:
         retro_assert(0);
         break;
      }
      current++;
   }

   out->val.map.len = current - out->val.map.items;
   return 0;
}

int main(int argc, char** argv)
{
   const char* rdb_path;
   dat_converter_match_key_t* match_key = NULL;
   RFILE* rdb_file;

   if (argc < 2)
   {
      printf("usage:\n%s <db file> [args ...]\n", *argv);
      dat_converter_exit(1);
   }
   argc--;
   argv++;

   rdb_path  = *argv;
   argc--;
   argv++;

   if (argc > 1 &&** argv)
   {
      match_key = dat_converter_match_key_create(*argv);
      argc--;
      argv++;
   }

   int dat_count                         = argc;
   char** dat_buffers                    = (char**)
      malloc(dat_count * sizeof(*dat_buffers));
   char** dat_buffer                     = dat_buffers;
   dat_converter_list_t* dat_parser_list = NULL;

   while (argc)
   {
      size_t dat_file_size;
      dat_converter_list_t* dat_lexer_list = NULL;
      FILE* dat_file                       = fopen(*argv, "r");

      if (!dat_file)
      {
         printf("  could not open dat file '%s': %s\n",
               *argv, strerror(errno));
         dat_converter_exit(1);
      }

      fseek(dat_file, 0, SEEK_END);
      dat_file_size = ftell(dat_file);
      fseek(dat_file, 0, SEEK_SET);
      *dat_buffer = (char*)malloc(dat_file_size + 1);
      fread(*dat_buffer, 1, dat_file_size, dat_file);
      fclose(dat_file);
      (*dat_buffer)[dat_file_size] = '\0';

      printf("  %s\n", *argv);
      dat_lexer_list  = dat_converter_lexer(*dat_buffer, *argv);
      dat_parser_list = dat_converter_parser(
            dat_parser_list, dat_lexer_list, match_key);

      dat_converter_list_free(dat_lexer_list);

      argc--;
      argv++;
      dat_buffer++;
   }

   rdb_file = filestream_open(rdb_path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!rdb_file)
   {
      printf(
         "Could not open destination file '%s': %s\n",
         rdb_path,
         strerror(errno)
      );
      dat_converter_exit(1);
   }

   dat_converter_list_item_t* current_item =
      &dat_parser_list->values[dat_parser_list->count];

   dat_converter_value_provider_init();
   libretrodb_create(rdb_file,
         (libretrodb_value_provider)&dat_converter_value_provider,
         &current_item);
   dat_converter_value_provider_free();

   filestream_close(rdb_file);

   dat_converter_list_free(dat_parser_list);

   while (dat_count--)
      free(dat_buffers[dat_count]);
   free(dat_buffers);

   dat_converter_match_key_free(match_key);

   return 0;
}
