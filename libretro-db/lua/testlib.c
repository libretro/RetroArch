#define LUA_LIB

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <streams/file_stream.h>

#include "lua.h"
#include "lauxlib.h"

#include "libretrodb.h"
#include "lua_common.h"

struct libretrodb
{
	RFILE *fd;
	uint64_t root;
	uint64_t count;
	uint64_t first_index_offset;
   char path[1024];
};

static void push_rmsgpack_value(lua_State *L, struct rmsgpack_dom_value *value)
{
   uint32_t i;

   switch(value->type)
   {
      case RDT_INT:
         lua_pushnumber(L, value->val.int_);
         break;
      case RDT_UINT:
         lua_pushnumber(L, value->val.uint_);
         break;
      case RDT_BINARY:
         lua_pushlstring(L, value->val.binary.buff, value->val.binary.len);
         break;
      case RDT_BOOL:
         lua_pushboolean(L, value->val.bool_);
         break;
      case RDT_NULL:
         lua_pushnil(L);
         break;
      case RDT_STRING:
         lua_pushlstring(L, value->val.string.buff, value->val.binary.len);
         break;
      case RDT_MAP:
         lua_createtable(L, 0, value->val.map.len);
         for (i = 0; i < value->val.map.len; i++)
         {
            push_rmsgpack_value(L, &value->val.map.items[i].key);
            push_rmsgpack_value(L, &value->val.map.items[i].value);
            lua_settable(L, -3);
         }
         break;
      case RDT_ARRAY:
         lua_createtable(L, value->val.array.len, 0);
         for (i = 0; i < value->val.array.len; i++)
         {
            lua_pushnumber(L, i + 1);
            push_rmsgpack_value(L, &value->val.array.items[i]);
            lua_settable(L, -3);
         }
         break;
   }
}

static int value_provider(void *ctx, struct rmsgpack_dom_value *out)
{
   int rv       = 0;
   lua_State *L = ctx;

   lua_getfield(L, LUA_REGISTRYINDEX, "testlib_get_value");

   if (lua_pcall(L, 0, 1, 0) != 0)
   {
      printf(
            "error running function `get_value': %s\n",
            lua_tostring(L, -1)
            );
   }

   if (lua_isnil(L, -1))
      rv = 1;
   else if (lua_istable(L, -1))
      rv = libretrodb_lua_to_rmsgpack_value(L, -1, out);
   else
      printf("function `get_value' must return a table or nil\n");

   lua_pop(L, 1);
   return rv;
}

static int create_db(lua_State *L)
{
   RFILE *dst;
   const char *db_file = luaL_checkstring(L, -2);

   if (!lua_isfunction(L, -1))
   {
      lua_pushstring(L, "second argument must be a function");
      lua_error(L);
   }
   lua_setfield(L, LUA_REGISTRYINDEX, "testlib_get_value");

   dst = filestream_open(db_file,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!dst)
   {
      lua_pushstring(L, "Could not open destination file");
      lua_error(L);
   }

   libretrodb_create(dst, &value_provider, L);
   filestream_close(dst);

   return 0;
}

static int db_new(lua_State *L)
{
   libretrodb_t *db = NULL;
   const char *db_file = NULL;
   int rv;
   db_file = luaL_checkstring(L, -1);
   db = lua_newuserdata(L, sizeof(libretrodb_t));
   if ((rv = libretrodb_open(db_file, db)) == 0)
   {
      luaL_getmetatable(L, "RarchDB.DB");
      lua_setmetatable(L, -2);
      lua_pushnil(L);
   }
   else
   {
      lua_pop(L, 1);
      lua_pushnil(L);
      lua_pushstring(L, strerror(-rv));
   }
   return 2;
}

static libretrodb_t *checkdb(lua_State *L)
{
	void *ud = luaL_checkudata(L, 1, "RarchDB.DB");
	luaL_argcheck(L, ud != NULL, 1, "`RarchDB.DB' expected");
	return ud;
}

static int db_close(lua_State *L)
{
	libretrodb_t *db = checkdb(L);
	libretrodb_close(db);
	return 0;
}

static int db_cursor_open(lua_State *L)
{
   int rv;
   libretrodb_cursor_t *cursor = NULL;
   libretrodb_t *db = checkdb(L);
   cursor = lua_newuserdata(L, sizeof(libretrodb_t));
   if ((rv = libretrodb_cursor_open(db, cursor, NULL)) == 0)
   {
      luaL_getmetatable(L, "RarchDB.Cursor");
      lua_setmetatable(L, -2);
      lua_pushnil(L);
   }
   else
   {
      lua_pop(L, 1);
      lua_pushnil(L);
      lua_pushstring(L, strerror(-rv));
   }
   return 2;
}

static int db_query(lua_State *L)
{
   libretrodb_t            *db = checkdb(L);
   const char           *query = luaL_checkstring(L, -1);
   const char           *error = NULL;
   libretrodb_query_t       *q = libretrodb_query_compile(
         db, query, strlen(query), &error);

   if (error)
   {
      lua_pushnil(L);
      lua_pushstring(L, error);
   }
   else
   {
      int rv;
      libretrodb_cursor_t *cursor = lua_newuserdata(L, sizeof(libretrodb_t));

      if ((rv = libretrodb_cursor_open(db, cursor, q)) == 0)
      {
         luaL_getmetatable(L, "RarchDB.Cursor");
         lua_setmetatable(L, -2);
         lua_pushnil(L);
      }
      else
      {
         lua_pop(L, 1);
         lua_pushnil(L);
         lua_pushstring(L, strerror(-rv));
      }
      libretrodb_query_free(q);
   }
   return 2;
}

static libretrodb_cursor_t *checkcursor(lua_State *L)
{
	void *ud = luaL_checkudata(L, 1, "RarchDB.Cursor");
	luaL_argcheck(L, ud != NULL, 1, "`RarchDB.Cursor' expected");
	return ud;
}

static int cursor_close(lua_State *L)
{
	libretrodb_cursor_t *cursor = checkcursor(L);
	libretrodb_cursor_close(cursor);
	return 0;
}

static int cursor_read(lua_State *L)
{
   libretrodb_cursor_t *cursor = checkcursor(L);
   struct rmsgpack_dom_value value;
   if (libretrodb_cursor_read_item(cursor, &value) == 0)
      push_rmsgpack_value(L, &value);
   else
      lua_pushnil(L);
   return 1;
}

static int cursor_iter(lua_State *L)
{
   libretrodb_cursor_t *cursor = checkcursor(L);

   (void)cursor;

   luaL_getmetafield(L, -1, "read");
   lua_pushvalue(L, -2);
   return 2;
}

static const luaL_Reg testlib[] = {
	{"create_db", create_db},
	{"RarchDB", db_new},
	{NULL, NULL}
};

static const struct luaL_Reg cursor_mt [] = {
	{"__gc", cursor_close},
	{"read", cursor_read},
	{"iter", cursor_iter},
	{NULL, NULL}
};

static const struct luaL_Reg libretrodb_mt [] = {
	{"__gc", db_close},
	{"list_all", db_cursor_open},
	{"query", db_query},
	{NULL, NULL}
};

LUALIB_API int luaopen_testlib(lua_State *L)
{
	luaL_newmetatable(L, "RarchDB.DB");
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);
	lua_settable(L, -3);
	luaL_openlib(L, NULL, libretrodb_mt, 0);

	luaL_newmetatable(L, "RarchDB.Cursor");
	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);
	lua_settable(L, -3);
	luaL_openlib(L, NULL, cursor_mt, 0);

	luaL_register(L, "testlib", testlib);
	return 1;
}
