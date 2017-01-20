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

#include "iwpaninfo.h"

const char *IWPANINFO_OPMODE_NAMES[] = {
	"Node",
	"Monitor",
	"Coordinator",
	"Unknown",
};

static const struct iwpaninfo_ops *backends[] = {
#ifdef USE_NL802154
	&nl802154_ops,
#endif
};

const char * iwpaninfo_type(const char *ifname)
{
	const struct iwpaninfo_ops *ops = iwpaninfo_backend(ifname);
	if (!ops)
		return NULL;

	return ops->name;
}

const struct iwpaninfo_ops * iwpaninfo_backend(const char *ifname)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(backends); i++)
		if (backends[i]->probe(ifname))
			return backends[i];

	return NULL;
}

const struct iwpaninfo_ops * iwpaninfo_backend_by_name(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(backends); i++)
		if (!strcmp(backends[i]->name, name))
			return backends[i];

	return NULL;
}

void iwpaninfo_finish(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(backends); i++)
		backends[i]->close();

	iwpaninfo_close();
}
