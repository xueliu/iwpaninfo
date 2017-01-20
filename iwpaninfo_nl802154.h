/*
 * iwpaninfo - 802.15.4 WPAN Information Library - NL802154 Headers
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

#ifndef __IWPANINFO_NL802154_H_
#define __IWPANINFO_NL802154_H_

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <sys/un.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>

#include "iwpaninfo.h"
#include "iwpaninfo/utils.h"
#include "api/nl802154.h"

struct nl802154_state {
	struct nl_sock *nl_sock;
	struct nl_cache *nl_cache;
	struct genl_family *nl802154;
	struct genl_family *nlctrl;
};

struct nl802154_msg_conveyor {
	struct nl_msg *msg;
	struct nl_cb *cb;
};

struct nl802154_event_conveyor {
	int wait;
	int recv;
};

struct nl802154_group_conveyor {
	const char *name;
	int id;
};

struct nl802154_array_buf {
	void *buf;
	int count;
};

#endif
