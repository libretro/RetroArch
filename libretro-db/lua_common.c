#include "lua_common.h"

#include <stdlib.h>
#include <string.h>

int libretrodb_lua_to_rmsgpack_value(lua_State * L, int index, struct rmsgpack_dom_value * out)
{
   lua_Number tmp_num;
   size_t tmp_len;
   int i, rv = -1;
   const char * tmp_string = NULL;
   char * tmp_buff = NULL;
   struct rmsgpack_dom_value * tmp_value;
   const int key_idx = -2;
   const int value_idx = -1;
   const int MAX_FIELDS = 100;

   out->type = RDT_MAP;
   out->map.len = 0;
   out->map.items = calloc(MAX_FIELDS, sizeof(struct rmsgpack_dom_pair));
   lua_pushnil(L);

   while (lua_next(L, index - 1) != 0)
   {
      if (out->map.len > MAX_FIELDS)
         printf("skipping due to too many keys\n");
      else if (!lua_isstring(L, key_idx))
         printf("skipping non string key\n");
      else if (lua_isnil(L, value_idx))
      {
         // Skipping nil value fields to save disk space
      }
      else
      {
         i = out->map.len;
         tmp_buff = strdup(lua_tostring(L, key_idx));
         out->map.items[i].key.type = RDT_STRING;
         out->map.items[i].key.string.len = strlen(tmp_buff);
         out->map.items[i].key.string.buff = tmp_buff;

         tmp_value = &out->map.items[i].value;
         switch (lua_type(L, value_idx))
         {
            case LUA_TNUMBER:
               tmp_num = lua_tonumber(L, value_idx);
               tmp_value->type = RDT_INT;
               tmp_value->int_ = tmp_num;
               break;
            case LUA_TBOOLEAN:
               tmp_value->type = RDT_BOOL;
               tmp_value->bool_ = lua_toboolean(L, value_idx);
               break;
            case LUA_TSTRING:
               tmp_buff = strdup(lua_tostring(L, value_idx));
               tmp_value->type = RDT_STRING;
               tmp_value->string.len = strlen(tmp_buff);
               tmp_value->string.buff = tmp_buff;
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
                     tmp_value->uint_ = tmp_num;
                     lua_pop(L, 1);
                  }
               }
               else
               {
                  tmp_string = lua_tolstring(L, -1, &tmp_len);
                  tmp_buff = malloc(tmp_len);
                  memcpy(tmp_buff, tmp_string, tmp_len);
                  tmp_value->type = RDT_BINARY;
                  tmp_value->binary.len = tmp_len;
                  tmp_value->binary.buff = tmp_buff;
                  lua_pop(L, 1);
               }
               break;
            default:
set_nil:
               tmp_value->type = RDT_NULL;
         }
         out->map.len++;
      }
      lua_pop(L, 1);
   }
   rv = 0;
   return rv;
}
