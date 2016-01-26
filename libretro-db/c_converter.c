#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <rhash.h>

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

static dat_converter_list_t* dat_converter_list_create(dat_converter_list_enum type)
{
   dat_converter_list_t* list = malloc(sizeof(*list));
   list->type = type;
   list->count = 0;
   list->capacity = (1 << 2);
   list->values = (dat_converter_list_item_t*)malloc(sizeof(*list->values) * list->capacity);
   list->bt_root = NULL;
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

static dat_converter_bt_node_t* dat_converter_bt_node_insert(dat_converter_list_t* list, dat_converter_bt_node_t** node,
      dat_converter_map_t* map)
{
   assert(map->key);
   assert(list->type == DAT_CONVERTER_MAP_LIST);

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
      dat_converter_list_free(list->values[(*node)->index].map.value.list);

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
         dat_converter_bt_node_t* new_node = dat_converter_bt_node_insert(dst, &dst->bt_root, map);
         if (new_node)
         {
            dst->values[dst->count].map = *map;
            new_node->index = dst->count;
            new_node->hash = map->hash;
         }
         else
            return;
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

static dat_converter_list_t* dat_converter_lexer(char* src, const char* dat_path)
{


   dat_converter_list_t* token_list = dat_converter_list_create(DAT_CONVERTER_TOKEN_LIST);
   dat_converter_token_t token = {NULL, 1, 1, dat_path};

   bool quoted_token = false;

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

static dat_converter_list_t* dat_parser_table(dat_converter_list_item_t** start_token)
{
   dat_converter_list_t* parsed_table = dat_converter_list_create(DAT_CONVERTER_MAP_LIST);
   dat_converter_map_t map = {0};
   dat_converter_list_item_t* current = *start_token;

   while (current->token.label)
   {

      if (!map.key)
      {
         if (!strcmp(current->token.label, ")"))
         {
            current++;
            *start_token = current;
            return parsed_table;
         }
         else if (!strcmp(current->token.label, "("))
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
         if (!strcmp(current->token.label, "("))
         {
            current++;
            map.type = DAT_CONVERTER_LIST_MAP;
            map.value.list = dat_parser_table(&current);
            dat_converter_list_append(parsed_table, &map);
         }
         else if (!strcmp(current->token.label, ")"))
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

static dat_converter_match_key_t* dat_converter_match_key_create(const char* format)
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
   if(!match_key)
      return;

   free(match_key->value);

   while(match_key)
   {
      dat_converter_match_key_t* next = match_key->next;
      free(match_key);
      match_key = next;
   }
}

static const char* dat_converter_get_match(dat_converter_list_t* list, dat_converter_match_key_t* match_key)
{
   int i;

   assert(match_key);

   if (list->type != DAT_CONVERTER_MAP_LIST)
      return NULL;

   for (i = 0; i < list->count; i++)
   {
      if(list->values[i].map.hash == match_key->hash)
      {
         assert(!strcmp(list->values[i].map.key, match_key->value));

         if(match_key->next)
            return dat_converter_get_match(list->values[i].map.value.list, match_key->next);
         else  if ((list->values[i].map.type == DAT_CONVERTER_STRING_MAP))
            return list->values[i].map.value.string;
         else
            return NULL;

      }
   }
   return NULL;
}

static dat_converter_list_t* dat_converter_parser(dat_converter_list_t* target, dat_converter_list_t* lexer_list,
      dat_converter_match_key_t* match_key)
{
   bool skip = true;
   dat_converter_list_item_t* current = lexer_list->values;
   dat_converter_map_t map;

   map.key = NULL;
   map.type = DAT_CONVERTER_LIST_MAP;

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
         if (!strcmp(current->token.label, "game"))
            skip = false;
         map.key = current->token.label;
         current++;
      }
      else
      {
         if (!strcmp(current->token.label, "("))
         {
            current++;
            map.value.list = dat_parser_table(&current);
            if (!skip)
            {
               if (match_key)
               {
                  map.key = dat_converter_get_match(map.value.list, match_key);
                  if (!map.key)
                  {
                     printf("missing match key '");
                     while(match_key->next)
                     {
                        printf("%s.", match_key->value);
                        match_key = match_key->next;
                     }
                     printf("%s' in one of the entries\n", match_key->value);
                     dat_converter_exit(1);
                  }
               }
               else
                  map.key = NULL;

               dat_converter_list_append(target, &map);
               skip = true;
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


static int value_provider(dat_converter_list_item_t** current_item, struct rmsgpack_dom_value* out)
{
   int i, j;
#if 1
   const char* ordered_keys[] =
   {
      "name",
      "description",
      "rom_name",
      "size",
      "users",
      "releasemonth",
      "releaseyear",
      "rumble",
      "analog",

      "famitsu_rating",
      "edge_rating",
      "edge_issue",
      "edge_review",

      "enhancement_hw",
      "barcode",
      "esrb_rating",
      "elspa_rating",
      "pegi_rating",
      "cero_rating",
      "franchise",

      "developer",
      "publisher",
      "origin",

      "crc",
      "md5",
      "sha1",
      "serial"
   };
#endif
   out->type = RDT_MAP;
   out->val.map.len = 0;
   out->val.map.items = calloc(sizeof(ordered_keys) / sizeof(char*), sizeof(struct rmsgpack_dom_pair));

   (*current_item)--;

   assert((*current_item)->map.type == DAT_CONVERTER_LIST_MAP);

   dat_converter_list_t* list = (*current_item)->map.value.list;

   if (!list)
      return 1;

   assert(list->type == DAT_CONVERTER_MAP_LIST);

   struct rmsgpack_dom_pair* current = out->val.map.items;

   for (i = 0; i < list->count; i++)
   {
      dat_converter_map_t* pair = &list->values[i].map;

      if (pair->type == DAT_CONVERTER_STRING_MAP)
      {
         current->key.type = RDT_STRING;
         current->key.val.string.len = strlen(pair->key);
         current->key.val.string.buff = strdup(pair->key);

         if (!strcmp(pair->key, "users") ||
               !strcmp(pair->key, "releasemonth") ||
               !strcmp(pair->key, "releaseyear") ||
               !strcmp(pair->key, "rumble") ||
               !strcmp(pair->key, "analog") ||
               !strcmp(pair->key, "famitsu_rating") ||
               !strcmp(pair->key, "edge_rating") ||
               !strcmp(pair->key, "edge_issue"))
         {
            current->value.type = RDT_UINT;
            if (pair->value.string[strlen(pair->value.string) - 1] == '\?')
               continue;
            current->value.val.uint_ = (uint64_t)atoll(pair->value.string);
         }
         else if (!strcmp(pair->key, "serial"))
         {
            current->value.type = RDT_BINARY;
            current->value.val.binary.len = strlen(pair->value.string);
            current->value.val.binary.buff = strdup(pair->value.string);
         }
         else
         {
            current->value.type = RDT_STRING;
            current->value.val.string.len = strlen(pair->value.string);
            current->value.val.string.buff = strdup(pair->value.string);
         }

         current++;
      }
      else if ((pair->type == DAT_CONVERTER_LIST_MAP) && (!strcmp(pair->key, "rom")))
      {
         assert(pair->value.list->type == DAT_CONVERTER_MAP_LIST);

         for (j = 0; j < pair->value.list->count; j++)
         {
            dat_converter_map_t* rom_pair = &pair->value.list->values[j].map;
            assert(rom_pair->type == DAT_CONVERTER_STRING_MAP);

            if (!strcmp(rom_pair->key, "name"))
            {
               current->key.type = RDT_STRING;
               current->key.val.string.len = strlen("rom_name");
               current->key.val.string.buff = strdup("rom_name");

               current->value.type = RDT_STRING;
               current->value.val.string.len = strlen(rom_pair->value.string);
               current->value.val.string.buff = strdup(rom_pair->value.string);
            }
            else
            {
               current->key.type = RDT_STRING;
               current->key.val.string.len = strlen(rom_pair->key);
               current->key.val.string.buff = strdup(rom_pair->key);

               if (!strcmp(rom_pair->key, "size"))
               {
                  current->value.type = RDT_UINT;
                  current->value.val.uint_ = (uint64_t)atoll(rom_pair->value.string);

               }
               else
               {
                  current->value.type = RDT_BINARY;

                  if (!strcmp(rom_pair->key, "serial"))
                  {
                     current->value.val.binary.len = strlen(rom_pair->value.string);
                     current->value.val.binary.buff = strdup(rom_pair->value.string);
                  }
                  else
                  {
                     current->value.val.binary.len = strlen(rom_pair->value.string) / 2;
                     current->value.val.binary.buff = malloc(current->value.val.binary.len);
                     const char* hex_char = rom_pair->value.string;
                     char* out_buff = current->value.val.binary.buff;
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
               }
            }
            current++;
         }

      }
      else
         assert(0);
   }

   out->val.map.len = current - out->val.map.items;
#if 1
   /* re-order to match lua_converter */
   struct rmsgpack_dom_pair* ordered_pairs = calloc(out->val.map.len, sizeof(struct rmsgpack_dom_pair));
   struct rmsgpack_dom_pair* ordered_pairs_outp = ordered_pairs;
   for (i = 0; i < (sizeof(ordered_keys) / sizeof(char*)); i++)
   {
      int j;
      for (j = 0; j < out->val.map.len; j++)
      {
         if (!strcmp(ordered_keys[i], out->val.map.items[j].key.val.string.buff))
         {
            *ordered_pairs_outp++ = out->val.map.items[j];
            break;
         }
      }
   }

   free(out->val.map.items);
   out->val.map.items = ordered_pairs;
   out->val.map.len = ordered_pairs_outp - ordered_pairs;
#endif
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

   if (argc > 1 && **argv)
   {
      match_key = dat_converter_match_key_create(*argv);
      argc--;
      argv++;
   }

   int dat_count = argc;
   char** dat_buffers = (char**)malloc(dat_count * sizeof(*dat_buffers));
   char** dat_buffer = dat_buffers;
   dat_converter_list_t* dat_parser_list = NULL;

   while (argc)
   {
      dat_converter_list_t* dat_lexer_list;
      size_t dat_file_size;
      FILE* dat_file = fopen(*argv, "r");

      if (!dat_file)
      {
         printf("could not open dat file '%s': %s\n", *argv, strerror(errno));
         dat_converter_exit(1);
      }

      fseek(dat_file, 0, SEEK_END);
      dat_file_size = ftell(dat_file);
      fseek(dat_file, 0, SEEK_SET);
      *dat_buffer = (char*)malloc(dat_file_size + 1);
      fread(*dat_buffer, 1, dat_file_size, dat_file);
      fclose(dat_file);
      (*dat_buffer)[dat_file_size] = '\0';

      printf("Parsing dat file '%s'...\n", *argv);
      dat_lexer_list = dat_converter_lexer(*dat_buffer, *argv);
      dat_parser_list = dat_converter_parser(dat_parser_list, dat_lexer_list, match_key);
      dat_converter_list_free(dat_lexer_list);

      argc--;
      argv++;
      dat_buffer++;
   }

   rdb_file = retro_fopen(rdb_path, RFILE_MODE_WRITE, -1);

   if (!rdb_file)
   {
      printf(
         "Could not open destination file '%s': %s\n",
         rdb_path,
         strerror(errno)
      );
      dat_converter_exit(1);
   }

   dat_converter_list_item_t* current_item = &dat_parser_list->values[dat_parser_list->count];
   libretrodb_create(rdb_file, (libretrodb_value_provider)&value_provider, &current_item);

   retro_fclose(rdb_file);

   dat_converter_list_free(dat_parser_list);

   while (dat_count--)
      free(dat_buffers[dat_count]);
   free(dat_buffers);

   dat_converter_match_key_free(match_key);


   return 0;
}

