/*
 * License: Dual MIT/GPL
 * Copyright (c) 2026 Microchip Technology Inc. and its subsidiaries.
 *
 * Userspace control for the Microchip AFI fast-injection (DTI) netlink API
 * ("mchp_afi" generic netlink family). Ported from scripts/mchp_afi.py.
 */

#include <errno.h>
#include <getopt.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

#define AFI_NETLINK_NAME	"mchp_afi"
#define AFI_NETLINK_VERSION	1

enum afi_nl_attr {
	AFI_NL_ATTR_NONE,
	AFI_NL_ATTR_PAD,
	AFI_NL_ATTR_IFINDEX,
	AFI_NL_ATTR_FRAME,
	AFI_NL_ATTR_BPS,
	AFI_NL_ATTR_SEQ_CNT,
	AFI_NL_ATTR_PRIO,
	AFI_NL_ATTR_FRM_SIZE,
	AFI_NL_ATTR_FASTID,
	AFI_NL_ATTR_BPS_ACTUAL,
	AFI_NL_ATTR_STATE,
	AFI_NL_ATTR_PORT,
	AFI_NL_ATTR_SEQ_ACTUAL,

	__AFI_NL_ATTR_END,
	AFI_NL_ATTR_MAX = __AFI_NL_ATTR_END - 1,
};

enum afi_nl_cmd {
	AFI_NL_CMD_NONE,
	AFI_NL_CMD_FAST_START,
	AFI_NL_CMD_FAST_STOP,
	AFI_NL_CMD_FAST_GET,
};

#define ETH_ZLEN_NOFCS		60
#define ETH_WIRE_OVERHEAD	24
#define AFI_FRAME_MAX		10240

static const char *state_name(uint8_t state)
{
	switch (state) {
	case 0: return "free";
	case 1: return "stopped";
	case 2: return "started";
	default: return "?";
	}
}

struct nl_ctx {
	struct nl_sock *sk;
	int family;
};

static int cb_finish(struct nl_msg *msg, void *arg)
{
	int *pending = arg;

	*pending = 0;
	return NL_STOP;
}

static int cb_ack(struct nl_msg *msg, void *arg)
{
	int *pending = arg;

	*pending = 0;
	return NL_STOP;
}

static int cb_error(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg)
{
	int *pending = arg;

	*pending = err->error;
	return NL_STOP;
}

static int afi_talk(struct nl_ctx *ctx, struct nl_msg *msg,
		    nl_recvmsg_msg_cb_t valid_cb, void *valid_arg)
{
	int pending = 1;
	struct nl_cb *cb;
	int rc;

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		nlmsg_free(msg);
		return -ENOMEM;
	}

	if (valid_cb)
		nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, valid_cb, valid_arg);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, cb_finish, &pending);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, cb_ack, &pending);
	nl_cb_err(cb, NL_CB_CUSTOM, cb_error, &pending);

	rc = nl_send_auto(ctx->sk, msg);
	nlmsg_free(msg);
	if (rc < 0) {
		nl_cb_put(cb);
		return rc;
	}

	while (pending == 1) {
		rc = nl_recvmsgs(ctx->sk, cb);
		if (rc < 0 && pending == 1) {
			pending = rc;
			break;
		}
	}

	nl_cb_put(cb);
	return pending;
}

static void afi_perror(const char *what, int ret)
{
	fprintf(stderr, "error: %s: %s\n", what, strerror(-ret));
}

static struct nl_msg *afi_msg(struct nl_ctx *ctx, uint8_t cmd, int flags)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return NULL;

	if (!genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, ctx->family, 0,
			 flags, cmd, AFI_NETLINK_VERSION)) {
		nlmsg_free(msg);
		return NULL;
	}

	return msg;
}

static int afi_connect(struct nl_ctx *ctx)
{
	int rc;

	ctx->sk = nl_socket_alloc();
	if (!ctx->sk) {
		fprintf(stderr, "error: nl_socket_alloc failed\n");
		return -ENOMEM;
	}

	rc = genl_connect(ctx->sk);
	if (rc < 0) {
		fprintf(stderr, "error: genl_connect: %s\n", nl_geterror(rc));
		return rc;
	}

	rc = genl_ctrl_resolve(ctx->sk, AFI_NETLINK_NAME);
	if (rc < 0) {
		fprintf(stderr,
			"error: family '%s' not found (is the driver loaded?)\n",
			AFI_NETLINK_NAME);
		return rc;
	}
	ctx->family = rc;

	return 0;
}

static int parse_hex(const char *text, unsigned char *out, size_t outsz,
		     size_t *out_len)
{
	size_t n = 0;
	int hi = -1;

	for (; *text; text++) {
		int v;
		char c = *text;

		if (c == ':' || c == ' ' || c == '\n' || c == '\t' || c == '\r')
			continue;
		if (c >= '0' && c <= '9')
			v = c - '0';
		else if (c >= 'a' && c <= 'f')
			v = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			v = c - 'A' + 10;
		else
			return -EINVAL;

		if (hi < 0) {
			hi = v;
		} else {
			if (n >= outsz)
				return -E2BIG;
			out[n++] = (hi << 4) | v;
			hi = -1;
		}
	}

	if (hi >= 0)
		return -EINVAL;

	*out_len = n;
	return 0;
}

static int ef_spec_to_hex(const char *spec, char *out, size_t outsz)
{
	char cmd[4096];
	size_t n = 0;
	FILE *fp;
	int rc;

	rc = snprintf(cmd, sizeof(cmd), "ef hex %s 2>/dev/null", spec);
	if (rc < 0 || (size_t)rc >= sizeof(cmd))
		return -E2BIG;

	fp = popen(cmd, "r");
	if (!fp) {
		fprintf(stderr, "error: cannot run 'ef' to parse frame spec\n");
		return -errno;
	}

	n = fread(out, 1, outsz - 1, fp);
	out[n] = '\0';

	rc = pclose(fp);
	if (rc != 0) {
		fprintf(stderr,
			"error: 'ef hex %s' failed (not 0x-hex and ef could not parse it)\n",
			spec);
		return -EINVAL;
	}

	return 0;
}

static int load_frame(const char *frame, const char *frame_file,
		      unsigned char *buf, size_t bufsz, size_t *out_len)
{
	if (frame_file) {
		FILE *fp = fopen(frame_file, "rb");
		size_t n;

		if (!fp) {
			afi_perror(frame_file, -errno);
			return -errno;
		}
		n = fread(buf, 1, bufsz, fp);
		fclose(fp);
		*out_len = n;
		return 0;
	}

	if (frame[0] == '0' && (frame[1] == 'x' || frame[1] == 'X'))
		return parse_hex(frame + 2, buf, bufsz, out_len);

	char hex[AFI_FRAME_MAX * 2 + 1];
	int rc = ef_spec_to_hex(frame, hex, sizeof(hex));

	if (rc)
		return rc;
	return parse_hex(hex, buf, bufsz, out_len);
}

struct start_reply {
	bool have_id;
	uint32_t fastid;
	uint64_t bps_actual;
	bool have_seq;
	uint32_t seq_actual;
};

static int start_cb(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[AFI_NL_ATTR_MAX + 1];
	struct start_reply *r = arg;

	if (nla_parse(tb, AFI_NL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		      genlmsg_attrlen(gnlh, 0), NULL) < 0)
		return NL_STOP;

	if (tb[AFI_NL_ATTR_FASTID]) {
		r->fastid = nla_get_u32(tb[AFI_NL_ATTR_FASTID]);
		r->have_id = true;
	}
	if (tb[AFI_NL_ATTR_BPS_ACTUAL])
		r->bps_actual = nla_get_u64(tb[AFI_NL_ATTR_BPS_ACTUAL]);
	if (tb[AFI_NL_ATTR_SEQ_ACTUAL]) {
		r->seq_actual = nla_get_u32(tb[AFI_NL_ATTR_SEQ_ACTUAL]);
		r->have_seq = true;
	}

	return NL_OK;
}

static int flow_cb(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[AFI_NL_ATTR_MAX + 1];
	int *count = arg;

	if (nla_parse(tb, AFI_NL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		      genlmsg_attrlen(gnlh, 0), NULL) < 0)
		return NL_STOP;

	printf("id=%-3u port=%-2u prio=%u state=%-7s seq_cnt=%-10u rate=%llu bps\n",
	       tb[AFI_NL_ATTR_FASTID] ? nla_get_u32(tb[AFI_NL_ATTR_FASTID]) : 0,
	       tb[AFI_NL_ATTR_PORT] ? nla_get_u32(tb[AFI_NL_ATTR_PORT]) : 0,
	       tb[AFI_NL_ATTR_PRIO] ? nla_get_u8(tb[AFI_NL_ATTR_PRIO]) : 0,
	       state_name(tb[AFI_NL_ATTR_STATE] ?
			  nla_get_u8(tb[AFI_NL_ATTR_STATE]) : 0),
	       tb[AFI_NL_ATTR_SEQ_CNT] ? nla_get_u32(tb[AFI_NL_ATTR_SEQ_CNT]) : 0,
	       (unsigned long long)(tb[AFI_NL_ATTR_BPS_ACTUAL] ?
				    nla_get_u64(tb[AFI_NL_ATTR_BPS_ACTUAL]) : 0));

	if (count)
		(*count)++;
	return NL_OK;
}

struct id_list {
	uint32_t *ids;
	size_t n;
	size_t cap;
};

static int collect_cb(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[AFI_NL_ATTR_MAX + 1];
	struct id_list *l = arg;

	if (nla_parse(tb, AFI_NL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		      genlmsg_attrlen(gnlh, 0), NULL) < 0)
		return NL_STOP;
	if (!tb[AFI_NL_ATTR_FASTID])
		return NL_OK;

	if (l->n == l->cap) {
		size_t cap = l->cap ? l->cap * 2 : 16;
		uint32_t *ids = realloc(l->ids, cap * sizeof(*ids));

		if (!ids)
			return NL_STOP;
		l->ids = ids;
		l->cap = cap;
	}
	l->ids[l->n++] = nla_get_u32(tb[AFI_NL_ATTR_FASTID]);
	return NL_OK;
}

struct seq_reply {
	bool found;
	uint32_t seq_cnt;
};

static int seq_cb(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[AFI_NL_ATTR_MAX + 1];
	struct seq_reply *r = arg;

	if (nla_parse(tb, AFI_NL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		      genlmsg_attrlen(gnlh, 0), NULL) < 0)
		return NL_STOP;

	if (tb[AFI_NL_ATTR_SEQ_CNT]) {
		r->seq_cnt = nla_get_u32(tb[AFI_NL_ATTR_SEQ_CNT]);
		r->found = true;
	}
	return NL_OK;
}

struct start_args {
	const char *ifname;
	const char *frame;
	const char *frame_file;
	uint64_t bps;
	uint32_t seq;
	uint8_t prio;
	long long frm_size;
};

static int cmd_start(struct nl_ctx *ctx, const struct start_args *a)
{
	unsigned char frame[AFI_FRAME_MAX];
	struct start_reply reply = {};
	uint32_t ifindex, frm_size;
	struct nl_msg *msg;
	size_t frame_len;
	int rc;

	ifindex = if_nametoindex(a->ifname);
	if (!ifindex) {
		afi_perror(a->ifname, -errno);
		return -1;
	}

	rc = load_frame(a->frame, a->frame_file, frame, sizeof(frame),
			&frame_len);
	if (rc) {
		if (rc == -E2BIG)
			fprintf(stderr, "error: frame exceeds %d bytes\n",
				AFI_FRAME_MAX);
		else if (rc == -EINVAL)
			fprintf(stderr, "error: invalid hex frame\n");
		return -1;
	}

	if (frame_len < ETH_ZLEN_NOFCS) {
		memset(frame + frame_len, 0, ETH_ZLEN_NOFCS - frame_len);
		frame_len = ETH_ZLEN_NOFCS;
	}

	if (a->frm_size >= 0)
		frm_size = (uint32_t)a->frm_size;
	else
		frm_size = frame_len + ETH_WIRE_OVERHEAD;

	msg = afi_msg(ctx, AFI_NL_CMD_FAST_START, NLM_F_REQUEST | NLM_F_ACK);
	if (!msg)
		return -1;

	if (nla_put_u32(msg, AFI_NL_ATTR_IFINDEX, ifindex) ||
	    nla_put(msg, AFI_NL_ATTR_FRAME, frame_len, frame) ||
	    nla_put_u64(msg, AFI_NL_ATTR_BPS, a->bps) ||
	    nla_put_u32(msg, AFI_NL_ATTR_SEQ_CNT, a->seq) ||
	    nla_put_u8(msg, AFI_NL_ATTR_PRIO, a->prio) ||
	    nla_put_u32(msg, AFI_NL_ATTR_FRM_SIZE, frm_size)) {
		nlmsg_free(msg);
		fprintf(stderr, "error: building message failed\n");
		return -1;
	}

	rc = afi_talk(ctx, msg, start_cb, &reply);
	if (rc) {
		afi_perror("start", rc);
		return -1;
	}

	if (!reply.have_id) {
		printf("started (no reply payload)\n");
		return 0;
	}

	printf("started flow id=%u on %s, actual rate=%llu bps%s",
	       reply.fastid, a->ifname,
	       (unsigned long long)reply.bps_actual,
	       reply.bps_actual == 0 ? " (line rate)" : "");
	if (a->seq) {
		if (reply.have_seq)
			printf(", seq=%u frames", reply.seq_actual);
		else
			printf(" (seq_actual unavailable)");
	}
	printf("\n");
	return 0;
}

static int stop_one(struct nl_ctx *ctx, uint32_t ifindex, const char *ifname,
		    uint32_t fastid)
{
	struct nl_msg *msg;
	int rc;

	msg = afi_msg(ctx, AFI_NL_CMD_FAST_STOP, NLM_F_REQUEST | NLM_F_ACK);
	if (!msg)
		return -1;

	if (nla_put_u32(msg, AFI_NL_ATTR_IFINDEX, ifindex) ||
	    nla_put_u32(msg, AFI_NL_ATTR_FASTID, fastid)) {
		nlmsg_free(msg);
		return -1;
	}

	rc = afi_talk(ctx, msg, NULL, NULL);
	if (rc) {
		afi_perror("stop", rc);
		return -1;
	}

	printf("stopped flow id=%u on %s\n", fastid, ifname);
	return 0;
}

static int cmd_stop(struct nl_ctx *ctx, const char *ifname, long long id)
{
	struct id_list list = {};
	struct nl_msg *msg;
	uint32_t ifindex;
	int rc, ret = 0;
	size_t i;

	ifindex = if_nametoindex(ifname);
	if (!ifindex) {
		afi_perror(ifname, -errno);
		return -1;
	}

	if (id >= 0)
		return stop_one(ctx, ifindex, ifname, (uint32_t)id);

	msg = afi_msg(ctx, AFI_NL_CMD_FAST_GET, NLM_F_REQUEST | NLM_F_DUMP);
	if (!msg)
		return -1;

	rc = afi_talk(ctx, msg, collect_cb, &list);
	if (rc) {
		afi_perror("list", rc);
		free(list.ids);
		return -1;
	}

	if (list.n == 0) {
		printf("no flows to stop\n");
		return 0;
	}

	for (i = 0; i < list.n; i++) {
		if (stop_one(ctx, ifindex, ifname, list.ids[i]))
			ret = -1;
	}

	free(list.ids);
	return ret;
}

static int cmd_get(struct nl_ctx *ctx, const char *ifname, uint32_t id)
{
	struct nl_msg *msg;
	uint32_t ifindex;
	int rc;

	ifindex = if_nametoindex(ifname);
	if (!ifindex) {
		afi_perror(ifname, -errno);
		return -1;
	}

	msg = afi_msg(ctx, AFI_NL_CMD_FAST_GET, NLM_F_REQUEST | NLM_F_ACK);
	if (!msg)
		return -1;

	if (nla_put_u32(msg, AFI_NL_ATTR_IFINDEX, ifindex) ||
	    nla_put_u32(msg, AFI_NL_ATTR_FASTID, id)) {
		nlmsg_free(msg);
		return -1;
	}

	rc = afi_talk(ctx, msg, flow_cb, NULL);
	if (rc) {
		afi_perror("get", rc);
		return -1;
	}

	return 0;
}

static int cmd_list(struct nl_ctx *ctx)
{
	struct nl_msg *msg;
	int count = 0;
	int rc;

	msg = afi_msg(ctx, AFI_NL_CMD_FAST_GET, NLM_F_REQUEST | NLM_F_DUMP);
	if (!msg)
		return -1;

	rc = afi_talk(ctx, msg, flow_cb, &count);
	if (rc) {
		afi_perror("list", rc);
		return -1;
	}

	if (count == 0)
		printf("no flows configured\n");
	return 0;
}

static double now_sec(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void sleep_sec(double sec)
{
	struct timespec ts;

	if (sec <= 0)
		return;
	ts.tv_sec = (time_t)sec;
	ts.tv_nsec = (long)((sec - ts.tv_sec) * 1e9);
	nanosleep(&ts, NULL);
}

static int cmd_wait_done(struct nl_ctx *ctx, const char *ifname, uint32_t id,
			 double timeout, double poll)
{
	uint32_t ifindex;
	double deadline;
	long long last = -1;

	ifindex = if_nametoindex(ifname);
	if (!ifindex) {
		afi_perror(ifname, -errno);
		return -1;
	}

	deadline = now_sec() + timeout;

	for (;;) {
		struct seq_reply reply = {};
		long long cnt = -1;
		struct nl_msg *msg;
		int rc;

		msg = afi_msg(ctx, AFI_NL_CMD_FAST_GET,
			      NLM_F_REQUEST | NLM_F_ACK);
		if (!msg)
			return -1;
		if (nla_put_u32(msg, AFI_NL_ATTR_IFINDEX, ifindex) ||
		    nla_put_u32(msg, AFI_NL_ATTR_FASTID, id)) {
			nlmsg_free(msg);
			return -1;
		}

		rc = afi_talk(ctx, msg, seq_cb, &reply);
		if (rc == -ENOENT) {
			cnt = -1;
		} else if (rc) {
			afi_perror("wait-done", rc);
			return -1;
		} else if (reply.found) {
			cnt = reply.seq_cnt;
		}

		if (cnt != last) {
			if (cnt < 0)
				printf("id=%u seq_cnt=gone\n", id);
			else
				printf("id=%u seq_cnt=%lld\n", id, cnt);
			last = cnt;
		}

		if (cnt <= 0) {
			printf("flow id=%u done\n", id);
			return 0;
		}

		if (now_sec() >= deadline) {
			printf("flow id=%u TIMEOUT seq_cnt=%lld after %gs\n",
			       id, cnt, timeout);
			return 1;
		}

		sleep_sec(poll);
	}
}

static void usage(const char *prog)
{
	printf(
"usage: %s <command> [args]\n"
"\n"
"Drive the Microchip AFI fast-injection (DTI) hardware frame generator.\n"
"\n"
"commands:\n"
"  start <ifname> (--frame HEX | --frame-file FILE) [--bps N] [--seq N]\n"
"                 [--prio N] [--frm-size N]\n"
"      Start a generator on <ifname>. --frame is a 0x-prefixed hex frame or an\n"
"      easyframes spec (converted via 'ef hex'); --frame-file is a raw frame.\n"
"      --bps line rate in bits/s (0 = line speed, default).\n"
"      --seq frames to inject (0 = forever, default); actual count is reported.\n"
"      --prio egress priority (default 0).\n"
"      --frm-size bytes per frame for the rate calc (default frame + %d for\n"
"                 FCS + preamble + IFG, so --bps is a true line rate).\n"
"\n"
"  stop <ifname> [--id N]\n"
"      Stop and free flow N, or every configured flow if --id is omitted.\n"
"\n"
"  get <ifname> --id N\n"
"      Show one flow.\n"
"\n"
"  list\n"
"      List all configured flows.\n"
"\n"
"  wait-done <ifname> --id N [--timeout S] [--poll S]\n"
"      Poll a finite-seq flow until injected (seq_cnt=0). Exits non-zero on\n"
"      timeout (default 30s, poll 0.2s).\n",
		prog, ETH_WIRE_OVERHEAD);
}

enum {
	OPT_FRAME = 256,
	OPT_FRAME_FILE,
	OPT_BPS,
	OPT_SEQ,
	OPT_PRIO,
	OPT_FRM_SIZE,
	OPT_ID,
	OPT_TIMEOUT,
	OPT_POLL,
};

static const struct option long_options[] = {
	{ "frame",	required_argument, NULL, OPT_FRAME },
	{ "frame-file",	required_argument, NULL, OPT_FRAME_FILE },
	{ "bps",	required_argument, NULL, OPT_BPS },
	{ "seq",	required_argument, NULL, OPT_SEQ },
	{ "prio",	required_argument, NULL, OPT_PRIO },
	{ "frm-size",	required_argument, NULL, OPT_FRM_SIZE },
	{ "id",		required_argument, NULL, OPT_ID },
	{ "timeout",	required_argument, NULL, OPT_TIMEOUT },
	{ "poll",	required_argument, NULL, OPT_POLL },
	{ "help",	no_argument,	   NULL, 'h' },
	{ NULL, 0, NULL, 0 },
};

int main(int argc, char *argv[])
{
	struct start_args sa = { .frm_size = -1 };
	double timeout = 30.0, poll = 0.2;
	const char *ifname = NULL;
	long long id = -1;
	struct nl_ctx ctx;
	const char *action;
	int ret, ch;

	setvbuf(stdout, NULL, _IOLBF, 0);

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	action = argv[1];
	if (strcmp(action, "-h") == 0 || strcmp(action, "--help") == 0) {
		usage(argv[0]);
		return 0;
	}

	if (strcmp(action, "start") != 0 && strcmp(action, "stop") != 0 &&
	    strcmp(action, "get") != 0 && strcmp(action, "wait-done") != 0 &&
	    strcmp(action, "list") != 0) {
		fprintf(stderr, "error: unknown command '%s'\n", action);
		usage(argv[0]);
		return 1;
	}

	if (strcmp(action, "list") != 0) {
		if (argc < 3 || argv[2][0] == '-') {
			fprintf(stderr, "error: '%s' needs an interface name\n",
				action);
			return 1;
		}
		ifname = argv[2];
		optind = 3;
	} else {
		optind = 2;
	}

	while ((ch = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
		switch (ch) {
		case OPT_FRAME:		sa.frame = optarg; break;
		case OPT_FRAME_FILE:	sa.frame_file = optarg; break;
		case OPT_BPS:		sa.bps = strtoull(optarg, NULL, 0); break;
		case OPT_SEQ:		sa.seq = strtoul(optarg, NULL, 0); break;
		case OPT_PRIO:		sa.prio = strtoul(optarg, NULL, 0); break;
		case OPT_FRM_SIZE:	sa.frm_size = strtoll(optarg, NULL, 0); break;
		case OPT_ID:		id = strtoll(optarg, NULL, 0); break;
		case OPT_TIMEOUT:	timeout = strtod(optarg, NULL); break;
		case OPT_POLL:		poll = strtod(optarg, NULL); break;
		case 'h':		usage(argv[0]); return 0;
		default:		usage(argv[0]); return 1;
		}
	}

	sa.ifname = ifname;

	if (strcmp(action, "start") == 0 && !sa.frame == !sa.frame_file) {
		fprintf(stderr,
			"error: start needs exactly one of --frame / --frame-file\n");
		return 1;
	}
	if ((strcmp(action, "get") == 0 || strcmp(action, "wait-done") == 0) &&
	    id < 0) {
		fprintf(stderr, "error: %s needs --id\n", action);
		return 1;
	}

	if (afi_connect(&ctx))
		return 1;

	if (strcmp(action, "start") == 0)
		ret = cmd_start(&ctx, &sa) ? 1 : 0;
	else if (strcmp(action, "stop") == 0)
		ret = cmd_stop(&ctx, ifname, id) ? 1 : 0;
	else if (strcmp(action, "get") == 0)
		ret = cmd_get(&ctx, ifname, (uint32_t)id) ? 1 : 0;
	else if (strcmp(action, "wait-done") == 0)
		ret = cmd_wait_done(&ctx, ifname, (uint32_t)id, timeout, poll);
	else
		ret = cmd_list(&ctx) ? 1 : 0;

	nl_socket_free(ctx.sk);
	return ret;
}
