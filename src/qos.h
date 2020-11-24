/*
 * License: Dual MIT/GPL
 * Copyright (c) 2020 Microchip Corporation
 */

#ifndef _QOS_H_
#define _QOS_H_

#include "kernel_types.h"

/* The following defs must match the corresponding defs in the kernel */
#define LAN966X_QOS_NETLINK "lan966x_qos_nl"

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

struct lan966x_qos_port_cfg {
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

#endif /* _QOS_H_ */
