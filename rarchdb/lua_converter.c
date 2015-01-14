#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "rarchdb.h"
int master_key = 1;

const char *LUA_COMMON = " \
function binary(s) if s ~= nil then return {binary = s} else return nil end end \
function uint(s) if s ~= nil then return {uint = s} else return nil end end \
";

static int call_init(lua_State *L, int argc, const char** argv) {
	int rv = -1;
	int i;

	lua_getglobal(L, "init");
	for (i = 0; i < argc; i++) {
		lua_pushstring(L, argv[i]);
	}

	if (lua_pcall(L, argc, 0, 0) != 0) {
		printf(
			"error running function `init': %s\n",
			lua_tostring(L, -1)
		);
	}

	return rv;
}


static int value_provider(void *ctx, struct rmsgpack_dom_value *out) {
	lua_State *L = ctx;

	int rv = -1;
	int i;
	const char *tmp_string = NULL;
	char *tmp_buff = NULL;
	struct rmsgpack_dom_value *tmp_value;
	const int key_idx = -2;
	const int value_idx = -1;
	const int MAX_FIELDS = 100;
	size_t tmp_len;
	lua_Number tmp_num;

	lua_getglobal(L, "get_value");

	if (lua_pcall(L, 0, 1, 0) != 0) {
		printf(
			"error running function `get_value': %s\n",
			lua_tostring(L, -1)
		);
	}

	if (lua_isnil(L, -1)) {
		rv = 1;
	} else if (lua_istable(L, -1)) {
		out->type = RDT_MAP;
		out->map.len = 0;
		out->map.items = calloc(MAX_FIELDS, sizeof(struct rmsgpack_dom_pair));
		lua_pushnil(L);
		while (lua_next(L, -2) != 0) {
			if (out->map.len > MAX_FIELDS) {
				printf("skipping due to too many keys\n");
			} else if (!lua_isstring(L, key_idx)) {
				printf("skipping non string key\n");
			} else if (lua_isnil(L, value_idx)) {
				// Skipping nil value fields to save disk space
			} else {
				i = out->map.len;
				tmp_buff = strdup(lua_tostring(L, key_idx));
				out->map.items[i].key.type = RDT_STRING;
				out->map.items[i].key.string.len = strlen(tmp_buff);
				out->map.items[i].key.string.buff = tmp_buff;

				tmp_value = &out->map.items[i].value;
				switch(lua_type(L, value_idx)) {
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
						if (!lua_isstring(L, -1)) {
							lua_pop(L, 1);
							lua_getfield(L, value_idx, "uint");
							if (!lua_isnumber(L, -1)) {
								lua_pop(L, 1);
								goto set_nil;
							} else {
								tmp_num = lua_tonumber(L, -1);
								tmp_value->type = RDT_UINT;
								tmp_value->uint_ = tmp_num;
								lua_pop(L, 1);
							}
						} else {
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
	} else {
		printf("function `get_value' must return a table or nil\n");
	}
	lua_pop(L, 1);
	return rv;
}

int main(int argc, char **argv)
{
	const char* db_file;
	const char* lua_file;
	int dst = -1;
	int rv = 0;

	if (argc < 3) {
		printf("usage:\n%s <db file> <lua file> [args ...]\n", argv[0]);
		return 1;
	}

	db_file = argv[1];
	lua_file = argv[2];

	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	luaL_dostring(L, LUA_COMMON);

	if (luaL_dofile(L, lua_file) != 0) {
		return 1;
	}

	call_init(L, argc - 2, (const char**) argv + 2);

	dst = open(db_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (dst == -1)
	{
		printf("Could not open destination file '%s': %s\n", db_file, strerror(errno));
		rv = errno;
		goto clean;
	}

	rv = rarchdb_create(dst, &value_provider, L);

clean:
	lua_close(L);
	if (dst != -1) {
		close(dst);
	}
	return rv;
}

