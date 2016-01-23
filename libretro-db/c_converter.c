#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "libretrodb.h"

void dat_converter_exit(int rc)
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

struct dat_converter_map_t
{
   const char* key;
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

dat_converter_list_t* dat_converter_list_create(dat_converter_list_enum type)
{
   dat_converter_list_t* list = malloc(sizeof(*list));
   list->type = type;
   list->count = 0;
   list->capacity = (1 << 2);
   list->values = (dat_converter_list_item_t*)malloc(sizeof(*list->values) * list->capacity);
   return list;
}

void dat_converter_list_free(dat_converter_list_t* list)
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
      break;
   default:
      break;
   }

   free(list->values);
   free(list);
}

void dat_converter_list_append(dat_converter_list_t* dst, void* item)
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
      int i;
      dat_converter_map_t* map = (dat_converter_map_t*) item;
      if(!map->key)
      {
         dst->values[dst->count].map = *map;
         break;
      }
      for (i = 0; i < dst->count; i++)
      {
         if (map->key && dst->values[i].map.key && !strcmp(dst->values[i].map.key, map->key))
            break;
      }
      if (i == dst->count)
         dst->values[dst->count].map = *map;
      else
      {
         if (dst->values[i].map.type == DAT_CONVERTER_LIST_MAP)
            dat_converter_list_free(dst->values[i].map.value.list);
         dst->values[i].map = *map;
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

void dat_converter_token_list_dump(dat_converter_list_t* list)
{
   int i;

   switch (list->type)
   {
   case DAT_CONVERTER_TOKEN_LIST:
      for (i = 0; i < list->count; i++)
         printf("\x1B[31m(%6i)@%s:%5i:%-5i\x1B[0m\"%s\"\n", i,
                list->values[i].token.fname,
                list->values[i].token.line_no,
                list->values[i].token.column,
                list->values[i].token.label);
      break;
   default:
      return;
   }

}

dat_converter_list_t* dat_converter_lexer(char* src, const char* dat_path)
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
   *src = '\0';

   token.label = NULL;
   dat_converter_list_append(token_list, &token);

   return token_list;
}

dat_converter_list_t* dat_parser_table(dat_converter_list_item_t** start_token)
{
   dat_converter_list_t* parsed_table = dat_converter_list_create(DAT_CONVERTER_MAP_LIST);
   const char* key = NULL;

   dat_converter_list_item_t* current = *start_token;

   while (current->token.label)
   {

      if (!key)
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
            key = current->token.label;
            current++;
         }
      }
      else
      {
         if (!strcmp(current->token.label, "("))
         {
            current++;
            dat_converter_map_t map;
            map.type = DAT_CONVERTER_LIST_MAP;
            map.key = key;
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
            dat_converter_map_t map;
            map.type = DAT_CONVERTER_STRING_MAP;
            map.key = key;
            map.value.string = current->token.label;
            dat_converter_list_append(parsed_table, &map);
            current++;
         }
         key = NULL;
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

const char* dat_converter_get_match_key(dat_converter_list_t* list, const char* match_key)
{
   int i;
   const char* res;
   char* key;
   char* dot;

   if (list->type != DAT_CONVERTER_MAP_LIST)
      return NULL;

   key = strdup(match_key);
   dot = strchr(key, '.');

   if (dot)
   {
      *dot = '\0';
      for (i = 0; i < list->count; i++)
         if (!strcmp(list->values[i].map.key, key))
         {
            if (list->values[i].map.type == DAT_CONVERTER_LIST_MAP)
            {
               res = dat_converter_get_match_key(list->values[i].map.value.list, dot + 1);
               free(key);
               return res;
            }
            break;
         }
   }
   else
   {
      for (i = 0; i < list->count; i++)
         if (!strcmp(list->values[i].map.key, key))
         {
            if ((list->values[i].map.type == DAT_CONVERTER_STRING_MAP))
            {
               free(key);
               return list->values[i].map.value.string;
            }
            break;
         }
   }

   return NULL;
}

dat_converter_list_t* dat_parser(dat_converter_list_t* target, dat_converter_list_t* lexer_list, const char* match_key)
{
   const char* key = NULL;
   bool skip = true;
   dat_converter_list_item_t* current = lexer_list->values;

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
      if (!key)
      {
         if (!strcmp(current->token.label, "game"))
            skip = false;
         key = current->token.label;
         current++;
      }
      else
      {
         if (!strcmp(current->token.label, "("))
         {
            current++;
            dat_converter_map_t map;
            map.type = DAT_CONVERTER_LIST_MAP;
            map.key = NULL;
            map.value.list = dat_parser_table(&current);
            if (!skip)
            {
               if (match_key)
               {
                  map.key = dat_converter_get_match_key(map.value.list, match_key);
                  if(!map.key)
                  {
                     printf("missing match key '%s' in one of the entries\n", match_key);
                     dat_converter_exit(1);
                  }
               }
               dat_converter_list_append(target, &map);
               skip = true;
            }
            else
               dat_converter_list_free(map.value.list);

            key = NULL;
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
   const char* match_key = NULL;
   RFILE* rdb_file;
   int i;

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

   if (argc > 1)
   {
      match_key = *argv;
      argc--;
      argv++;
   }

   int dat_count = argc;
   char** dat_buffers = (char**)malloc(dat_count * sizeof(*dat_buffers));
   dat_converter_list_t** dat_lexer_lists = (dat_converter_list_t**)malloc(dat_count * sizeof(*dat_lexer_lists));
   dat_converter_list_t* dat_parser_list = NULL;

   for (i = 0; i < dat_count; i++)
   {
      size_t dat_file_size;
      FILE* dat_file = fopen(argv[i], "r");

      if (!dat_file)
      {
         printf("could not open dat file '%s': %s\n", argv[i], strerror(errno));
         dat_converter_exit(1);
      }

      fseek(dat_file, 0, SEEK_END);
      dat_file_size = ftell(dat_file);
      fseek(dat_file, 0, SEEK_SET);
      dat_buffers[i] = (char*)malloc(dat_file_size + 1);
      fread(dat_buffers[i], 1, dat_file_size, dat_file);
      fclose(dat_file);
      dat_buffers[i][dat_file_size] = '\0';

      printf("Parsing dat file '%s'...\n", argv[i]);
      dat_lexer_lists[i] = dat_converter_lexer(dat_buffers[i], argv[i]);
      dat_parser_list = dat_parser(dat_parser_list, dat_lexer_lists[i], match_key);
      //      dat_converter_token_list_dump(*dat_lexer_dst);

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

   for (i = 0; i < dat_count; i++)
   {
      dat_converter_list_free(dat_lexer_lists[i]);
      free(dat_buffers[i]);
   }

   free(dat_lexer_lists);
   free(dat_buffers);
   dat_converter_list_free(dat_parser_list);


   return 0;
}

