#ifndef __RARCHDB_LUA_COMMON_H__
#define __RARCHDB_LUA_COMMON_H__

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "rmsgpack_dom.h"

int libretrodb_lua_to_rmsgpack_value(lua_State *L, int index, struct rmsgpack_dom_value *out);

#endif
