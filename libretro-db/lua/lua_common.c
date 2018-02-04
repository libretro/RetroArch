#include "lua_common.h"

#include <stdlib.h>
#include <string.h>

#include <string/stdstring.h>

int libretrodb_lua_to_rmsgpack_value(lua_State *L, int index,
      struct rmsgpack_dom_value * out)
{
   size_t tmp_len;
   lua_Number tmp_num;
   struct rmsgpack_dom_value * tmp_value;
   int i, rv               = -1;
   const char * tmp_string = NULL;
   char * tmp_buff         = NULL;
   const int key_idx       = -2;
   const int value_idx     = -1;
   const int MAX_FIELDS    = 100;

   out->type = RDT_MAP;
   out->val.map.len = 0;
   out->val.map.items = calloc(MAX_FIELDS, sizeof(struct rmsgpack_dom_pair));
   lua_pushnil(L);
   while (lua_next(L, index - 1) != 0)
   {
      if (out->val.map.len > MAX_FIELDS)
         printf("skipping due to too many keys\n");
      else if (!lua_isstring(L, key_idx))
         printf("skipping non string key\n");
      else if (lua_isnil(L, value_idx))
      {
         /* Skipping nil value fields to save disk space */
      }
      else
      {
         i = out->val.map.len;
         tmp_buff = strdup(lua_tostring(L, key_idx));
         out->val.map.items[i].key.type = RDT_STRING;
         out->val.map.items[i].key.val.string.len = strlen(tmp_buff);
         out->val.map.items[i].key.val.string.buff = tmp_buff;

         tmp_value = &out->val.map.items[i].value;
         switch (lua_type(L, value_idx))
         {
            case LUA_TNUMBER:
               tmp_num = lua_tonumber(L, value_idx);
               tmp_value->type = RDT_INT;
               tmp_value->val.int_ = tmp_num;
               break;
            case LUA_TBOOLEAN:
               tmp_value->type = RDT_BOOL;
               tmp_value->val.bool_ = lua_toboolean(L, value_idx);
               break;
            case LUA_TSTRING:
               tmp_buff = strdup(lua_tostring(L, value_idx));
               tmp_value->type = RDT_STRING;
               tmp_value->val.string.len = strlen(tmp_buff);
               tmp_value->val.string.buff = tmp_buff;
               break;
            case LUA_TTABLE:
               lua_getfield(L, value_idx, "binary");
               if (!lua_isstring(L, -1))
               {
                  lua_pop(L, 1);
                  lua_getfield(L, value_idx, "uint");
                  if (!lua_isnumber(L, -1))
                  {
                     lua_pop(L, 1);
                     goto set_nil;
                  }
                  else
                  {
                     tmp_num = lua_tonumber(L, -1);
                     tmp_value->type = RDT_UINT;
                     tmp_value->val.uint_ = tmp_num;
                     lua_pop(L, 1);
                  }
               }
               else
               {
                  tmp_string = lua_tolstring(L, -1, &tmp_len);
                  tmp_buff = malloc(tmp_len);
                  memcpy(tmp_buff, tmp_string, tmp_len);
                  tmp_value->type = RDT_BINARY;
                  tmp_value->val.binary.len = tmp_len;
                  tmp_value->val.binary.buff = tmp_buff;
                  lua_pop(L, 1);
               }
               break;
            default:
set_nil:
               tmp_value->type = RDT_NULL;
         }
         out->val.map.len++;
      }
      lua_pop(L, 1);
   }
#if 1
   /* re-order to avoid random output each run */
   struct rmsgpack_dom_pair* ordered_pairs = calloc(out->val.map.len, sizeof(struct rmsgpack_dom_pair));
   struct rmsgpack_dom_pair* ordered_pairs_outp = ordered_pairs;
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
   for(i = 0; i < (sizeof(ordered_keys)/sizeof(char*)); i++)
   {
      int j;
      for(j = 0; j < out->val.map.len; j++)
      {
         if(string_is_equal(ordered_keys[i], out->val.map.items[j].key.val.string.buff))
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

   rv = 0;
   return rv;
}
