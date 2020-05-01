/*
 * License: Dual MIT/GPL
 * Copyright (c) 2020 Microchip Corporation
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdbool.h>
#include <stdint.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

int lan966x_genl_start(const char *family_name, uint8_t cmd,
		       uint8_t version, struct nl_sock **skp,
		       struct nl_msg **msgp);

#endif /* _COMMON_H_ */
