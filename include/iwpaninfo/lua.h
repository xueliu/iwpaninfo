/*
 * iwpaninfo - 802.15.4 WPAN Information Library - Lua Headers
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
 * The framework of code is derived from the iwinfo library
 */

#ifndef __IWPANINFO_LUALIB_H_
#define __IWPANINFO_LUALIB_H_

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "iwpaninfo.h"


#define IWPANINFO_META		"iwpaninfo"

#ifdef USE_NL802154
#define IWPANINFO_NL802154_META	"iwpaninfo.nl802154"
#endif

#define LUA_REG(type,op) \
	{ #op, iwinfo_L_##type##_##op }

#define LUA_WRAP_INT_OP(type,op)				\
	static int iwinfo_L_##type##_##op(lua_State *L)		\
	{							\
		const char *ifname = luaL_checkstring(L, 1);	\
		int rv;						\
		if( !type##_ops.op(ifname, &rv) )		\
			lua_pushnumber(L, rv);			\
		else						\
			lua_pushnil(L);				\
		return 1;					\
	}

#define LUA_WRAP_UINT64_OP(type,op)				\
	static int iwinfo_L_##type##_##op(lua_State *L)		\
	{							\
		const char *ifname = luaL_checkstring(L, 1);	\
		uint64_t rv;						\
		if( !type##_ops.op(ifname, &rv) )		\
			lua_pushnumber(L, rv);			\
		else						\
			lua_pushnil(L);				\
		return 1;					\
	}

#define LUA_WRAP_STRING_OP(type,op)				\
	static int iwinfo_L_##type##_##op(lua_State *L)		\
	{							\
		const char *ifname = luaL_checkstring(L, 1);	\
		char rv[IWPANINFO_BUFSIZE];			\
		memset(rv, 0, IWPANINFO_BUFSIZE);			\
		if( !type##_ops.op(ifname, rv) )		\
			lua_pushstring(L, rv);			\
		else						\
			lua_pushnil(L);				\
		return 1;					\
	}

#define LUA_WRAP_STRUCT_OP(type,op)				\
	static int iwinfo_L_##type##_##op(lua_State *L)		\
	{							\
		return iwinfo_L_##op(L, type##_ops.op);		\
	}

#endif
