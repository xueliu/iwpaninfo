/*
 * iwpaninfo - 802.15.4 WPAN Information Library - Lua Bindings
 *
 * Copyright (C) 2017 Xue Liu <liuxuenetmail@gmail.com>
 *
 * The iwpaninfo library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * The iwpaninfo library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the iwpaninfo library. If not, see http://www.gnu.org/licenses/.
 *
 *
 * The framework of code is derived from the iwinfo library.
 */

#include "iwpaninfo/lua.h"


/* Determine type */
static int iwpaninfo_L_type(lua_State *L)
{
	const char *ifname = luaL_checkstring(L, 1);
	const char *type = iwpaninfo_type(ifname);

	if (type)
		lua_pushstring(L, type);
	else
		lua_pushnil(L);

	return 1;
}

/* Shutdown backends */
static int iwpaninfo_L__gc(lua_State *L)
{
	iwpaninfo_finish();
	return 0;
}

/* Wrapper for mode */
static int iwinfo_L_mode(lua_State *L, int (*func)(const char *, int *))
{
	int mode;
	const char *ifname = luaL_checkstring(L, 1);

	if ((*func)(ifname, &mode))
		mode = IWPANINFO_OPMODE_UNKNOWN;

	lua_pushstring(L, IWPANINFO_OPMODE_NAMES[mode]);
	return 1;
}

/* Wrapper for tx power list */
static int iwinfo_L_txpwrlist(lua_State *L, int (*func)(const char *, char *, int *))
{
	int i, x, len;
	char rv[IWPANINFO_BUFSIZE];
	const char *ifname = luaL_checkstring(L, 1);
	struct iwpaninfo_txpwrlist_entry *e;

	memset(rv, 0, sizeof(rv));

	if (!(*func)(ifname, rv, &len))
	{
		lua_newtable(L);

		for (i = 0, x = 1; i < len; i += sizeof(struct iwpaninfo_txpwrlist_entry), x++)
		{
			e = (struct iwpaninfo_txpwrlist_entry *) &rv[i];

			lua_newtable(L);

//			lua_pushnumber(L, e->mw);
//			lua_setfield(L, -2, "mw");

			lua_pushnumber(L, e->dbm);
			lua_setfield(L, -2, "dbm");

			lua_rawseti(L, -2, x);
		}

		return 1;
	}

	return 0;
}

/* Wrapper for frequency list */
static int iwinfo_L_freqlist(lua_State *L,
		int (*func)(const char *, char *, int *)) {
	int i, x, len;
	char rv[IWPANINFO_BUFSIZE];
	const char *ifname = luaL_checkstring(L, 1);
	struct iwpaninfo_freqlist_entry *e;

	lua_newtable(L);
	memset(rv, 0, sizeof(rv));

	if (!(*func)(ifname, rv, &len)) {
		for (i = 0, x = 1; i < len;
				i +=
				sizeof(struct iwpaninfo_freqlist_entry), x++) {
			e = (struct iwpaninfo_freqlist_entry *) &rv[i];

			lua_newtable(L);

			/* MHz */
			lua_pushinteger(L, e->mhz);
			lua_setfield(L, -2, "mhz");

			/* Channel */
			lua_pushinteger(L, e->channel);
			lua_setfield(L, -2, "channel");

			lua_rawseti(L, -2, x);
		}
	}

	return 1;
}

#ifdef USE_NL802154
/* NL802154 */
LUA_WRAP_INT_OP(nl802154, channel)
LUA_WRAP_INT_OP(nl802154, frequency)
LUA_WRAP_INT_OP(nl802154, txpower)
LUA_WRAP_STRING_OP(nl802154, phyname)
LUA_WRAP_STRUCT_OP(nl802154, mode)
LUA_WRAP_STRUCT_OP(nl802154, txpwrlist)
LUA_WRAP_STRUCT_OP(nl802154, freqlist)
LUA_WRAP_INT_OP(nl802154, panid)
LUA_WRAP_INT_OP(nl802154, short_address)
LUA_WRAP_UINT64_OP(nl802154, extended_address)
LUA_WRAP_INT_OP(nl802154, page)
LUA_WRAP_INT_OP(nl802154, min_be)
LUA_WRAP_INT_OP(nl802154, max_be)
LUA_WRAP_INT_OP(nl802154, csma_backoff)
LUA_WRAP_INT_OP(nl802154, frame_retry)
LUA_WRAP_INT_OP(nl802154, lbt_mode)
LUA_WRAP_INT_OP(nl802154, cca_mode)
#endif

#ifdef USE_NL802154
/* NL802154 table */
static const luaL_reg R_nl802154[] = {
	LUA_REG(nl802154, channel),
	LUA_REG(nl802154, frequency),
	LUA_REG(nl802154, txpower),
	LUA_REG(nl802154, mode),
	LUA_REG(nl802154, txpwrlist),
LUA_REG(nl802154, freqlist),
	LUA_REG(nl802154, phyname),
	LUA_REG(nl802154, panid),
	LUA_REG(nl802154, short_address),
	LUA_REG(nl802154, extended_address),
	LUA_REG(nl802154, page),
	LUA_REG(nl802154, min_be),
	LUA_REG(nl802154, max_be),
	LUA_REG(nl802154, csma_backoff),
	LUA_REG(nl802154, frame_retry),
	LUA_REG(nl802154, lbt_mode),
	LUA_REG(nl802154, cca_mode),
	{ NULL, NULL }
};
#endif

/* Common */
static const luaL_reg R_common[] = {
	{ "type", iwpaninfo_L_type },
	{ "__gc", iwpaninfo_L__gc  },
	{ NULL, NULL }
};


LUALIB_API int luaopen_iwpaninfo(lua_State *L) {
	luaL_register(L, IWPANINFO_META, R_common);

#ifdef USE_NL802154
	luaL_newmetatable(L, IWPANINFO_NL802154_META);
	luaL_register(L, NULL, R_common);
	luaL_register(L, NULL, R_nl802154);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_setfield(L, -2, "nl802154");
#endif

	return 1;
}
