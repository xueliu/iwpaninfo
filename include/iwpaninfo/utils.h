/*
 * iwpaninfo - 802.15.4 WPAN Information Library - Utility Headers
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

#ifndef __IWPANINFO_UTILS_H_
#define __IWPANINFO_UTILS_H_

#include <sys/socket.h>
#include <net/if.h>
#include <uci.h>

#include "iwpaninfo.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define LOG10_MAGIC	1.25892541179

extern struct uci_context *uci_ctx;

int iwpaninfo_ioctl(int cmd, void *ifr);

int iwpaninfo_dbm2mw(int in);
int iwpaninfo_mw2dbm(int in);
static inline int iwpaninfo_mbm2dbm(int gain)
{
	return gain / 100;
}

int iwpaninfo_ifup(const char *ifname);
int iwpaninfo_ifdown(const char *ifname);
int iwpaninfo_ifmac(const char *ifname);

void iwpaninfo_close(void);

struct uci_section *iwpaninfo_uci_get_radio(const char *name, const char *type);
void iwpaninfo_uci_free(void);

#endif
