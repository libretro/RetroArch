#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <streams/file_stream.h>

#include "libretrodb.h"
#include "lua_common.h"

int master_key = 1;

const char * LUA_COMMON = " \
function binary(s) if s ~= nil then return {binary = s} else return nil end end \
function uint(s) if s ~= nil then return {uint = s} else return nil end end \
";

static int call_init(lua_State * L, int argc, const char ** argv)
{
   int rv = -1;
   int i;

   lua_getglobal(L, "init");
   for (i = 0; i < argc; i++)
      lua_pushstring(L, argv[i]);

   if (lua_pcall(L, argc, 0, 0) != 0)
   {
      printf(
            "error running function `init': %s\n",
            lua_tostring(L, -1)
            );
   }

   return rv;
}

static int value_provider(void * ctx, struct rmsgpack_dom_value *out)
{
   int rv        = 0;
   lua_State * L = ctx;

   lua_getglobal(L, "get_value");

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

int main(int argc, char ** argv)
{
   lua_State *L;
   const char *db_file;
   const char *lua_file;
   RFILE *dst;
   int rv = 0;

   if (argc < 3)
   {
      printf("usage:\n%s <db file> <lua file> [args ...]\n", argv[0]);
      return 1;
   }

   db_file  = argv[1];
   lua_file = argv[2];
   L        = luaL_newstate();

   luaL_openlibs(L);
   luaL_dostring(L, LUA_COMMON);

   if (luaL_dofile(L, lua_file) != 0)
      return 1;

   call_init(L, argc - 2, (const char **) argv + 2);

   dst = filestream_open(db_file,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!dst)
   {
      printf(
            "Could not open destination file '%s': %s\n",
            db_file,
            strerror(errno)
            );
      rv = errno;
      goto clean;
   }

   rv = libretrodb_create(dst, &value_provider, L);

clean:
   lua_close(L);
   filestream_close(dst);
   return rv;
}
