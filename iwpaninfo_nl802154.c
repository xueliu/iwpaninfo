/*
 * iwpaninfo - 802.15.4 WPAN Information Library - nl802154 Backend
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

#include <limits.h>
#include <glob.h>
#include <fnmatch.h>
#include <stdarg.h>
#include <limits.h>

#include "iwpaninfo_nl802154.h"
#include "nl_extras.h"

#define min(x, y) ((x) < (y)) ? (x) : (y)

#define BIT(x) (1ULL<<(x))

static struct nl802154_state *nls = NULL;

static void nl802154_close(void)
{
	if (nls)
	{
		if (nls->nlctrl)
			genl_family_put(nls->nlctrl);

		if (nls->nl802154)
			genl_family_put(nls->nl802154);

		if (nls->nl_sock)
			nl_socket_free(nls->nl_sock);

		if (nls->nl_cache)
			nl_cache_free(nls->nl_cache);

		free(nls);
		nls = NULL;
	}
}

static int nl802154_init(void)
{
	int err, fd;

	if (!nls)
	{
		nls = malloc(sizeof(struct nl802154_state));
		if (!nls) {
			err = -ENOMEM;
			goto err;
		}

		memset(nls, 0, sizeof(*nls));

		nls->nl_sock = nl_socket_alloc();
		if (!nls->nl_sock) {
			err = -ENOMEM;
			goto err;
		}

		if (genl_connect(nls->nl_sock)) {
			err = -ENOLINK;
			goto err;
		}

		fd = nl_socket_get_fd(nls->nl_sock);
		if (fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC) < 0) {
			err = -EINVAL;
			goto err;
		}

		if (genl_ctrl_alloc_cache(nls->nl_sock, &nls->nl_cache)) {
			err = -ENOMEM;
			goto err;
		}

		nls->nl802154 = genl_ctrl_search_by_name(nls->nl_cache, "nl802154");
		if (!nls->nl802154) {
			err = -ENOENT;
			goto err;
		}

		nls->nlctrl = genl_ctrl_search_by_name(nls->nl_cache, "nlctrl");
		if (!nls->nlctrl) {
			err = -ENOENT;
			goto err;
		}
	}

	return 0;

err:
	nl802154_close();
	return err;
}

static int nl802154_readint(const char *path)
{
	int fd;
	int rv = -1;
	char buffer[16];

	if ((fd = open(path, O_RDONLY)) > -1)
	{
		if (read(fd, buffer, sizeof(buffer)) > 0)
			rv = atoi(buffer);

		close(fd);
	}

	return rv;
}

static int nl802154_readstr(const char *path, char *buffer, int length)
{
	int fd;
	int rv = -1;

	if ((fd = open(path, O_RDONLY)) > -1)
	{
		if ((rv = read(fd, buffer, length - 1)) > 0)
		{
			if (buffer[rv - 1] == '\n')
				rv--;

			buffer[rv] = 0;
		}

		close(fd);
	}

	return rv;
}


static int nl802154_msg_error(struct sockaddr_nl *nla,
	struct nlmsgerr *err, void *arg)
{
	int *ret = arg;
	*ret = err->error;
	return NL_STOP;
}

static int nl802154_msg_finish(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;
	return NL_SKIP;
}

static int nl802154_msg_ack(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;
	return NL_STOP;
}

static int nl802154_msg_response(struct nl_msg *msg, void *arg)
{
	return NL_SKIP;
}

static void nl802154_free(struct nl802154_msg_conveyor *cv)
{
	if (cv)
	{
		if (cv->cb)
			nl_cb_put(cv->cb);

		if (cv->msg)
			nlmsg_free(cv->msg);

		cv->cb  = NULL;
		cv->msg = NULL;
	}
}

static struct nl802154_msg_conveyor * nl802154_new(struct genl_family *family,
                                                 int cmd, int flags)
{
	static struct nl802154_msg_conveyor cv;

	struct nl_msg *req = NULL;
	struct nl_cb *cb = NULL;

	req = nlmsg_alloc();
	if (!req)
		goto err;

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb)
		goto err;

	genlmsg_put(req, 0, 0, genl_family_get_id(family), 0, flags, cmd, 0);

	cv.msg = req;
	cv.cb  = cb;

	return &cv;

err:
	if (req)
		nlmsg_free(req);

	return NULL;
}

static int nl802154_phy_idx_from_uci_phy(struct uci_section *s)
{
	const char *opt;
	char buf[128];

	opt = uci_lookup_option_string(uci_ctx, s, "phy");
	if (!opt)
		return -1;

	snprintf(buf, sizeof(buf), "/sys/class/ieee802154/%s/index", opt);
	return nl802154_readint(buf);
}

static int nl802154_phy_idx_from_uci(const char *name)
{
	struct uci_section *s;
	int idx = -1;

	s = iwpaninfo_uci_get_radio(name, "mac802154");
	if (!s)
		goto free;

	idx = nl802154_phy_idx_from_uci_phy(s);

free:
	iwpaninfo_uci_free();
	return idx;
}

static struct nl802154_msg_conveyor * nl802154_msg(const char *ifname,
                                                 int cmd, int flags)
{
	int ifidx = -1, phyidx = -1;
	struct nl802154_msg_conveyor *cv;

	if (ifname == NULL)
		return NULL;

	if (nl802154_init() < 0)
		return NULL;

	if (!strncmp(ifname, "phy", 3))
		phyidx = atoi(&ifname[3]);
	else if (!strncmp(ifname, "radio", 5))
		phyidx = nl802154_phy_idx_from_uci(ifname);
	else if (!strncmp(ifname, "mon.", 4))
		ifidx = if_nametoindex(&ifname[4]);
	else
		ifidx = if_nametoindex(ifname);

	/* Valid ifidx must be greater than 0 */
	if ((ifidx <= 0) && (phyidx < 0))
		return NULL;

	cv = nl802154_new(nls->nl802154, cmd, flags);
	if (!cv)
		return NULL;

	if (ifidx > -1)
		NLA_PUT_U32(cv->msg, NL802154_ATTR_IFINDEX, ifidx);

	if (phyidx > -1)
		NLA_PUT_U32(cv->msg, NL802154_ATTR_WPAN_PHY, phyidx);

	return cv;

nla_put_failure:
	nl802154_free(cv);
	return NULL;
}

static struct nl802154_msg_conveyor * nl802154_send(
	struct nl802154_msg_conveyor *cv,
	int (*cb_func)(struct nl_msg *, void *), void *cb_arg
) {
	static struct nl802154_msg_conveyor rcv;
	int err = 1;

	if (cb_func)
		nl_cb_set(cv->cb, NL_CB_VALID, NL_CB_CUSTOM, cb_func, cb_arg);
	else
		nl_cb_set(cv->cb, NL_CB_VALID, NL_CB_CUSTOM, nl802154_msg_response, &rcv);

	if (nl_send_auto_complete(nls->nl_sock, cv->msg) < 0)
		goto err;

	nl_cb_err(cv->cb,               NL_CB_CUSTOM, nl802154_msg_error,  &err);
	nl_cb_set(cv->cb, NL_CB_FINISH, NL_CB_CUSTOM, nl802154_msg_finish, &err);
	nl_cb_set(cv->cb, NL_CB_ACK,    NL_CB_CUSTOM, nl802154_msg_ack,    &err);

	while (err > 0)
		nl_recvmsgs(nls->nl_sock, cv->cb);

	return &rcv;

err:
	nl_cb_put(cv->cb);
	nlmsg_free(cv->msg);

	return NULL;
}

static struct nlattr ** nl802154_parse(struct nl_msg *msg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	static struct nlattr *attr[NL802154_ATTR_MAX + 1];

	nla_parse(attr, NL802154_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
	          genlmsg_attrlen(gnlh, 0), NULL);

	return attr;
}

static float nl802154_channel2freq(int channel_page, int channel) {
	float freq = 0;

	switch (channel_page) {
	case 0:
		if (channel == 0) {
			freq = 868.3;
			break;
		} else if (channel > 0 && channel < 11) {
			freq = 906 + 2 * (channel - 1);
		} else {
			freq = 2405 + 5 * (channel - 11);
		}
		break;
	case 1:
		if (channel == 0) {
			freq = 868.3;
			break;
		} else if (channel >= 1 && channel <= 10) {
			freq = 906 + 2 * (channel - 1);
		}
		break;
	case 2:
		if (channel == 0) {
			freq = 868.3;
			break;
		} else if (channel >= 1 && channel <= 10) {
			freq = 906 + 2 * (channel - 1);
		}
		break;
	case 3:
		if (channel >= 0 && channel <= 12) {
			freq = 2412 + 5 * channel;
		} else if (channel == 13) {
			freq = 2484;
		}
		break;
	case 4:
		switch (channel) {
		case 0:
			freq = 499.2;
			break;
		case 1:
			freq = 3494.4;
			break;
		case 2:
			freq = 3993.6;
			break;
		case 3:
			freq = 4492.8;
			break;
		case 4:
			freq = 3993.6;
			break;
		case 5:
			freq = 6489.6;
			break;
		case 6:
			freq = 6988.8;
			break;
		case 7:
			freq = 6489.6;
			break;
		case 8:
			freq = 7488.0;
			break;
		case 9:
			freq = 7987.2;
			break;
		case 10:
			freq = 8486.4;
			break;
		case 11:
			freq = 7987.2;
			break;
		case 12:
			freq = 8985.6;
			break;
		case 13:
			freq = 9484.8;
			break;
		case 14:
			freq = 9984.0;
			break;
		case 15:
			freq = 9484.8;
			break;
		}
		break;
	case 5:
		if (channel >= 0 && channel <= 3) {
			freq = 780 + 2 * channel;
		} else if (channel >= 4 && channel <= 7) {
			freq = 780 + 2 * (channel - 4);
		}
		break;
	case 6:
		if (channel >= 0 && channel <= 7) {
			freq = 951.2 + 0.6 * channel;
		} else if (channel >= 8 && channel <= 9) {
			freq = 954.4 + 0.2 * (channel - 8);
		} else if (channel >= 10 && channel <= 21) {
			freq = 951.1 + 0.4 * (channel - 10);
		}
		break;
	default:
		freq = 0;
		break;
	}

	return freq;
}


static int nl802154_ifname2phy_cb(struct nl_msg *msg, void *arg)
{
	char *buf = arg;
	struct nlattr **attr = nl802154_parse(msg);

	if (attr[NL802154_ATTR_WPAN_PHY_NAME])
		memcpy(buf, nla_data(attr[NL802154_ATTR_WPAN_PHY_NAME]), nla_len(attr[NL802154_ATTR_WPAN_PHY_NAME]));
	else
		buf[0] = 0;

	return NL_SKIP;
}

static char * nl802154_ifname2phy(const char *ifname)
{
	static char phy[32] = { 0 };
	struct nl802154_msg_conveyor *req;

	memset(phy, 0, sizeof(phy));

	req = nl802154_msg(ifname, NL802154_CMD_GET_WPAN_PHY, 0);
	if (req)
	{
		nl802154_send(req, nl802154_ifname2phy_cb, phy);
		nl802154_free(req);
	}

	return phy[0] ? phy : NULL;
}

static char * nl802154_phy2ifname(const char *ifname)
{
	int ifidx = -1, cifidx = -1, phyidx = -1;
	char buffer[64];
	static char nif[IFNAMSIZ] = { 0 };

	DIR *d;
	struct dirent *e;

	/* Only accept phy name of the form phy%d or radio%d */
	if (!ifname)
		return NULL;
	else if (!strncmp(ifname, "phy", 3))
		phyidx = atoi(&ifname[3]);
	else if (!strncmp(ifname, "radio", 5))
		phyidx = nl802154_phy_idx_from_uci(ifname);
	else
		return NULL;

	memset(nif, 0, sizeof(nif));

	if (phyidx > -1)
	{
		if ((d = opendir("/sys/class/net")) != NULL)
		{
			while ((e = readdir(d)) != NULL)
			{
				snprintf(buffer, sizeof(buffer),
				         "/sys/class/net/%s/phy80211/index", e->d_name);

				if (nl802154_readint(buffer) == phyidx)
				{
					snprintf(buffer, sizeof(buffer),
					         "/sys/class/net/%s/ifindex", e->d_name);

					if ((cifidx = nl802154_readint(buffer)) >= 0 &&
					    ((ifidx < 0) || (cifidx < ifidx)))
					{
						ifidx = cifidx;
						strncpy(nif, e->d_name, sizeof(nif) - 1);
					}
				}
			}

			closedir(d);
		}
	}

	return nif[0] ? nif : NULL;
}

static int nl802154_get_mode_cb(struct nl_msg *msg, void *arg)
{
	int *mode = arg;
	struct nlattr **tb = nl802154_parse(msg);
	const int ifmodes[NL802154_IFTYPE_MAX + 2] = {
		IWPANINFO_OPMODE_NODE,      /* node */
		IWPANINFO_OPMODE_MONITOR,	/* monitor */
		IWPANINFO_OPMODE_COORD,		/* coordinator */
		IWPANINFO_OPMODE_UNKNOWN,	/* unspecified */
	};

	if (tb[NL802154_ATTR_IFTYPE])
		*mode = ifmodes[nla_get_u32(tb[NL802154_ATTR_IFTYPE])];

	return NL_SKIP;
}


static int nl802154_get_mode(const char *ifname, int *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	res = nl802154_phy2ifname(ifname);
	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_INTERFACE, 0);
	*buf = IWPANINFO_OPMODE_UNKNOWN;

	if (req)
	{
		nl802154_send(req, nl802154_get_mode_cb, buf);
		nl802154_free(req);
	}

	return (*buf == IWPANINFO_OPMODE_UNKNOWN) ? -1 : 0;
}

static int nl802154_probe(const char *ifname)
{
	char *phy = nl802154_ifname2phy(ifname);
	return !!phy;
}

static int nl802154_get_channel_info_cb(struct nl_msg *msg, void *arg)
{
	int *channel = arg;
	struct nlattr **tb = nl802154_parse(msg);

	if (tb[NL802154_ATTR_CHANNEL])
		*channel = nla_get_u8(tb[NL802154_ATTR_CHANNEL]);

	return NL_SKIP;
}

static int nl802154_get_channel(const char *ifname, int *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	/* try to find channel from interface info */
	res = nl802154_phy2ifname(ifname);

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_WPAN_PHY, 0);
	*buf = 0;

	if (req)
	{
		nl802154_send(req, nl802154_get_channel_info_cb, buf);
		nl802154_free(req);
	}

	return (*buf == 0) ? -1 : 0;
}

static int nl802154_get_txpower_cb(struct nl_msg *msg, void *arg)
{
	int *buf = arg;
	struct nlattr **tb = nl802154_parse(msg);

	if (tb[NL802154_ATTR_TX_POWER])
		*buf = nla_get_u32(tb[NL802154_ATTR_TX_POWER]);

	return NL_SKIP;
}

static int nl802154_get_txpower(const char *ifname, int *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	res = nl802154_phy2ifname(ifname);
	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_WPAN_PHY, 0);

	if (req)
	{
		*buf = INT_MIN;
		nl802154_send(req, nl802154_get_txpower_cb, buf);
		nl802154_free(req);
	}

	return (*buf == INT_MIN) ? -1 : 0;
}

static int nl802154_get_phyname(const char *ifname, char *buf)
{
	const char *name;

	name = nl802154_ifname2phy(ifname);

	if (name)
	{
		strcpy(buf, name);
		return 0;
	}
	else if ((name = nl802154_phy2ifname(ifname)) != NULL)
	{
		name = nl802154_ifname2phy(name);

		if (name)
		{
			strcpy(buf, ifname);
			return 0;
		}
	}

	return -1;
}

static int nl802154_get_txpwrlist_cb(struct nl_msg *msg, void *arg)
{
	int ret;

	struct nl802154_array_buf *arr = arg;
	struct iwpaninfo_txpwrlist_entry *e = arr->buf;

	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

	struct nlattr *tb_caps[NL802154_CAP_ATTR_MAX + 1];
	struct nlattr *tb_msg[NL802154_ATTR_MAX + 1];

	static struct nla_policy caps_policy[NL802154_CAP_ATTR_MAX + 1] = {
		[NL802154_CAP_ATTR_TX_POWERS]		= { .type = NLA_NESTED },
	};

	nla_parse(tb_msg, NL802154_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	ret = nla_parse_nested(tb_caps, NL802154_CAP_ATTR_MAX,
			tb_msg[NL802154_ATTR_WPAN_PHY_CAPS],
			       caps_policy);

	if (ret) {
		printf("failed to parse caps\n");
		return -EIO;
	}

	if (tb_caps[NL802154_CAP_ATTR_TX_POWERS]) {
		int rem_pwrs;
		struct nlattr *nl_pwrs;

		nla_for_each_nested(nl_pwrs, tb_caps[NL802154_CAP_ATTR_TX_POWERS], rem_pwrs) {
			e->dbm = MBM_TO_DBM(nla_get_s32(nl_pwrs));
//			e->mw = iwpaninfo_dbm2mw(e->dbm);
			e++;
			arr->count++;
		}
	}

	return NL_SKIP;
}

static int nl802154_get_txpwrlist(const char *ifname, char *buf, int *len)
{
	int ch_cur;
	char* res;
	struct nl802154_msg_conveyor *req;
	struct nl802154_array_buf arr = { .buf = buf, .count = 0 };

	/* try to find tx power list from phy info */
	res = nl802154_phy2ifname(ifname);

	if (nl802154_get_channel(ifname, &ch_cur))
		ch_cur = 0;

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_WPAN_PHY, 0);
	if (req)
	{
		nl802154_send(req, nl802154_get_txpwrlist_cb, &arr);
		nl802154_free(req);
	}

	if (arr.count > 0)
	{
		*len = arr.count * sizeof(struct iwpaninfo_cca_ed_lvl_list_entry);
		return 0;
	}

	return -1;
}

static int nl802154_get_cca_ed_lvl_cb(struct nl_msg *msg, void *arg)
{
	int ret;

	struct nl802154_array_buf *arr = arg;
	struct iwpaninfo_cca_ed_lvl_list_entry *e = arr->buf;

	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

	struct nlattr *tb_caps[NL802154_CAP_ATTR_MAX + 1];
	struct nlattr *tb_msg[NL802154_ATTR_MAX + 1];

	static struct nla_policy caps_policy[NL802154_CAP_ATTR_MAX + 1] = {
		[NL802154_CAP_ATTR_CCA_ED_LEVELS]		= { .type = NLA_NESTED },
	};

	nla_parse(tb_msg, NL802154_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	ret = nla_parse_nested(tb_caps, NL802154_CAP_ATTR_MAX,
			tb_msg[NL802154_ATTR_WPAN_PHY_CAPS],
			       caps_policy);

	if (ret) {
		printf("failed to parse caps\n");
		return -EIO;
	}

	if (tb_caps[NL802154_CAP_ATTR_CCA_ED_LEVELS]) {
		int rem_cca_ed_lvls;
		struct nlattr *nl_cca_ed_lvls;

		nla_for_each_nested(nl_cca_ed_lvls, tb_caps[NL802154_CAP_ATTR_CCA_ED_LEVELS], rem_cca_ed_lvls) {
			e->dbm = MBM_TO_DBM(nla_get_s32(nl_cca_ed_lvls));
			e++;
			arr->count++;
		}
	}

	return NL_SKIP;
}


static int nl802154_get_cca_ed_lvl_list(const char *ifname, char *buf, int *len)
{
	char* res;
	struct nl802154_msg_conveyor *req;
	struct nl802154_array_buf arr = { .buf = buf, .count = 0 };

	/* try to find CCA ED level list from phy info */
	res = nl802154_phy2ifname(ifname);

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_WPAN_PHY, 0);
	if (req)
	{
		nl802154_send(req, nl802154_get_cca_ed_lvl_cb, &arr);
		nl802154_free(req);
	}

	if (arr.count > 0)
	{
		*len = arr.count * sizeof(struct iwpaninfo_txpwrlist_entry);
		return 0;
	}

	return -1;
}

static int nl802154_get_freqlist_cb(struct nl_msg *msg, void *arg)
{
	struct nl802154_array_buf *arr = arg;
	struct iwpaninfo_freqlist_entry *e = arr->buf;

	int ret;
	int current_page;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

	struct nlattr *tb_caps[NL802154_CAP_ATTR_MAX + 1];
	struct nlattr *tb_msg[NL802154_ATTR_MAX + 1];

	static struct nla_policy caps_policy[NL802154_CAP_ATTR_MAX + 1] = {
			[NL802154_CAP_ATTR_CHANNELS]		= { .type = NLA_NESTED },
		};

	nla_parse(tb_msg, NL802154_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
			  genlmsg_attrlen(gnlh, 0), NULL);

	ret = nla_parse_nested(tb_caps, NL802154_CAP_ATTR_MAX,
			tb_msg[NL802154_ATTR_WPAN_PHY_CAPS],
				       caps_policy);

	if (ret) {
		printf("failed to parse caps\n");
		return -EIO;
	}

	if (tb_msg[NL802154_ATTR_PAGE])
		current_page = nla_get_u8(tb_msg[NL802154_ATTR_PAGE]);

	if (tb_caps[NL802154_CAP_ATTR_CHANNELS]) {
		int rem_pages;
		struct nlattr *nl_pages;
		nla_for_each_nested(nl_pages, tb_caps[NL802154_CAP_ATTR_CHANNELS], rem_pages)
		{
			int rem_ch;
			struct nlattr *nl_ch;
			nla_for_each_nested(nl_ch, nl_pages, rem_ch)
			{
				e->channel = nla_type(nl_ch);
				e->mhz = nl802154_channel2freq(current_page, e->channel);
				e++;
				arr->count++;
			}
		}
	}

	return NL_SKIP;
}

static int nl802154_get_freqlist(const char *ifname, char *buf, int *len)
{
	struct nl802154_msg_conveyor *req;
	struct nl802154_array_buf arr = { .buf = buf, .count = 0 };

	req = nl802154_msg(ifname, NL802154_CMD_GET_WPAN_PHY, 0);
	if (req)
	{
		nl802154_send(req, nl802154_get_freqlist_cb, &arr);
		nl802154_free(req);
	}

	if (arr.count > 0)
	{
		*len = arr.count * sizeof(struct iwpaninfo_freqlist_entry);
		return 0;
	}

	return -1;
}

static int nl802154_lookup_phyname(const char *section, char *buf)
{
	int fd, pos;

	snprintf(buf, IWPANINFO_BUFSIZE, "/sys/class/ieee802154/%s/index", section);

	fd = open(buf, O_RDONLY);
	if (fd < 0) return -1;
	pos = read(fd, buf, sizeof(buf)- 1);
	if (pos < 0) {
		close(fd);
		return -1;
	}
	buf[pos] = '\0';
	close(fd);
	sprintf(buf, "phy%d", atoi(buf));
	return 0;
}

static int nl802154_get_panid_info_cb(struct nl_msg *msg, void *arg)
{
	uint16_t * panid = arg;
	struct nlattr **tb = nl802154_parse(msg);

	if (tb[NL802154_ATTR_PAN_ID]) *panid = le16toh(nla_get_u16(tb[NL802154_ATTR_PAN_ID]));

	return NL_SKIP;
}

static int nl802154_get_panid(const char *ifname, int *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	/* try to find PAN ID from interface info */
	res = nl802154_phy2ifname(ifname);

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_INTERFACE, 0);
	*buf = 0;

	if (req)
	{
		nl802154_send(req, nl802154_get_panid_info_cb, buf);
		nl802154_free(req);
	}

	return (*buf == 0) ? -1 : 0;
}

static int nl802154_get_short_address_info_cb(struct nl_msg *msg, void *arg)
{
	uint16_t * short_addr = arg;
	struct nlattr **tb = nl802154_parse(msg);

	if (tb[NL802154_ATTR_PAN_ID]) *short_addr = le16toh(nla_get_u16(tb[NL802154_ATTR_SHORT_ADDR]));

	return NL_SKIP;
}

static int nl802154_get_short_address(const char *ifname, int *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	/* try to find short address from interface info */
	res = nl802154_phy2ifname(ifname);

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_INTERFACE, 0);
	*buf = 0;

	if (req)
	{
		nl802154_send(req, nl802154_get_short_address_info_cb, buf);
		nl802154_free(req);
	}

	return (*buf == 0) ? -1 : 0;
}

static int nl802154_get_extended_address_info_cb(struct nl_msg *msg, void *arg)
{
	uint64_t * extended_addr = arg;
	struct nlattr **tb = nl802154_parse(msg);

	if (tb[NL802154_ATTR_PAN_ID]) *extended_addr = le64toh(nla_get_u64(tb[NL802154_ATTR_EXTENDED_ADDR]));

	return NL_SKIP;
}

static int nl802154_get_extended_address(const char *ifname, uint64_t *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	/* try to find extended address from interface info */
	res = nl802154_phy2ifname(ifname);

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_INTERFACE, 0);
	*buf = 0;

	if (req)
	{
		nl802154_send(req, nl802154_get_extended_address_info_cb, buf);
		nl802154_free(req);
	}

	return (*buf == 0) ? -1 : 0;
}

static int nl802154_get_page_info_cb(struct nl_msg *msg, void *arg)
{
	int *page = arg;
	struct nlattr **tb = nl802154_parse(msg);

	if (tb[NL802154_ATTR_PAGE])
		*page = nla_get_u8(tb[NL802154_ATTR_PAGE]);

	return NL_SKIP;
}

static int nl802154_get_page(const char *ifname, int *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	/* try to find channel page from interface info */
	res = nl802154_phy2ifname(ifname);

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_WPAN_PHY, 0);
	*buf = -1;

	if (req)
	{
		nl802154_send(req, nl802154_get_page_info_cb, buf);
		nl802154_free(req);
	}

	return *buf;
}

static int nl802154_get_frequency(const char *ifname, int *buf) {
	int current_page = -1;
	if (!nl802154_get_page(ifname, buf)) {
		current_page = nl802154_get_page(ifname, buf);
		if (!nl802154_get_channel(ifname, buf)) {
			*buf = nl802154_channel2freq(current_page, *buf);
			return 0;
		}
	}

	return -1;
}

static int nl802154_get_min_be_info_cb(struct nl_msg *msg, void *arg)
{
	int *min_be = arg;
	struct nlattr **tb = nl802154_parse(msg);

	if (tb[NL802154_ATTR_MIN_BE])
		*min_be = nla_get_u8(tb[NL802154_ATTR_MIN_BE]);

	return NL_SKIP;
}

static int nl802154_get_min_be(const char *ifname, int *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	/* try to find min be from interface info */
	res = nl802154_phy2ifname(ifname);

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_INTERFACE, 0);
	*buf = -1;

	if (req)
	{
		nl802154_send(req, nl802154_get_min_be_info_cb, buf);
		nl802154_free(req);
	}

	return *buf;
}

static int nl802154_get_max_be_info_cb(struct nl_msg *msg, void *arg)
{
	int *max_be = arg;
	struct nlattr **tb = nl802154_parse(msg);

	if (tb[NL802154_ATTR_MAX_BE])
		*max_be = nla_get_u8(tb[NL802154_ATTR_MAX_BE]);

	return NL_SKIP;
}

static int nl802154_get_max_be(const char *ifname, int *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	/* try to find max be from interface info */
	res = nl802154_phy2ifname(ifname);

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_INTERFACE, 0);
	*buf = -1;

	if (req)
	{
		nl802154_send(req, nl802154_get_max_be_info_cb, buf);
		nl802154_free(req);
	}

	return *buf;
}

static int nl802154_get_csma_backoff_info_cb(struct nl_msg *msg, void *arg)
{
	int *csma_backoff = arg;
	struct nlattr **tb = nl802154_parse(msg);

	if (tb[NL802154_ATTR_MAX_CSMA_BACKOFFS])
		*csma_backoff = nla_get_u8(tb[NL802154_ATTR_MAX_CSMA_BACKOFFS]);

	return NL_SKIP;
}

static int nl802154_get_csma_backoff(const char *ifname, int *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	/* try to find max be from interface info */
	res = nl802154_phy2ifname(ifname);

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_INTERFACE, 0);
	*buf = -1;

	if (req) {
		nl802154_send(req, nl802154_get_csma_backoff_info_cb, buf);
		nl802154_free(req);
	}

	return *buf;
}

static int nl802154_get_frame_retry_info_cb(struct nl_msg *msg, void *arg)
{
	int *frame_retry = arg;
	struct nlattr **tb = nl802154_parse(msg);

	if (tb[NL802154_ATTR_MAX_CSMA_BACKOFFS])
		*frame_retry = nla_get_u8(tb[NL802154_ATTR_MAX_FRAME_RETRIES]);

	return NL_SKIP;
}

static int nl802154_get_frame_retry(const char *ifname, int *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	/* try to find frame retry from interface info */
	res = nl802154_phy2ifname(ifname);

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_INTERFACE, 0);
	*buf = -1;

	if (req)
	{
		nl802154_send(req, nl802154_get_frame_retry_info_cb, buf);
		nl802154_free(req);
	}

	return *buf;
}

static int nl802154_get_lbt_mode_info_cb(struct nl_msg *msg, void *arg)
{
	int *lbt_mode = arg;
	struct nlattr **tb = nl802154_parse(msg);

	if (tb[NL802154_ATTR_LBT_MODE])
		*lbt_mode = nla_get_u8(tb[NL802154_ATTR_LBT_MODE]);

	return NL_SKIP;
}

static int nl802154_get_lbt_mode(const char *ifname, int *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	/* try to find lbt mode from interface info */
	res = nl802154_phy2ifname(ifname);

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_INTERFACE, 0);
	*buf = -1;

	if (req)
	{
		nl802154_send(req, nl802154_get_lbt_mode_info_cb, buf);
		nl802154_free(req);
	}

	return *buf;
}

static int nl802154_get_cca_mode_info_cb(struct nl_msg *msg, void *arg)
{
	int *cca_mode = arg;
	struct nlattr **tb = nl802154_parse(msg);

	if (tb[NL802154_ATTR_CCA_MODE])
		*cca_mode = nla_get_u8(tb[NL802154_ATTR_CCA_MODE]);

	return NL_SKIP;
}

static int nl802154_get_cca_mode(const char *ifname, int *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	/* try to find cca mode from interface info */
	res = nl802154_phy2ifname(ifname);

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_WPAN_PHY, 0);
	*buf = -1;

	if (req)
	{
		nl802154_send(req, nl802154_get_cca_mode_info_cb, buf);
		nl802154_free(req);
	}

	return *buf;
}

static int nl802154_get_cca_opt_info_cb(struct nl_msg *msg, void *arg)
{
	int *cca_mode = arg;
	struct nlattr **tb = nl802154_parse(msg);

	if (tb[NL802154_ATTR_CCA_OPT])
		*cca_mode = nla_get_u8(tb[NL802154_ATTR_CCA_OPT]);

	return NL_SKIP;
}

static int nl802154_get_cca_opt(const char *ifname, int *buf)
{
	char *res;
	struct nl802154_msg_conveyor *req;

	/* try to find cca opt from interface info */
	res = nl802154_phy2ifname(ifname);

	req = nl802154_msg(res ? res : ifname, NL802154_CMD_GET_WPAN_PHY, 0);
	*buf = -1;

	if (req)
	{
		nl802154_send(req, nl802154_get_cca_opt_info_cb, buf);
		nl802154_free(req);
	}

	return *buf;
}

const struct iwpaninfo_ops nl802154_ops = {
	.name				= "nl802154",
	.probe				= nl802154_probe,
	.channel			= nl802154_get_channel,
	.frequency			= nl802154_get_frequency,
	.txpower			= nl802154_get_txpower,
	.mode				= nl802154_get_mode,
	.phyname			= nl802154_get_phyname,
	.txpwrlist			= nl802154_get_txpwrlist,
	.freqlist			= nl802154_get_freqlist,
	.cca_ed_lvl_list	= nl802154_get_cca_ed_lvl_list,
	.lookup_phy			= nl802154_lookup_phyname,
	.panid				= nl802154_get_panid,
	.short_address		= nl802154_get_short_address,
	.extended_address	= nl802154_get_extended_address,
	.page				= nl802154_get_page,
	.min_be				= nl802154_get_min_be,
	.max_be				= nl802154_get_max_be,
	.csma_backoff		= nl802154_get_csma_backoff,
	.frame_retry		= nl802154_get_frame_retry,
	.lbt_mode			= nl802154_get_lbt_mode,
	.cca_mode 			= nl802154_get_cca_mode,
	.cca_opt			= nl802154_get_cca_opt,
	.close				= nl802154_close
};
