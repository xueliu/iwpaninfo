/*
 * iwpaninfo - 802.15.4 WPAN Information Library - Shared utility routines
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

#include "iwpaninfo/utils.h"


static int ioctl_socket = -1;
struct uci_context *uci_ctx = NULL;

static int iwpaninfo_ioctl_socket(void)
{
	/* Prepare socket */
	if (ioctl_socket == -1)
	{
		ioctl_socket = socket(AF_INET, SOCK_DGRAM, 0);
		fcntl(ioctl_socket, F_SETFD, fcntl(ioctl_socket, F_GETFD) | FD_CLOEXEC);
	}

	return ioctl_socket;
}

int iwpaninfo_ioctl(int cmd, void *ifr)
{
	int s = iwpaninfo_ioctl_socket();
	return ioctl(s, cmd, ifr);
}

int iwpaninfo_dbm2mw(int in)
{
	double res = 1.0;
	int ip = in / 10;
	int fp = in % 10;
	int k;

	for(k = 0; k < ip; k++) res *= 10;
	for(k = 0; k < fp; k++) res *= LOG10_MAGIC;

	return (int)res;
}

int iwpaninfo_mw2dbm(int in)
{
	double fin = (double) in;
	int res = 0;

	while(fin > 10.0)
	{
		res += 10;
		fin /= 10.0;
	}

	while(fin > 1.000001)
	{
		res += 1;
		fin /= LOG10_MAGIC;
	}

	return (int)res;
}

int iwpaninfo_ifup(const char *ifname)
{
	struct ifreq ifr;

	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

	if (iwpaninfo_ioctl(SIOCGIFFLAGS, &ifr))
		return 0;

	ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);

	return !iwpaninfo_ioctl(SIOCSIFFLAGS, &ifr);
}

int iwpaninfo_ifdown(const char *ifname)
{
	struct ifreq ifr;

	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

	if (iwpaninfo_ioctl(SIOCGIFFLAGS, &ifr))
		return 0;

	ifr.ifr_flags &= ~(IFF_UP | IFF_RUNNING);

	return !iwpaninfo_ioctl(SIOCSIFFLAGS, &ifr);
}

int iwpaninfo_ifmac(const char *ifname)
{
	struct ifreq ifr;

	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

	if (iwpaninfo_ioctl(SIOCGIFHWADDR, &ifr))
		return 0;

	ifr.ifr_hwaddr.sa_data[0] |= 0x02;
	ifr.ifr_hwaddr.sa_data[1]++;
	ifr.ifr_hwaddr.sa_data[2]++;

	return !iwpaninfo_ioctl(SIOCSIFHWADDR, &ifr);
}

void iwpaninfo_close(void)
{
	if (ioctl_socket > -1)
		close(ioctl_socket);

	ioctl_socket = -1;
}

struct uci_section *iwpaninfo_uci_get_radio(const char *name, const char *type)
{
	struct uci_ptr ptr = {
		.package = "wireless",
		.section = name,
		.flags = (name && *name == '@') ? UCI_LOOKUP_EXTENDED : 0,
	};
	const char *opt;

	if (!uci_ctx) {
		uci_ctx = uci_alloc_context();
		if (!uci_ctx)
			return NULL;
	}

	if (uci_lookup_ptr(uci_ctx, &ptr, NULL, true))
		return NULL;

	if (!ptr.s || strcmp(ptr.s->type, "wpan-device") != 0)
		return NULL;

	opt = uci_lookup_option_string(uci_ctx, ptr.s, "type");
	if (!opt || strcmp(opt, type) != 0)
		return NULL;

	return ptr.s;
}

void iwpaninfo_uci_free(void)
{
	if (!uci_ctx)
		return;

	uci_free_context(uci_ctx);
	uci_ctx = NULL;
}
