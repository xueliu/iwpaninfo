#ifndef __IWPANINFO_H_
#define __IWPANINFO_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <glob.h>
#include <ctype.h>
#include <dirent.h>
#include <stdint.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <net/if.h>
#include <errno.h>

#define IWPANINFO_BUFSIZE	32 * 1024

enum iwpaninfo_opmode {
	IWPANINFO_OPMODE_NODE		= 0,
	IWPANINFO_OPMODE_MONITOR	= 1,
	IWPANINFO_OPMODE_COORD	    = 2,
	IWPANINFO_OPMODE_UNKNOWN,
};

extern const char *IWPANINFO_OPMODE_NAMES[];

struct iwpaninfo_txpwrlist_entry {
	float dbm;
//	float mw;
};

struct iwpaninfo_cca_ed_lvl_list_entry {
	float dbm;
};

struct iwpaninfo_freqlist_entry {
	uint8_t channel;
	float mhz;
};

struct iwpaninfo_ops {
	const char *name;

	int (*probe)(const char *ifname);
	int (*mode)(const char *, int *);
	int (*channel)(const char *, int *);
	int (*frequency)(const char *, int *);
	int (*txpower)(const char *, int *);
	int (*phyname)(const char *, char *);
	int (*txpwrlist)(const char *, char *, int *);
	int (*freqlist)(const char *, char *, int *);
	int (*cca_ed_lvl_list)(const char *, char *, int *);
	int (*lookup_phy)(const char *, char *);
	int (*panid)(const char *, int *);
	int (*short_address)(const char *, int *);
	int (*extended_address)(const char *, uint64_t *);
	int (*page)(const char *, int *);
	int (*min_be)(const char *, int *);
	int (*max_be)(const char *, int *);
	int (*csma_backoff)(const char *, int *);
	int (*frame_retry)(const char *, int *);
	int (*lbt_mode)(const char *, int *);
	int (*cca_mode)(const char *, int *);
	void (*close)(void);
};

const char * iwpaninfo_type(const char *ifname);
const struct iwpaninfo_ops * iwpaninfo_backend(const char *ifname);
const struct iwpaninfo_ops * iwpaninfo_backend_by_name(const char *name);
void iwpaninfo_finish(void);

extern const struct iwpaninfo_ops nl802154_ops;

#include "iwpaninfo/utils.h"

#endif
