/*
 * License: Dual MIT/GPL
 * Copyright (c) 2020 Microchip Corporation
 */

#include "common.h"
#include <getopt.h>
#include <errno.h>
#include <net/if.h>
#include "kernel_types.h"
#include "mchp_ui_qos.h"

struct command
{
	int nargs;
	const char *name;
	int (*func)(const struct command *cmd, int argc, char *const *argv);
	const char *format;
	char *(*help)(void);
};

static void command_help(const struct command *cmd);

static struct nla_policy mchp_frer_genl_policy[MCHP_FRER_ATTR_END] = {
	[MCHP_FRER_ATTR_NONE] = { .type = NLA_UNSPEC },
	[MCHP_FRER_ATTR_ID] = { .type = NLA_U32 },
	[MCHP_FRER_ATTR_DEV1] = { .type = NLA_U32 },
	[MCHP_FRER_ATTR_DEV2] = { .type = NLA_U32 },
	[MCHP_FRER_ATTR_STREAM_CFG] = { .type = NLA_BINARY },
	[MCHP_FRER_ATTR_STREAM_CNT] = { .type = NLA_BINARY },
	[MCHP_FRER_ATTR_IFLOW_CFG] = { .type = NLA_BINARY },
	[MCHP_FRER_ATTR_VLAN_CFG] = { .type = NLA_BINARY },
};

/* cmd_cs */
static int mchp_frer_genl_cs_cfg_set(u32 cs_id,
					const struct mchp_frer_stream_cfg *cfg)
{
	RETURN_IF_PC;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
				MCHP_FRER_GENL_CS_CFG_SET, 1, &sk, &msg);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_ID, cs_id);
	NLA_PUT(msg, MCHP_FRER_ATTR_STREAM_CFG, sizeof(*cfg), cfg);

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

static int mchp_frer_genl_cs_cfg_get_cb(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *attrs[MCHP_FRER_ATTR_END];
	struct mchp_frer_stream_cfg *cfg = arg;

	if (nla_parse(attrs, MCHP_FRER_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_frer_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_FRER_ATTR_STREAM_CFG]) {
		printf("ATTR_STREAM_CFG not found\n");
		return -1;
	}

	nla_memcpy(cfg, attrs[MCHP_FRER_ATTR_STREAM_CFG],
		   sizeof(struct mchp_frer_stream_cfg));

	return NL_OK;
}

static int mchp_frer_genl_cs_cfg_get(u32 cs_id,
					struct mchp_frer_stream_cfg *cfg)
{
	RETURN_IF_PC;
	struct mchp_frer_stream_cfg tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
				MCHP_FRER_GENL_CS_CFG_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_frer_genl_cs_cfg_get_cb, &tmp);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_ID, cs_id);

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

static int mchp_frer_genl_cs_cnt_get_cb(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *attrs[MCHP_FRER_ATTR_END];
	struct mchp_frer_cnt *cnt = arg;

	if (nla_parse(attrs, MCHP_FRER_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_frer_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_FRER_ATTR_STREAM_CNT]) {
		printf("ATTR_STREAM_CNT not found\n");
		return -1;
	}

	nla_memcpy(cnt, attrs[MCHP_FRER_ATTR_STREAM_CNT],
		   sizeof(struct mchp_frer_cnt));

	return NL_OK;
}

static int mchp_frer_genl_cs_cnt_get(u32 cs_id, struct mchp_frer_cnt *cnt)
{
	RETURN_IF_PC;
	struct mchp_frer_cnt tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
				MCHP_FRER_GENL_CS_CNT_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_frer_genl_cs_cnt_get_cb, &tmp);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_ID, cs_id);

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

	memcpy(cnt, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);

	return rc;
}

static int mchp_frer_genl_cs_cnt_clr(u32 cs_id)
{
	RETURN_IF_PC;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
				MCHP_FRER_GENL_CS_CNT_CLR, 1, &sk, &msg);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_ID, cs_id);

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

static char *mchp_frer_cs_help(void)
{
	return "--enable:                 Enable recovery\n"
		" --alg:                    frerSeqRcvyAlgorithm (0: Vector, 1: Match)\n"
		" --hlen:                   frerSeqRcvyHistoryLength\n"
		" --reset_time:             frerSeqRcvyResetMSec\n"
		" --take_no_seq:            frerSeqRcvyTakeNoSequence\n"
		" --cnt:                    Show counters\n"
		" --clr:                    Clear counters\n"
		" --help:                   Show this help text\n";
}

static int cmd_cs(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"enable", required_argument, NULL, 'a'},
		{"alg", required_argument, NULL, 'b'},
		{"hlen", required_argument, NULL, 'c'},
		{"reset_time", required_argument, NULL, 'd'},
		{"take_no_seq", required_argument, NULL, 'e'},
		{"cnt", no_argument, NULL, 'f'},
		{"clr", no_argument, NULL, 'g'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	struct mchp_frer_stream_cfg cfg = {};
	struct mchp_frer_stream_cfg tmp;
	struct mchp_frer_cnt cnt = {};
	int do_help = 0;
	int do_cnt = 0;
	int do_clr = 0;
	u32 cs_id = 0;
	int ch, rc;

	/* read the id */
	cs_id = atoi(argv[0]);

	if (mchp_frer_genl_cs_cfg_get(cs_id, &cfg) < 0)
		return 0;

	memcpy(&tmp, &cfg, sizeof(cfg));

	while ((ch = getopt_long(argc, argv, "a:b:c:d:e:fgh", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			cfg.enable = !!atoi(optarg);
			break;
		case 'b':
			cfg.alg = !!atoi(optarg);
			break;
		case 'c':
			cfg.hlen = atoi(optarg);
			break;
		case 'd':
			cfg.reset_time = atoi(optarg);
			break;
		case 'e':
			cfg.take_no_seq = !!atoi(optarg);
			break;
		case 'f':
			do_cnt = 1;
			break;
		case 'g':
			do_clr = 1;
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
			
	if (do_cnt) {
		rc = mchp_frer_genl_cs_cnt_get(cs_id, &cnt);
		if (rc == 0) {
			printf("%-18s: %16" PRIu64 "\n", "OutOfOrderPackets", cnt.out_of_order_packets);
			printf("%-18s: %16" PRIu64 "\n", "RoguePackets", cnt.rogue_packets);
			printf("%-18s: %16" PRIu64 "\n", "PassedPackets", cnt.passed_packets);
			printf("%-18s: %16" PRIu64 "\n", "DiscardedPackets", cnt.discarded_packets);
			printf("%-18s: %16" PRIu64 "\n", "LostPackets", cnt.lost_packets);
			printf("%-18s: %16" PRIu64 "\n", "TaglessPackets", cnt.tagless_packets);
			printf("%-18s: %16" PRIu64 "\n", "Resets", cnt.resets);
		}
		return rc;
	}

	if (do_clr)
		return mchp_frer_genl_cs_cnt_clr(cs_id);

	if (memcmp(&tmp, &cfg, sizeof(cfg)) == 0) {
		printf("%-14s %8d\n", "enable:", cfg.enable);
		printf("%-14s %8d\n", "alg:", cfg.alg);
		printf("%-14s %8u\n", "hlen:", cfg.hlen);
		printf("%-14s %8u\n", "reset_time:", cfg.reset_time);
		printf("%-14s %8d\n", "take_no_seq:", cfg.take_no_seq);
		return 0;
	}

	return mchp_frer_genl_cs_cfg_set(cs_id, &cfg);
}

/* cmd_msa */
static int mchp_frer_genl_ms_alloc_cb(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *attrs[MCHP_FRER_ATTR_END];
	u32 *id = arg;

	if (nla_parse(attrs, MCHP_FRER_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_frer_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_FRER_ATTR_ID]) {
		printf("ATTR_ID not found\n");
		return -1;
	}

	*id = nla_get_u32(attrs[MCHP_FRER_ATTR_ID]);

	return NL_OK;
}

static int mchp_frer_genl_ms_alloc(u32 ifindex1, u32 ifindex2, u32 *ms_id)
{
	RETURN_IF_PC;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;
	u32 tmp;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
			     MCHP_FRER_GENL_MS_ALLOC, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_frer_genl_ms_alloc_cb, &tmp);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_DEV1, ifindex1);
	NLA_PUT_U32(msg, MCHP_FRER_ATTR_DEV2, ifindex2);

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

	*ms_id = tmp;

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);

	return rc;
}

static char *mchp_frer_msa_help(void)
{
	return "--help:                   Show this help text\n";
}

static int cmd_msa(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	u32 ifindex1 = 0;
	u32 ifindex2 = 0;
	int do_help = 0;
	u32 ms_id = 0;
	int ch, rc;

	/* read device 1 and skip it */
	ifindex1 = if_nametoindex(argv[0]);
	if (ifindex1 == 0) {
		fprintf(stderr, "%s: %s!\n", argv[0], strerror(errno));
		return 1;
	}
	argc--;
	argv++;

	if (argc) {
		/* read optional device 2*/
		ifindex2 = if_nametoindex(argv[0]);
		if (ifindex2 == 0) {
			fprintf(stderr, "%s: %s!\n", argv[0], strerror(errno));
			return 1;
		}
	}

	while ((ch = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
		switch (ch) {
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
			
	rc = mchp_frer_genl_ms_alloc(ifindex1, ifindex2, &ms_id);
	if (rc == 0) {
		printf("%u\n", ms_id);
	}
	return rc;
}

/* cmd_msf */
static int mchp_frer_genl_ms_free(u32 ms_id)
{
	RETURN_IF_PC;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
				MCHP_FRER_GENL_MS_FREE, 1, &sk, &msg);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_ID, ms_id);

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

static char *mchp_frer_msf_help(void)
{
	return "--help:                   Show this help text\n";
}

static int cmd_msf(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	int do_help = 0;
	u32 ms_id = 0;
	int ch;

	/* read the id */
	ms_id = atoi(argv[0]);

	while ((ch = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
		switch (ch) {
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
			
	return mchp_frer_genl_ms_free(ms_id);
}

/* cmd_ms */
static int mchp_frer_genl_ms_cfg_set(u32 ifindex, u32 ms_id,
					const struct mchp_frer_stream_cfg *cfg)
{
	RETURN_IF_PC;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
				MCHP_FRER_GENL_MS_CFG_SET, 1, &sk, &msg);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_ID, ms_id);
	NLA_PUT_U32(msg, MCHP_FRER_ATTR_DEV1, ifindex);
	NLA_PUT(msg, MCHP_FRER_ATTR_STREAM_CFG, sizeof(*cfg), cfg);

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

static int mchp_frer_genl_ms_cfg_get_cb(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *attrs[MCHP_FRER_ATTR_END];
	struct mchp_frer_stream_cfg *cfg = arg;

	if (nla_parse(attrs, MCHP_FRER_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_frer_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_FRER_ATTR_STREAM_CFG]) {
		printf("ATTR_STREAM_CFG not found\n");
		return -1;
	}

	nla_memcpy(cfg, attrs[MCHP_FRER_ATTR_STREAM_CFG],
		   sizeof(struct mchp_frer_stream_cfg));

	return NL_OK;
}

static int mchp_frer_genl_ms_cfg_get(u32 ifindex, u32 ms_id,
					struct mchp_frer_stream_cfg *cfg)
{
	RETURN_IF_PC;
	struct mchp_frer_stream_cfg tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
				MCHP_FRER_GENL_MS_CFG_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_frer_genl_ms_cfg_get_cb, &tmp);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_ID, ms_id);
	NLA_PUT_U32(msg, MCHP_FRER_ATTR_DEV1, ifindex);

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

static int mchp_frer_genl_ms_cnt_get_cb(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *attrs[MCHP_FRER_ATTR_END];
	struct mchp_frer_cnt *cnt = arg;

	if (nla_parse(attrs, MCHP_FRER_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_frer_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_FRER_ATTR_STREAM_CNT]) {
		printf("ATTR_STREAM_CNT not found\n");
		return -1;
	}

	nla_memcpy(cnt, attrs[MCHP_FRER_ATTR_STREAM_CNT],
		   sizeof(struct mchp_frer_cnt));

	return NL_OK;
}

static int mchp_frer_genl_ms_cnt_get(u32 ifindex, u32 ms_id, struct mchp_frer_cnt *cnt)
{
	RETURN_IF_PC;
	struct mchp_frer_cnt tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
				MCHP_FRER_GENL_MS_CNT_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_frer_genl_ms_cnt_get_cb, &tmp);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_ID, ms_id);
	NLA_PUT_U32(msg, MCHP_FRER_ATTR_DEV1, ifindex);

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

	memcpy(cnt, &tmp, sizeof(tmp));

nla_put_failure:
	nlmsg_free(msg);
	nl_socket_free(sk);

	return rc;
}

static int mchp_frer_genl_ms_cnt_clr(u32 ifindex, u32 ms_id)
{
	RETURN_IF_PC;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
				MCHP_FRER_GENL_MS_CNT_CLR, 1, &sk, &msg);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_ID, ms_id);
	NLA_PUT_U32(msg, MCHP_FRER_ATTR_DEV1, ifindex);

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

static char *mchp_frer_ms_help(void)
{
	return "--enable:                 Enable recovery\n"
		" --alg:                    frerSeqRcvyAlgorithm (0: Vector, 1: Match)\n"
		" --hlen:                   frerSeqRcvyHistoryLength\n"
		" --reset_time:             frerSeqRcvyResetMSec\n"
		" --take_no_seq:            frerSeqRcvyTakeNoSequence\n"
		" --cs_id:                  Compound stream ID\n"
		" --cnt:                    Show counters\n"
		" --clr:                    Clear counters\n"
		" --help:                   Show this help text\n";
}

static int cmd_ms(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"enable", required_argument, NULL, 'a'},
		{"alg", required_argument, NULL, 'b'},
		{"hlen", required_argument, NULL, 'c'},
		{"reset_time", required_argument, NULL, 'd'},
		{"take_no_seq", required_argument, NULL, 'e'},
		{"cs_id", required_argument, NULL, 'f'},
		{"cnt", no_argument, NULL, 'g'},
		{"help", no_argument, NULL, 'h'},
		{"clr", no_argument, NULL, 'i'},
		{NULL, 0, NULL, 0}
	};
	struct mchp_frer_cnt cnt = {};
	struct mchp_frer_stream_cfg cfg = {};
	struct mchp_frer_stream_cfg tmp;
	u32 ifindex = 0;
	int do_help = 0;
	int do_cnt = 0;
	int do_clr = 0;
	u32 ms_id = 0;
	int ch, rc;

	/* read the device and skip it */
	ifindex = if_nametoindex(argv[0]);
	if (ifindex == 0) {
		fprintf(stderr, "%s: %s!\n", argv[0], strerror(errno));
		return 1;
	}
	argc--;
	argv++;

	/* read the id */
	ms_id = atoi(argv[0]);


	if (mchp_frer_genl_ms_cfg_get(ifindex, ms_id, &cfg) < 0)
		return 0;

	memcpy(&tmp, &cfg, sizeof(cfg));

	while ((ch = getopt_long(argc, argv, "a:b:c:d:e:f:ghi", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			cfg.enable = !!atoi(optarg);
			break;
		case 'b':
			cfg.alg = !!atoi(optarg);
			break;
		case 'c':
			cfg.hlen = atoi(optarg);
			break;
		case 'd':
			cfg.reset_time = atoi(optarg);
			break;
		case 'e':
			cfg.take_no_seq = !!atoi(optarg);
			break;
		case 'f':
			cfg.cs_id = atoi(optarg);
			break;
		case 'g':
			do_cnt = 1;
			break;
		case 'i':
			do_clr = 1;
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
			
	if (do_cnt) {
		rc = mchp_frer_genl_ms_cnt_get(ifindex, ms_id, &cnt);
		if (rc == 0) {
			printf("%-18s: %16" PRIu64 "\n", "OutOfOrderPackets", cnt.out_of_order_packets);
			printf("%-18s: %16" PRIu64 "\n", "RoguePackets", cnt.rogue_packets);
			printf("%-18s: %16" PRIu64 "\n", "PassedPackets", cnt.passed_packets);
			printf("%-18s: %16" PRIu64 "\n", "DiscardedPackets", cnt.discarded_packets);
			printf("%-18s: %16" PRIu64 "\n", "LostPackets", cnt.lost_packets);
			printf("%-18s: %16" PRIu64 "\n", "TaglessPackets", cnt.tagless_packets);
			printf("%-18s: %16" PRIu64 "\n", "Resets", cnt.resets);
		}
		return rc;
	}

	if (do_clr)
		return mchp_frer_genl_ms_cnt_clr(ifindex, ms_id);

	if (memcmp(&tmp, &cfg, sizeof(cfg)) == 0) {
		printf("%-14s %8d\n", "enable:", cfg.enable);
		printf("%-14s %8d\n", "alg:", cfg.alg);
		printf("%-14s %8u\n", "hlen:", cfg.hlen);
		printf("%-14s %8u\n", "reset_time:", cfg.reset_time);
		printf("%-14s %8d\n", "take_no_seq:", cfg.take_no_seq);
		printf("%-14s %8d\n", "cs_id:", cfg.cs_id);
		return 0;
	}

	return mchp_frer_genl_ms_cfg_set(ifindex, ms_id, &cfg);
}

/* cmd_iflow */
/* Transfer split_mask out of band via DEV1 and DEV2 */
struct mchp_iflow_cmb_cfg {
	struct mchp_iflow_cfg iflow;
	u32 ifindex1;
	u32 ifindex2;
};

static int mchp_frer_genl_iflow_cfg_set(u32 id,
					   const struct mchp_iflow_cmb_cfg *cfg)
{
	RETURN_IF_PC;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
				MCHP_FRER_GENL_IFLOW_CFG_SET, 1, &sk, &msg);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_ID, id);
	NLA_PUT_U32(msg, MCHP_FRER_ATTR_DEV1, cfg->ifindex1);
	NLA_PUT_U32(msg, MCHP_FRER_ATTR_DEV2, cfg->ifindex2);
	NLA_PUT(msg, MCHP_FRER_ATTR_IFLOW_CFG, sizeof(cfg->iflow), &cfg->iflow);

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

static int mchp_frer_genl_iflow_cfg_get_cb(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *attrs[MCHP_FRER_ATTR_END];
	struct mchp_iflow_cmb_cfg *cfg = arg;

	if (nla_parse(attrs, MCHP_FRER_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_frer_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_FRER_ATTR_IFLOW_CFG]) {
		printf("ATTR_IFLOW_CFG not found\n");
		return -1;
	}

	if (!attrs[MCHP_FRER_ATTR_DEV1]) {
		printf("ATTR_DEV1 not found\n");
		return -1;
	}

	if (!attrs[MCHP_FRER_ATTR_DEV2]) {
		printf("ATTR_DEV2 not found\n");
		return -1;
	}

	nla_memcpy(&cfg->iflow, attrs[MCHP_FRER_ATTR_IFLOW_CFG],
		   sizeof(struct mchp_iflow_cfg));
	cfg->ifindex1 = nla_get_u32(attrs[MCHP_FRER_ATTR_DEV1]);
	cfg->ifindex2 = nla_get_u32(attrs[MCHP_FRER_ATTR_DEV2]);

	return NL_OK;
}

static int mchp_frer_genl_iflow_cfg_get(u32 id,
					   struct mchp_iflow_cmb_cfg *cfg)
{
	RETURN_IF_PC;
	struct mchp_iflow_cmb_cfg tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
				MCHP_FRER_GENL_IFLOW_CFG_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_frer_genl_iflow_cfg_get_cb, &tmp);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_ID, id);

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

static char *mchp_frer_iflow_help(void)
{
	return "--ms_enable:              Enable member stream\n"
		" --ms_id:                  Allocated member stream ID\n"
		" --generation:             Enable sequence generation\n"
		" --pop:                    Enable popping of R-tag\n"
		" --dev1:                   Split device 1 or '-'\n"
		" --dev2:                   Split device 2 or '-'\n"
		" --help:                   Show this help text\n";
}

static int cmd_iflow(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"ms_enable", required_argument, NULL, 'a'},
		{"ms_id", required_argument, NULL, 'b'},
		{"generation", required_argument, NULL, 'c'},
		{"pop", required_argument, NULL, 'd'},
		{"dev1", required_argument, NULL, 'e'},
		{"dev2", required_argument, NULL, 'f'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	struct mchp_iflow_cmb_cfg cfg = {};
	struct mchp_iflow_cmb_cfg tmp;
	int do_help = 0;
	u32 id = 0;
	int ch;

	/* read the id */
	id = atoi(argv[0]);

	if (mchp_frer_genl_iflow_cfg_get(id, &cfg) < 0)
		return 0;

	memcpy(&tmp, &cfg, sizeof(cfg));

	while ((ch = getopt_long(argc, argv, "a:b:c:d:e:fh", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			cfg.iflow.frer.ms_enable = !!atoi(optarg);
			break;
		case 'b':
			cfg.iflow.frer.ms_id = atoi(optarg);
			break;
		case 'c':
			cfg.iflow.frer.generation = !!atoi(optarg);
			break;
		case 'd':
			cfg.iflow.frer.pop = !!atoi(optarg);
			break;
		case 'e':
			if (optarg[0] == '-') {
				cfg.ifindex1 = 0; /* Remove device */
			} else {
				cfg.ifindex1 = if_nametoindex(optarg);
				if (cfg.ifindex1 == 0) {
					fprintf(stderr, "%s: %s!\n", optarg, strerror(errno));
					return 1;
				}
			}
			break;
		case 'f':
			if (optarg[0] == '-') {
				cfg.ifindex2 = 0; /* Remove device */
			} else {
				cfg.ifindex2 = if_nametoindex(optarg);
				if (cfg.ifindex2 == 0) {
					fprintf(stderr, "%s: %s!\n", optarg, strerror(errno));
					return 1;
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
		char if1[IF_NAMESIZE] = {};
		char if2[IF_NAMESIZE] = {};
		if (!if_indextoname(cfg.ifindex1, if1))
			if1[0] = '-';
		if (!if_indextoname(cfg.ifindex2, if2))
			if2[0] = '-';
		printf("%-14s %8d\n", "ms_enable:", cfg.iflow.frer.ms_enable);
		printf("%-14s %8u\n", "ms_id:", cfg.iflow.frer.ms_id);
		printf("%-14s %8d\n", "generation:", cfg.iflow.frer.generation);
		printf("%-14s %8d\n", "pop:", cfg.iflow.frer.pop);
		printf("%-14s %8s\n", "dev1:", if1 ? if1 : "-");
		printf("%-14s %8s\n", "dev2:", if2 ? if2 : "-");
		return 0;
	}

	return mchp_frer_genl_iflow_cfg_set(id, &cfg);
}

/* cmd_vlan */
static int mchp_frer_genl_vlan_cfg_set(u32 vid,
					  const struct mchp_frer_vlan_cfg *cfg)
{
	RETURN_IF_PC;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
				MCHP_FRER_GENL_VLAN_CFG_SET, 1, &sk, &msg);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_ID, vid);
	NLA_PUT(msg, MCHP_FRER_ATTR_VLAN_CFG, sizeof(*cfg), cfg);

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

static int mchp_frer_genl_vlan_cfg_get_cb(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *hdr = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *attrs[MCHP_FRER_ATTR_END];
	struct mchp_frer_vlan_cfg *cfg = arg;

	if (nla_parse(attrs, MCHP_FRER_ATTR_MAX, genlmsg_attrdata(hdr, 0),
		      genlmsg_attrlen(hdr, 0), mchp_frer_genl_policy)) {
		printf("nla_parse() failed\n");
		return NL_STOP;
	}

	if (!attrs[MCHP_FRER_ATTR_VLAN_CFG]) {
		printf("ATTR_VLAN_CFG not found\n");
		return -1;
	}

	nla_memcpy(cfg, attrs[MCHP_FRER_ATTR_VLAN_CFG],
		   sizeof(struct mchp_frer_vlan_cfg));

	return NL_OK;
}

static int mchp_frer_genl_vlan_cfg_get(u32 vid,
					  struct mchp_frer_vlan_cfg *cfg)
{
	RETURN_IF_PC;
	struct mchp_frer_vlan_cfg tmp;
	struct nl_sock *sk;
	struct nl_msg *msg;
	int rc = 0;

	rc = mchp_genl_start(MCHP_FRER_NETLINK,
				MCHP_FRER_GENL_VLAN_CFG_GET, 1, &sk, &msg);

	nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			    mchp_frer_genl_vlan_cfg_get_cb, &tmp);

	NLA_PUT_U32(msg, MCHP_FRER_ATTR_ID, vid);

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

static char *mchp_frer_vlan_help(void)
{
	return "--flood_disable:          Disable flooding in VLAN\n"
		" --learn_disable:          Disable learning in VLAN\n"
		" --help:                   Show this help text\n";
}

static int cmd_vlan(const struct command *cmd, int argc, char *const *argv)
{
	static struct option long_options[] =
	{
		{"flood_disable", required_argument, NULL, 'a'},
		{"learn_disable", required_argument, NULL, 'b'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	struct mchp_frer_vlan_cfg cfg = {};
	struct mchp_frer_vlan_cfg tmp;
	int do_help = 0;
	u32 vid = 0;
	int ch;

	/* read the id */
	vid = atoi(argv[0]);

	if (mchp_frer_genl_vlan_cfg_get(vid, &cfg) < 0)
		return 0;

	memcpy(&tmp, &cfg, sizeof(cfg));

	while ((ch = getopt_long(argc, argv, "a:b:h", long_options, NULL)) != -1) {
		switch (ch) {
		case 'a':
			cfg.flood_disable = !!atoi(optarg);
			break;
		case 'b':
			cfg.learn_disable = !!atoi(optarg);
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
		printf("%-14s %8d\n", "flood_disable:", cfg.flood_disable);
		printf("%-14s %8d\n", "learn_disable:", cfg.learn_disable);
		return 0;
	}

	return mchp_frer_genl_vlan_cfg_set(vid, &cfg);
}

/* commands */
static const struct command commands[] =
{
	{1, "cs", cmd_cs, "cs cs_id [options]", mchp_frer_cs_help},
	{1, "msa", cmd_msa, "msa dev1 [dev2] [options]", mchp_frer_msa_help},
	{1, "msf", cmd_msf, "msf ms_id [options]", mchp_frer_msf_help},
	{2, "ms", cmd_ms, "ms dev ms_id [options]", mchp_frer_ms_help},
	{1, "iflow", cmd_iflow, "iflow id [options]", mchp_frer_iflow_help},
	{1, "vlan", cmd_vlan, "vlan vid [options]", mchp_frer_vlan_help},
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
	printf("Usage: frer cs|msa|msf|ms|iflow|vlan [options]\n");
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

	/* skip program name ('frer') */
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
