/*
 * License: Dual MIT/GPL
 * Copyright (c) 2019 Microsemi Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/types.h>
#include <net/if.h>

enum lan966x_qos_fp_port_attr {
	LAN966X_QOS_FP_PORT_ATTR_NONE,
	LAN966X_QOS_FP_PORT_ATTR_CONF,
	LAN966X_QOS_FP_PORT_ATTR_STATUS,
	LAN966X_QOS_FP_PORT_ATTR_IDX,

	/* This must be the last entry */
	LAN966X_QOS_FP_PORT_ATTR_END,
};

#define LAN966X_QOS_FP_PORT_ATTR_MAX		LAN966X_QOS_FP_PORT_ATTR_END - 1

enum lan966x_qos_fp_port_genl {
	LAN966X_QOS_FP_PORT_GENL_CONF_SET,
	LAN966X_QOS_FP_PORT_GENL_CONF_GET,
	LAN966X_QOS_FP_PORT_GENL_STATUS_GET,
};

static struct nla_policy lan966x_qos_fp_port_genl_policy[LAN966X_QOS_FP_PORT_ATTR_END] = {
	[LAN966X_QOS_FP_PORT_ATTR_NONE] = { .type = NLA_UNSPEC },
	[LAN966X_QOS_FP_PORT_ATTR_CONF] = { .type = NLA_BINARY },
	[LAN966X_QOS_FP_PORT_ATTR_STATUS] = { .type = NLA_BINARY },
	[LAN966X_QOS_FP_PORT_ATTR_IDX] = { .type = NLA_U32 },
};

struct lan966x_qos_fp_port_conf {
	uint8_t  admin_status;      // IEEE802.1Qbu: framePreemptionStatusTable
	uint8_t  enable_tx;         // IEEE802.3br: aMACMergeEnableTx
	uint8_t  verify_disable_tx; // IEEE802.3br: aMACMergeVerifyDisableTx
	uint8_t  verify_time;       // IEEE802.3br: aMACMergeVerifyTime [msec]
	uint8_t  add_frag_size;     // IEEE802.3br: aMACMergeAddFragSize
};

enum lan966x_mm_status_verify {
	LAN966X_MM_STATUS_VERIFY_INITIAL,   /**< INIT_VERIFICATION */
	LAN966X_MM_STATUS_VERIFY_IDLE,      /**< VERIFICATION_IDLE */
	LAN966X_MM_STATUS_VERIFY_SEND,      /**< SEND_VERIFY */
	LAN966X_MM_STATUS_VERIFY_WAIT,      /**< WAIT_FOR_RESPONSE */
	LAN966X_MM_STATUS_VERIFY_SUCCEEDED, /**< VERIFIED */
	LAN966X_MM_STATUS_VERIFY_FAILED,    /**< VERIFY_FAIL */
	LAN966X_MM_STATUS_VERIFY_DISABLED   /**< Verification process is disabled */
};

struct lan966x_qos_fp_port_status {
	uint32_t hold_advance;      // TBD: IEEE802.1Qbu: holdAdvance [nsec]
	uint32_t release_advance;   // TBD: IEEE802.1Qbu: releaseAdvance [nsec]
	uint8_t preemption_active; // IEEE802.1Qbu: preemptionActive, IEEE802.3br: aMACMergeStatusTx
	uint8_t hold_request;      // TBD: IEEE802.1Qbu: holdRequest
	enum lan966x_mm_status_verify status_verify;     // IEEE802.3br: aMACMergeStatusVerify
};

/* From here here there can be changes */
static char *get_status_verify(enum lan966x_mm_status_verify status)
{
	switch (status) {
		case LAN966X_MM_STATUS_VERIFY_INITIAL: return "Initial";
		case LAN966X_MM_STATUS_VERIFY_IDLE: return "Idle";
		case LAN966X_MM_STATUS_VERIFY_SEND: return "Send";
		case LAN966X_MM_STATUS_VERIFY_WAIT: return "Wait";
		case LAN966X_MM_STATUS_VERIFY_SUCCEEDED: return "Succeeded";
		case LAN966X_MM_STATUS_VERIFY_FAILED: return "Failed";
		case LAN966X_MM_STATUS_VERIFY_DISABLED: return "Disabled";
		default: return "Unknown";
	}

	return "Unknown";
}

static int mchp_genl_start(const char *family_name, uint8_t cmd,
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

static int lan966x_qos_fp_port_read_conf(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct lan966x_qos_fp_port_conf *conf = arg;
	struct nlattr *attrs[LAN966X_QOS_FP_PORT_ATTR_END];

	if (nla_parse(attrs, LAN966X_QOS_FP_PORT_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), lan966x_qos_fp_port_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[LAN966X_QOS_FP_PORT_ATTR_CONF]) {
		printf("ATTR_CONF not found\n");
		return -1;
	}

	nla_memcpy(conf, attrs[LAN966X_QOS_FP_PORT_ATTR_CONF],
		   sizeof(struct lan966x_qos_fp_port_conf));

	return NL_OK;
}

static int lan966x_qos_fp_port_read_status(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct lan966x_qos_fp_port_status *status = arg;
	struct nlattr *attrs[LAN966X_QOS_FP_PORT_ATTR_END];

	if (nla_parse(attrs, LAN966X_QOS_FP_PORT_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), lan966x_qos_fp_port_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[LAN966X_QOS_FP_PORT_ATTR_STATUS]) {
		printf("ATTR_STATUS not found\n");
		return -1;
	}

	nla_memcpy(status, attrs[LAN966X_QOS_FP_PORT_ATTR_STATUS],
		   sizeof(struct lan966x_qos_fp_port_status));

	return NL_OK;
}

void lan966x_help(void)
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

void lan966x_conf_set(uint32_t index, struct lan966x_qos_fp_port_conf *config)
{
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc;

	rc = mchp_genl_start("lan966x_netlink",
			     LAN966X_QOS_FP_PORT_GENL_CONF_SET, 1, &sk, &msg);

	NLA_PUT(msg, LAN966X_QOS_FP_PORT_ATTR_CONF, sizeof(*config), config);
	NLA_PUT_U32(msg, LAN966X_QOS_FP_PORT_ATTR_IDX, index);

	rc = nl_send_auto(sk, msg);
	if (rc < 0) {
		printf("nl_send_auto() failed, rc: %d\n", rc);
		goto nla_put_failure;
	}

	rc = nl_recvmsgs_default(sk);
	if (rc < 0)
		printf("nl_recvmsgs_default() failed, rc: %d\n", rc);

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);
}

void lan966x_conf_get(uint32_t index, struct lan966x_qos_fp_port_conf *config)
{
	struct lan966x_qos_fp_port_conf tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc;

	rc = mchp_genl_start("lan966x_netlink",
			     LAN966X_QOS_FP_PORT_GENL_CONF_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    lan966x_qos_fp_port_read_conf, &tmp);

	NLA_PUT_U32(msg, LAN966X_QOS_FP_PORT_ATTR_IDX, index);

	rc = nl_send_auto(sk, msg);
	if (rc < 0) {
		printf("nl_send_auto() failed, rc: %d\n", rc);
		goto nla_put_failure;
	}

	rc = nl_recvmsgs_default(sk);
	if (rc < 0)
		printf("nl_recvmsgs_default() failed, rc: %d\n", rc);

	memcpy(config, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);
}

void lan966x_status_get(uint32_t index)
{
	struct lan966x_qos_fp_port_status status;
	char ifname[IF_NAMESIZE];
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc;

	memset(&status, 0x0, sizeof(status));
	memset(ifname, 0, IF_NAMESIZE);

	rc = mchp_genl_start("lan966x_netlink",
			     LAN966X_QOS_FP_PORT_GENL_STATUS_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    lan966x_qos_fp_port_read_status, &status);

	NLA_PUT_U32(msg, LAN966X_QOS_FP_PORT_ATTR_IDX, index);

	rc = nl_send_auto(sk, msg);
	if (rc < 0) {
		printf("nl_send_auto() failed, rc: %d\n", rc);
		goto nla_put_failure;
	}

	rc = nl_recvmsgs_default(sk);
	if (rc < 0) {
		printf("nl_recvmsgs_default() failed, rc: %d\n", rc);
		goto nla_put_failure;
	}

	printf("dev: %s\n", if_indextoname(index, ifname));
	printf("hold_advance: %d\n", status.hold_advance);
	printf("release_advance: %d\n", status.release_advance);
	printf("preemption_active: %d\n", status.preemption_active);
	printf("hold_request: %d\n", status.hold_request);
	printf("status_verify: %s\n", get_status_verify(status.status_verify));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);
}

int main(int argc, char *argv[])
{
	struct lan966x_qos_fp_port_conf config = {};
	struct lan966x_qos_fp_port_conf tmp = {};
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
		lan966x_help();
		return 0;
	}

	if (status) {
		if (ifindex != 0)
			lan966x_status_get(ifindex);
		else
			printf("dev is not set\n");
		return 0;
	}

	lan966x_conf_get(ifindex, &config);
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
		printf("enable_tx: %d\n", config.enable_tx);
		printf("verify_disable_tx: %d\n", config.verify_disable_tx);
		printf("verify_time: %d\n", config.verify_time);
		printf("add_frag_size: %d\n", config.add_frag_size);
		return 0;
	}

	lan966x_conf_set(ifindex, &config);

	return 0;
}
