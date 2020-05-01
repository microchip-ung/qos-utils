/*
 * License: Dual MIT/GPL
 * Copyright (c) 2020 Microchip Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/types.h>
#include <net/if.h>
#include <stdbool.h>

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#define LAN966X_PSFP_NETLINK	"lan966x_psfp_nl"

enum lan966x_psfp_attr {
	LAN966X_PSFP_ATTR_NONE,
	LAN966X_PSFP_SF_ATTR_CONF,
	LAN966X_PSFP_SF_ATTR_STATUS,
	LAN966X_PSFP_SF_ATTR_SFI,
	LAN966X_PSFP_GCE_ATTR_CONF,
	LAN966X_PSFP_GCE_ATTR_SGI,
	LAN966X_PSFP_GCE_ATTR_GCI,
	LAN966X_PSFP_SG_ATTR_CONF,
	LAN966X_PSFP_SG_ATTR_STATUS,
	LAN966X_PSFP_SG_ATTR_SGI,
	LAN966X_PSFP_FM_ATTR_CONF,
	LAN966X_PSFP_FM_ATTR_FMI,

	/* This must be the last entry */
	LAN966X_PSFP_ATTR_END,
};

#define LAN966X_PSFP_ATTR_MAX		LAN966X_PSFP_ATTR_END - 1

enum lan966x_psfp_genl {
	LAN966X_PSFP_SF_GENL_CONF_SET,
	LAN966X_PSFP_SF_GENL_CONF_GET,
	LAN966X_PSFP_SF_GENL_STATUS_GET,
	LAN966X_PSFP_GCE_GENL_CONF_SET,
	LAN966X_PSFP_GCE_GENL_CONF_GET,
	LAN966X_PSFP_GCE_GENL_STATUS_GET,
	LAN966X_PSFP_SG_GENL_CONF_SET,
	LAN966X_PSFP_SG_GENL_CONF_GET,
	LAN966X_PSFP_SG_GENL_STATUS_GET,
	LAN966X_PSFP_FM_GENL_CONF_SET,
	LAN966X_PSFP_FM_GENL_CONF_GET,
};

static struct nla_policy lan966x_psfp_genl_policy[LAN966X_PSFP_ATTR_END] = {
	[LAN966X_PSFP_ATTR_NONE] = { .type = NLA_UNSPEC },
	[LAN966X_PSFP_SF_ATTR_CONF] = { .type = NLA_BINARY },
	[LAN966X_PSFP_SF_ATTR_STATUS] = { .type = NLA_BINARY },
	[LAN966X_PSFP_SF_ATTR_SFI] = { .type = NLA_U32 },
	[LAN966X_PSFP_GCE_ATTR_CONF] = { .type = NLA_BINARY },
	[LAN966X_PSFP_GCE_ATTR_SGI] = { .type = NLA_U32 },
	[LAN966X_PSFP_GCE_ATTR_GCI] = { .type = NLA_U32 },
	[LAN966X_PSFP_SG_ATTR_CONF] = { .type = NLA_BINARY },
	[LAN966X_PSFP_SG_ATTR_STATUS] = { .type = NLA_BINARY },
	[LAN966X_PSFP_SG_ATTR_SGI] = { .type = NLA_U32 },
	[LAN966X_PSFP_FM_ATTR_CONF] = { .type = NLA_BINARY },
	[LAN966X_PSFP_FM_ATTR_FMI] = { .type = NLA_U32 },
};

/* PSFP Stream Filter configuration */
struct lan966x_psfp_sf_conf {
	bool enable;     /* Enable filter */
	uint16_t max_sdu;     /* Maximum SDU size (zero disables SDU check) */
	bool block_oversize_enable; /* StreamBlockedDueToOversizeFrameEnable */
	bool block_oversize;        /* StreamBlockedDueToOversizeFrame */
};

/* PSFP Stream Filter counters */
struct lan966x_psfp_sf_counters {
	uint64_t matching_frames_count;
	uint64_t passing_frames_count;
	uint64_t not_passing_frames_count;
	uint64_t passing_sdu_count;
	uint64_t not_passing_sdu_count;
	uint64_t red_frames_count;
};

/* PSFP Gate Control Entry configuration/status */
struct lan966x_psfp_gce {
	bool gate_open;    /* StreamGateState */
	bool ipv_enable;   /* enable IPV */
	uint8_t ipv;            /* IPV */
	uint32_t time_interval; /* TimeInterval (nsec) */
	uint32_t octet_max;     /* IntervalOctetMax (zero disables check) */
};

/* PSFP Gate Control List configuration */
struct lan966x_psfp_gcl_conf {
	int64_t base_time;  /* PSFPAdminBaseTime/PSFPOperBaseTime */
	uint32_t cycle_time;     /* PSFPAdminCycleTime/PSFPOperCycleTime */
	uint32_t cycle_time_ext; /* PSFPAdminCycleTimeExtension/
			       PSFPOperCycleTimeExtension */
	uint32_t gcl_length;     /* PSFPAdminControlListLength/
			       PSFPOperControlListLength */
};

/* PSFP Stream Gate configuration */
struct lan966x_psfp_sg_conf{
	bool enable;               /* PSFPGateEnabled: Enable/disable Gate */
	bool gate_open;            /* PSFPAdminGateStates: Initial gate state */
	bool ipv_enable;           /* Enable PSFPAdminIPV */
	uint8_t ipv;                    /* PSFPAdminIPV */
	bool close_invalid_rx_enable; /* PSFPGateClosedDueToInvalidRxEnable */
	bool close_invalid_rx;        /* PSFPGateClosedDueToInvalidRx */
	bool close_octets_exceeded_enable; /* PSFPGateClosedDueToOctetsExceededEnable */
	bool close_octets_exceeded; /* PSFPGateClosedDueOctetsExceeded */
	bool config_change;         /* PSFPConfigChange: Apply config */
	struct lan966x_psfp_gcl_conf admin; /* PSFPAdmin* */
};

/* PSFP Stream Gate status */
struct lan966x_psfp_sg_status {
	bool gate_open;             /* PSFPOperGateStates */
	bool ipv_enable;            /* PSFPOperIPV enabled */
	uint8_t ipv;                /* PSFPOperIPV */
	int64_t config_change_time; /* PSFPConfigChangeTime */
	int64_t current_time;       /* PSFPCurrentTime */
	bool config_pending;        /* PSFPConfigPending */
	struct lan966x_psfp_gcl_conf oper; /* PSFPOper* */
};

/* PSFP Flow Meter configuration */
struct lan966x_psfp_fm_conf {
	bool enable; /* Enable flow meter */
	uint32_t cir; /* kbit/s */
	uint32_t cbs; /* octets */
	uint32_t eir; /* kbit/s */
	uint32_t ebs; /* octets */
	bool cf;
	bool drop_on_yellow;
	bool mark_red_enable;
	bool mark_red;
};

/* commands */
struct command
{
	int nargs;
	const char *name;
	int (*func) (int argc, char *const *argv);
	const char *format;
	char *(*help)(void);
};

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

static int lan966x_psfp_sf_conf_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct lan966x_psfp_sf_conf *conf = arg;
	struct nlattr *attrs[LAN966X_PSFP_ATTR_END];

	if (nla_parse(attrs, LAN966X_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), lan966x_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[LAN966X_PSFP_SF_ATTR_CONF]) {
		printf("ATTR_CONF not found\n");
		return -1;
	}

	nla_memcpy(conf, attrs[LAN966X_PSFP_SF_ATTR_CONF],
		   sizeof(struct lan966x_psfp_sf_conf));

	return NL_OK;
}

static int lan966x_psfp_sf_conf_get(uint32_t sfi_id,
				    struct lan966x_psfp_sf_conf *conf)
{
	struct lan966x_psfp_sf_conf tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(LAN966X_PSFP_NETLINK,
			     LAN966X_PSFP_SF_GENL_CONF_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    lan966x_psfp_sf_conf_read, &tmp);

	NLA_PUT_U32(msg, LAN966X_PSFP_SF_ATTR_SFI, sfi_id);

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

	memcpy(conf, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);

	return rc;
}

static int lan966x_psfp_sf_conf_set(uint32_t sfi_id,
				    struct lan966x_psfp_sf_conf *conf)
{
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(LAN966X_PSFP_NETLINK,
			     LAN966X_PSFP_SF_GENL_CONF_SET, 1, &sk, &msg);

	NLA_PUT(msg, LAN966X_PSFP_SF_ATTR_CONF, sizeof(*conf), conf);
	NLA_PUT_U32(msg, LAN966X_PSFP_SF_ATTR_SFI, sfi_id);

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

	return rc;
}

static int lan966x_psfp_sf_counters_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct lan966x_psfp_sf_counters *counters = arg;
	struct nlattr *attrs[LAN966X_PSFP_ATTR_END];

	if (nla_parse(attrs, LAN966X_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), lan966x_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[LAN966X_PSFP_SF_ATTR_STATUS]) {
		printf("ATTR_STATUS not found\n");
		return -1;
	}

	nla_memcpy(counters, attrs[LAN966X_PSFP_SF_ATTR_STATUS],
		   sizeof(struct lan966x_psfp_sf_counters));

	return NL_OK;
}

static void lan966x_psfp_sf_status_get(uint32_t sfi_id)
{
	struct lan966x_psfp_sf_counters counters;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc;

	memset(&counters, 0x0, sizeof(counters));

	rc = mchp_genl_start(LAN966X_PSFP_NETLINK,
			     LAN966X_PSFP_SF_GENL_STATUS_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    lan966x_psfp_sf_counters_read, &counters);

	NLA_PUT_U32(msg, LAN966X_PSFP_SF_ATTR_SFI, sfi_id);

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

	printf("matching_frames_count: %ld\n", counters.matching_frames_count);
	printf("passing_frames_count: %ld\n", counters.passing_frames_count);
	printf("not_passing_frames_count: %ld\n", counters.not_passing_frames_count);
	printf("passing_sdu_count: %ld\n", counters.passing_sdu_count);
	printf("not_passing_sdu_count: %ld\n", counters.not_passing_sdu_count);
	printf("red_frames_count: %ld\n", counters.red_frames_count);

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);
}

static char *lan966x_psfp_sf_help(void)
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
	struct lan966x_psfp_sf_conf config;
	struct lan966x_psfp_sf_conf tmp;
	uint32_t sfi_id = 0;
	int status = 0;
	int ch;

	/* read the id */
	sfi_id = atoi(argv[0]);

	if (lan966x_psfp_sf_conf_get(sfi_id, &config) < 0)
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
		lan966x_psfp_sf_status_get(sfi_id);
		return 0;
	}

	if (memcmp(&tmp, &config, sizeof(config)) == 0) {
		printf("enable: %d\n", config.enable);
		printf("max_sdu: %d\n", config.max_sdu);
		printf("block_oversize_enable: %d\n", config.block_oversize_enable);
		printf("block_oversize: %d\n", config.block_oversize);
		return 0;
	}

	lan966x_psfp_sf_conf_set(sfi_id, &config);

	return 0;
}

static int lan966x_psfp_sg_conf_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct lan966x_psfp_sg_conf *conf = arg;
	struct nlattr *attrs[LAN966X_PSFP_ATTR_END];

	if (nla_parse(attrs, LAN966X_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), lan966x_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[LAN966X_PSFP_SG_ATTR_CONF]) {
		printf("ATTR_CONF not found\n");
		return -1;
	}

	nla_memcpy(conf, attrs[LAN966X_PSFP_SG_ATTR_CONF],
		   sizeof(struct lan966x_psfp_sg_conf));

	return NL_OK;
}

static int lan966x_psfp_sg_conf_get(uint32_t sgi_id,
				    struct lan966x_psfp_sg_conf *conf)
{
	struct lan966x_psfp_sg_conf tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(LAN966X_PSFP_NETLINK,
			     LAN966X_PSFP_SG_GENL_CONF_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    lan966x_psfp_sg_conf_read, &tmp);

	NLA_PUT_U32(msg, LAN966X_PSFP_SG_ATTR_SGI, sgi_id);

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

	memcpy(conf, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);

	return rc;
}

static int lan966x_psfp_sg_conf_set(uint32_t sgi_id,
				    struct lan966x_psfp_sg_conf *conf)
{
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(LAN966X_PSFP_NETLINK,
			     LAN966X_PSFP_SG_GENL_CONF_SET, 1, &sk, &msg);

	NLA_PUT(msg, LAN966X_PSFP_SG_ATTR_CONF, sizeof(*conf), conf);
	NLA_PUT_U32(msg, LAN966X_PSFP_SG_ATTR_SGI, sgi_id);

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

	return rc;
}

static int lan966x_psfp_sg_status_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct lan966x_psfp_sg_status *status = arg;
	struct nlattr *attrs[LAN966X_PSFP_ATTR_END];

	if (nla_parse(attrs, LAN966X_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), lan966x_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[LAN966X_PSFP_SG_ATTR_STATUS]) {
		printf("ATTR_STATUS not found\n");
		return -1;
	}

	nla_memcpy(status, attrs[LAN966X_PSFP_SG_ATTR_STATUS],
		   sizeof(struct lan966x_psfp_sg_status));

	return NL_OK;
}

static void lan966x_psfp_sg_status_get(uint32_t sgi_id)
{
	struct lan966x_psfp_sg_status status;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc;

	memset(&status, 0x0, sizeof(status));

	rc = mchp_genl_start(LAN966X_PSFP_NETLINK,
			     LAN966X_PSFP_SG_GENL_STATUS_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    lan966x_psfp_sg_status_read, &status);

	NLA_PUT_U32(msg, LAN966X_PSFP_SG_ATTR_SGI, sgi_id);

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

	printf("gate_open: %d\n", status.gate_open);
	printf("ipv_enable: %d\n", status.ipv_enable);
	printf("ipv: %d\n", status.ipv);
	printf("config_change_time: %ld\n", status.config_change_time);
	printf("current_time: %ld\n", status.current_time);
	printf("config_pending: %d\n", status.config_pending);
	printf("base_time: %ld\n", status.oper.base_time);
	printf("cycle_time: %d\n", status.oper.cycle_time);
	printf("cycle_time_ext: %d\n", status.oper.cycle_time_ext);
	printf("gcl_length: %d\n", status.oper.gcl_length);

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);
}

static char *lan966x_psfp_sg_help(void)
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
	struct lan966x_psfp_sg_conf config;
	struct lan966x_psfp_sg_conf tmp;
	uint32_t sgi_id = 0;
	int status = 0;
	int ch;

	/* read the id */
	sgi_id = atoi(argv[0]);

	if (lan966x_psfp_sg_conf_get(sgi_id, &config) < 0)
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
			config.admin.base_time = atol(optarg);
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
		lan966x_psfp_sg_status_get(sgi_id);
		return 0;
	}

	if (memcmp(&tmp, &config, sizeof(config)) == 0) {
		printf("enable: %d\n", config.enable);
		printf("gate_open: %d\n", config.gate_open);
		printf("ipv_enable: %d\n", config.ipv_enable);
		printf("ipv: %d\n", config.ipv);
		printf("close_invalid_rx_enable: %d\n", config.close_invalid_rx_enable);
		printf("close_invalid_rx: %d\n", config.close_invalid_rx);
		printf("close_octets_exceeded_enable: %d\n", config.close_octets_exceeded_enable);
		printf("close_octets_exceeded: %d\n", config.close_octets_exceeded);
		printf("config_change: %d\n", config.config_change);
		printf("base_time: %ld\n", config.admin.base_time);
		printf("cycle_time: %d\n", config.admin.cycle_time);
		printf("cycle_time_ext: %d\n", config.admin.cycle_time_ext);
		printf("gcl_length: %d\n", config.admin.gcl_length);
		return 0;
	}

	lan966x_psfp_sg_conf_set(sgi_id, &config);

	return 0;
}

static int lan966x_psfp_gce_conf_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct lan966x_psfp_gce *conf = arg;
	struct nlattr *attrs[LAN966X_PSFP_ATTR_END];

	if (nla_parse(attrs, LAN966X_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), lan966x_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[LAN966X_PSFP_GCE_ATTR_CONF]) {
		printf("ATTR_CONF not found\n");
		return -1;
	}

	nla_memcpy(conf, attrs[LAN966X_PSFP_GCE_ATTR_CONF],
		   sizeof(struct lan966x_psfp_gce));

	return NL_OK;
}

static int lan966x_psfp_gce_conf_get(uint32_t sgi_id, uint32_t gce_id,
				     struct lan966x_psfp_gce *conf)
{
	struct lan966x_psfp_gce tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(LAN966X_PSFP_NETLINK,
			     LAN966X_PSFP_GCE_GENL_CONF_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    lan966x_psfp_gce_conf_read, &tmp);

	NLA_PUT_U32(msg, LAN966X_PSFP_GCE_ATTR_SGI, sgi_id);
	NLA_PUT_U32(msg, LAN966X_PSFP_GCE_ATTR_GCI, gce_id);

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

	memcpy(conf, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);

	return rc;
}

static int lan966x_psfp_gce_conf_set(uint32_t sgi_id, uint32_t gce_id,
				     struct lan966x_psfp_gce *conf)
{
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(LAN966X_PSFP_NETLINK,
			     LAN966X_PSFP_GCE_GENL_CONF_SET, 1, &sk, &msg);

	NLA_PUT(msg, LAN966X_PSFP_GCE_ATTR_CONF, sizeof(*conf), conf);
	NLA_PUT_U32(msg, LAN966X_PSFP_GCE_ATTR_SGI, sgi_id);
	NLA_PUT_U32(msg, LAN966X_PSFP_GCE_ATTR_GCI, gce_id);

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

	return rc;
}

static int lan966x_psfp_gce_status_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct lan966x_psfp_gce *status = arg;
	struct nlattr *attrs[LAN966X_PSFP_ATTR_END];

	if (nla_parse(attrs, LAN966X_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), lan966x_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[LAN966X_PSFP_GCE_ATTR_CONF]) {
		printf("ATTR_STATUS not found\n");
		return -1;
	}

	nla_memcpy(status, attrs[LAN966X_PSFP_GCE_ATTR_CONF],
		   sizeof(struct lan966x_psfp_gce));

	return NL_OK;
}

static void lan966x_psfp_gce_status_get(uint32_t sgi_id, uint32_t gce_id)
{
	struct lan966x_psfp_gce status;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc;

	memset(&status, 0x0, sizeof(status));

	rc = mchp_genl_start(LAN966X_PSFP_NETLINK,
			     LAN966X_PSFP_GCE_GENL_STATUS_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    lan966x_psfp_gce_status_read, &status);

	NLA_PUT_U32(msg, LAN966X_PSFP_GCE_ATTR_SGI, sgi_id);
	NLA_PUT_U32(msg, LAN966X_PSFP_GCE_ATTR_GCI, gce_id);

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

	printf("gate_open: %d\n", status.gate_open);
	printf("ipv_enable: %d\n", status.ipv_enable);
	printf("ipv: %d\n", status.ipv);
	printf("time_interval: %d\n", status.time_interval);
	printf("octet_max: %d\n", status.octet_max);

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);
}

static char *lan966x_psfp_gce_help(void)
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
	struct lan966x_psfp_gce config;
	struct lan966x_psfp_gce tmp;
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

	if (lan966x_psfp_gce_conf_get(sgi_id, gce_id, &config) < 0)
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
		lan966x_psfp_gce_status_get(sgi_id, gce_id);
		return 0;
	}

	if (memcmp(&tmp, &config, sizeof(config)) == 0) {
		printf("gate_open: %d\n", config.gate_open);
		printf("ipv_enable: %d\n", config.ipv_enable);
		printf("ipv: %d\n", config.ipv);
		printf("time_interval: %d\n", config.time_interval);
		printf("octet_max: %d\n", config.octet_max);
		return 0;
	}

	lan966x_psfp_gce_conf_set(sgi_id, gce_id, &config);

	return 0;
}

static int lan966x_psfp_fm_conf_read(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct lan966x_psfp_fm_conf *conf = arg;
	struct nlattr *attrs[LAN966X_PSFP_ATTR_END];

	if (nla_parse(attrs, LAN966X_PSFP_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), lan966x_psfp_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[LAN966X_PSFP_FM_ATTR_CONF]) {
		printf("ATTR_CONF not found\n");
		return -1;
	}

	nla_memcpy(conf, attrs[LAN966X_PSFP_FM_ATTR_CONF],
		   sizeof(struct lan966x_psfp_fm_conf));

	return NL_OK;
}

static int lan966x_psfp_fm_conf_get(uint32_t fmi_id,
				    struct lan966x_psfp_fm_conf *conf)
{
	struct lan966x_psfp_fm_conf tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(LAN966X_PSFP_NETLINK,
			     LAN966X_PSFP_FM_GENL_CONF_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    lan966x_psfp_fm_conf_read, &tmp);

	NLA_PUT_U32(msg, LAN966X_PSFP_FM_ATTR_FMI, fmi_id);

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

	memcpy(conf, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);

	return rc;
}

static int lan966x_psfp_fm_conf_set(uint32_t fmi_id,
				     struct lan966x_psfp_fm_conf *conf)
{
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(LAN966X_PSFP_NETLINK,
			     LAN966X_PSFP_FM_GENL_CONF_SET, 1, &sk, &msg);

	NLA_PUT(msg, LAN966X_PSFP_FM_ATTR_CONF, sizeof(*conf), conf);
	NLA_PUT_U32(msg, LAN966X_PSFP_FM_ATTR_FMI, fmi_id);

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

	return rc;
}

static char *lan966x_psfp_fm_help(void)
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
	struct lan966x_psfp_fm_conf config;
	struct lan966x_psfp_fm_conf tmp;
	uint32_t fmi_id = 0;
	int status = 0;
	int ch;

	/* read the id */
	fmi_id = atoi(argv[0]);

	if (lan966x_psfp_fm_conf_get(fmi_id, &config) < 0)
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
		printf("cir: %d\n", config.cir);
		printf("cbs: %d\n", config.cbs);
		printf("eir: %d\n", config.eir);
		printf("ebs: %d\n", config.ebs);
		printf("cf: %d\n", config.cf);
		printf("drop_on_yellow: %d\n", config.drop_on_yellow);
		printf("mark_red_enable: %d\n", config.mark_red_enable);
		printf("mark_red: %d\n", config.mark_red);
		return 0;
	}

	lan966x_psfp_fm_conf_set(fmi_id, &config);

	return 0;
}

static const struct command commands[] =
{
	/* Add/delete bridges */
	{1, "sf", cmd_sf, "sf sfi [options]", lan966x_psfp_sf_help},
	{1, "sg", cmd_sg, "sg sgi [options]", lan966x_psfp_sg_help},
	{2, "gce", cmd_gce, "gce sgi gce [options]", lan966x_psfp_gce_help},
	{1, "fm", cmd_fm, "fm fmi [options]", lan966x_psfp_fm_help},
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

