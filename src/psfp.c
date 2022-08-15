/*
 * License: Dual MIT/GPL
 * Copyright (c) 2020 Microchip Corporation
 */

#include "common.h"
#include <getopt.h>
#include "kernel_types.h"
#include "mchp_ui_qos.h"

/* commands */
struct command
{
	int nargs;
	const char *name;
	int (*func) (int argc, char *const *argv);
	const char *format;
	char *(*help)(void);
};

static struct nla_policy mchp_psfp_genl_policy[MCHP_PSFP_ATTR_END] = {
	[MCHP_PSFP_ATTR_NONE] = { .type = NLA_UNSPEC },
	[MCHP_PSFP_SF_ATTR_CONF] = { .type = NLA_BINARY },
	[MCHP_PSFP_SF_ATTR_STATUS] = { .type = NLA_BINARY },
	[MCHP_PSFP_SF_ATTR_SFI] = { .type = NLA_U32 },
	[MCHP_PSFP_GCE_ATTR_CONF] = { .type = NLA_BINARY },
	[MCHP_PSFP_GCE_ATTR_SGI] = { .type = NLA_U32 },
	[MCHP_PSFP_GCE_ATTR_GCI] = { .type = NLA_U32 },
	[MCHP_PSFP_SG_ATTR_CONF] = { .type = NLA_BINARY },
	[MCHP_PSFP_SG_ATTR_STATUS] = { .type = NLA_BINARY },
	[MCHP_PSFP_SG_ATTR_SGI] = { .type = NLA_U32 },
	[MCHP_PSFP_FM_ATTR_CONF] = { .type = NLA_BINARY },
	[MCHP_PSFP_FM_ATTR_FMI] = { .type = NLA_U32 },
};

static int mchp_psfp_sf_conf_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct mchp_psfp_sf_conf *conf = arg;
	struct nlattr *attrs[MCHP_PSFP_ATTR_END];

	if (nla_parse(attrs, MCHP_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_PSFP_SF_ATTR_CONF]) {
		printf("ATTR_CONF not found\n");
		return -1;
	}

	nla_memcpy(conf, attrs[MCHP_PSFP_SF_ATTR_CONF],
		   sizeof(struct mchp_psfp_sf_conf));

	return NL_OK;
}

static int mchp_psfp_sf_conf_get(uint32_t sfi_id,
				    struct mchp_psfp_sf_conf *conf)
{
	struct mchp_psfp_sf_conf tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_PSFP_NETLINK,
				MCHP_PSFP_SF_GENL_CONF_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_psfp_sf_conf_read, &tmp);

	NLA_PUT_U32(msg, MCHP_PSFP_SF_ATTR_SFI, sfi_id);

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

	memcpy(conf, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);

	return rc;
}

static int mchp_psfp_sf_conf_set(uint32_t sfi_id,
				    struct mchp_psfp_sf_conf *conf)
{
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_PSFP_NETLINK,
				MCHP_PSFP_SF_GENL_CONF_SET, 1, &sk, &msg);

	NLA_PUT(msg, MCHP_PSFP_SF_ATTR_CONF, sizeof(*conf), conf);
	NLA_PUT_U32(msg, MCHP_PSFP_SF_ATTR_SFI, sfi_id);

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

	return rc;
}

static int mchp_psfp_sf_counters_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct mchp_psfp_sf_counters *counters = arg;
	struct nlattr *attrs[MCHP_PSFP_ATTR_END];

	if (nla_parse(attrs, MCHP_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_PSFP_SF_ATTR_STATUS]) {
		printf("ATTR_STATUS not found\n");
		return -1;
	}

	nla_memcpy(counters, attrs[MCHP_PSFP_SF_ATTR_STATUS],
		   sizeof(struct mchp_psfp_sf_counters));

	return NL_OK;
}

static void mchp_psfp_sf_status_get(uint32_t sfi_id)
{
	struct mchp_psfp_sf_counters counters;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc;

	memset(&counters, 0x0, sizeof(counters));

	rc = mchp_genl_start(MCHP_PSFP_NETLINK,
				MCHP_PSFP_SF_GENL_STATUS_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_psfp_sf_counters_read, &counters);

	NLA_PUT_U32(msg, MCHP_PSFP_SF_ATTR_SFI, sfi_id);

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

	printf("matching_frames_count: %" PRIu64 "\n", counters.matching_frames_count);
	printf("passing_frames_count: %" PRIu64 "\n", counters.passing_frames_count);
	printf("not_passing_frames_count: %" PRIu64 "\n", counters.not_passing_frames_count);
	printf("passing_sdu_count: %" PRIu64 "\n", counters.passing_sdu_count);
	printf("not_passing_sdu_count: %" PRIu64 "\n", counters.not_passing_sdu_count);
	printf("red_frames_count: %" PRIu64 "\n", counters.red_frames_count);

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);
}

static char *mchp_psfp_sf_help(void)
{
	return "--enable:                 Enable filter\n"
		" --max_sdu:                Maximum SDU size (zero disables SDU check)\n"
		" --block_oversize_enable:  StreamBlockedDueToOversizeFrameEnable\n"
		" --block_oversize:         StreamBlockedDueToOversizeFrame\n"
		" --status:                 Status\n";
}

static int cmd_sf(int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"enable", required_argument, NULL, 'a'},
		{"max_sdu", required_argument, NULL, 'b'},
		{"block_oversize_enable", required_argument, NULL, 'c'},
		{"block_oversize", required_argument, NULL, 'd'},
		{"status", no_argument, NULL, 'e'},
		{NULL, 0, NULL, 0}
	};
	struct mchp_psfp_sf_conf config;
	struct mchp_psfp_sf_conf tmp;
	uint32_t sfi_id = 0;
	int status = 0;
	int ch;

	/* read the id */
	sfi_id = atoi(argv[0]);

	if (mchp_psfp_sf_conf_get(sfi_id, &config) < 0)
		return 0;

	memcpy(&tmp, &config, sizeof(config));

	while ((ch = getopt_long(argc, argv, "a:b:c:d:e", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			config.enable = atoi(optarg);
			break;
		case 'b':
			config.max_sdu = atoi(optarg);
			break;
		case 'c':
			config.block_oversize_enable = atoi(optarg);
			break;
		case 'd':
			config.block_oversize = atoi(optarg);
			break;
		case 'e':
			status = 1;
			break;
		}
	}

	if (status) {
		mchp_psfp_sf_status_get(sfi_id);
		return 0;
	}

	if (memcmp(&tmp, &config, sizeof(config)) == 0) {
		printf("enable: %d\n", config.enable);
		printf("max_sdu: %u\n", config.max_sdu);
		printf("block_oversize_enable: %d\n", config.block_oversize_enable);
		printf("block_oversize: %d\n", config.block_oversize);
		return 0;
	}

	mchp_psfp_sf_conf_set(sfi_id, &config);

	return 0;
}

static int mchp_psfp_sg_conf_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct mchp_psfp_sg_conf *conf = arg;
	struct nlattr *attrs[MCHP_PSFP_ATTR_END];

	if (nla_parse(attrs, MCHP_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_PSFP_SG_ATTR_CONF]) {
		printf("ATTR_CONF not found\n");
		return -1;
	}

	nla_memcpy(conf, attrs[MCHP_PSFP_SG_ATTR_CONF],
		   sizeof(struct mchp_psfp_sg_conf));

	return NL_OK;
}

static int mchp_psfp_sg_conf_get(uint32_t sgi_id,
				    struct mchp_psfp_sg_conf *conf)
{
	struct mchp_psfp_sg_conf tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_PSFP_NETLINK,
				MCHP_PSFP_SG_GENL_CONF_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_psfp_sg_conf_read, &tmp);

	NLA_PUT_U32(msg, MCHP_PSFP_SG_ATTR_SGI, sgi_id);

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

	memcpy(conf, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);

	return rc;
}

static int mchp_psfp_sg_conf_set(uint32_t sgi_id,
				    struct mchp_psfp_sg_conf *conf)
{
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_PSFP_NETLINK,
				MCHP_PSFP_SG_GENL_CONF_SET, 1, &sk, &msg);

	NLA_PUT(msg, MCHP_PSFP_SG_ATTR_CONF, sizeof(*conf), conf);
	NLA_PUT_U32(msg, MCHP_PSFP_SG_ATTR_SGI, sgi_id);

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

	return rc;
}

static int mchp_psfp_sg_status_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct mchp_psfp_sg_status *status = arg;
	struct nlattr *attrs[MCHP_PSFP_ATTR_END];

	if (nla_parse(attrs, MCHP_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_PSFP_SG_ATTR_STATUS]) {
		printf("ATTR_STATUS not found\n");
		return -1;
	}

	nla_memcpy(status, attrs[MCHP_PSFP_SG_ATTR_STATUS],
		   sizeof(struct mchp_psfp_sg_status));

	return NL_OK;
}

static void mchp_psfp_sg_status_get(uint32_t sgi_id)
{
	struct mchp_psfp_sg_status status;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc;

	memset(&status, 0x0, sizeof(status));

	rc = mchp_genl_start(MCHP_PSFP_NETLINK,
				MCHP_PSFP_SG_GENL_STATUS_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_psfp_sg_status_read, &status);

	NLA_PUT_U32(msg, MCHP_PSFP_SG_ATTR_SGI, sgi_id);

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

	printf("gate_open: %d\n", status.gate_open);
	printf("ipv_enable: %d\n", status.ipv_enable);
	printf("ipv: %u\n", status.ipv);
	printf("config_change_time: %" PRIu64 "\n", status.config_change_time);
	printf("current_time: %" PRIu64 "\n", status.current_time);
	printf("config_pending: %d\n", status.config_pending);
	printf("base_time: %" PRIu64 "\n", status.oper.base_time);
	printf("cycle_time: %u\n", status.oper.cycle_time);
	printf("cycle_time_ext: %u\n", status.oper.cycle_time_ext);
	printf("gcl_length: %u\n", status.oper.gcl_length);

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);
}

static char *mchp_psfp_sg_help(void)
{
	return "--enable:                       PSFPGateEnabled: Enable/disable Gate\n"
		" --gate_open:                    PSFPAdminGateStates: Initial gate state\n"
		" --ipv_enable:                   Enable PSFPAdminIPV\n"
		" --ipv:                          PSFPAdminIPV\n"
		" --close_invalid_rx_enable:      PSFPGateClosedDueToInvalidRxEnable\n"
		" --close_invalid_rx:             PSFPGateClosedDueToInvalidRx\n"
		" --close_octets_exceeded_enable: PSFPGateClosedDueToOctetsExceededEnable\n"
		" --close_octets_exceeded:        PSFPGateClosedDueOctetsExceeded\n"
		" --config_change:                PSFPConfigChange: Apply config\n"
		" --base_time:                    PSFPAdminBaseTime/PSFPOperBaseTime\n"
		" --cycle_time:                   PSFPAdminCycleTime/PSFPOperCycleTime\n"
		" --cycle_time_ext:               PSFPAdminCycleTimeExtension/PSFPOperCycleTimeExtension\n"
		" --gcl_length:                   PSFPAdminControlListLength/ PSFPOperControlListLength\n"
		" --status:                       Status\n";
}

static int cmd_sg(int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"enable", required_argument, NULL, 'a'},
		{"gate_open", required_argument, NULL, 'b'},
		{"ipv_enable", required_argument, NULL, 'c'},
		{"ipv", required_argument, NULL, 'd'},
		{"close_invalid_rx_enable", required_argument, NULL, 'e'},
		{"close_invalid_rx", required_argument, NULL, 'f'},
		{"close_octets_exceeded_enable", required_argument, NULL, 'g'},
		{"close_octets_exceeded", required_argument, NULL, 'h'},
		{"config_change", required_argument, NULL, 'i'},
		{"base_time", required_argument, NULL, 'j'},
		{"cycle_time", required_argument, NULL, 'k'},
		{"cycle_time_ext", required_argument, NULL, 'l'},
		{"gcl_length", required_argument, NULL, 'm'},
		{"status", no_argument, NULL, 'n'},
		{NULL, 0, NULL, 0}
	};
	struct mchp_psfp_sg_conf config;
	struct mchp_psfp_sg_conf tmp;
	uint32_t sgi_id = 0;
	int status = 0;
	int ch;

	/* read the id */
	sgi_id = atoi(argv[0]);

	if (mchp_psfp_sg_conf_get(sgi_id, &config) < 0)
		return 0;

	memcpy(&tmp, &config, sizeof(config));

	while ((ch = getopt_long(argc, argv, "a:b:c:d:e:f:g:h:i:j:k:l:m:n", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			config.enable = atoi(optarg);
			break;
		case 'b':
			config.gate_open = atoi(optarg);
			break;
		case 'c':
			config.ipv_enable = atoi(optarg);
			break;
		case 'd':
			config.ipv = atoi(optarg);
			break;
		case 'e':
			config.close_invalid_rx_enable = atoi(optarg);
			break;
		case 'f':
			config.close_invalid_rx = atoi(optarg);
			break;
		case 'g':
			config.close_octets_exceeded_enable = atoi(optarg);
			break;
		case 'h':
			config.close_octets_exceeded = atoi(optarg);
			break;
		case 'i':
			config.config_change = atoi(optarg);
			break;
		case 'j':
			config.admin.base_time = atoll(optarg);
			break;
		case 'k':
			config.admin.cycle_time = atoi(optarg);
			break;
		case 'l':
			config.admin.cycle_time_ext = atoi(optarg);
			break;
		case 'm':
			config.admin.gcl_length = atoi(optarg);
			break;
		case 'n':
			status = 1;
			break;
		}
	}

	if (status) {
		mchp_psfp_sg_status_get(sgi_id);
		return 0;
	}

	if (memcmp(&tmp, &config, sizeof(config)) == 0) {
		printf("enable: %d\n", config.enable);
		printf("gate_open: %d\n", config.gate_open);
		printf("ipv_enable: %d\n", config.ipv_enable);
		printf("ipv: %u\n", config.ipv);
		printf("close_invalid_rx_enable: %d\n", config.close_invalid_rx_enable);
		printf("close_invalid_rx: %d\n", config.close_invalid_rx);
		printf("close_octets_exceeded_enable: %d\n", config.close_octets_exceeded_enable);
		printf("close_octets_exceeded: %d\n", config.close_octets_exceeded);
		printf("config_change: %d\n", config.config_change);
		printf("base_time: %" PRIu64 "\n", config.admin.base_time);
		printf("cycle_time: %u\n", config.admin.cycle_time);
		printf("cycle_time_ext: %u\n", config.admin.cycle_time_ext);
		printf("gcl_length: %u\n", config.admin.gcl_length);
		return 0;
	}

	mchp_psfp_sg_conf_set(sgi_id, &config);

	return 0;
}

static int mchp_psfp_gce_conf_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct mchp_psfp_gce *conf = arg;
	struct nlattr *attrs[MCHP_PSFP_ATTR_END];

	if (nla_parse(attrs, MCHP_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_PSFP_GCE_ATTR_CONF]) {
		printf("ATTR_CONF not found\n");
		return -1;
	}

	nla_memcpy(conf, attrs[MCHP_PSFP_GCE_ATTR_CONF],
		   sizeof(struct mchp_psfp_gce));

	return NL_OK;
}

static int mchp_psfp_gce_conf_get(uint32_t sgi_id, uint32_t gce_id,
				     struct mchp_psfp_gce *conf)
{
	struct mchp_psfp_gce tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_PSFP_NETLINK,
				MCHP_PSFP_GCE_GENL_CONF_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_psfp_gce_conf_read, &tmp);

	NLA_PUT_U32(msg, MCHP_PSFP_GCE_ATTR_SGI, sgi_id);
	NLA_PUT_U32(msg, MCHP_PSFP_GCE_ATTR_GCI, gce_id);

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

	memcpy(conf, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);

	return rc;
}

static int mchp_psfp_gce_conf_set(uint32_t sgi_id, uint32_t gce_id,
				     struct mchp_psfp_gce *conf)
{
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_PSFP_NETLINK,
				MCHP_PSFP_GCE_GENL_CONF_SET, 1, &sk, &msg);

	NLA_PUT(msg, MCHP_PSFP_GCE_ATTR_CONF, sizeof(*conf), conf);
	NLA_PUT_U32(msg, MCHP_PSFP_GCE_ATTR_SGI, sgi_id);
	NLA_PUT_U32(msg, MCHP_PSFP_GCE_ATTR_GCI, gce_id);

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

	return rc;
}

static int mchp_psfp_gce_status_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct mchp_psfp_gce *status = arg;
	struct nlattr *attrs[MCHP_PSFP_ATTR_END];

	if (nla_parse(attrs, MCHP_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_PSFP_GCE_ATTR_CONF]) {
		printf("ATTR_STATUS not found\n");
		return -1;
	}

	nla_memcpy(status, attrs[MCHP_PSFP_GCE_ATTR_CONF],
		   sizeof(struct mchp_psfp_gce));

	return NL_OK;
}

static void mchp_psfp_gce_status_get(uint32_t sgi_id, uint32_t gce_id)
{
	struct mchp_psfp_gce status;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc;

	memset(&status, 0x0, sizeof(status));

	rc = mchp_genl_start(MCHP_PSFP_NETLINK,
				MCHP_PSFP_GCE_GENL_STATUS_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_psfp_gce_status_read, &status);

	NLA_PUT_U32(msg, MCHP_PSFP_GCE_ATTR_SGI, sgi_id);
	NLA_PUT_U32(msg, MCHP_PSFP_GCE_ATTR_GCI, gce_id);

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

	printf("gate_open: %d\n", status.gate_open);
	printf("ipv_enable: %d\n", status.ipv_enable);
	printf("ipv: %u\n", status.ipv);
	printf("time_interval: %u\n", status.time_interval);
	printf("octet_max: %u\n", status.octet_max);

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);
}

static char *mchp_psfp_gce_help(void)
{
	return "--gate_open:     StreamGateState\n"
		" --ipv_enable:    enable IPV\n"
		" --ipv:           IPV\n"
		" --time_interval: TimeInterval (nsec)\n"
		" --octet_max:     IntervalOctetMax (zero disables check)\n"
		" --status:        Status\n";
}

static int cmd_gce(int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"gate_open", required_argument, NULL, 'a'},
		{"ipv_enable", required_argument, NULL, 'b'},
		{"ipv", required_argument, NULL, 'c'},
		{"time_interval", required_argument, NULL, 'd'},
		{"octet_max", required_argument, NULL, 'e'},
		{"status", no_argument, NULL, 'f'},
		{NULL, 0, NULL, 0}
	};
	struct mchp_psfp_gce config;
	struct mchp_psfp_gce tmp;
	uint32_t sgi_id = 0;
	uint32_t gce_id = 0;
	int status = 0;
	int ch;

	/* read the id and skip it */
	sgi_id = atoi(argv[0]);
	argc--;
	argv++;

	/* read the next id */
	gce_id = atoi(argv[0]);

	if (mchp_psfp_gce_conf_get(sgi_id, gce_id, &config) < 0)
		return 0;

	memcpy(&tmp, &config, sizeof(config));

	while ((ch = getopt_long(argc, argv, "a:b:c:d:e:f", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			config.gate_open = atoi(optarg);
			break;
		case 'b':
			config.ipv_enable = atoi(optarg);
			break;
		case 'c':
			config.ipv = atoi(optarg);
			break;
		case 'd':
			config.time_interval = atoi(optarg);
			break;
		case 'e':
			config.octet_max = atoi(optarg);
			break;
		case 'f':
			status = 1;
			break;
		}
	}

	if (status) {
		mchp_psfp_gce_status_get(sgi_id, gce_id);
		return 0;
	}

	if (memcmp(&tmp, &config, sizeof(config)) == 0) {
		printf("gate_open: %d\n", config.gate_open);
		printf("ipv_enable: %d\n", config.ipv_enable);
		printf("ipv: %u\n", config.ipv);
		printf("time_interval: %u\n", config.time_interval);
		printf("octet_max: %u\n", config.octet_max);
		return 0;
	}

	mchp_psfp_gce_conf_set(sgi_id, gce_id, &config);

	return 0;
}

static int mchp_psfp_fm_conf_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct mchp_psfp_fm_conf *conf = arg;
	struct nlattr *attrs[MCHP_PSFP_ATTR_END];

	if (nla_parse(attrs, MCHP_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_PSFP_FM_ATTR_CONF]) {
		printf("ATTR_CONF not found\n");
		return -1;
	}

	nla_memcpy(conf, attrs[MCHP_PSFP_FM_ATTR_CONF],
		   sizeof(struct mchp_psfp_fm_conf));

	return NL_OK;
}

static int mchp_psfp_fm_conf_get(uint32_t fmi_id,
				    struct mchp_psfp_fm_conf *conf)
{
	struct mchp_psfp_fm_conf tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_PSFP_NETLINK,
				MCHP_PSFP_FM_GENL_CONF_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_psfp_fm_conf_read, &tmp);

	NLA_PUT_U32(msg, MCHP_PSFP_FM_ATTR_FMI, fmi_id);

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

	memcpy(conf, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);

	return rc;
}

static int mchp_psfp_fm_conf_set(uint32_t fmi_id,
				     struct mchp_psfp_fm_conf *conf)
{
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_PSFP_NETLINK,
				MCHP_PSFP_FM_GENL_CONF_SET, 1, &sk, &msg);

	NLA_PUT(msg, MCHP_PSFP_FM_ATTR_CONF, sizeof(*conf), conf);
	NLA_PUT_U32(msg, MCHP_PSFP_FM_ATTR_FMI, fmi_id);

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

	return rc;
}

static char *mchp_psfp_fm_help(void)
{
	return "--enable:          Enable flow meter\n"
		" --cir:             kbit/s\n"
		" --cbs:             octets\n"
		" --eir:             kbit/s\n"
		" --ebs:             octets\n"
		" --cf:\n"
		" --drop_on_yellow:\n"
		" --mark_red_enable:\n"
		" --mark_red:\n";
}

static int cmd_fm(int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"enable", required_argument, NULL, 'a'},
		{"cir", required_argument, NULL, 'b'},
		{"cbs", required_argument, NULL, 'c'},
		{"eir", required_argument, NULL, 'd'},
		{"ebs",required_argument, NULL, 'e'},
		{"cf",required_argument, NULL, 'f'},
		{"drop_on_yellow",required_argument, NULL, 'g'},
		{"mark_red_enable",required_argument, NULL, 'h'},
		{"mark_red",required_argument, NULL, 'i'},
		{NULL, 0, NULL, 0}
	};
	struct mchp_psfp_fm_conf config;
	struct mchp_psfp_fm_conf tmp;
	uint32_t fmi_id = 0;
	int status = 0;
	int ch;

	/* read the id */
	fmi_id = atoi(argv[0]);

	if (mchp_psfp_fm_conf_get(fmi_id, &config) < 0)
		return 0;

	memcpy(&tmp, &config, sizeof(config));

	while ((ch = getopt_long(argc, argv, "a:b:c:d:e:f:g:h:i:", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			config.enable = atoi(optarg);
			break;
		case 'b':
			config.cir = atoi(optarg);
			break;
		case 'c':
			config.cbs = atoi(optarg);
			break;
		case 'd':
			config.eir = atoi(optarg);
			break;
		case 'e':
			config.ebs = atoi(optarg);
			break;
		case 'f':
			config.cf = atoi(optarg);
			break;
		case 'g':
			config.drop_on_yellow = atoi(optarg);
			break;
		case 'h':
			config.mark_red_enable = atoi(optarg);
			break;
		case 'i':
			config.mark_red = atoi(optarg);
			break;
		}
	}

	if (memcmp(&tmp, &config, sizeof(config)) == 0) {
		printf("enable: %d\n", config.enable);
		printf("cir: %u\n", config.cir);
		printf("cbs: %u\n", config.cbs);
		printf("eir: %u\n", config.eir);
		printf("ebs: %u\n", config.ebs);
		printf("cf: %d\n", config.cf);
		printf("drop_on_yellow: %d\n", config.drop_on_yellow);
		printf("mark_red_enable: %d\n", config.mark_red_enable);
		printf("mark_red: %d\n", config.mark_red);
		return 0;
	}

	mchp_psfp_fm_conf_set(fmi_id, &config);

	return 0;
}

static const struct command commands[] =
{
	/* Add/delete bridges */
	{1, "sf", cmd_sf, "sf sfi [options]", mchp_psfp_sf_help},
	{1, "sg", cmd_sg, "sg sgi [options]", mchp_psfp_sg_help},
	{2, "gce", cmd_gce, "gce sgi gce [options]", mchp_psfp_gce_help},
	{1, "fm", cmd_fm, "fm fmi [options]", mchp_psfp_fm_help},
};

static void command_helpall(void)
{
	int i;

	for (i = 0; i < COUNT_OF(commands); ++i) {
		printf("%s:   %-16s\n %s\n", commands[i].name,
		       commands[i].format, commands[i].help());
	}
}

static void help(void)
{
	printf("Usage: psfp sf|sg|gce|fm [options]\n");
	printf("options:\n");
	printf("  -h | --help              Show this help text\n");
	printf("options:\n");
	command_helpall();
}

static const struct command *command_lookup(const char *cmd)
{
	int i;

	for(i = 0; i < COUNT_OF(commands); ++i)
	{
		if(!strcmp(cmd, commands[i].name))
			return &commands[i];
	}

	return NULL;
}

static const struct command *command_lookup_and_validate(int argc,
							 char *const *argv,
							 int line_num)
{
	const struct command *cmd;

	cmd = command_lookup(argv[0]);
	if (!cmd) {
		if (line_num > 0)
			fprintf(stderr, "Error on line %d:\n", line_num);
		fprintf(stderr, "Unknown command [%s]\n", argv[0]);
		if (line_num == 0) {
			help();
			return NULL;
		}
	}

	return cmd;
}

int main(int argc, char *argv[])
{
	const struct command *cmd;
	int f;
	int ret;

	/* skip program name ('psfp') */
	argv++;
	argc--;

	if (!argc) {
		help();
		return 1;
	}

	cmd = command_lookup_and_validate(argc, argv, 0);
	if (!cmd)
		return 1;

	/* skip command (e.g. 'sf') */
	argv++;
	argc--;

	ret = cmd->func(argc, argv);
	return ret;
}

