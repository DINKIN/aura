#define LUA_LIB

#include <aura/aura.h>
#include <search.h>
#include <lua.h>
#include <lauxlib.h>


int aura_typeerror (lua_State *L, int narg, const char *tname) 
{
	const char *msg = lua_pushfstring(L, "%s expected, got %s",
					  tname, luaL_typename(L, narg));
	return luaL_argerror(L, narg, msg);
}

static void aura_do_check_args (lua_State *L, const char *func, int need) 
{
	int got = lua_gettop(L);
	if (got < need) { 
		luaL_error(L, "%s expects %d args, %d given",
				  func, need, got);
	}
}

#define aura_check_args(L, need) aura_do_check_args(L, __FUNCTION__, need)

static int l_etable_create (lua_State *L) 
{
	struct aura_node *node;
	int count = 0;
	struct aura_export_table *tbl; 

	aura_check_args(L, 2);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	if (!lua_isnumber(L, 2)) {
		aura_typeerror(L, 1, "number");
	}

	node  = lua_touserdata(L, 1);
	count = lua_tonumber(L, 2); 
	tbl = aura_etable_create(node, count); 
	if (!tbl)
		return luaL_error(L, "error creating etable for %d elements", count);

	lua_pushlightuserdata(L, tbl);
	return 1;
}

static int l_etable_add (lua_State *L) 
{
	struct aura_export_table *tbl; 
	const char *name, *arg, *ret; 
 
	aura_check_args(L, 4);

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	if (!lua_isstring(L, 2)) {
		aura_typeerror(L, 1, "string");
	}
	if (!lua_isstring(L, 3)) {
		aura_typeerror(L, 1, "string");
	}
	if (!lua_isstring(L, 4)) {
		aura_typeerror(L, 1, "string");
	}

	tbl  = lua_touserdata(L, 1);
	name = lua_tostring(L,  2);
	arg  = lua_tostring(L,  3);
	ret  = lua_tostring(L,  4);	

	aura_etable_add(tbl, name, arg, ret);	
	return 0;
}

static int l_etable_activate(lua_State *L)
{
	struct aura_export_table *tbl; 
	aura_check_args(L, 1);

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}

	tbl = lua_touserdata(L, 1);
	aura_etable_activate(tbl);
	return 0;
}

static inline int check_and_push(lua_State *L, struct aura_node *node) 
{
	if (node)
		lua_pushlightuserdata(L, node);
	else
		lua_pushnil(L);
	return 1;
}

static int l_open_susb(lua_State *L)
{
	const char* cname;
	struct aura_node *node; 
	aura_check_args(L, 1);
	cname = lua_tostring(L, 1); 
	node = aura_open("simpleusb", cname);
	return check_and_push(L, node);
}

static int l_open_usb(lua_State *L)
{
	const char *vendor, *product, *serial;
	int vid, pid; 
	struct aura_node *node;
	if (lua_gettop(L) < 2)
		return luaL_error(L, "usb needs at least vid and pid to open");
	vid   = lua_tonumber(L, 1);
	pid   = lua_tonumber(L, 2);

	vendor  = lua_tostring(L, 3);
	product = lua_tostring(L, 4);
	serial  = lua_tostring(L, 5);

	node = aura_open("usb", vid, pid, vendor, product, serial);
	return check_and_push(L, node);
}

static int l_slog_init(lua_State *L)
{
	const char *fname;
	int level; 

	aura_check_args(L, 2);
	fname = lua_tostring(L, 1); 
	level = lua_tonumber(L, 2); 
	slog_init(fname, level);
	return 0;
}

static const luaL_Reg openfuncs[] = {
	{ "simpleusb", l_open_susb },
	{ "usb",       l_open_usb },
	{NULL, NULL}
};

static const luaL_Reg libfuncs[] = {
	{ "etable_create",   l_etable_create    },
	{ "etable_add",      l_etable_add       },
	{ "etable_activate", l_etable_activate  },
	{ "slog_init",       l_slog_init        },
	{NULL, NULL}
};


LUALIB_API int luaopen_auracore (lua_State *L) 
{
	printf("A.U.R.A. Extension loaded.\n");
	luaL_register(L, "auracore", libfuncs);
	lua_pushstring(L, "openfuncs");
	lua_newtable(L);
	luaL_register(L, NULL, openfuncs);	
	lua_settable(L, -3);
	return 0;
}
