/*
 * License: Dual MIT/GPL
 * Copyright (c) 2020 Microchip Corporation
 */

#ifndef _FRER_H_
#define _FRER_H_

#include "kernel_types.h"

/* The following defs must match the corresponding defs in the kernel */
#define LAN966X_FRER_NETLINK "lan966x_frer_nl"

#define LAN966X_FRER_MAX_PORTS     2 /* Max # of ports for split and mstreams */

/* FRER recovery counters */
struct lan966x_frer_cnt {
	u64 out_of_order_packets; /* frerCpsSeqRcvyOutOfOrderPackets */
	u64 rogue_packets;        /* frerCpsSeqRcvyRoguePackets */
	u64 passed_packets;       /* frerCpsSeqRcvyPassedPackets */
	u64 discarded_packets;    /* frerCpsSeqRcvyDiscardedPackets */
	u64 lost_packets;         /* frerCpsSeqRcvyLostPackets */
	u64 tagless_packets;      /* frerCpsSeqRcvyTaglessPackets */
	u64 resets;               /* frerCpsSeqRcvyResets */
};

/* FRER recovery algorithm */
enum lan966x_frer_rec_alg {
	LAN966X_FRER_REC_ALG_VECTOR, /* Vector recovery */
	LAN966X_FRER_REC_ALG_MATCH,  /* Match recovery */
};

/* FRER member/compound stream configuration */
struct lan966x_frer_stream_cfg {
	bool                      enable;      /* Enable/disable recovery */
	enum lan966x_frer_rec_alg alg;         /* frerSeqRcvyAlgorithm */
	u8                        hlen;        /* frerSeqRcvyHistoryLength */
	u16                       reset_time;  /* frerSeqRcvyResetMSec */
	bool                      take_no_seq; /* frerSeqRcvyTakeNoSequence */
	u16                       cs_id; /* Compound stream ID (mstream only) */
};

/* FRER ingress flow configuration */
struct lan966x_frer_iflow_cfg {
	bool ms_enable;  /* Enable member stream */
	u16 ms_id;       /* Member stream base ID */
	bool generation; /* Enable/disable sequence generation */
	bool pop;        /* Pop R-tag */
	u8 split_mask;   /* Egress ports to split into */
};

/* Ingress flow configuration */
struct lan966x_iflow_cfg {
	struct lan966x_frer_iflow_cfg frer; /* FRER iflow configuration */
};

/* VLAN configuration */
struct lan966x_frer_vlan_cfg {
	bool flood_disable; /* Disable flooding */
	bool learn_disable; /* Disable learning */
};

#endif /* _FRER_H_ */
