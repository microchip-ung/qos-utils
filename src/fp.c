/*
 * License: Dual MIT/GPL
 * Copyright (c) 2020 Microsemi Corporation
 */

#include "common.h"
#include <getopt.h>
#include <net/if.h>
#include "kernel_types.h"
#include "mchp_ui_qos.h"

static struct nla_policy mchp_qos_fp_port_genl_policy[MCHP_QOS_FP_PORT_ATTR_END] = {
	[MCHP_QOS_FP_PORT_ATTR_NONE] = { .type = NLA_UNSPEC },
	[MCHP_QOS_FP_PORT_ATTR_CONF] = { .type = NLA_BINARY },
	[MCHP_QOS_FP_PORT_ATTR_STATUS] = { .type = NLA_BINARY },
	[MCHP_QOS_FP_PORT_ATTR_IDX] = { .type = NLA_U32 },
};

/* From here here there can be changes */
static char *get_status_verify(enum mchp_mm_status_verify status)
{
	switch (status) {
		case MCHP_MM_STATUS_VERIFY_INITIAL: return "Initial";
		case MCHP_MM_STATUS_VERIFY_IDLE: return "Idle";
		case MCHP_MM_STATUS_VERIFY_SEND: return "Send";
		case MCHP_MM_STATUS_VERIFY_WAIT: return "Wait";
		case MCHP_MM_STATUS_VERIFY_SUCCEEDED: return "Succeeded";
		case MCHP_MM_STATUS_VERIFY_FAILED: return "Failed";
		case MCHP_MM_STATUS_VERIFY_DISABLED: return "Disabled";
		default: return "Unknown";
	}

	return "Unknown";
}

static struct option long_options[] =
{
	{"dev", required_argument, NULL, 'a'},
	{"admin_status", required_argument, NULL, 'b'},
	{"enable_tx", required_argument, NULL, 'c'},
	{"verify_disable_tx", required_argument, NULL, 'd'},
	{"verify_time", required_argument, NULL, 'e'},
	{"add_frag_size", required_argument, NULL, 'f'},
	{"status", no_argument, NULL, 'g'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0}
};

static int mchp_qos_fp_port_read_conf(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct mchp_qos_fp_port_conf *conf = arg;
	struct nlattr *attrs[MCHP_QOS_FP_PORT_ATTR_END];

	if (nla_parse(attrs, MCHP_QOS_FP_PORT_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_qos_fp_port_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_QOS_FP_PORT_ATTR_CONF]) {
		printf("ATTR_CONF not found\n");
		return -1;
	}

	nla_memcpy(conf, attrs[MCHP_QOS_FP_PORT_ATTR_CONF],
		   sizeof(struct mchp_qos_fp_port_conf));

	return NL_OK;
}

static int mchp_qos_fp_port_read_status(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct mchp_qos_fp_port_status *status = arg;
	struct nlattr *attrs[MCHP_QOS_FP_PORT_ATTR_END];

	if (nla_parse(attrs, MCHP_QOS_FP_PORT_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_qos_fp_port_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_QOS_FP_PORT_ATTR_STATUS]) {
		printf("ATTR_STATUS not found\n");
		return -1;
	}

	nla_memcpy(status, attrs[MCHP_QOS_FP_PORT_ATTR_STATUS],
		   sizeof(struct mchp_qos_fp_port_status));

	return NL_OK;
}

void mchp_help(void)
{
	printf("options:\n"
		"--dev:                    dev name\n"
		"--admin_status:           admin status\n"
		"--enable_tx:              enable tx\n"
		"--verify_disable_tx:      verify disable tx\n"
		"--verify_time:            verify time\n"
		"--add_frag_size:          add frag size\n"
		"--status:                 status\n"
		"--help:                   help\n");
}

void mchp_conf_set(uint32_t index, struct mchp_qos_fp_port_conf *config)
{
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc;

	rc = mchp_genl_start("mchp_netlink",
				MCHP_QOS_FP_PORT_GENL_CONF_SET, 1, &sk, &msg);

	NLA_PUT(msg, MCHP_QOS_FP_PORT_ATTR_CONF, sizeof(*config), config);
	NLA_PUT_U32(msg, MCHP_QOS_FP_PORT_ATTR_IDX, index);

	rc = nl_send_auto(sk, msg);
	if (rc < 0) {
		printf("nl_send_auto() failed, rc: %d\n", rc);
		goto nla_put_failure;
	}

	rc = nl_recvmsgs_default(sk);
	if (rc < 0)
		printf("nl_recvmsgs_default() failed, rc: %d (%s)\n", rc,
		       nl_geterror(rc));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);
}

void mchp_conf_get(uint32_t index, struct mchp_qos_fp_port_conf *config)
{
	struct mchp_qos_fp_port_conf tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc;

	rc = mchp_genl_start("mchp_netlink",
				MCHP_QOS_FP_PORT_GENL_CONF_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_qos_fp_port_read_conf, &tmp);

	NLA_PUT_U32(msg, MCHP_QOS_FP_PORT_ATTR_IDX, index);

	rc = nl_send_auto(sk, msg);
	if (rc < 0) {
		printf("nl_send_auto() failed, rc: %d\n", rc);
		goto nla_put_failure;
	}

	rc = nl_recvmsgs_default(sk);
	if (rc < 0)
		printf("nl_recvmsgs_default() failed, rc: %d (%s)\n", rc,
		       nl_geterror(rc));

	memcpy(config, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);
}

void mchp_status_get(uint32_t index)
{
	struct mchp_qos_fp_port_status status;
	char ifname[IF_NAMESIZE];
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc;

	memset(&status, 0x0, sizeof(status));
	memset(ifname, 0, IF_NAMESIZE);

	rc = mchp_genl_start("mchp_netlink",
				MCHP_QOS_FP_PORT_GENL_STATUS_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_qos_fp_port_read_status, &status);

	NLA_PUT_U32(msg, MCHP_QOS_FP_PORT_ATTR_IDX, index);

	rc = nl_send_auto(sk, msg);
	if (rc < 0) {
		printf("nl_send_auto() failed, rc: %d\n", rc);
		goto nla_put_failure;
	}

	rc = nl_recvmsgs_default(sk);
	if (rc < 0) {
		printf("nl_recvmsgs_default() failed, rc: %d (%s)\n", rc,
		       nl_geterror(rc));
		goto nla_put_failure;
	}

	printf("dev: %s\n", if_indextoname(index, ifname));
	printf("hold_advance: %u\n", status.hold_advance);
	printf("release_advance: %u\n", status.release_advance);
	printf("preemption_active: %u\n", status.preemption_active);
	printf("hold_request: %u\n", status.hold_request);
	printf("status_verify: %s\n", get_status_verify(status.status_verify));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);
}

int main(int argc, char *argv[])
{
	struct mchp_qos_fp_port_conf config = {};
	struct mchp_qos_fp_port_conf tmp = {};
	uint32_t ifindex;
	int rc, ch;
	int status = 0;
	int help = 0;

	memset(&config, 0, sizeof(config));

	while ((ch = getopt_long(argc, argv, "a:b:c:d:e:f:gh", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			ifindex = if_nametoindex(optarg);
			break;
		case 'g':
			status = 1;
			break;
		case 'h':
			help = 1;
			break;
		}
	}
	optind = 1;

	if (help) {
		mchp_help();
		return 0;
	}

	if (status) {
		if (ifindex != 0)
			mchp_status_get(ifindex);
		else
			printf("dev is not set\n");
		return 0;
	}

	mchp_conf_get(ifindex, &config);
	memcpy(&tmp, &config, sizeof(config));

	while ((ch = getopt_long(argc, argv, "a:b:c:d:e:f:gh", long_options, NULL)) != -1) {
		switch (ch) {
		case 'b':
			sscanf(optarg, "0x%hhx", &config.admin_status);
			break;
		case 'c':
			config.enable_tx = atoi(optarg);
			break;
		case 'd':
			config.verify_disable_tx = atoi(optarg);
			break;
		case 'e':
			config.verify_time = atoi(optarg);
			break;
		case 'f':
			config.add_frag_size = atoi(optarg);
			break;
		}
	}

	if (memcmp(&tmp, &config, sizeof(config)) == 0) {
		printf("admin_status: 0x%x\n", config.admin_status);
		printf("enable_tx: %u\n", config.enable_tx);
		printf("verify_disable_tx: %u\n", config.verify_disable_tx);
		printf("verify_time: %u\n", config.verify_time);
		printf("add_frag_size: %u\n", config.add_frag_size);
		return 0;
	}

	mchp_conf_set(ifindex, &config);

	return 0;
}
