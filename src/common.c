/*
 * License: Dual MIT/GPL
 * Copyright (c) 2020 Microchip Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "version.h"

void mchp_version_check(int argc, char *argv[], const char *prog)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-v") == 0 ||
		    strcmp(argv[i], "--version") == 0) {
			printf("%s version: %s\n", prog, gGIT_VERSION);
			exit(0);
		}
	}
}

#define MCHP_QOS_NL_EVAR	"MCHP_NETLINK_QOS"
#define MCHP_FP_NL_EVAR		"MCHP_NETLINK_FP"
#define MCHP_FRER_NL_EVAR	"MCHP_NETLINK_FRER"
#define MCHP_PSFP_NL_EVAR	"MCHP_NETLINK_PSFP"

static const char *rd_family_evar(const char *family_name) {
	char *fenv = NULL;

	if (strcmp(family_name, MCHP_QOS_NETLINK) == 0) {
		fenv = getenv(MCHP_QOS_NL_EVAR);
	} else if (strcmp(family_name, MCHP_FRER_NETLINK) == 0) {
		fenv = getenv(MCHP_FRER_NL_EVAR);
	} else if (strcmp(family_name, MCHP_FP_NETLINK) == 0) {
		fenv = getenv(MCHP_FP_NL_EVAR);
	} else if (strcmp(family_name, MCHP_PSFP_NETLINK) == 0) {
		fenv = getenv(MCHP_PSFP_NL_EVAR);
	}

	if (fenv)
		return fenv;

	return family_name;
}

int mchp_genl_start(const char *family_name, uint8_t cmd,
		    uint8_t version, struct nl_sock **skp,
		    struct nl_msg **msgp)
{
	int err, family_id;
	const char *fname;

	*skp = nl_socket_alloc();
	if (!*skp) {
		printf("nl_socket_alloc() failed\n");
		return -1;
	}

	err = genl_connect(*skp);
	if (err < 0) {
		printf("genl_connect() failed\n");
		goto err_free_socket;
	}

	fname = rd_family_evar(family_name);
	err = genl_ctrl_resolve(*skp, fname);
	if (err < 0) {
		printf("genl_ctrl_resolve() failed family name: '%s'\n", fname);
		goto err_free_socket;
	}
	family_id = err;

	*msgp = nlmsg_alloc();
	if (!*msgp) {
		printf("nlmsg_alloc() failed\n");
		err = -1;
		goto err_free_socket;
	}

	if (!genlmsg_put(*msgp,
			 NL_AUTO_PORT,
			 NL_AUTO_SEQ,
			 family_id,
			 0,
			 NLM_F_REQUEST | NLM_F_ACK,
			 cmd,
			 version)) {
		printf("genlmsg_put() failed\n");
		goto nla_put_failure;
	}

	return 0;

nla_put_failure:
	nlmsg_free(*msgp);

err_free_socket:
	nl_socket_free(*skp);
	return err;
}
