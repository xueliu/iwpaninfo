/*
 * iwpaninfo - 802.15.4 WPAN Information Library - Command line frontend
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

#include <stdio.h>
#include <glob.h>
#include <inttypes.h>

#include "iwpaninfo.h"
#include "api/nl802154.h"

static char * format_channel(int ch)
{
	static char buf[8];

	if (ch <= 0)
		snprintf(buf, sizeof(buf), "unknown");
	else
		snprintf(buf, sizeof(buf), "%d", ch);

	return buf;
}

static char * format_frequency(int freq)
{
	static char buf[10];

	if (freq <= 0)
		snprintf(buf, sizeof(buf), "unknown");
	else
		snprintf(buf, sizeof(buf), "%.3f GHz", ((float)freq / 1000.0));

	return buf;
}

static char * format_txpower(int pwr)
{
	static char buf[10];

	if (pwr < -99)
		snprintf(buf, sizeof(buf), "unknown");
	else
		snprintf(buf, sizeof(buf), "%d dBm", pwr);

	return buf;
}

static char* print_mode(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int mode;
	static char buf[128];

	if (iwpan->mode(ifname, &mode))
		mode = IWPANINFO_OPMODE_UNKNOWN;

	snprintf(buf, sizeof(buf), "%s", IWPANINFO_OPMODE_NAMES[mode]);

	return buf;
}

static char* print_channel(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int ch;
	if (iwpan->channel(ifname, &ch))
		ch = -1;

	return format_channel(ch);
}

static char* print_frequency(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int freq;
	if (iwpan->frequency(ifname, &freq))
		freq = -1;

	return format_frequency(freq);
}

static char* print_txpower(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int pwr = 0;

	if (iwpan->txpower(ifname, &pwr))
		pwr = -100;

	return format_txpower(pwr);
}

static char* print_phyname(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	static char buf[32];

	if (!iwpan->phyname(ifname, buf))
		return buf;

	return "?";
}

static char* print_panid(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int panid;
	static char buf[12];

	if (iwpan->panid(ifname, &panid))
		snprintf(buf, sizeof(buf), "unknown");
	else
		snprintf(buf, sizeof(buf), "0x%04x", panid);

	return buf;
}

static char* print_short_address(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int short_addr;
	static char buf[12];

	if (iwpan->short_address(ifname, &short_addr))
		snprintf(buf, sizeof(buf), "unknown");
	else
		snprintf(buf, sizeof(buf), "0x%04x", short_addr);

	return buf;
}

static char* print_extended_address(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	uint64_t extended_addr;
	static char buf[20];

	if (iwpan->extended_address(ifname, &extended_addr))
		snprintf(buf, sizeof(buf), "unknown");
	else
		snprintf(buf, sizeof(buf), "0x%016" PRIx64, extended_addr);

	return buf;
}

static char* print_page(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int page;
	static char buf[20];

	if (0 != iwpan->page(ifname, &page))
		snprintf(buf, sizeof(buf), "unknown");
	else
		snprintf(buf, sizeof(buf), "%d", page);

	return buf;
}

static char* print_min_be(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int min_be;
	static char buf[8];

	if (-1 == iwpan->min_be(ifname, &min_be))
		snprintf(buf, sizeof(buf), "unknown");
	else
		snprintf(buf, sizeof(buf), "%d", min_be);

	return buf;
}

static char* print_max_be(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int max_be;
	static char buf[8];

	if (-1 == iwpan->max_be(ifname, &max_be))
		snprintf(buf, sizeof(buf), "unknown");
	else
		snprintf(buf, sizeof(buf), "%d", max_be);

	return buf;
}

static char* print_csma_backoff(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int csma_backoff;
	static char buf[8];

	if (-1 == iwpan->csma_backoff(ifname, &csma_backoff))
		snprintf(buf, sizeof(buf), "unknown");
	else
		snprintf(buf, sizeof(buf), "%d", csma_backoff);

	return buf;
}

static char* print_frame_retry(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int frame_retry;
	static char buf[8];

	if (-1 == iwpan->csma_backoff(ifname, &frame_retry))
		snprintf(buf, sizeof(buf), "unknown");
	else
		snprintf(buf, sizeof(buf), "%d", frame_retry);

	return buf;
}

static char* print_lbt_mode(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int lbt_mode;
	static char buf[8];

	if (-1 == iwpan->lbt_mode(ifname, &lbt_mode))
		snprintf(buf, sizeof(buf), "unknown");
	else if (0 == lbt_mode)
		snprintf(buf, sizeof(buf), "%s", "false");
	else
		snprintf(buf, sizeof(buf), "%s", "true");
	return buf;
}

static char* print_cca_opt(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int cca_opt;
	static char buf[32];

	if (-1 == iwpan->cca_opt(ifname, &cca_opt))
		snprintf(buf, sizeof(buf), "unknown");
	else if (NL802154_CCA_OPT_ENERGY_CARRIER_AND == cca_opt)
		snprintf(buf, sizeof(buf), "%s", "logical operator is 'and' ");
	else if (NL802154_CCA_OPT_ENERGY_CARRIER_OR == cca_opt)
		snprintf(buf, sizeof(buf), "%s", "logical operator is 'or' ");
	return buf;
}

static char* print_cca_mode(const struct iwpaninfo_ops *iwpan, const char *ifname)
{
	int cca_mode;
	static char buf[64];

	if (-1 == iwpan->cca_mode(ifname, &cca_mode))
		snprintf(buf, sizeof(buf), "unknown");
	else if (NL802154_CCA_ENERGY == cca_mode)
		snprintf(buf, sizeof(buf), "%d (Energy above threshold)", cca_mode);
	else if (NL802154_CCA_CARRIER == cca_mode)
		snprintf(buf, sizeof(buf), "%d (Carrier sense only)", cca_mode);
	else if (NL802154_CCA_ENERGY_CARRIER == cca_mode)
		snprintf(buf, sizeof(buf), "%d (Carrier sense with energy above threshold)", cca_mode);
	return buf;
}

static void print_info(const struct iwpaninfo_ops *iw, const char *ifname)
{
	printf("%-9s \n",ifname);
	printf("\tPhy Name: %s \n", print_phyname(iw, ifname));
	printf("\tMode: %s \n", print_mode(iw, ifname));
	printf("\tTx-Power: %s  \n", print_txpower(iw, ifname));
	printf("\tPage: %s  \n", print_page(iw, ifname));
	printf("\tChannel: %s (%s) \n", print_channel(iw, ifname),
			print_frequency(iw, ifname));
	printf("\tPAN ID: %s\n", print_panid(iw, ifname));
	printf("\tShort Address: %s\n", print_short_address(iw, ifname));
	printf("\tExtended Address: %s\n", print_extended_address(iw, ifname));
	printf("\tMin be: %s\n", print_min_be(iw, ifname));
	printf("\tMax be: %s\n", print_max_be(iw, ifname));
	printf("\tCSMA Backoff: %s\n", print_csma_backoff(iw, ifname));
	printf("\tFrame Retry: %s\n", print_frame_retry(iw, ifname));
	printf("\tLBT Mode: %s\n", print_lbt_mode(iw, ifname));
	printf("\tCCA Mode: %s\n", print_cca_mode(iw, ifname));
	printf("\tCCA OPT: %s\n", print_cca_opt(iw, ifname));
}

static void print_txpwrlist(const struct iwpaninfo_ops *iw, const char *ifname)
{
	int len, i;
	char buf[IWPANINFO_BUFSIZE];
	struct iwpaninfo_txpwrlist_entry *e;

	if (iw->txpwrlist(ifname, buf, &len) || len <= 0)
	{
		printf("No TX power information available\n");
		return;
	}

	for (i = 0; i < len; i += sizeof(struct iwpaninfo_txpwrlist_entry))
	{
		e = (struct iwpaninfo_txpwrlist_entry *) &buf[i];

		printf("%.3g dBm \n", e->dbm);
	}
}

static void print_cca_ed_lvl_list(const struct iwpaninfo_ops *iw, const char *ifname)
{
	int len, i;
	char buf[IWPANINFO_BUFSIZE];
	struct iwpaninfo_cca_ed_lvl_list_entry *e;

	if (iw->cca_ed_lvl_list(ifname, buf, &len) || len <= 0)
	{
		printf("No CCA ED level information available\n");
		return;
	}

	for (i = 0; i < len; i += sizeof(struct iwpaninfo_cca_ed_lvl_list_entry))
	{
		e = (struct iwpaninfo_cca_ed_lvl_list_entry *) &buf[i];

		printf("%.3g dBm \n", e->dbm);
	}
}

static void print_freq_handler(int channel_page, int channel) {
	float freq = 0;

	switch (channel_page) {
	case 0:
		if (channel == 0) {
			freq = 868.3;
			printf("%5.1f", freq);
			break;
		} else if (channel > 0 && channel < 11) {
			freq = 906 + 2 * (channel - 1);
		} else {
			freq = 2405 + 5 * (channel - 11);
		}
		printf("%5.0f", freq);
		break;
	case 1:
		if (channel == 0) {
			freq = 868.3;
			printf("%5.1f", freq);
			break;
		} else if (channel >= 1 && channel <= 10) {
			freq = 906 + 2 * (channel - 1);
		}
		printf("%5.0f", freq);
		break;
	case 2:
		if (channel == 0) {
			freq = 868.3;
			printf("%5.1f", freq);
			break;
		} else if (channel >= 1 && channel <= 10) {
			freq = 906 + 2 * (channel - 1);
		}
		printf("%5.0f", freq);
		break;
	case 3:
		if (channel >= 0 && channel <= 12) {
			freq = 2412 + 5 * channel;
		} else if (channel == 13) {
			freq = 2484;
		}
		printf("%4.0f", freq);
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
		printf("%6.1f", freq);
		break;
	case 5:
		if (channel >= 0 && channel <= 3) {
			freq = 780 + 2 * channel;
		} else if (channel >= 4 && channel <= 7) {
			freq = 780 + 2 * (channel - 4);
		}
		printf("%3.0f", freq);
		break;
	case 6:
		if (channel >= 0 && channel <= 7) {
			freq = 951.2 + 0.6 * channel;
		} else if (channel >= 8 && channel <= 9) {
			freq = 954.4 + 0.2 * (channel - 8);
		} else if (channel >= 10 && channel <= 21) {
			freq = 951.1 + 0.4 * (channel - 10);
		}

		printf("%5.1f", freq);
		break;
	default:
		printf("Unknown");
		break;
	}
}

static void print_freqlist(const struct iwpaninfo_ops *iw, const char *ifname)
{
	int i, len, ch, page;
	char buf[IWPANINFO_BUFSIZE];
	struct iwpaninfo_freqlist_entry *e;

	if (iw->freqlist(ifname, buf, &len) || len <= 0)
	{
		printf("No frequency information available\n");
		return;
	}

	if (iw->channel(ifname, &ch))
		ch = -1;

	if (iw->page(ifname, &page))
		page = -1;

	for (i = 0; i < len; i += sizeof(struct iwpaninfo_freqlist_entry))
	{
		e = (struct iwpaninfo_freqlist_entry *) &buf[i];

		printf("%s ", (ch == e->channel) ? "*" : " ");
		print_freq_handler(page, e->channel);
		printf(" MHz ");
		printf("(Channel %s) \n", format_channel(e->channel));
	}
}

static void lookup_phy(const struct iwpaninfo_ops *iw, const char *section)
{
	char buf[IWPANINFO_BUFSIZE];
	if (!iw->lookup_phy)
	{
		fprintf(stderr, "Not supported\n");
		return;
	}

	if (iw->lookup_phy(section, buf))
	{
		fprintf(stderr, "802.15.4 phy not found\n");
		return;
	}

	printf("%s\n", buf);
}


int main(int argc, char **argv)
{
	int i, rv = 0;
	char *p;
	const struct iwpaninfo_ops *iwpan;
	glob_t globbuf;

	if (argc > 1 && argc < 3)
	{
		fprintf(stderr,
			"Usage:\n"
			"	iwpaninfo <device> info\n"
			"	iwpaninfo <device> txpowerlist\n"
			"	iwpaninfo <device> freqlist\n"
			"	iwpaninfo <device> ccaedlvllist\n"
			"	iwpaninfo <backend> phyname <section>\n"
		);

		return 1;
	}

	if (argc == 1)
	{
		glob("/sys/class/net/*", 0, NULL, &globbuf);

		for (i = 0; i < globbuf.gl_pathc; i++)
		{
			p = strrchr(globbuf.gl_pathv[i], '/');

			if (!p)
				continue;

			iwpan = iwpaninfo_backend(++p);

			if (!iwpan)
				continue;

			print_info(iwpan, p);
			printf("\n");
		}

		globfree(&globbuf);
		return 0;
	}

	if (argc > 3)
	{
		iwpan = iwpaninfo_backend_by_name(argv[1]);

		if (!iwpan)
		{
			fprintf(stderr, "No such wpan backend: %s\n", argv[1]);
			rv = 1;
		}
		else
		{
			switch (argv[2][0])
			{
			case 'p':
				lookup_phy(iwpan, argv[3]);
				break;

			default:
				fprintf(stderr, "Unknown command: %s\n", argv[2]);
				rv = 1;
			}
		}
	}
	else
	{
		iwpan = iwpaninfo_backend(argv[1]);

		if (!iwpan)
		{
			fprintf(stderr, "No such wpan device: %s\n", argv[1]);
			rv = 1;
		}
		else
		{
			for (i = 2; i < argc; i++)
			{
				switch(argv[i][0])
				{
				case 'i':
					print_info(iwpan, argv[1]);
					break;
				case 't':
					print_txpwrlist(iwpan, argv[1]);
					break;
				case 'f':
					print_freqlist(iwpan, argv[1]);
					break;
				case 'c':
					print_cca_ed_lvl_list(iwpan, argv[1]);
					break;
				default:
					fprintf(stderr, "Unknown command: %s\n", argv[i]);
					rv = 1;
				}
			}
		}
	}

	iwpaninfo_finish();

	return rv;
}
