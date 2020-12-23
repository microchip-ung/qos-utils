/*
 * License: Dual MIT/GPL
 * Copyright (c) 2020 Microchip Corporation
 */

#include "common.h"
#include <getopt.h>
#include <errno.h>
#include <net/if.h>
#include "kernel_types.h"
#include "lan966x_ui_qos.h"

struct command
{
	int nargs;
	const char *name;
	int (*func)(const struct command *cmd, int argc, char *const *argv);
	const char *format;
	char *(*help)(void);
};

static void command_help(const struct command *cmd);

static struct nla_policy lan966x_qos_genl_policy[LAN966X_QOS_ATTR_END] = {
	[LAN966X_QOS_ATTR_NONE] = { .type = NLA_UNSPEC },
	[LAN966X_QOS_ATTR_DEV] = { .type = NLA_U32 },
	[LAN966X_QOS_ATTR_PORT_CFG] = { .type = NLA_BINARY },
	[LAN966X_QOS_ATTR_DSCP] = { .type = NLA_U32 },
	[LAN966X_QOS_ATTR_DSCP_PRIO_DPL] = { .type = NLA_BINARY },
};

static char *i_tag_map_help(void)
{
	return " --prio:   Ingress map of TAG PCP,DEI to (SKB)Priority.\n"
	       "            16 values. PCP/DEI(0) and PCP/DEI(1). 0123456701234567 (default)\n"
	       "  --dpl:    Ingress map of TAG PCP,DEI to (color)DPL.\n"
	       "            16 values. PCP/DEI(0) and PCP/DEI(1). 0000000011111111 (default)\n"
	       "  --help:   Show this help text\n";
}

static char *i_dscp_map_help(void)
{
	return " --enable: Ingress enable of map of DSCP value to (SKB)Priority.\n"
	       "  --prio:   Ingress map of DSCP value to (SKB)Priority.\n"
	       "  --dpl:    Ingress map of DSCP value to (color)DPL.\n"
	       "  --help:   Show this help text\n";
}

static char *i_def_help(void)
{
	return " --prio: Ingress default Priority (SKB). \n"
	       "  --pcp:  Ingress default untagged frames PCP\n"
	       "  --dei:  Ingress default untagged frames DEI\n"
	       "  --dpl:  Ingress default DPL\n"
	       "  --help: Show this help text\n";
}

static char *i_mode_help(void)
{
	return " --tag:   Ingress enable of TAG PCP,DEI mapping to (SKB)Priority and DPL\n"
	       "  --dscp:  Ingress enable of DSCP mapping to (SKB)Priority and DPL.\n"
	       "  --help:  Show this help text\n";
}

static char *e_tag_map_help(void)
{
	return " --pcp:  Egress map of (SKB)Priority,(color)DPL to TAG PCP.\n"
	       "          16 values. Prio/DPL(0) and Prio/DPL(1). 0123456701234567 (default)\n"
	       "  --dei:  Egress map of (SKB)Priority,(color)DPL to TAG DEI.\n"
	       "          16 values. Prio/DPL(0) and Prio/DPL(1). 0000000011111111 (default)\n"
	       "  --help: Show this help text\n";
}

static char *e_def_help(void)
{
	return " --pcp:  Egress default TAG PCP\n"
	       "  --dei:  Egress default TAG DEI\n"
	       "  --help: Show this help text\n";
}

static char *e_mode_help(void)
{
	return " --default:    Egress enable of default as TAG PCP,DEI.\n"
	       "  --classified: Egress enable of ingress classified PCP,DEI as TAG PCP,DEI.\n"
	       "  --mapped:     Egress enable of (SKB)Priority and DPL mapped as TAG PCP,DEI.\n"
	       "  --help:       Show this help text\n";
}

static int lan966x_qos_genl_port_cfg_set(u32 ifindex,
					 const struct lan966x_qos_port_conf *cfg)
{
	RETURN_IF_PC;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = lan966x_genl_start(LAN966X_QOS_NETLINK,
				LAN966X_QOS_GENL_PORT_CFG_SET, 1, &sk, &msg);

	NLA_PUT_U32(msg, LAN966X_QOS_ATTR_DEV, ifindex);
	NLA_PUT(msg, LAN966X_QOS_ATTR_PORT_CFG, sizeof(*cfg), cfg);

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

static int lan966x_qos_genl_port_cfg_get_cb(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *attrs[LAN966X_QOS_ATTR_END];
	struct lan966x_qos_port_conf *cfg = arg;

	if (nla_parse(attrs, LAN966X_QOS_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), lan966x_qos_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[LAN966X_QOS_ATTR_PORT_CFG]) {
		printf("ATTR_PORT_CFG not found\n");
		return -1;
	}

	nla_memcpy(cfg, attrs[LAN966X_QOS_ATTR_PORT_CFG],
		   sizeof(struct lan966x_qos_port_conf));

	return NL_OK;
}

static int lan966x_qos_genl_port_cfg_get(u32 ifindex,
					 struct lan966x_qos_port_conf *cfg)
{
	RETURN_IF_PC;
	struct lan966x_qos_port_conf tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = lan966x_genl_start(LAN966X_QOS_NETLINK,
				LAN966X_QOS_GENL_PORT_CFG_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    lan966x_qos_genl_port_cfg_get_cb, &tmp);

	NLA_PUT_U32(msg, LAN966X_QOS_ATTR_DEV, ifindex);

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

	memcpy(cfg, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);

	return rc;
}

static int lan966x_qos_genl_dscp_prio_dpl_set(u32 dscp,
					      const struct lan966x_qos_dscp_prio_dpl *cfg)
{
	RETURN_IF_PC;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = lan966x_genl_start(LAN966X_QOS_NETLINK,
				LAN966X_QOS_GENL_DSCP_PRIO_DPL_SET, 1, &sk, &msg);

	NLA_PUT_U32(msg, LAN966X_QOS_ATTR_DSCP, dscp);
	NLA_PUT(msg, LAN966X_QOS_ATTR_DSCP_PRIO_DPL, sizeof(*cfg), cfg);

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

static int lan966x_qos_genl_dscp_prio_dpl_get_cb(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *attrs[LAN966X_QOS_ATTR_END];
	struct lan966x_qos_dscp_prio_dpl *cfg = arg;

	if (nla_parse(attrs, LAN966X_QOS_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), lan966x_qos_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[LAN966X_QOS_ATTR_DSCP_PRIO_DPL]) {
		printf("ATTR_PORT_CFG not found\n");
		return -1;
	}

	nla_memcpy(cfg, attrs[LAN966X_QOS_ATTR_DSCP_PRIO_DPL],
		   sizeof(struct lan966x_qos_dscp_prio_dpl));

	return NL_OK;
}

static int lan966x_qos_genl_dscp_prio_dpl_get(u32 dscp,
					      struct lan966x_qos_dscp_prio_dpl *cfg)
{
	RETURN_IF_PC;
	struct lan966x_qos_dscp_prio_dpl tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = lan966x_genl_start(LAN966X_QOS_NETLINK,
				LAN966X_QOS_GENL_DSCP_PRIO_DPL_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    lan966x_qos_genl_dscp_prio_dpl_get_cb, &tmp);

	NLA_PUT_U32(msg, LAN966X_QOS_ATTR_DSCP, dscp);

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

	memcpy(cfg, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);

	return rc;
}

static int cmd_i_tag_map(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"prio", required_argument, NULL, 'a'},
		{"dpl", required_argument, NULL, 'b'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	struct lan966x_qos_port_conf cfg = {};
	struct lan966x_qos_port_conf tmp;
	int do_help = 0;
	u32 ifindex = 0;
	int ch, len, i;

	/* read device and skip it */
	ifindex = if_nametoindex(argv[0]);
	if (ifindex == 0) {
		fprintf(stderr, "%s: %s!\n", argv[0], strerror(errno));
		return 1;
	}

	if (lan966x_qos_genl_port_cfg_get(ifindex, &cfg) < 0)
		return 0;

	memcpy(&tmp, &cfg, sizeof(cfg));

	while ((ch = getopt_long(argc, argv, "a:b:c:h", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			len = strlen(optarg);
			if ((len != 0) && (len != 8) && (len != 16)) {
				fprintf(stderr, "Invalid argument length %u argument %s\n",
					len , optarg);
				return 1;
			}
			for (i = 0; i < PCP_COUNT; ++i) {
				cfg.i_pcp_dei_prio_dpl_map[i][0].prio = (u8)optarg[i] - 48;
			}
			if (len == 16) {
				for (i = 0; i < PCP_COUNT; ++i) {
					cfg.i_pcp_dei_prio_dpl_map[i][1].prio = optarg[i + 8] - 48;
				}
			}
			break;
		case 'b':
			len = strlen(optarg);
			if ((len != 0) && (len != 8) && (len != 16)) {
				fprintf(stderr, "Invalid argument length %u argument %s\n",
					len , optarg);
				return 1;
			}
			for (i = 0; i < PCP_COUNT; ++i) {
				cfg.i_pcp_dei_prio_dpl_map[i][0].dpl = (u8)optarg[i] - 48;
			}
			if (len == 16) {
				for (i = 0; i < PCP_COUNT; ++i) {
					cfg.i_pcp_dei_prio_dpl_map[i][1].dpl = optarg[i + 8] - 48;
				}
			}
			break;
		case 'h':
		case '?':
			do_help = 1;
			break;
		}
	}

	if (do_help) {
		command_help(cmd);
		return 0;
	}

	if (memcmp(&tmp, &cfg, sizeof(cfg)) == 0) {
		printf("i_tag_map --prio ");
		for (i = 0; i < PCP_COUNT; ++i) {
			printf("%d", cfg.i_pcp_dei_prio_dpl_map[i][0].prio);
		}
		for (i = 0; i < PCP_COUNT; ++i) {
			printf("%d", cfg.i_pcp_dei_prio_dpl_map[i][1].prio);
		}
		printf(" --dpl ");
		for (i = 0; i < PCP_COUNT; ++i) {
			printf("%d", cfg.i_pcp_dei_prio_dpl_map[i][0].dpl);
		}
		for (i = 0; i < PCP_COUNT; ++i) {
			printf("%d", cfg.i_pcp_dei_prio_dpl_map[i][1].dpl);
		}
		printf("\n");
		return 0;
	}

	return lan966x_qos_genl_port_cfg_set(ifindex, &cfg);
}

static int cmd_i_dscp_map(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"enable", required_argument, NULL, 'a'},
		{"prio", required_argument, NULL, 'b'},
		{"dpl", required_argument, NULL, 'c'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	struct lan966x_qos_dscp_prio_dpl cfg = {};
	struct lan966x_qos_dscp_prio_dpl tmp;
	int do_help = 0;
	u32 dscp = 0;
	int ch;

	/* read the DSCP value to map */
	dscp = atoi(argv[0]);

	if (lan966x_qos_genl_dscp_prio_dpl_get(dscp, &cfg) < 0)
		return 0;

	memcpy(&tmp, &cfg, sizeof(cfg));

	while ((ch = getopt_long(argc, argv, "a:b:c:h", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			cfg.trust = !!atoi(optarg);
			break;
		case 'b':
			cfg.prio = atoi(optarg);
			break;
		case 'c':
			cfg.dpl = atoi(optarg);
			break;
		case 'h':
		case '?':
			do_help = 1;
			break;
		}
	}

	if (do_help) {
		command_help(cmd);
		return 0;
	}

	if (memcmp(&tmp, &cfg, sizeof(cfg)) == 0) {
		printf("i_dscp_map --enable %u --prio %u --dpl %u\n", (cfg.trust) ? 1 : 0, cfg.prio, cfg.dpl);
		return 0;
	}

	return lan966x_qos_genl_dscp_prio_dpl_set(dscp, &cfg);
}

static int cmd_i_def(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"prio", required_argument, NULL, 'a'},
		{"pcp", required_argument, NULL, 'b'},
		{"dei", required_argument, NULL, 'c'},
		{"dpl", required_argument, NULL, 'd'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	struct lan966x_qos_port_conf cfg = {};
	struct lan966x_qos_port_conf tmp;
	int do_help = 0;
	u32 ifindex = 0;
	int ch;

	/* read device and skip it */
	ifindex = if_nametoindex(argv[0]);
	if (ifindex == 0) {
		fprintf(stderr, "%s: %s!\n", argv[0], strerror(errno));
		return 1;
	}

	if (lan966x_qos_genl_port_cfg_get(ifindex, &cfg) < 0)
		return 0;

	memcpy(&tmp, &cfg, sizeof(cfg));

	while ((ch = getopt_long(argc, argv, "a:b:c:d:h", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			cfg.i_default_prio = atoi(optarg);
			break;
		case 'b':
			cfg.i_default_pcp = atoi(optarg);
			break;
		case 'c':
			cfg.i_default_dei = atoi(optarg);
			break;
		case 'd':
			cfg.i_default_dpl = atoi(optarg);
			break;
		case 'h':
		case '?':
			do_help = 1;
			break;
		}
	}

	if (do_help) {
		command_help(cmd);
		return 0;
	}

	if (memcmp(&tmp, &cfg, sizeof(cfg)) == 0) {
		printf("i_def --prio %u --pcp %u --dei %u --dpl %u\n",
		       cfg.i_default_prio, cfg.i_default_pcp, cfg.i_default_dei, cfg.i_default_dpl);
		return 0;
	}

	return lan966x_qos_genl_port_cfg_set(ifindex, &cfg);
}

static int cmd_i_mode(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"tag", required_argument, NULL, 'a'},
		{"dscp", required_argument, NULL, 'b'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	struct lan966x_qos_port_conf cfg = {};
	struct lan966x_qos_port_conf tmp;
	int do_help = 0;
	u32 ifindex = 0;
	int ch;

	/* read device and skip it */
	ifindex = if_nametoindex(argv[0]);
	if (ifindex == 0) {
		fprintf(stderr, "%s: %s!\n", argv[0], strerror(errno));
		return 1;
	}

	if (lan966x_qos_genl_port_cfg_get(ifindex, &cfg) < 0)
		return 0;

	memcpy(&tmp, &cfg, sizeof(cfg));

	while ((ch = getopt_long(argc, argv, "a:b:h", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			cfg.i_mode.tag_map_enable = !!atoi(optarg);
			break;
		case 'b':
			cfg.i_mode.dscp_map_enable = !!atoi(optarg);
			break;
		case 'h':
		case '?':
			do_help = 1;
			break;
		}
	}

	if (do_help) {
		command_help(cmd);
		return 0;
	}

	if (memcmp(&tmp, &cfg, sizeof(cfg)) == 0) {
		printf("i_mode --tag %u --dscp %u\n",
		       cfg.i_mode.tag_map_enable, cfg.i_mode.dscp_map_enable);
		return 0;
	}

	return lan966x_qos_genl_port_cfg_set(ifindex, &cfg);
}

static int cmd_e_tag_map(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"pcp", required_argument, NULL, 'a'},
		{"dei", required_argument, NULL, 'b'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	struct lan966x_qos_port_conf cfg = {};
	struct lan966x_qos_port_conf tmp;
	int do_help = 0;
	u32 ifindex = 0;
	int ch, len, i;

	/* read device and skip it */
	ifindex = if_nametoindex(argv[0]);
	if (ifindex == 0) {
		fprintf(stderr, "%s: %s!\n", argv[0], strerror(errno));
		return 1;
	}

	if (lan966x_qos_genl_port_cfg_get(ifindex, &cfg) < 0)
		return 0;

	memcpy(&tmp, &cfg, sizeof(cfg));

	while ((ch = getopt_long(argc, argv, "a:b:c:h", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			len = strlen(optarg);
			if ((len != 0) && (len != 8) && (len != 16)) {
				fprintf(stderr, "Invalid argument length %u argument %s\n",
					len , optarg);
				return 1;
			}
			for (i = 0; i < PRIO_COUNT; ++i) {
				cfg.e_prio_dpl_pcp_dei_map[i][0].pcp = (u8)optarg[i] - 48;
			}
			if (len == 16) {
				for (i = 0; i < PRIO_COUNT; ++i) {
					cfg.e_prio_dpl_pcp_dei_map[i][1].pcp = optarg[i + 8] - 48;
				}
			}
			break;
		case 'b':
			len = strlen(optarg);
			if ((len != 0) && (len != 8) && (len != 16)) {
				fprintf(stderr, "Invalid argument length %u argument %s\n",
					len , optarg);
				return 1;
			}
			for (i = 0; i < PRIO_COUNT; ++i) {
				cfg.e_prio_dpl_pcp_dei_map[i][0].dei = (u8)optarg[i] - 48;
			}
			if (len == 16) {
				for (i = 0; i < PRIO_COUNT; ++i) {
					cfg.e_prio_dpl_pcp_dei_map[i][1].dei = optarg[i + 8] - 48;
				}
			}
			break;
		case 'h':
		case '?':
			do_help = 1;
			break;
		}
	}

	if (do_help) {
		command_help(cmd);
		return 0;
	}

	if (memcmp(&tmp, &cfg, sizeof(cfg)) == 0) {
		printf("e_tag_map --pcp ");
		for (i = 0; i < PCP_COUNT; ++i) {
			printf("%d", cfg.e_prio_dpl_pcp_dei_map[i][0].pcp);
		}
		for (i = 0; i < PCP_COUNT; ++i) {
			printf("%d", cfg.e_prio_dpl_pcp_dei_map[i][1].pcp);
		}
		printf(" --dei ");
		for (i = 0; i < PCP_COUNT; ++i) {
			printf("%d", cfg.e_prio_dpl_pcp_dei_map[i][0].dei);
		}
		for (i = 0; i < PCP_COUNT; ++i) {
			printf("%d", cfg.e_prio_dpl_pcp_dei_map[i][1].dei);
		}
		printf("\n");
		return 0;
	}

	return lan966x_qos_genl_port_cfg_set(ifindex, &cfg);
}

static int cmd_e_def(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"pcp", required_argument, NULL, 'a'},
		{"dei", required_argument, NULL, 'b'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	struct lan966x_qos_port_conf cfg = {};
	struct lan966x_qos_port_conf tmp;
	int do_help = 0;
	u32 ifindex = 0;
	int ch;

	/* read device and skip it */
	ifindex = if_nametoindex(argv[0]);
	if (ifindex == 0) {
		fprintf(stderr, "%s: %s!\n", argv[0], strerror(errno));
		return 1;
	}

	if (lan966x_qos_genl_port_cfg_get(ifindex, &cfg) < 0)
		return 0;

	memcpy(&tmp, &cfg, sizeof(cfg));

	while ((ch = getopt_long(argc, argv, "a:b:c:d:h", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			cfg.e_default_pcp = atoi(optarg);
			break;
		case 'b':
			cfg.e_default_dei = atoi(optarg);
			break;
		case 'h':
		case '?':
			do_help = 1;
			break;
		}
	}

	if (do_help) {
		command_help(cmd);
		return 0;
	}

	if (memcmp(&tmp, &cfg, sizeof(cfg)) == 0) {
		printf("e_def --pcp %u --dei %u\n",
		       cfg.e_default_pcp, cfg.e_default_dei);
		return 0;
	}

	return lan966x_qos_genl_port_cfg_set(ifindex, &cfg);
}

static int cmd_e_mode(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"default", required_argument, NULL, 'a'},
		{"classified", required_argument, NULL, 'b'},
		{"mapped", required_argument, NULL, 'c'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	struct lan966x_qos_port_conf cfg = {};
	struct lan966x_qos_port_conf tmp;
	int do_help = 0;
	u32 ifindex = 0;
	int ch;

	/* read device and skip it */
	ifindex = if_nametoindex(argv[0]);
	if (ifindex == 0) {
		fprintf(stderr, "%s: %s!\n", argv[0], strerror(errno));
		return 1;
	}

	if (lan966x_qos_genl_port_cfg_get(ifindex, &cfg) < 0)
		return 0;

	memcpy(&tmp, &cfg, sizeof(cfg));

	while ((ch = getopt_long(argc, argv, "a:b:h", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			cfg.e_mode = LAN966X_E_MODE_DEFAULT;
			break;
		case 'b':
			cfg.e_mode = LAN966X_E_MODE_CLASSIFIED;
			break;
		case 'c':
			cfg.e_mode = LAN966X_E_MODE_MAPPED;
			break;
		case 'h':
		case '?':
			do_help = 1;
			break;
		}
	}

	if (do_help) {
		command_help(cmd);
		return 0;
	}

	if (memcmp(&tmp, &cfg, sizeof(cfg)) == 0) {
		printf("e_mode --default %u --classified %u --mapped %u\n",
		       (cfg.e_mode == LAN966X_E_MODE_DEFAULT) ? 1 : 0,
		       (cfg.e_mode == LAN966X_E_MODE_CLASSIFIED) ? 1 : 0,
		       (cfg.e_mode == LAN966X_E_MODE_MAPPED) ? 1 : 0);
		return 0;
	}

	return lan966x_qos_genl_port_cfg_set(ifindex, &cfg);
}

/* commands */
static const struct command commands[] =
{
	{1, "i_tag_map", cmd_i_tag_map, "i_tag_map dev [options]", i_tag_map_help},
	{1, "i_dscp_map", cmd_i_dscp_map, "i_dscp_map dscp [options]", i_dscp_map_help},
	{1, "i_def", cmd_i_def, "i_def dev [options]", i_def_help},
	{1, "i_mode", cmd_i_mode, "i_mode dev [options]", i_mode_help},
	{1, "e_tag_map", cmd_e_tag_map, "e_tag_map dev [options]", e_tag_map_help},
	{1, "e_def", cmd_e_def, "e_def dev [options]", e_def_help},
	{1, "e_mode", cmd_e_mode, "e_mode dev [options]", e_mode_help},
};

static void command_help(const struct command *cmd)
{
	printf("%s:   %-16s\n %s\n", cmd->name, cmd->format, cmd->help());
}

static void command_help_all(void)
{
	for (int i = 0; i < COUNT_OF(commands); ++i)
		command_help(&commands[i]);
}

static void help(void)
{
	printf("Usage: qos i_tag_map|i_dscp_map|i_def|i_mode|e_tag_map|e_def|e_mode [options]\n");
	printf("options:\n");
	printf(" --help                    Show this help text\n");
	printf("commands:\n");
	command_help_all();
}

static const struct command *command_lookup(const char *cmd)
{
	for (int i = 0; i < COUNT_OF(commands); ++i)
		if (!strcmp(cmd, commands[i].name))
			return &commands[i];

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

	/* skip program name ('qos') */
	argv++;
	argc--;

	if (!argc || (strcmp(argv[0], "--help") == 0)) {
		help();
		return 1;
	}

	cmd = command_lookup_and_validate(argc, argv, 0);
	if (!cmd)
		return 1;

	/* skip command (e.g. 'i_def') */
	argv++;
	argc--;

	if (argc < cmd->nargs) {
		fprintf(stderr, "Missing argument!\n");
		command_help(cmd);
		return 1;
	}

	return cmd->func(cmd, argc, argv);
}
