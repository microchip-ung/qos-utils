/*
 * License: Dual MIT/GPL
 * Copyright (c) 2020 Microchip Corporation
 */

#include "common.h"

int mchp_genl_start(const char *family_name, uint8_t cmd,
		       uint8_t version, struct nl_sock **skp,
		       struct nl_msg **msgp)
{
	int err, family_id;

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

	err = genl_ctrl_resolve(*skp, family_name);
	if (err < 0) {
		printf("genl_ctrl_resolve() failed\n");
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
