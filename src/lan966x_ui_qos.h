/*
 * License: Dual MIT/GPL
 * Copyright (c) 2020 Microchip Corporation
 */

#ifndef _QOS_H_
#define _QOS_H_

#define LAN966X_QOS_NETLINK "lan966x_qos_nl"

enum lan966x_qos_attr {
	LAN966X_QOS_ATTR_NONE,
	LAN966X_QOS_ATTR_DEV,
	LAN966X_QOS_ATTR_PORT_CFG,
	LAN966X_QOS_ATTR_DSCP,
	LAN966X_QOS_ATTR_DSCP_PRIO_DPL,

	/* This must be the last entry */
	LAN966X_QOS_ATTR_END,
};

#define LAN966X_QOS_ATTR_MAX (LAN966X_QOS_ATTR_END - 1)

enum lan966x_qos_genl {
	LAN966X_QOS_GENL_PORT_CFG_SET,
	LAN966X_QOS_GENL_PORT_CFG_GET,
	LAN966X_QOS_GENL_DSCP_PRIO_DPL_SET,
	LAN966X_QOS_GENL_DSCP_PRIO_DPL_GET,
};

/* QOS port configuration */
#define DEI_COUNT 2
#define DPL_COUNT 2
#define PCP_COUNT 8
#define PRIO_COUNT 8

struct lan966x_qos_i_mode {
	bool tag_map_enable;
	bool dhcp_map_enable;
};

enum lan966x_qos_e_mode{
	LAN966X_E_MODE_CLASSIFIED = 0, /* Use ingress classified PCP,DEI as TAG PCP,DEI */
	LAN966X_E_MODE_DEFAULT    = 2, /* Use default PCP,DEI as TAG PCP,DEI */
	LAN966X_E_MODE_MAPPED     = 3  /* Use mapped (SKB)Priority and DPL as TAG PCP,DEI */
};

struct lan966x_pcp_dei_prio_dpl {
	u8 prio;
	u8 dpl;
};

struct lan966x_prio_dpl_pcp_dei {
	u8 pcp;
	u8 dei;
};

struct lan966x_qos_port_conf {
	u8 i_default_prio;
	u8 i_default_dpl;
	u8 i_default_pcp;
	u8 i_default_dei;
	struct lan966x_pcp_dei_prio_dpl i_pcp_dei_prio_dpl_map[PCP_COUNT][DEI_COUNT];
	struct lan966x_qos_i_mode i_mode;

	u8 e_default_pcp;
	u8 e_default_dei;
	struct lan966x_prio_dpl_pcp_dei e_prio_dpl_pcp_dei_map[PRIO_COUNT][DPL_COUNT];
	enum lan966x_qos_e_mode e_mode;
};

/* QOS DSCP configuration */
struct lan966x_qos_dscp_prio_dpl {
	bool trust; /* Only trusted DSCP values are used for QOS class and DP level classification  */
	u8 prio;
	u8 dpl;
};



#define LAN966X_FRER_NETLINK "lan966x_frer_nl"

enum lan966x_frer_attr {
	LAN966X_FRER_ATTR_NONE,
	LAN966X_FRER_ATTR_ID,
	LAN966X_FRER_ATTR_DEV1,
	LAN966X_FRER_ATTR_DEV2,
	LAN966X_FRER_ATTR_STREAM_CFG,
	LAN966X_FRER_ATTR_STREAM_CNT,
	LAN966X_FRER_ATTR_IFLOW_CFG,
	LAN966X_FRER_ATTR_VLAN_CFG,

	/* This must be the last entry */
	LAN966X_FRER_ATTR_END,
};

#define LAN966X_FRER_ATTR_MAX (LAN966X_FRER_ATTR_END - 1)

enum lan966x_frer_genl {
	LAN966X_FRER_GENL_CS_CFG_SET,
	LAN966X_FRER_GENL_CS_CFG_GET,
	LAN966X_FRER_GENL_CS_CNT_GET,
	LAN966X_FRER_GENL_CS_CNT_CLR,
	LAN966X_FRER_GENL_MS_ALLOC,
	LAN966X_FRER_GENL_MS_FREE,
	LAN966X_FRER_GENL_MS_CFG_SET,
	LAN966X_FRER_GENL_MS_CFG_GET,
	LAN966X_FRER_GENL_MS_CNT_GET,
	LAN966X_FRER_GENL_MS_CNT_CLR,
	LAN966X_FRER_GENL_IFLOW_CFG_SET,
	LAN966X_FRER_GENL_IFLOW_CFG_GET,
	LAN966X_FRER_GENL_VLAN_CFG_SET,
	LAN966X_FRER_GENL_VLAN_CFG_GET,
};

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

#endif /* _QOS_H_ */
