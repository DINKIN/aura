#define LUA_LIB

#include <aura/aura.h>
#include <aura/private.h>
#include <search.h>
#include <lua.h>
#include <lauxlib.h>

#define REF_NODE_CONTAINER (1<<0)
#define REF_STATUS_CB      (1<<1)
#define REF_ETABLE_CB      (1<<2)
#define REF_EVENT_CB       (1<<3)

#define TRACE_BCALLS

#ifdef TRACE_BCALLS
#define TRACE() slog(0, SLOG_DEBUG, "Bindings call: %s", __func__)
#else 
#define TRACE()
#endif

#define aura_check_args(L, need) aura_do_check_args(L, __FUNCTION__, need)

#define laura_eventloop_type "laura_eventloop"
#define laura_node_type      "laura_node"

struct laura_node { 
	lua_State *L;
	struct aura_node *node;
	const char *current_call;
	uint32_t refs;
	int node_container; /* lua table representing this node */
	int status_changed_ref;
	int status_changed_arg_ref;
	int etable_changed_ref;
	int etable_changed_arg_ref;
	int inbound_event_ref;
	int inbound_event_arg_ref;
};

static inline int check_node_and_push(lua_State *L, struct aura_node *node) 
{
	if (node) {
		struct laura_node *bdata = lua_newuserdata(L, sizeof(*bdata));
		if (!bdata)
			return luaL_error(L, "Memory allocation error");
		bzero(bdata, sizeof(*bdata));
		bdata->L = L;
		bdata->node = node; 
		aura_set_userdata(node, bdata);
		luaL_setmetatable(L, laura_node_type);
	} else {  
		lua_pushnil(L);
	}
	return 1;
}


#define PREPARE_ARG(n)							\
	const void *arg ## n ##_ptr;					\
	const int arg ## n ##_int;					\
	if (lua_gettop(L) > (1 + n)) {					\
		if (lua_isstring(L, (1 + n)))				\
			arg ## n ## _ptr = lua_tostring(L, (1 + n));	\
		else if (lua_isnumber(L, (1 + n)))			\
			arg ## n ## _int = lua_tonumber(L, (1 + n));	\
	}								\
									\

#define ARG(n) (lua_isstring(L, 1 + n) ? arg ## n ## _ptr : arg ## n ## _int)


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


static void lua_setfield_string(lua_State *L, const char *key, const char *value)
{
	lua_pushstring(L, key);
	lua_pushstring(L, value);
	lua_settable(L, -3);
} 

static void lua_setfield_int(lua_State *L, const char *key, long value)
{
	lua_pushstring(L, key);
	lua_pushnumber(L, value);
	lua_settable(L, -3);
} 

static void lua_setfield_bool(lua_State *L, const char *key, bool value)
{
	lua_pushstring(L, key);
	lua_pushboolean(L, value);
	lua_settable(L, -3);
} 

static int lua_push_etable(lua_State *L, struct aura_export_table *tbl)
{
	int i;
	if (!tbl) { 
		lua_pushnil(L);
		return 1;
	}

	lua_newtable(L);
	for (i=0; i<tbl->next; i++) { 
		struct aura_object *o = &tbl->objects[i];
		lua_pushinteger(L, i);
		lua_newtable(L);

		lua_setfield_string(L, "name",  o->name);
		lua_setfield_bool(L,   "valid", o->valid);
		lua_setfield_int(L,    "id",    o->id);
		if (o->arg_fmt)
			lua_setfield_string(L, "arg", o->arg_fmt);
		if (o->ret_fmt)
			lua_setfield_string(L, "ret", o->ret_fmt);
		if (o->arg_pprinted)
			lua_setfield_string(L, "arg_pprint", o->arg_pprinted);
		if (o->arg_pprinted)
			lua_setfield_string(L, "ret_pprint", o->arg_pprinted);

		lua_settable(L, -3);
	}
	return 1;	
}

static int buffer_to_lua(lua_State *L, struct aura_node *node, struct aura_object *o, struct aura_buffer *buf)
{
	const char *fmt = o->ret_fmt;
	int nargs = 0; 
		
	while (*fmt) { 
		double tmp;
		switch (*fmt++) { 
		case URPC_U8:
			tmp = aura_buffer_get_u8(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_S8:
			tmp = aura_buffer_get_s8(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_U16:
			tmp = aura_buffer_get_u16(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_S16:
			tmp = aura_buffer_get_s16(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_U32:
			tmp = aura_buffer_get_u32(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_S32:
			tmp = aura_buffer_get_s32(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_U64:
			tmp = aura_buffer_get_u64(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_S64:
			tmp = aura_buffer_get_s64(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_BIN:
		{
			void *udata;
			const void *srcdata; 
			int len = atoi(fmt);

			if (len == 0) 
				BUG(node, "Internal deserilizer bug processing: %s", fmt);
			udata = lua_newuserdata(L, len);
			if (!udata)
				BUG(node, "Failed to allocate userdata");
			srcdata = aura_buffer_get_bin(buf, len);
			memcpy(udata, srcdata, len);
			break;
			while (*fmt && (*fmt++ != '.'));
		}
		default:
			BUG(node, "Unexpected format token: %s", --fmt);
		}
		nargs++;
	};

	return nargs;
}

static struct aura_buffer *lua_to_buffer(lua_State *L, struct aura_node *node, int stackpos, struct aura_object *o)
{
	int id, ret, i;
	struct aura_buffer *buf;
	const char *fmt;

	fmt = o->arg_fmt;
	slog(0, SLOG_DEBUG, "fmt %s", fmt);
	if (lua_gettop(L) - stackpos + 1 != o->num_args) {
		slog(0, SLOG_ERROR, "Invalid argument count for %s: %d / %d", 
		     o->name, lua_gettop(L) - stackpos, o->num_args);
		return NULL;
	}
	
	buf = aura_buffer_request(node, o->arglen);
	if (!buf) {
		slog(0, SLOG_ERROR, "Epic fail during buffer allocation");
		return NULL;
	}
	
	/* Let's serialize the data, arguments are on the stack, 
	 * Starting from #3.
	 */

	for (i=stackpos; i<=lua_gettop(L); i++) { 
		double tmp; 
		
		switch (*fmt++) { 
		case URPC_U8:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_u8(buf, tmp);
			break;
		case URPC_S8:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_s8(buf, tmp);
			break;
		case URPC_U16:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_u16(buf, (uint16_t) tmp);
			break;
		case URPC_S16:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_s16(buf, tmp);
			break;
		case URPC_U32:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_u32(buf, tmp);
			break;
		case URPC_S32:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_s32(buf, tmp);
			break;
		case URPC_S64:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_s64(buf, tmp);
			break;
		case URPC_U64:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_u64(buf, tmp);
			break;
			
			/* Binary is the tricky part. String or usata? */	
		case URPC_BIN:
		{
			const char *srcbuf; 
			int len = 0; 
			int blen;
			if (lua_isstring(L, i)) { 
				srcbuf = lua_tostring(L, i);
				len = strlen(srcbuf);
			} else if (lua_isuserdata(L, i)) {
				srcbuf = lua_touserdata(L, i);
			}

			blen = atoi(fmt);

			if (blen == 0) {
				slog(0, SLOG_ERROR, "Internal serilizer bug processing: %s", fmt);
				goto err;
			}

			if (!srcbuf) {
				slog(0, SLOG_ERROR, "Internal bug fetching src pointer");
				goto err;
			}

			if (blen < len)
				len = blen;

			aura_buffer_put_bin(buf, srcbuf, len);
			
			while (*fmt && (*fmt++ != '.'));

			break;
		}
		default: 
			BUG(node, "Unknown token: %c\n", *(--fmt));
			break;
		}
	}

	return buf;
err:
	aura_buffer_release(node, buf);
	return NULL;
}


/* --------------------------- */

static int l_open_node(lua_State *L)
{

	int n;
	struct aura_node *node;
	TRACE();
	aura_check_args(L, 2);
	node = aura_open(lua_tostring(L, 1), lua_tostring(L, 2));
	return check_node_and_push(L, node);
}

static int l_node_gc(lua_State *L)
{
	slog(4, SLOG_DEBUG, "Garbage-collecting a node");
	lua_stackdump(L);
	return 0;
}

static int laura_do_sync_call(lua_State *L){

	struct laura_node *lnode = lua_touserdata(L, 1);
	struct aura_buffer *buf, *retbuf;
	struct aura_object *o;
	int ret;
	TRACE();

	o = aura_etable_find(lnode->node->tbl, lnode->current_call); 
	if (!o)
		luaL_error(L, "Attempt to call non-existend method");

	lua_stackdump(L);
	buf = lua_to_buffer(L, lnode->node, 2, o);
	if (!buf)
		luaL_error(L, "Serializer failed!");

	ret = aura_core_call(lnode->node, o, &retbuf, buf);
	if (ret != 0) 
		luaL_error(L, "Call for %s failed", o->name);

	ret = buffer_to_lua(L, lnode->node, o, retbuf);
	aura_buffer_release(lnode->node, retbuf);
	return ret;
}

static laura_do_async_call(lua_State *L){
	TRACE();
	lua_stackdump(L);
}

static int l_node_index(lua_State *L)
{
	struct laura_node *lnode = lua_touserdata(L, 1);
	const char *name = lua_tostring(L, -1);

	TRACE();
	/* FixMe: Can this get gc-d by the time we actually use it? */
	lnode->current_call = name;
	if (strcmp("__", name)==0)
		lua_pushcfunction(L, laura_do_async_call);
	else
		lua_pushcfunction(L, laura_do_sync_call);
	return 1;
}

static const struct luaL_Reg node_meta[] = {
  {"__gc",      l_node_gc         },
  {"__index",   l_node_index      },
  {NULL,     NULL           }
};


static int l_etable_get(lua_State *L)
{
	struct aura_node *node; 

	TRACE();
	aura_check_args(L, 1);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	node = lua_touserdata(L, 1);
	return lua_push_etable(L, node->tbl);
}


static int l_etable_create (lua_State *L) 
{
	struct aura_node *node;
	int count = 0;	
	struct aura_export_table *tbl; 
	
	TRACE();
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
 
	TRACE();
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
	
	TRACE();
	aura_check_args(L, 1);

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}

	tbl = lua_touserdata(L, 1);
	aura_etable_activate(tbl);
	return 0;
}


/* --------------------------------------- */


/*
static int l_eventloop_create(lua_State *L)
{
	struct aura_node *node; 
	struct aura_eventloop *loop; 

	TRACE();
	aura_check_args(L, 1);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	node = lua_touserdata(L, 1);
	loop = aura_eventloop_create(node);
	if (!loop) 
		lua_pushnil(L);
	else
		lua_pushlightuserdata(L, loop);
	return 1;
}

static int l_eventloop_add(lua_State *L)
{
	struct aura_node *node; 
	struct aura_eventloop *loop; 

	TRACE();
	aura_check_args(L, 2);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata (loop)");
	}
	if (!lua_islightuserdata(L, 2)) {
		aura_typeerror(L, 2, "ludata (node)");
	}

	loop = lua_touserdata(L, 1);
	node = lua_touserdata(L, 2);
	
	aura_eventloop_add(loop, node);
	return 0;
}

static int l_eventloop_del(lua_State *L)
{
	struct aura_node *node; 

	TRACE();
	aura_check_args(L, 1);

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata (node)");
	}

	node = lua_touserdata(L, 2);
	
	aura_eventloop_del(node);
	return 0;
}

static int l_eventloop_destroy(lua_State *L)
{
	struct aura_eventloop *loop; 

	TRACE();
	aura_check_args(L, 1);

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata (loop)");
	}

	loop = lua_touserdata(L, 1);
	aura_eventloop_destroy(loop);

	return 0;
}


static const struct luaL_Reg evtloop_meta[] = {
  {"__gc",   l_eventloop_gc},
  {"__call", l_eventloop_create},
  {NULL, NULL}
};

*/


static int l_slog_init(lua_State *L)
{
	const char *fname;
	int level; 

	TRACE();
	aura_check_args(L, 2);
	fname = lua_tostring(L, 1); 
	level = lua_tonumber(L, 2); 
	slog_init(fname, level);
	return 0;
}


static const luaL_Reg libfuncs[] = {
	{ "slog_init",                 l_slog_init                   },	
	{ "etable_create",             l_etable_create               },
	{ "etable_get",                l_etable_get                  },
	{ "etable_add",                l_etable_add                  },
	{ "etable_activate",           l_etable_activate             },
	{ "open_node",                 l_open_node                   },


/*
	{ "set_node_containing_table", l_set_node_container          }, 

	{ "status_cb",                 l_set_status_change_cb        },
	{ "etable_cb",                 l_set_etable_change_cb        },
	{ "event_cb",                  l_set_event_cb                },
	{ "core_close",                l_close                       },

	{ "handle_events",             l_handle_events               },
	{ "eventloop_create",          l_eventloop_create            },
	{ "eventloop_add",             l_eventloop_add               },
	{ "eventloop_del",             l_eventloop_del               },
	{ "eventloop_destroy",         l_eventloop_destroy           },
	
	{ "start_call",                l_start_call                  },
	{ "node_status",               l_node_status                 },
*/
	{NULL,                         NULL}
};


LUALIB_API int luaopen_auracore (lua_State *L) 
{
	luaL_newmetatable(L, laura_node_type);
	luaL_setfuncs(L, node_meta, 0);

	luaL_newlib(L, libfuncs);


	/* 
	 * Push open functions as aura["openfunc"]
	 */

	/*
	lua_pushstring(L, "openfuncs");
	lua_newtable(L);
	luaL_setfuncs(L, openfuncs, 0);	
	lua_settable(L, -3);
	*/

	/* Return One result */
	return 1;
}
