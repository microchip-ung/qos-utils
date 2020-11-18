/*
 * License: Dual MIT/GPL
 * Copyright (c) 2020 Microchip Corporation
 */

#include "qos.h"
#include "common.h"
#include <getopt.h>
#include <errno.h>
#include <net/if.h>

struct command
{
	int nargs;
	const char *name;
	int (*func)(const struct command *cmd, int argc, char *const *argv);
	const char *format;
	char *(*help)(void);
};

static void command_help(const struct command *cmd);

enum lan966x_qos_attr {
	LAN966X_QOS_ATTR_NONE,
	LAN966X_QOS_ATTR_DEV,
	LAN966X_QOS_ATTR_PORT_CFG,

	/* This must be the last entry */
	LAN966X_QOS_ATTR_END,
};

#define LAN966X_QOS_ATTR_MAX (LAN966X_QOS_ATTR_END - 1)

static struct nla_policy lan966x_qos_genl_policy[LAN966X_QOS_ATTR_END] = {
	[LAN966X_QOS_ATTR_NONE] = { .type = NLA_UNSPEC },
	[LAN966X_QOS_ATTR_DEV] = { .type = NLA_U32 },
	[LAN966X_QOS_ATTR_PORT_CFG] = { .type = NLA_BINARY },
};

enum lan966x_qos_genl {
	LAN966X_QOS_GENL_PORT_CFG_SET,
	LAN966X_QOS_GENL_PORT_CFG_GET,
};

static char *i_tag_map_help(void)
{
	return " --prio:   Ingress map of TAG PCP,DEI to (SKB)Priority.\n"
	       "           16 values. PCP/DEI(0) and PCP/DEI(1). 0123456701234567 (default)\n"
	       " --dpl:    Ingress map of TAG PCP,DEI to (color)DPL.\n"
	       "           16 values. PCP/DEI(0) and PCP/DEI(1). 0000000011111111 (default)\n"
	       " --help:   Show this help text\n";
}

static char *i_dhcp_map_help(void)
{
	return " --enable: Ingress enable of map of DHCP value to (SKB)Priority.\n"
	       " --prio:   Ingress map of DHCP value to (SKB)Priority.\n"
	       " --dpl:    Ingress map of DHCP value to (color)DPL.\n"
	       " --help:   Show this help text\n";
}

static char *i_def_help(void)
{
	return " --prio: Ingress default Priority (SKB). \n"
	       " --pcp:  Ingress default untagged frames PCP\n"
	       " --dei:  Ingress default untagged frames DEI\n"
	       " --dpl:  Ingress default DPL\n"
	       " --help: Show this help text\n";
}

static char *i_mode_help(void)
{
	return " --tag:   Ingress enable of TAG PCP,DEI mapping to (SKB)Priority and DPL\n"
	       " --dhcp:  Ingress enable of DHCP mapping to (SKB)Priority and DPL.\n"
	       " --help:  Show this help text\n";
}

static char *e_tag_map_help(void)
{
	return " --pcp:  Egress map of (SKB)Priority,(color)DPL to TAG PCP.\n"
	       "         16 values. Prio/DPL(0) and Prio/DPL(1). 0123456701234567 (default)\n"
	       " --dei:  Egress map of (SKB)Priority,(color)DPL to TAG DEI.\n"
	       "         16 values. Prio/DPL(0) and Prio/DPL(1). 0000000011111111 (default)\n"
	       " --help: Show this help text\n";
}

static char *e_def_help(void)
{
	return " --pcp:  Egress default TAG PCP\n"
	       " --dei:  Egress default TAG DEI\n"
	       " --help: Show this help text\n";
}

static char *e_mode_help(void)
{
	return " --default:    Egress enable of default as TAG PCP,DEI.\n"
	       " --classified: Egress enable of (SKB)Priority and DPL as TAG PCP,DEI.\n"
	       " --mapped:     Egress enable of (SKB)Priority and DPL mapped as TAG PCP,DEI.\n"
	       " --help:       Show this help text\n";
}

/* cmd_vlan */
static int lan966x_qos_genl_port_cfg_set(u32 ifindex,
					 const struct lan966x_qos_port_cfg *cfg)
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
	struct lan966x_qos_vlan_cfg *cfg = arg;

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
		   sizeof(struct lan966x_qos_port_cfg));

	return NL_OK;
}

static int lan966x_qos_genl_port_cfg_get(u32 ifindex,
					 struct lan966x_qos_port_cfg *cfg)
{
	RETURN_IF_PC;
	struct lan966x_qos_port_cfg tmp;
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

static int cmd_i_tag_map(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"enable", no_argument, NULL, 'a'},
		{"prio", required_argument, NULL, 'b'},
		{"dpl", required_argument, NULL, 'c'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	struct lan966x_qos_port_cfg cfg = {};
	struct lan966x_qos_port_cfg tmp;
	int do_help = 0;
	u32 vid = 0;
	int ch;

	/* read the id */
	vid = atoi(argv[0]);

	if (lan966x_qos_genl_port_cfg_get(vid, &cfg) < 0)
		return 0;

	memcpy(&tmp, &cfg, sizeof(cfg));

	while ((ch = getopt_long(argc, argv, "a:b:c:h", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			break;
		case 'b':
			break;
		case 'c':
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
//		printf("%-14s %8d\n", "flood_disable:", cfg.flood_disable);
//		printf("%-14s %8d\n", "learn_disable:", cfg.learn_disable);
		return 0;
	}

	return lan966x_qos_genl_port_cfg_set(vid, &cfg);
}

static int cmd_i_dhcp_map(const struct command *cmd, int argc, char *const *argv)
{
	return 0;
}

static int cmd_i_def(const struct command *cmd, int argc, char *const *argv)
{
	return 0;
}

static int cmd_i_mode(const struct command *cmd, int argc, char *const *argv)
{
	return 0;
}

static int cmd_e_tag_map(const struct command *cmd, int argc, char *const *argv)
{
	return 0;
}

static int cmd_e_def(const struct command *cmd, int argc, char *const *argv)
{
	return 0;
}

static int cmd_e_mode(const struct command *cmd, int argc, char *const *argv)
{
	return 0;
}

/* commands */
static const struct command commands[] =
{
	{1, "i_tag_map", cmd_i_tag_map, "i_tag_map dev [options]", i_tag_map_help},
	{1, "i_dhcp_map", cmd_i_dhcp_map, "i_dhcp_map dhcp [options]", i_dhcp_map_help},
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
	printf("Usage: qos i_map|e_map|i_def|e_def|i_mode|e_mode [options]\n");
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

	/* skip command (e.g. 'cs') */
	argv++;
	argc--;

	if (argc < cmd->nargs) {
		fprintf(stderr, "Missing argument!\n");
		command_help(cmd);
		return 1;
	}

	return cmd->func(cmd, argc, argv);
}
